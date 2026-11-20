#include "PCH.h"

#include "ConfirmationModal.h"
#include "Config.h"
#include "EventPayloadBuilder.h"
#include "ModEventDispatch.h"
#include "ThreadRegistry.h"
#include "ui/ModalManager.h"
#include "ui/ModalTypes.h"
#include "ui/SessionEpoch.h"
#include "ui/UIManager.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <format>
#include <sstream>
#include <utility>

namespace OStimNet {

    namespace {

        bool IsPlayer(RE::Actor* actor) {
            if (!actor) return false;
            auto* player = RE::PlayerCharacter::GetSingleton();
            return player && actor->GetFormID() == player->GetFormID();
        }

        std::string ToLowerCopy(std::string_view sv) {
            std::string out(sv);
            std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return out;
        }

        std::string BuildCooldownKey(const char* actionType, RE::FormID formID) {
            std::string lower = ToLowerCopy(actionType ? actionType : "");
            // Treat pre-, post-eval, and after-scan variants as the same cooldown bucket.
            if (lower == "startnewsexpreeval" || lower == "startnewsexposteval" || lower == "startnewsexafterscan") {
                lower = "startnewsex";
            }
            std::ostringstream oss;
            oss << lower << ":" << std::hex << formID;
            return oss.str();
        }

        struct ActionDisplayInfo {
            std::string title;
            UI::ModalTone tone{ UI::ModalTone::Default };
            std::string message;
            std::string detailsText;
        };

        void AppendDetailLine(std::string& details, std::string_view label, std::string_view value) {
            if (value.empty()) return;
            if (!details.empty()) {
                details.push_back('\n');
            }
            details.append(label);
            details.append(": ");
            details.append(value);
        }

        ActionDisplayInfo BuildActionDisplay(
            const std::string& actionType,
            const std::string& mainActorList,
            const std::string& secondaryActorList,
            Intent intent,
            const std::string& activityStr,
            const std::string& positionsStr,
            const std::string& furnitureStr,
            const std::string& excludedNamesStr,
            const std::string& intentStr) {
            ActionDisplayInfo info;
            const bool hasFurniture = !furnitureStr.empty() && furnitureStr != "floor" && furnitureStr != "none";

            if (actionType == "startnewsexposteval") {
                info.title = "Participants Adjusted";
                info.tone = UI::ModalTone::Warning;
                info.message = "After evaluation, the participant list has been adjusted.\n";
                if (secondaryActorList.empty()) {
                    info.message += mainActorList + " will proceed alone.";
                } else {
                    info.message += mainActorList + " will proceed with " + secondaryActorList + ".";
                }
                info.message += "\nDo you want to go ahead?";

                AppendDetailLine(info.detailsText, "Excluded", excludedNamesStr);
                AppendDetailLine(info.detailsText, "Activity", activityStr);
                AppendDetailLine(info.detailsText, "Position", positionsStr);
                if (hasFurniture) AppendDetailLine(info.detailsText, "Furniture", furnitureStr);
                AppendDetailLine(info.detailsText, "Intent", intentStr);
                return info;
            }

            if (actionType == "startnewsexpreeval" || actionType == "startnewsexafterscan") {
                info.title = "Confirm Intimate Encounter";
                info.tone = (intent == Intent::Aggressive) ? UI::ModalTone::Danger : UI::ModalTone::Default;
                if (secondaryActorList.empty()) {
                    info.message = mainActorList + " wants to start a solo sexual encounter.";
                } else {
                    info.message = mainActorList + " wants to start a sexual encounter with " + secondaryActorList + ".";
                }
                if (hasFurniture) AppendDetailLine(info.detailsText, "Furniture", furnitureStr);
                AppendDetailLine(info.detailsText, "Intent", intentStr);
                return info;
            }

            if (actionType == "startcarescene") {
                info.title = "Confirm Affectionate Scene";
                info.tone = UI::ModalTone::Success;
                info.message = mainActorList + " wants to start an affectionate scene";
                if (!secondaryActorList.empty()) {
                    info.message += " with " + secondaryActorList;
                }
                info.message += ".\nDo you accept this?";
                AppendDetailLine(info.detailsText, "Activity", activityStr);
                AppendDetailLine(info.detailsText, "Intent", intentStr);
                return info;
            }

            if (actionType == "joinongoingsex") {
                info.title = "Join Ongoing Scene";
                info.tone = (intent == Intent::Aggressive) ? UI::ModalTone::Danger : UI::ModalTone::Default;
                if (secondaryActorList.empty()) {
                    info.message = mainActorList + " wants to join the sexual encounter.";
                } else {
                    info.message = mainActorList + " wants to join " + secondaryActorList + "'s sexual encounter.";
                }
                info.message += "\nDo you want to let them in?";
                AppendDetailLine(info.detailsText, "New Intent", intentStr);
                return info;
            }

            if (actionType == "invitetoyoursex") {
                info.title = "Invite to Scene";
                info.tone = UI::ModalTone::Default;
                const std::string& invited = secondaryActorList.empty() ? "someone" : secondaryActorList;
                info.message = mainActorList + " wants to invite " + invited +
                               " to the ongoing sexual encounter.\nDo you want to accept them?";
                return info;
            }

            if (actionType == "changesexsceneposition") {
                info.title = "Change Scene Activity";
                info.tone = UI::ModalTone::Default;
                info.message = mainActorList + " wants to change the sexual activity";
                if (!secondaryActorList.empty()) {
                    info.message += " with " + secondaryActorList;
                }
                info.message += ".";
                AppendDetailLine(info.detailsText, "Activity", activityStr);
                AppendDetailLine(info.detailsText, "Position", positionsStr);
                return info;
            }

            if (actionType == "changesexsceneintent") {
                info.title = "Change Scene Intent";
                info.tone = (intent == Intent::Aggressive) ? UI::ModalTone::Danger : UI::ModalTone::Warning;
                info.message = mainActorList + " wants to change the intent of the sexual encounter";
                if (!secondaryActorList.empty()) {
                    info.message += " with " + secondaryActorList;
                }
                info.message += ".\nDo you accept this change?";
                AppendDetailLine(info.detailsText, "Requested Intent", intentStr);
                return info;
            }

            if (actionType == "changesexscenepace") {
                info.title = "Change Scene Pace";
                info.tone = UI::ModalTone::Default;
                info.message = mainActorList + " wants to change the pace of the sexual encounter";
                if (!secondaryActorList.empty()) {
                    info.message += " with " + secondaryActorList;
                }
                info.message += ".";
                AppendDetailLine(info.detailsText, "Activity", activityStr);
                AppendDetailLine(info.detailsText, "Intent", intentStr);
                return info;
            }

            if (actionType == "stopsex") {
                info.title = "End Intimate Scene";
                info.tone = UI::ModalTone::Warning;
                info.message = mainActorList + " wants to end the ongoing sexual encounter";
                if (!secondaryActorList.empty()) {
                    info.message += " with " + secondaryActorList;
                }
                info.message += ".";
                return info;
            }

            info.title = "Confirm Scene Action";
            info.tone = UI::ModalTone::Default;
            info.message = mainActorList + " wants to start " + (!actionType.empty() ? actionType : "an activity");
            if (!secondaryActorList.empty()) {
                info.message += " with " + secondaryActorList;
            }
            info.message += ".";
            AppendDetailLine(info.detailsText, "Activity", activityStr);
            AppendDetailLine(info.detailsText, "Position", positionsStr);
            if (hasFurniture) AppendDetailLine(info.detailsText, "Furniture", furnitureStr);
            AppendDetailLine(info.detailsText, "Intent", intentStr);
            return info;
        }

        std::string BuildDeclineMsg(
            const std::string& actionType,
            const std::string& mainActorList,
            const std::string& secondaryActorList,
            const std::string& activity) {
            auto* playerActor = RE::PlayerCharacter::GetSingleton();
            std::string player = playerActor ? playerActor->GetDisplayFullName() : "the player";
            std::string withSecondary = secondaryActorList.empty() ? "" : (" with " + secondaryActorList);

            if (actionType == "stopsex") {
                return mainActorList + " wanted to stop the sexual activity, but " + player + " refused to end it.";
            }
            if (actionType == "changesexscenepace") {
                return mainActorList + " wanted to change the pace of sexual activity" + withSecondary +
                       ", but " + player + " refused the change.";
            }
            if (actionType == "joinongoingsex") {
                std::string targetScene = secondaryActorList.empty() ? "the" : (secondaryActorList + "'s");
                return mainActorList + " wanted to join " + targetScene + " sexual scene, but " + player +
                       " refused to let them in.";
            }
            if (actionType == "invitetoyoursex") {
                return mainActorList + " wanted to invite more people to the scene, but " + player + " refused.";
            }
            if (actionType == "changesexsceneposition") {
                return mainActorList + " wanted to change the sexual activity" + withSecondary + ", but " + player +
                       " refused.";
            }
            if (actionType == "changesexsceneintent") {
                return mainActorList + " wanted to change the sexual intent" + withSecondary + ", but " + player +
                       " refused.";
            }
            if (actionType == "startcarescene") {
                return mainActorList + " attempted " + activity + withSecondary + ", " + player +
                       " did not go through with it.";
            }
            if (secondaryActorList.empty()) {
                return mainActorList + " wanted to start a solo sexual encounter, but " + player + " declined.";
            }
            return mainActorList + " wanted to start a new sexual encounter with " + secondaryActorList +
                   ", but " + player + " declined the proposal.";
        }

        std::string BuildDeclineJson(
            const std::string& actionType,
            Intent intent,
            bool isSexual,
            const std::string& activity,
            const std::string& mainActorList,
            const std::string& secondaryActorList,
            bool skipTrigger) {
            std::string msg = BuildDeclineMsg(actionType, mainActorList, secondaryActorList, activity);
            std::string speakerName = ThreadDataStore::GetActorDisplayName(
                RE::PlayerCharacter::GetSingleton(),
                "Player");
            return BuildBaseEventJson(
                       "action_decline",
                       msg,
                       0,
                       skipTrigger,
                       actionType,
                       intent,
                       isSexual,
                       mainActorList,
                       secondaryActorList,
                       speakerName)
                .dump();
        }

    }  // namespace

    ConfirmationModal& ConfirmationModal::GetSingleton() {
        static ConfirmationModal instance;
        return instance;
    }

    bool ConfirmationModal::IsOnCooldown(const char* actionType, RE::Actor* originator) {
        if (IsPlayer(originator)) return false;
        const std::string key = BuildCooldownKey(actionType, originator ? originator->GetFormID() : 0);
        std::shared_lock lock(_cooldownMutex);
        const auto it = _cooldowns.find(key);
        const auto now = std::chrono::steady_clock::now();
        const bool onCooldown = (it != _cooldowns.end() && now < it->second);
        if (onCooldown) {
            const auto remaining = std::chrono::duration_cast<std::chrono::seconds>(it->second - now).count();
            SKSE::log::trace("ConfirmationModal::IsOnCooldown - key='{}' ON COOLDOWN ({}s remaining)", key, remaining);
        } else {
            SKSE::log::trace("ConfirmationModal::IsOnCooldown - key='{}' not on cooldown", key);
        }
        return onCooldown;
    }

    void ConfirmationModal::Reset() {
        std::unique_lock lock(_cooldownMutex);
        _cooldowns.clear();
        SKSE::log::info("ConfirmationModal: reset on game load");
    }

    std::int32_t ConfirmationModal::CheckAndSet(const char* actionType, const std::vector<RE::Actor*>& actors) {
        const int normalSec = Config::GetSingleton().ActionsCooldown();
        std::vector<std::string> keys;
        keys.reserve(actors.size());
        for (RE::Actor* actor : actors) {
            if (!IsPlayer(actor)) {
                keys.push_back(BuildCooldownKey(actionType, actor ? actor->GetFormID() : 0));
            }
        }
        if (keys.empty()) return 0;

        const auto now = std::chrono::steady_clock::now();
        const auto expiry = now + std::chrono::seconds(normalSec);
        std::unique_lock lock(_cooldownMutex);
        for (const auto& key : keys) {
            const auto it = _cooldowns.find(key);
            if (it != _cooldowns.end() && now < it->second) {
                const auto remaining = std::chrono::duration_cast<std::chrono::seconds>(it->second - now).count();
                SKSE::log::info("ConfirmationModal::CheckAndSet - key='{}' blocked by cooldown ({}s remaining)",
                                key,
                                remaining);
                return 1;
            }
        }
        for (const auto& key : keys) {
            _cooldowns[key] = expiry;
            SKSE::log::info("ConfirmationModal::CheckAndSet - key='{}' cooldown set ({}s)", key, normalSec);
        }
        return 0;
    }

    void ConfirmationModal::SetCooldown(const std::string& key, int seconds) {
        const auto expiry = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
        std::unique_lock lock(_cooldownMutex);
        _cooldowns[key] = expiry;
        SKSE::log::info("ConfirmationModal::SetCooldown - key='{}' set ({}s)", key, seconds);
    }

    void ConfirmationModal::SetCooldownAll(const std::vector<std::string>& keys, int seconds) {
        const auto expiry = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
        std::unique_lock lock(_cooldownMutex);
        for (const auto& key : keys) {
            _cooldowns[key] = expiry;
            SKSE::log::info("ConfirmationModal::SetCooldownAll - key='{}' set ({}s)", key, seconds);
        }
    }

    RE::BSScript::LatentStatus ConfirmationModal::Show(
        RE::BSScript::Internal::VirtualMachine* vm,
        RE::VMStackID stackID,
        const char* actionType,
        const std::vector<RE::Actor*>& mainActors,
        const std::vector<RE::Actor*>& secondaryActors,
        const char* paramsJson) {
        if (!vm || !SKSE::GetTaskInterface()) {
            SKSE::log::error("ConfirmationModal::Show - VirtualMachine or SKSE TaskInterface unavailable");
            return RE::BSScript::LatentStatus::kFailed;
        }

        const UI::SessionEpoch expectedEpoch = UI::GetSessionEpoch();

        const std::string mainActorList =
            mainActors.empty() ? "Someone" : ThreadDataStore::FormatActorNames(mainActors);
        const std::string secondaryActorList =
            secondaryActors.empty() ? "" : ThreadDataStore::FormatActorNames(secondaryActors);
        const std::string actionTypeLower = ToLowerCopy(actionType ? actionType : "");

        std::string intentStr;
        bool isSexual = false;
        std::string activityStr;
        std::string positionsStr;
        std::string furnitureStr;
        std::string excludedNamesStr;

        if (paramsJson && paramsJson[0] != '\0') {
            auto j = nlohmann::json::parse(paramsJson, nullptr, /*exceptions=*/false);
            if (j.is_object()) {
                if (j.contains("intent") && j["intent"].is_string()) {
                    intentStr = j["intent"].get<std::string>();
                }
                if (j.contains("isSexual") && j["isSexual"].is_boolean()) {
                    isSexual = j["isSexual"].get<bool>();
                }
                if (j.contains("activity") && j["activity"].is_string()) {
                    activityStr = j["activity"].get<std::string>();
                }
                if (activityStr.empty() && j.contains("sexualActivities") && j["sexualActivities"].is_string()) {
                    activityStr = j["sexualActivities"].get<std::string>();
                }
                if (j.contains("positions") && j["positions"].is_string()) {
                    positionsStr = j["positions"].get<std::string>();
                }
                if (positionsStr.empty() && j.contains("sexualPosition") && j["sexualPosition"].is_string()) {
                    positionsStr = j["sexualPosition"].get<std::string>();
                }
                if (j.contains("furniture") && j["furniture"].is_string()) {
                    furnitureStr = j["furniture"].get<std::string>();
                }
                if (j.contains("excluded") && j["excluded"].is_array()) {
                    std::vector<RE::Actor*> excludedActors;
                    for (const auto& item : j["excluded"]) {
                        if (!item.is_number_unsigned()) continue;
                        if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(item.get<std::uint32_t>())) {
                            excludedActors.push_back(actor);
                        }
                    }
                    if (!excludedActors.empty()) {
                        excludedNamesStr = ThreadDataStore::FormatActorNames(excludedActors);
                    }
                }
                if (!isSexual &&
                    (actionTypeLower == "startnewsexpreeval" || actionTypeLower == "startnewsexposteval")) {
                    isSexual = true;
                }
            }
        }

        const Intent intent = IntentFromString(intentStr);
        SKSE::log::info(
            "ConfirmationModal::Show - actionType='{}' intent={} mainActors={} secondaryActors={} activity='{}'",
            actionType ? actionType : "(null)",
            static_cast<int>(intent),
            mainActorList,
            secondaryActorList,
            activityStr);

        const int normalSec = Config::GetSingleton().ActionsCooldown();
        const int declineSec = Config::GetSingleton().ActionsDeclineCooldown();

        std::vector<std::string> keys;
        keys.reserve(mainActors.size());
        for (RE::Actor* actor : mainActors) {
            if (!IsPlayer(actor)) {
                keys.push_back(BuildCooldownKey(actionType, actor ? actor->GetFormID() : 0));
            }
        }

        auto ReturnNow = [vm, stackID, expectedEpoch](int code) -> RE::BSScript::LatentStatus {
            SKSE::log::info("ConfirmationModal::Show - early return {}", code);
            if (UI::AddEpochTask(expectedEpoch, [vm, stackID, code]() {
                    if (vm) {
                        vm->ReturnLatentResult(stackID, code);
                    }
                })) {
                return RE::BSScript::LatentStatus::kStarted;
            }
            return RE::BSScript::LatentStatus::kFailed;
        };

        // Player-in-actors check
        int associatedThreadId = -1;
        {
            int threadId = -1;
            if (!mainActors.empty() && mainActors[0]) {
                threadId = ThreadDataStore::GetSingleton().GetActorThreadID(mainActors[0]->GetFormID());
            }
            if (threadId == -1 && !secondaryActors.empty() && secondaryActors[0]) {
                threadId = ThreadDataStore::GetSingleton().GetActorThreadID(secondaryActors[0]->GetFormID());
            }
            associatedThreadId = threadId;

            const bool showWhenNoPlayer =
                Config::GetSingleton().ShowConfirmationModalWithNoPlayer() &&
                (actionTypeLower == "startnewsexpreeval" || actionTypeLower == "startnewsexposteval" ||
                 actionTypeLower == "startcarescene" || actionTypeLower == "startnewsexafterscan");

            RE::Actor* player = RE::PlayerCharacter::GetSingleton();
            bool hasPlayer = (threadId == 0);
            for (RE::Actor* actor : mainActors) {
                if (actor && player && actor->GetFormID() == player->GetFormID()) {
                    hasPlayer = true;
                    break;
                }
            }
            if (!hasPlayer) {
                for (RE::Actor* actor : secondaryActors) {
                    if (actor && player && actor->GetFormID() == player->GetFormID()) {
                        hasPlayer = true;
                        break;
                    }
                }
            }
            if (!hasPlayer && !showWhenNoPlayer) {
                SetCooldownAll(keys, normalSec);
                return ReturnNow(4);
            }
        }

        // Intent check — skip modal for aggressive intent unless config says otherwise.
        if (intent == Intent::Aggressive && !Config::GetSingleton().ShowConfirmationModalForAggressiveIntent()) {
            SetCooldownAll(keys, normalSec);
            return ReturnNow(5);
        }

        // Per-action confirmation check — auto-accept (code 0) if disabled.
        {
            bool confirmEnabled = true;
            if (actionTypeLower == "startnewsexpreeval" || actionTypeLower == "startnewsexposteval" ||
                actionTypeLower == "startnewsexafterscan") {
                confirmEnabled = Config::GetSingleton().ConfirmStartNewSex();
            } else if (actionTypeLower == "joinongoingsex") {
                confirmEnabled = Config::GetSingleton().ConfirmJoinOngoingSex();
            } else if (actionTypeLower == "invitetoyoursex") {
                confirmEnabled = Config::GetSingleton().ConfirmInviteToYourSex();
            } else if (actionTypeLower == "changesexsceneposition") {
                confirmEnabled = Config::GetSingleton().ConfirmChangeSexScenePosition();
            } else if (actionTypeLower == "changesexsceneintent") {
                confirmEnabled = Config::GetSingleton().ConfirmChangeSexSceneIntent();
            } else if (actionTypeLower == "changesexscenepace") {
                confirmEnabled = Config::GetSingleton().ConfirmChangeSexScenePace();
            } else if (actionTypeLower == "stopsex") {
                confirmEnabled = Config::GetSingleton().ConfirmStopSex();
            } else if (actionTypeLower == "startcarescene") {
                confirmEnabled = Config::GetSingleton().ConfirmStartCareScene();
            }

            if (!confirmEnabled) {
                SKSE::log::info("ConfirmationModal::Show - auto-accepting '{}' (confirmation disabled in config)",
                                actionTypeLower);
                SetCooldownAll(keys, normalSec);
                return ReturnNow(0);
            }
        }

        if (!UI::UIManager::GetSingleton().IsInitialized()) {
            SKSE::log::warn("ConfirmationModal::Show - Magelight UI unavailable for '{}'; auto-accepting (0)",
                            actionTypeLower);
            SetCooldownAll(keys, normalSec);
            return ReturnNow(0);
        }

        const ActionDisplayInfo display = BuildActionDisplay(
            actionTypeLower,
            mainActorList,
            secondaryActorList,
            intent,
            activityStr,
            positionsStr,
            furnitureStr,
            excludedNamesStr,
            intentStr);

        UI::ModalRequest req;
        req.modalType = "generic";
        req.title = display.title;
        if (associatedThreadId >= 0 &&
            actionTypeLower != "startnewsexpreeval" &&
            actionTypeLower != "startnewsexposteval" &&
            actionTypeLower != "startnewsexafterscan" &&
            actionTypeLower != "startcarescene") {
            req.associatedThreadId = associatedThreadId;
        }
        req.subtitle = secondaryActorList.empty()
                           ? mainActorList
                           : std::format("{} & {}", mainActorList, secondaryActorList);
        req.tone = display.tone;
        req.size = display.detailsText.empty() ? UI::ModalSize::Sm : UI::ModalSize::Md;
        req.message = display.message;

        if (!display.detailsText.empty()) {
            req.controls.push_back(UI::ModalControl::MakeInfoBox(
                "scene_details",
                "Proposed Details",
                display.detailsText,
                display.tone));
        }

        req.buttons.push_back(
            UI::ModalButton::Make("accept", "Accept", UI::ModalButtonVariant::Primary));
        req.buttons.push_back(UI::ModalButton::Make(
            "decline_explain",
            "Decline (Explain)",
            UI::ModalButtonVariant::Outline,
            false,
            "Decline and type a reason for SkyrimNet"));
        req.buttons.push_back(UI::ModalButton::Make(
            "cancel",
            "Cancel",
            UI::ModalButtonVariant::Secondary,
            false,
            "Silently cancel without recording a SkyrimNet event"));

        SKSE::log::info("ConfirmationModal::Show - queuing Magelight modal for '{}': '{}'",
                        actionTypeLower,
                        display.message);

        UI::ModalManager::GetSingleton().ShowModal(
            std::move(req),
            [this,
             vm,
             stackID,
             expectedEpoch,
             keys,
             normalSec,
             declineSec,
             actionTypeLower,
             intent,
             isSexual,
             activityStr,
             mainActorList,
             secondaryActorList](const UI::ModalResponse& resp) {
                if (!UI::IsSessionEpochCurrent(expectedEpoch)) {
                    SKSE::log::debug("ConfirmationModal: dropped stale modal response for '{}'", actionTypeLower);
                    return;
                }

                auto resolveLatent = [vm, stackID, expectedEpoch](int code) {
                    UI::AddEpochTask(expectedEpoch, [vm, stackID, code]() {
                        if (vm) {
                            vm->ReturnLatentResult(stackID, code);
                        }
                    });
                };

                if (resp.aborted) {
                    SetCooldownAll(keys, declineSec);
                    resolveLatent(1);
                    return;
                }

                if (resp.buttonId == "accept") {
                    SetCooldownAll(keys, normalSec);
                    resolveLatent(0);
                    return;
                }

                if (resp.buttonId == "decline_explain") {
                    SetCooldownAll(keys, declineSec);

                    const std::string json = BuildDeclineJson(
                        actionTypeLower,
                        intent,
                        isSexual,
                        activityStr,
                        mainActorList,
                        secondaryActorList,
                        /*skipTrigger=*/true);
                    FireModEvent("ostimnet_decline", json.c_str(), 0.0f, RE::PlayerCharacter::GetSingleton());

                    UI::AddEpochTask(expectedEpoch, []() {
                        auto* innerVm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
                        if (!innerVm) {
                            SKSE::log::warn("ConfirmationModal: VirtualMachine unavailable for TriggerTextInput");
                            return;
                        }
                        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> nullCallback;
                        if (!innerVm->DispatchStaticCall(
                                "SkyrimNetApi",
                                "TriggerTextInput",
                                RE::MakeFunctionArguments(),
                                nullCallback)) {
                            SKSE::log::warn("ConfirmationModal: Failed to dispatch SkyrimNetApi::TriggerTextInput");
                        }
                    });

                    resolveLatent(2);
                    return;
                }

                // Cancel or fallback
                SetCooldownAll(keys, declineSec);
                resolveLatent(1);
            });

        return RE::BSScript::LatentStatus::kStarted;
    }

}  // namespace OStimNet
