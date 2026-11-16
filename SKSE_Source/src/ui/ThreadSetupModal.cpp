#include "PCH.h"

#include "ui/ThreadSetupModal.h"
#include "Config.h"
#include "EventPayloadBuilder.h"
#include "OStimEventListener.h"
#include "SkyrimNetIntegration.h"
#include "SpeakerSelector.h"
#include "ThreadRegistry.h"
#include "api/OstimNG-API-Thread.h"
#include "ui/ModalManager.h"
#include "ui/ModalTypes.h"
#include "ui/SessionEpoch.h"
#include "ui/UIManager.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <format>
#include <unordered_set>
#include <utility>
#include <vector>

namespace OStimNet::UI {

    namespace {

        std::string ToLowerCopy(std::string_view sv) {
            std::string out(sv);
            std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return out;
        }

        std::string CapitalizeCopy(std::string_view sv) {
            std::string out(sv);
            if (!out.empty()) {
                out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
            }
            return out;
        }

        std::string FormatFormIdHex(RE::FormID id) {
            return std::format("{:08X}", id);
        }

        std::string_view RoleLabelForIntent(std::string_view intentLower) {
            if (intentLower == "platonic" || intentLower == "romantic" || intentLower == "lustful") {
                return "Initiators";
            }
            if (intentLower == "transactional") {
                return "Service Receivers";
            }
            if (intentLower == "dom") {
                return "Dominant Actors";
            }
            if (intentLower == "aggressive") {
                return "Aggressors";
            }
            return "Primary Participants";
        }

        ModalTone ToneForIntent(std::string_view intentLower) {
            if (intentLower == "aggressive") {
                return ModalTone::Danger;
            }
            if (intentLower == "dom") {
                return ModalTone::Warning;
            }
            return ModalTone::Default;
        }

        std::vector<RE::Actor*> ResolveSelectedActorsFromIds(
            const std::vector<RE::Actor*>& allActors,
            const std::vector<std::string>& selectedIds) {
            std::unordered_set<std::string> idSet;
            idSet.reserve(selectedIds.size());
            for (const auto& id : selectedIds) {
                idSet.insert(ToLowerCopy(id));
            }

            std::vector<RE::Actor*> result;
            result.reserve(allActors.size());
            for (RE::Actor* actor : allActors) {
                if (!actor) continue;
                const std::string hexLower = ToLowerCopy(FormatFormIdHex(actor->GetFormID()));
                if (idSet.contains(hexLower)) {
                    result.push_back(actor);
                }
            }
            return result;
        }

        std::vector<RE::Actor*> DeriveSecondaryActors(
            const std::vector<RE::Actor*>& allActors,
            const std::vector<RE::Actor*>& mainActors) {
            std::unordered_set<RE::FormID> mainIDs;
            mainIDs.reserve(mainActors.size());
            for (RE::Actor* actor : mainActors) {
                if (actor) {
                    mainIDs.insert(actor->GetFormID());
                }
            }

            std::vector<RE::Actor*> secondaryActors;
            secondaryActors.reserve(allActors.size());
            for (RE::Actor* actor : allActors) {
                if (actor && !mainIDs.contains(actor->GetFormID())) {
                    secondaryActors.push_back(actor);
                }
            }
            return secondaryActors;
        }

        std::vector<std::string> ExtractMainActorIds(const ModalResponse& resp) {
            auto checkedIds = resp.GetCheckedItemIds("mainActors");
            if (!checkedIds.empty()) {
                return checkedIds;
            }

            if (resp.values.is_object()) {
                auto it = resp.values.find("mainActorIds");
                if (it != resp.values.end() && it->is_array()) {
                    checkedIds.reserve(it->size());
                    for (const auto& elem : *it) {
                        if (elem.is_string()) {
                            checkedIds.push_back(elem.get<std::string>());
                        }
                    }
                }
            }
            return checkedIds;
        }

    }  // namespace

    ThreadSetupModal& ThreadSetupModal::GetSingleton() {
        static ThreadSetupModal instance;
        return instance;
    }

    void ThreadSetupModal::ApplyThreadIntentAndRoles(
        int threadID,
        const std::string& intentLower,
        std::vector<RE::Actor*> mainActors) {
        auto& registry = ThreadRegistry::GetSingleton();
        const Intent intentEnum = IntentFromString(intentLower);

        const std::string oldIntentStr = IntentToString(registry.GetIntent(threadID));
        const std::vector<RE::Actor*> oldMainActors = registry.GetMainActors(threadID);
        registry.SetIntent(threadID, intentEnum);

        bool mainActorsSame = false;
        if (mainActors.empty()) {
            SKSE::log::info(
                "[ThreadSetupModal] ApplyThreadIntentAndRoles threadID={} intent={} — keeping existing actor roles",
                threadID,
                intentLower);
            mainActorsSame = true;
        } else {
            const auto& allActors = registry.GetActorPtrs(threadID);
            std::unordered_set<RE::FormID> mainIDs;
            for (RE::Actor* a : mainActors) {
                if (a) mainIDs.insert(a->GetFormID());
            }
            std::vector<RE::Actor*> secondaryActors = DeriveSecondaryActors(allActors, mainActors);

            std::unordered_set<RE::FormID> oldMainIDs;
            for (auto* a : oldMainActors) {
                if (a) oldMainIDs.insert(a->GetFormID());
            }
            mainActorsSame = (mainIDs == oldMainIDs);

            registry.SetMainActors(threadID, std::move(mainActors));
            registry.SetSecondaryActors(threadID, std::move(secondaryActors));
        }

        RE::Actor* speaker = SelectSpeakerActor(threadID, Config::GetSingleton().CommentGenderPriority());
        const std::string json = EventPayloadBuilder::BuildIntentChanged(
            threadID,
            oldIntentStr,
            intentLower,
            mainActorsSame,
            speaker);
        if (auto* source = SKSE::GetModCallbackEventSource()) {
            SKSE::ModCallbackEvent e{ "ostimnet_intent_changed", json.c_str(), static_cast<float>(threadID), speaker };
            source->SendEvent(&e);
        }
        SKSE::log::info("[ThreadSetupModal] Fired ostimnet_intent_changed: {}", json);
    }

    void ThreadSetupModal::ShowExternalThreadSetup(int threadID) {
        const auto expectedEpoch = GetSessionEpoch();
        auto& store = ThreadDataStore::GetSingleton();
        const auto actors = store.GetActorPtrs(threadID);

        if (actors.empty()) {
            SKSE::log::warn("[ThreadSetupModal] ShowExternalThreadSetup: no actors for thread {}", threadID);
            if (auto* listener = OStimEventListener::GetInstance()) {
                listener->CancelPendingThread(threadID);
            } else {
                store.ClearThread(threadID);
            }
            return;
        }

        auto collectFormIDs = [](const std::vector<RE::Actor*>& actorList) {
            std::vector<RE::FormID> fids;
            fids.reserve(actorList.size());
            for (RE::Actor* a : actorList) {
                if (a) fids.push_back(a->GetFormID());
            }
            return fids;
        };

        if (!UIManager::GetSingleton().IsInitialized()) {
            SKSE::log::warn(
                "[ThreadSetupModal] Magelight UI unavailable for external thread {}; delegating to LLM evaluation",
                threadID);
            SkyrimNetIntegration::EvaluateExternalSexualThread(collectFormIDs(actors), threadID);
            return;
        }

        nlohmann::json actorsJson = nlohmann::json::array();
        for (RE::Actor* actor : actors) {
            if (!actor) continue;
            actorsJson.push_back({
                { "id", FormatFormIdHex(actor->GetFormID()) },
                { "formId", actor->GetFormID() },
                { "name", ThreadDataStore::GetActorDisplayName(actor, "Actor") },
            });
        }

        ModalRequest req;
        req.modalType = "external_thread_setup";
        req.title = "Configure Scene";
        req.subtitle = ThreadDataStore::FormatActorNames(actors);
        req.tone = ModalTone::Default;
        req.size = ModalSize::Md;
        req.customData = nlohmann::json{
            { "threadId", threadID },
            { "actorCount", actorsJson.size() },
            { "actors", std::move(actorsJson) },
            { "enableAggressiveIntent", Config::GetSingleton().EnableAggressiveIntent() },
        };
        req.buttons.push_back(ModalButton::Make("confirm", "Confirm", ModalButtonVariant::Primary));
        req.buttons.push_back(ModalButton::Make("cancel", "Cancel", ModalButtonVariant::Secondary));

        SKSE::log::info("[ThreadSetupModal] Showing Magelight external thread setup modal for thread {}", threadID);

        ModalManager::GetSingleton().ShowModal(
            std::move(req),
            [expectedEpoch, threadID](const ModalResponse& resp) {
                if (!IsSessionEpochCurrent(expectedEpoch)) {
                    return;
                }

                auto& storeInner = ThreadDataStore::GetSingleton();
                if (!storeInner.IsRegisteredAndPending(threadID)) {
                    SKSE::log::warn(
                        "[ThreadSetupModal] External thread {} no longer registered/pending on modal close",
                        threadID);
                    return;
                }

                if (g_ostimThreadInterface &&
                    !g_ostimThreadInterface->IsThreadValid(static_cast<std::uint32_t>(threadID))) {
                    SKSE::log::warn("[ThreadSetupModal] External thread {} is no longer valid in OStim", threadID);
                    storeInner.ClearThread(threadID);
                    return;
                }

                if (resp.aborted || resp.buttonId != "confirm") {
                    if (auto* listener = OStimEventListener::GetInstance()) {
                        listener->CancelPendingThread(threadID);
                    } else {
                        storeInner.ClearThread(threadID);
                    }
                    RE::SendHUDMessage::ShowHUDMessage("OStimNet: Scene setup cancelled.");
                    return;
                }

                const std::string sceneType = ToLowerCopy(resp.GetString("sceneType", "sexual"));
                const bool isSexual = (sceneType != "non_sexual");
                std::string intentLower = ToLowerCopy(resp.GetString("intent", isSexual ? "romantic" : "platonic"));
                if (intentLower.empty()) {
                    intentLower = isSexual ? "romantic" : "platonic";
                }

                const auto currentActors = storeInner.GetActorPtrs(threadID);
                if (currentActors.empty()) {
                    storeInner.ClearThread(threadID);
                    return;
                }

                if (intentLower == "ai") {
                    RE::SendHUDMessage::ShowHUDMessage("OStimNet: Delegating scene evaluation to AI...");
                    storeInner.SetPhase(threadID, ThreadPhase::Pending);
                    std::vector<RE::FormID> fids;
                    fids.reserve(currentActors.size());
                    for (RE::Actor* a : currentActors) {
                        if (a) fids.push_back(a->GetFormID());
                    }
                    SkyrimNetIntegration::EvaluateExternalSexualThread(fids, threadID);
                    return;
                }

                std::vector<RE::Actor*> mainActors;
                if (currentActors.size() == 1) {
                    mainActors.push_back(currentActors[0]);
                } else {
                    mainActors = ResolveSelectedActorsFromIds(currentActors, ExtractMainActorIds(resp));
                    if (mainActors.empty()) {
                        mainActors.push_back(currentActors[0]);
                    }
                }
                std::vector<RE::Actor*> secondaryActors = DeriveSecondaryActors(currentActors, mainActors);

                storeInner.SetOStimNet(threadID, true);
                storeInner.SetIntent(threadID, IntentFromString(intentLower));
                storeInner.SetSexual(threadID, isSexual);
                storeInner.SetMainActors(threadID, std::move(mainActors));
                storeInner.SetSecondaryActors(threadID, std::move(secondaryActors));

                if (auto* listener = OStimEventListener::GetInstance()) {
                    listener->ClaimPendingNonOStimNetThread(threadID);
                    SKSE::log::info(
                        "[ThreadSetupModal] Claimed external thread {} (intent={}, isSexual={})",
                        threadID,
                        intentLower,
                        isSexual);
                }

                if (currentActors.size() == 1) {
                    RE::SendHUDMessage::ShowHUDMessage(
                        std::format("OStimNet: Scene configured with {} intent.", intentLower).c_str());
                }
            });
    }

    void ThreadSetupModal::ShowThreadRolesModal(int threadID, std::string_view requestedIntent) {
        const auto expectedEpoch = GetSessionEpoch();
        std::string requestedLower = ToLowerCopy(requestedIntent);

        if (!UIManager::GetSingleton().IsOnMainThread()) {
            AddEpochTask(expectedEpoch, [threadID, requestedLower = std::move(requestedLower)]() {
                ThreadSetupModal::GetSingleton().ShowThreadRolesModal(threadID, requestedLower);
            });
            return;
        }

        if (g_ostimThreadInterface &&
            !g_ostimThreadInterface->IsThreadValid(static_cast<std::uint32_t>(threadID))) {
            SKSE::log::warn("[ThreadSetupModal] ShowThreadRolesModal: thread {} is not valid", threadID);
            return;
        }

        auto& registry = ThreadRegistry::GetSingleton();
        const auto actors = registry.GetActorPtrs(threadID);
        if (actors.empty()) {
            SKSE::log::warn("[ThreadSetupModal] ShowThreadRolesModal: no actors found for thread {}", threadID);
            return;
        }

        const bool isChangingIntent = !requestedLower.empty();
        const std::string currentIntent = ToLowerCopy(IntentToString(registry.GetIntent(threadID)));
        const std::string targetIntent = isChangingIntent ? requestedLower : currentIntent;

        if (isChangingIntent && currentIntent == targetIntent) {
            RE::SendHUDMessage::ShowHUDMessage(
                std::format("Intent is already {}, no changes made.", targetIntent).c_str());
            return;
        }

        if (!isChangingIntent && actors.size() <= 1) {
            RE::SendHUDMessage::ShowHUDMessage("Not enough actors in the scene to change roles.");
            return;
        }

        if (isChangingIntent && actors.size() == 1) {
            ApplyThreadIntentAndRoles(threadID, targetIntent, { actors[0] });
            return;
        }

        if (!UIManager::GetSingleton().IsInitialized()) {
            SKSE::log::warn("[ThreadSetupModal] Magelight UI unavailable for ShowThreadRolesModal");
            if (isChangingIntent) {
                ApplyThreadIntentAndRoles(threadID, targetIntent, {});
            }
            return;
        }

        const std::string_view roleLabel = RoleLabelForIntent(targetIntent);

        std::unordered_set<RE::FormID> currentMainIds;
        for (RE::Actor* a : registry.GetMainActors(threadID)) {
            if (a) currentMainIds.insert(a->GetFormID());
        }

        std::size_t preCheckedCount = 0;
        for (RE::Actor* a : actors) {
            if (a && currentMainIds.contains(a->GetFormID())) {
                ++preCheckedCount;
            }
        }
        if (preCheckedCount == 0 || preCheckedCount >= actors.size()) {
            currentMainIds.clear();
            if (actors[0]) {
                currentMainIds.insert(actors[0]->GetFormID());
            }
        }

        std::vector<ModalChecklistItem> items;
        items.reserve(actors.size());
        for (RE::Actor* actor : actors) {
            if (!actor) continue;
            ModalChecklistItem item;
            item.id = FormatFormIdHex(actor->GetFormID());
            item.label = ThreadDataStore::GetActorDisplayName(actor, "Actor");
            item.checked = currentMainIds.contains(actor->GetFormID());
            items.push_back(std::move(item));
        }

        const std::int32_t maxPrimary = static_cast<std::int32_t>(actors.size() - 1);

        ModalRequest req;
        req.modalType = "generic";
        req.title = isChangingIntent
                        ? std::format("Change Scene Intent: {}", CapitalizeCopy(targetIntent))
                        : "Change Actor Roles";
        req.subtitle = ThreadDataStore::FormatActorNames(actors);
        req.tone = ToneForIntent(targetIntent);
        req.size = ModalSize::Md;
        req.controls.push_back(ModalControl::MakeChecklist(
            "mainActors",
            std::format("Select {}", roleLabel),
            std::move(items),
            /*showSelectAllToggle=*/false,
            std::format(
                "Select 1 to {} participant(s) as {}. Unchecked participants will take secondary roles.",
                maxPrimary,
                roleLabel)));

        ModalButton applyBtn = ModalButton::Make("apply", "Apply", ModalButtonVariant::Primary);
        applyBtn.requireChecklistId = "mainActors";
        applyBtn.minCheckedCount = 1;
        applyBtn.maxCheckedCount = maxPrimary;
        req.buttons.push_back(std::move(applyBtn));
        req.buttons.push_back(ModalButton::Make("cancel", "Cancel", ModalButtonVariant::Secondary));

        ModalManager::GetSingleton().ShowModal(
            std::move(req),
            [expectedEpoch, threadID, targetIntent](const ModalResponse& resp) {
                if (!IsSessionEpochCurrent(expectedEpoch)) {
                    return;
                }
                if (g_ostimThreadInterface &&
                    !g_ostimThreadInterface->IsThreadValid(static_cast<std::uint32_t>(threadID))) {
                    SKSE::log::warn(
                        "[ThreadSetupModal] Thread {} ended before role/intent modal resolved",
                        threadID);
                    return;
                }
                if (resp.aborted || resp.buttonId != "apply") {
                    return;
                }

                const auto& currentActors = ThreadRegistry::GetSingleton().GetActorPtrs(threadID);
                std::vector<RE::Actor*> selectedMain =
                    ResolveSelectedActorsFromIds(currentActors, resp.GetCheckedItemIds("mainActors"));
                if (selectedMain.empty()) {
                    RE::SendHUDMessage::ShowHUDMessage("No changes made to actors roles.");
                    return;
                }
                ApplyThreadIntentAndRoles(threadID, targetIntent, std::move(selectedMain));
            });
    }

}  // namespace OStimNet::UI
