#include "PCH.h"

#include "ui/ModalManager.h"
#include "ui/UIManager.h"

#include <algorithm>
#include <format>
#include <utility>
#include <vector>

namespace OStimNet::UI {

    ModalManager& ModalManager::GetSingleton() {
        static ModalManager instance;
        return instance;
    }

    std::string ModalManager::ShowModal(ModalRequest request, ModalCallback callback) {
        std::string modalId = request.modalId;
        if (modalId.empty()) {
            modalId = std::format("modal_{}", nextId_.fetch_add(1, std::memory_order_relaxed));
        }

        std::vector<std::string> buttonIds;
        buttonIds.reserve(request.buttons.size());
        for (const auto& btn : request.buttons) {
            buttonIds.push_back(btn.id);
        }

        const bool hasTextInput = std::any_of(
            request.controls.begin(),
            request.controls.end(),
            [](const ModalControl& ctrl) { return ctrl.type == ModalControlType::TextInput; });

        const nlohmann::json spec = request.ToJson(modalId);
        std::string specJson = spec.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
        return EnqueueEntry(
            std::move(modalId),
            std::move(specJson),
            std::move(buttonIds),
            hasTextInput,
            std::move(callback));
    }

    std::string ModalManager::ShowRawJsonModal(
        const std::string& modalSpecJson,
        ModalCallback callback) {
        nlohmann::json parsed = nlohmann::json::parse(modalSpecJson, nullptr, false);
        if (parsed.is_discarded() || !parsed.is_object()) {
            SKSE::log::error("[ModalManager] Rejected malformed raw modal JSON payload");
            if (callback) {
                ModalResponse errResp;
                errResp.aborted = true;
                callback(errResp);
            }
            return {};
        }

        std::string modalId;
        if (auto it = parsed.find("modalId"); it != parsed.end() && it->is_string()) {
            modalId = it->get<std::string>();
        }
        if (modalId.empty()) {
            modalId = std::format("modal_{}", nextId_.fetch_add(1, std::memory_order_relaxed));
            parsed["modalId"] = modalId;
        }

        std::vector<std::string> buttonIds;
        if (auto it = parsed.find("buttons"); it != parsed.end() && it->is_array()) {
            buttonIds.reserve(it->size());
            for (const auto& btn : *it) {
                if (btn.is_object()) {
                    if (auto idIt = btn.find("id"); idIt != btn.end() && idIt->is_string()) {
                        buttonIds.push_back(idIt->get<std::string>());
                    } else {
                        buttonIds.emplace_back();
                    }
                }
            }
        }

        bool hasTextInput = false;
        if (auto it = parsed.find("controls"); it != parsed.end() && it->is_array()) {
            for (const auto& ctrl : *it) {
                if (ctrl.is_object()) {
                    if (auto typeIt = ctrl.find("type");
                        typeIt != ctrl.end() && typeIt->is_string() &&
                        typeIt->get_ref<const std::string&>() == "text") {
                        hasTextInput = true;
                        break;
                    }
                }
            }
        }

        std::string dumped = parsed.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
        return EnqueueEntry(
            std::move(modalId),
            std::move(dumped),
            std::move(buttonIds),
            hasTextInput,
            std::move(callback));
    }

    std::string ModalManager::EnqueueEntry(
        std::string modalId,
        std::string specJson,
        std::vector<std::string> buttonIds,
        bool hasTextInput,
        ModalCallback callback) {
        ModalEntry entry;
        entry.modalId = modalId;
        entry.specJson = std::move(specJson);
        entry.buttonIds = std::move(buttonIds);
        entry.hasTextInput = hasTextInput;
        entry.callback = std::move(callback);
        entry.expectedEpoch = GetSessionEpoch();

        bool pushedActive = false;
        std::size_t queueSize = 0;
        {
            std::lock_guard lock(mutex_);
            if (!activeModal_.has_value()) {
                activeModal_ = std::move(entry);
                lastSentModalId_.clear();
                pushedActive = true;
            } else {
                queue_.push_back(std::move(entry));
                queueSize = queue_.size();
            }
        }

        if (pushedActive) {
            SKSE::log::debug("[ModalManager] Activated modal '{}'", modalId);
        } else {
            SKSE::log::debug("[ModalManager] Queued modal '{}' (pending={})", modalId, queueSize);
        }

        ScheduleProcessQueue();
        return modalId;
    }

    void ModalManager::ScheduleProcessQueue() {
        AddCurrentEpochTask([]() {
            ModalManager::GetSingleton().ProcessQueueOnGameThread();
        });
    }

    bool ModalManager::HasActiveModal() const {
        std::lock_guard lock(mutex_);
        return activeModal_.has_value();
    }

    bool ModalManager::ActiveModalHasTextInput() const {
        std::lock_guard lock(mutex_);
        return activeModal_.has_value() && activeModal_->hasTextInput;
    }

    std::string ModalManager::GetActiveModalId() const {
        std::lock_guard lock(mutex_);
        return activeModal_ ? activeModal_->modalId : std::string{};
    }

    void ModalManager::SendActiveModalToUI(bool forceResend) {
        std::string specJson;
        std::string modalId;
        {
            std::lock_guard lock(mutex_);
            if (!activeModal_.has_value()) {
                return;
            }
            if (!forceResend && lastSentModalId_ == activeModal_->modalId) {
                return;
            }
            modalId = activeModal_->modalId;
            specJson = activeModal_->specJson;
            lastSentModalId_ = modalId;
        }

        SKSE::log::debug("[ModalManager] Sending modal '{}' to UI (force={})", modalId, forceResend);
        UIManager::GetSingleton().SendToUI("showOStimNetModal", specJson);
    }

    void ModalManager::ProcessQueueOnGameThread() {
        bool hasActive = false;
        std::string promotedId;
        {
            std::lock_guard lock(mutex_);
            if (!activeModal_.has_value() && !queue_.empty()) {
                activeModal_ = std::move(queue_.front());
                queue_.pop_front();
                lastSentModalId_.clear();
                promotedId = activeModal_->modalId;
            }
            hasActive = activeModal_.has_value();
        }

        if (!promotedId.empty()) {
            SKSE::log::debug("[ModalManager] Promoted queued modal '{}' to active", promotedId);
        }

        auto& uiMgr = UIManager::GetSingleton();
        if (hasActive && !uiMgr.IsModalDisplayBlocked()) {
            SendActiveModalToUI(false);
        }
        uiMgr.SyncViewVisibilityAndFocus();
    }

    bool ModalManager::HandleModalResponse(const nlohmann::json& payload) {
        if (!payload.is_object()) {
            SKSE::log::warn("[ModalManager] Ignored non-object modalResponse payload");
            return false;
        }

        std::string incomingId;
        if (auto it = payload.find("modalId"); it != payload.end() && it->is_string()) {
            incomingId = it->get<std::string>();
        }
        if (incomingId.empty()) {
            SKSE::log::warn("[ModalManager] Ignored modalResponse missing valid string modalId");
            return false;
        }

        std::string buttonId;
        if (auto it = payload.find("buttonId"); it != payload.end() && it->is_string()) {
            buttonId = it->get<std::string>();
        }

        std::int32_t buttonIndex = -1;
        if (auto it = payload.find("buttonIndex"); it != payload.end() && it->is_number_integer()) {
            buttonIndex = it->get<std::int32_t>();
        }

        bool aborted = false;
        if (auto it = payload.find("aborted"); it != payload.end() && it->is_boolean()) {
            aborted = it->get<bool>();
        }

        nlohmann::json values = nlohmann::json::object();
        if (auto it = payload.find("values"); it != payload.end() && it->is_object()) {
            values = *it;
        }

        ModalEntry resolved;
        {
            std::lock_guard lock(mutex_);
            if (!activeModal_.has_value() || activeModal_->modalId != incomingId) {
                SKSE::log::warn("[ModalManager] Ignored modalResponse for inactive/mismatched modal '{}'",
                                incomingId);
                return false;
            }
            resolved = std::move(*activeModal_);
            activeModal_.reset();
            lastSentModalId_.clear();
        }

        SKSE::log::debug("[ModalManager] Resolved modal '{}' (button='{}', aborted={})",
                         resolved.modalId,
                         buttonId,
                         aborted);

        UIManager::GetSingleton().SendToUI("clearOStimNetModal", "");
        ScheduleProcessQueue();

        if (resolved.callback) {
            if (IsSessionEpochCurrent(resolved.expectedEpoch)) {
                if (buttonIndex < 0 && !buttonId.empty()) {
                    for (std::size_t i = 0; i < resolved.buttonIds.size(); ++i) {
                        if (resolved.buttonIds[i] == buttonId) {
                            buttonIndex = static_cast<std::int32_t>(i);
                            break;
                        }
                    }
                }
                ModalResponse response;
                response.modalId = std::move(resolved.modalId);
                response.buttonId = std::move(buttonId);
                response.buttonIndex = buttonIndex;
                response.aborted = aborted;
                response.values = std::move(values);
                response.rawPayload = payload;
                try {
                    resolved.callback(response);
                } catch (const std::exception& e) {
                    SKSE::log::error("[ModalManager] Exception in modal callback '{}': {}",
                                     response.modalId,
                                     e.what());
                } catch (...) {
                    SKSE::log::error("[ModalManager] Unknown exception in modal callback '{}'",
                                     response.modalId);
                }
            } else {
                SKSE::log::debug(
                    "[ModalManager] Dropped stale modal callback '{}' (expectedEpoch={}, currentEpoch={})",
                    resolved.modalId,
                    resolved.expectedEpoch,
                    GetSessionEpoch());
            }
        }

        return true;
    }

    void ModalManager::Revert() {
        std::vector<ModalEntry> cancelled;
        {
            std::lock_guard lock(mutex_);
            if (activeModal_.has_value()) {
                cancelled.push_back(std::move(*activeModal_));
                activeModal_.reset();
            }
            while (!queue_.empty()) {
                cancelled.push_back(std::move(queue_.front()));
                queue_.pop_front();
            }
            lastSentModalId_.clear();
        }

        if (!cancelled.empty()) {
            SKSE::log::info("[ModalManager] Reverting {} active/queued modal(s)", cancelled.size());
        }

        for (auto& entry : cancelled) {
            if (!entry.callback) {
                continue;
            }
            ModalResponse abortedResp;
            abortedResp.modalId = std::move(entry.modalId);
            abortedResp.aborted = true;
            try {
                entry.callback(abortedResp);
            } catch (const std::exception& e) {
                SKSE::log::error("[ModalManager] Exception aborting modal '{}' on revert: {}",
                                 abortedResp.modalId,
                                 e.what());
            } catch (...) {
                SKSE::log::error("[ModalManager] Unknown exception aborting modal '{}' on revert",
                                 abortedResp.modalId);
            }
        }

        auto& uiMgr = UIManager::GetSingleton();
        uiMgr.SendToUI("clearOStimNetModal", "");
        uiMgr.SyncViewVisibilityAndFocus();
    }

}  // namespace OStimNet::UI
