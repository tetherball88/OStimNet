#pragma once
#include <chrono>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "Config.h"
#include "EventPayloadBuilder.h"
#include "ThreadRegistry.h"
#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

namespace OStimNet {

// ---------------------------------------------------------------------------
// ConfirmationModal — singleton. Registers as a LATENT Papyrus native so the
// Papyrus stack suspends instead of blocking the main thread.
//
// Return codes (via vm->ReturnLatentResult):
//    0 = player accepted
//    1 = player declined
//    2 = player chose "Decline (Explain)" — TriggerTextInput fired, decline cooldown applied
//    3 = cooldown still active (modal not shown)
//    4 = player not among actors (accept by default)
//    5 = intent is "aggressive" or "dom" (accept by default)
// ---------------------------------------------------------------------------
class ConfirmationModal {
public:
    static ConfirmationModal& GetSingleton() {
        static ConfirmationModal instance;
        return instance;
    }

    bool IsOnCooldown(const char* actionType, RE::Actor* originator) {
        if (IsPlayer(originator)) return false;
        std::string key = BuildCooldownKey(actionType, originator ? originator->GetFormID() : 0);
        std::shared_lock lock(_cooldownMutex);
        auto it = _cooldowns.find(key);
        bool onCooldown = it != _cooldowns.end() && std::chrono::steady_clock::now() < it->second;
        if (onCooldown) {
            auto remaining = std::chrono::duration_cast<std::chrono::seconds>(it->second - std::chrono::steady_clock::now()).count();
            SKSE::log::debug("ConfirmationModal::IsOnCooldown - key='{}' ON COOLDOWN ({}s remaining)", key, remaining);
        } else {
            SKSE::log::debug("ConfirmationModal::IsOnCooldown - key='{}' not on cooldown", key);
        }
        return onCooldown;
    }

    // Atomic check-and-set for the normal (non-decline) cooldown.
    // Called by the Papyrus CheckAndSetActionCooldown native at the start of every action,
    // before SetActorsPending and before ShowConfirmationModal.
    // Returns 0 if the cooldown was free and has now been set; 1 if already on cooldown (blocked).
    int32_t CheckAndSet(const char* actionType, const std::vector<RE::Actor*>& actors) {
        const int normalSec = Config::GetSingleton().ActionsCooldown();
        std::vector<std::string> keys;
        keys.reserve(actors.size());
        for (RE::Actor* actor : actors)
            if (!IsPlayer(actor))
                keys.push_back(BuildCooldownKey(actionType, actor ? actor->GetFormID() : 0));
        if (keys.empty()) return 0;
        auto expiry = std::chrono::steady_clock::now() + std::chrono::seconds(normalSec);
        std::unique_lock lock(_cooldownMutex);
        for (const auto& key : keys) {
            auto it = _cooldowns.find(key);
            if (it != _cooldowns.end() && std::chrono::steady_clock::now() < it->second) {
                auto remaining = std::chrono::duration_cast<std::chrono::seconds>(it->second - std::chrono::steady_clock::now()).count();
                SKSE::log::info("ConfirmationModal::CheckAndSet - key='{}' blocked by cooldown ({}s remaining)", key, remaining);
                return 1;
            }
        }
        for (const auto& key : keys) {
            _cooldowns[key] = expiry;
            SKSE::log::info("ConfirmationModal::CheckAndSet - key='{}' cooldown set ({}s)", key, normalSec);
        }
        return 0;
    }

    // Called from the Papyrus native (which is registered as latent).
    // Returns immediately; result delivered via vm->ReturnLatentResult() on the game thread.
    RE::BSScript::LatentStatus Show(RE::BSScript::Internal::VirtualMachine* vm,
              RE::VMStackID stackID,
              const char*   actionType,
              const std::vector<RE::Actor*>& mainActors,
              const std::vector<RE::Actor*>& secondaryActors,
              const char*   paramsJson) {

        std::string mainActorList      = mainActors.empty()      ? "Someone" : ThreadDataStore::FormatActorNames(mainActors);
        std::string secondaryActorList = secondaryActors.empty() ? "Someone" : ThreadDataStore::FormatActorNames(secondaryActors);
        std::string actionTypeLower = actionType ? actionType : "";
        std::transform(actionTypeLower.begin(), actionTypeLower.end(), actionTypeLower.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        // Parse params JSON — fields vary by actionType.
        // StartNewSex / ChangeSexScenePosition: { intent, isSexual, activity, positions }
        std::string intentStr;
        bool        isSexual     = false;
        std::string activityStr;
        std::string positionsStr;
        std::string furnitureStr;
        std::string excludedNamesStr;
        if (paramsJson && paramsJson[0] != '\0') {
            auto j = nlohmann::json::parse(paramsJson, nullptr, /*exceptions=*/false);
            if (j.is_object()) {
                if (j.contains("intent")         && j["intent"].is_string())         intentStr    = j["intent"].get<std::string>();
                if (j.contains("isSexual")       && j["isSexual"].is_boolean())      isSexual     = j["isSexual"].get<bool>();
                if (j.contains("activity")       && j["activity"].is_string())       activityStr  = j["activity"].get<std::string>();
                // result JSON from evaluation uses "sexualActivities" instead of "activity"
                if (activityStr.empty() && j.contains("sexualActivities") && j["sexualActivities"].is_string())
                    activityStr = j["sexualActivities"].get<std::string>();
                if (j.contains("positions")      && j["positions"].is_string())      positionsStr = j["positions"].get<std::string>();
                // result JSON from evaluation uses "sexualPosition" instead of "positions"
                if (positionsStr.empty() && j.contains("sexualPosition") && j["sexualPosition"].is_string())
                    positionsStr = j["sexualPosition"].get<std::string>();
                if (j.contains("furniture")      && j["furniture"].is_string())      furnitureStr = j["furniture"].get<std::string>();
                // Resolve excluded FormID array directly to actor names
                if (j.contains("excluded") && j["excluded"].is_array()) {
                    std::vector<RE::Actor*> excludedActors;
                    for (const auto& item : j["excluded"]) {
                        if (!item.is_number_unsigned()) continue;
                        auto* actor = RE::TESForm::LookupByID<RE::Actor>(item.get<uint32_t>());
                        if (actor) excludedActors.push_back(actor);
                    }
                    if (!excludedActors.empty())
                        excludedNamesStr = ThreadDataStore::FormatActorNames(excludedActors);
                }
                // startnewsex is always sexual regardless of whether isSexual was in the JSON
                if (!isSexual && (actionTypeLower == "startnewsexpreeval" || actionTypeLower == "startnewsexposteval")) {
                    isSexual = true;
                }
            }
        }
        Intent intent = IntentFromString(intentStr);

        SKSE::log::info("ConfirmationModal::Show - actionType='{}' intent={} mainActors={} secondaryActors={} activity='{}'",
            actionType ? actionType : "(null)", static_cast<int>(intent),
            mainActorList, secondaryActorList, activityStr);

        const int normalSec  = Config::GetSingleton().ActionsCooldown();
        const int declineSec = Config::GetSingleton().ActionsDeclineCooldown();

        // Build one cooldown key per main actor (player excluded).
        std::vector<std::string> keys;
        keys.reserve(mainActors.size());
        for (RE::Actor* actor : mainActors)
            if (!IsPlayer(actor))
                keys.push_back(BuildCooldownKey(actionType, actor ? actor->GetFormID() : 0));

        // Helper: queue a ReturnLatentResult via AddTask so it always fires
        // after the native has returned and the Papyrus stack is suspended.
        auto ReturnNow = [vm, stackID](int code) -> RE::BSScript::LatentStatus {
            SKSE::log::info("ConfirmationModal::Show - early return {}", code);
            if (auto* taskIF = SKSE::GetTaskInterface()) {
                taskIF->AddTask([vm, stackID, code]() {
                    vm->ReturnLatentResult(stackID, code);
                });
                return RE::BSScript::LatentStatus::kStarted;
            }
            return RE::BSScript::LatentStatus::kFailed;
        };

        // Player-in-actors check — must appear in mainActors.
        {
            SKSE::log::info("ConfirmationModal::Show - Main Actors count: {}", mainActors.size());
            for (RE::Actor* actor : mainActors) {
                if (actor) SKSE::log::info("  MainActor: {} ({:08X})", actor->GetName() ? actor->GetName() : "None", actor->GetFormID());
            }
            SKSE::log::info("ConfirmationModal::Show - Secondary Actors count: {}", secondaryActors.size());
            for (RE::Actor* actor : secondaryActors) {
                if (actor) SKSE::log::info("  SecondaryActor: {} ({:08X})", actor->GetName() ? actor->GetName() : "None", actor->GetFormID());
            }

            int threadId = -1;
            if (!mainActors.empty() && mainActors[0]) {
                threadId = ThreadDataStore::GetSingleton().GetActorThreadID(mainActors[0]->GetFormID());
            }
            if (threadId == -1 && !secondaryActors.empty() && secondaryActors[0]) {
                threadId = ThreadDataStore::GetSingleton().GetActorThreadID(secondaryActors[0]->GetFormID());
            }

            bool showWhenNoPlayer = Config::GetSingleton().ShowConfirmationModalWithNoPlayer() && (actionTypeLower == "startnewsexpreeval" || actionTypeLower == "startnewsexposteval" || actionTypeLower == "startcarescene" || actionTypeLower == "startnewsexafterscan" );

            RE::Actor* player = RE::PlayerCharacter::GetSingleton();
            bool hasPlayer = (threadId == 0);
            for (RE::Actor* actor : mainActors)
                if (actor && player && actor->GetFormID() == player->GetFormID()) { hasPlayer = true; break; }
            for (RE::Actor* actor : secondaryActors)
                if (actor && player && actor->GetFormID() == player->GetFormID()) { hasPlayer = true; break; }
            if (!hasPlayer && !showWhenNoPlayer) { SetCooldownAll(keys, normalSec); return ReturnNow(4); }
        }

        // Intent check — skip modal for aggressive intent unless config says otherwise.
        if (intent == Intent::Aggressive && !Config::GetSingleton().ShowConfirmationModalForAggressiveIntent()) {
            SetCooldownAll(keys, normalSec); return ReturnNow(5);
        }

        // Per-action confirmation check — auto-accept (code 0) if that action's modal is disabled.
        {
            bool confirmEnabled = true;
            if (actionTypeLower == "startnewsexpreeval" || actionTypeLower == "startnewsexposteval" || actionTypeLower == "startnewsexafterscan")
                confirmEnabled = Config::GetSingleton().ConfirmStartNewSex();
            else if (actionTypeLower == "joinongoingsex")
                confirmEnabled = Config::GetSingleton().ConfirmJoinOngoingSex();
            else if (actionTypeLower == "invitetoyoursex")
                confirmEnabled = Config::GetSingleton().ConfirmInviteToYourSex();
            else if (actionTypeLower == "changesexsceneposition")
                confirmEnabled = Config::GetSingleton().ConfirmChangeSexScenePosition();
            else if (actionTypeLower == "changesexsceneintent")
                confirmEnabled = Config::GetSingleton().ConfirmChangeSexSceneIntent();
            else if (actionTypeLower == "changesexscenepace")
                confirmEnabled = Config::GetSingleton().ConfirmChangeSexScenePace();
            else if (actionTypeLower == "stopsex")
                confirmEnabled = Config::GetSingleton().ConfirmStopSex();
            else if (actionTypeLower == "startcarescene")
                confirmEnabled = Config::GetSingleton().ConfirmStartCareScene();

            if (!confirmEnabled) {
                SKSE::log::info("ConfirmationModal::Show - auto-accepting '{}' (confirmation disabled in config)", actionTypeLower);
                SetCooldownAll(keys, normalSec);
                return ReturnNow(0);
            }
        }

        RE::Actor*  originator    = mainActors.empty() ? nullptr : mainActors[0];
        std::string actionTypeStr = actionType ? actionType : "";
        std::transform(actionTypeStr.begin(), actionTypeStr.end(), actionTypeStr.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        RE::FormID  originatorID  = originator ? originator->GetFormID() : 0;

        std::string message = BuildMessage(actionTypeStr, mainActorList, secondaryActorList, isSexual, activityStr, positionsStr, furnitureStr, excludedNamesStr, intentStr);

        SKSE::log::info("ConfirmationModal::Show - queuing modal: '{}'", message);

        if (auto* taskIF = SKSE::GetTaskInterface()) {
            taskIF->AddTask([this, vm, stackID, message, keys, normalSec, declineSec,
                             actionTypeStr, originatorID, intent, isSexual, activityStr,
                             mainActorList, secondaryActorList]() {
                SKSE::log::info("ConfirmationModal - game thread: creating MessageBoxData");
                auto* factory = RE::MessageDataFactoryManager::GetSingleton();
                auto* ifStr   = RE::InterfaceStrings::GetSingleton();
                auto* creator = factory ? factory->GetCreator<RE::MessageBoxData>(ifStr->messageBoxData) : nullptr;
                if (!creator) {
                    SKSE::log::error("ConfirmationModal - null creator");
                    vm->ReturnLatentResult(stackID, -1);
                    return;
                }
                auto* msgData = creator->Create();
                if (!msgData) {
                    SKSE::log::error("ConfirmationModal - null msgData");
                    vm->ReturnLatentResult(stackID, -1);
                    return;
                }
                msgData->bodyText = message.c_str();
                msgData->buttonText.push_back(RE::BSString("Accept"));
                msgData->buttonText.push_back(RE::BSString("Cancel"));
                msgData->buttonText.push_back(RE::BSString("Decline (Explain)"));
                msgData->cancelOptionIndex = 1;
                msgData->callback = RE::make_smart<ModalCallback>(vm, stackID, this, keys, normalSec, declineSec,
                    actionTypeStr, originatorID, intent, isSexual, activityStr,
                    mainActorList, secondaryActorList);
                SKSE::log::info("ConfirmationModal - game thread: calling QueueMessage");
                RE::MessageBoxMenu::QueueMessage(msgData);
                SKSE::log::info("ConfirmationModal - game thread: QueueMessage returned");
            });
        } else {
            SKSE::log::error("ConfirmationModal::Show - no task interface");
            return RE::BSScript::LatentStatus::kFailed;
        }
        // Papyrus stack suspended; ModalCallback will call ReturnLatentResult on button press
        return RE::BSScript::LatentStatus::kStarted;
    }

private:
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> _cooldowns;
    std::shared_mutex _cooldownMutex;

    // Nested class — accesses ConfirmationModal private members directly.
    class ModalCallback : public RE::IMessageBoxCallback {
    public:
        ModalCallback(RE::BSScript::Internal::VirtualMachine* vm,
                      RE::VMStackID stackID,
                      ConfirmationModal* owner,
                      std::vector<std::string> keys,
                      int normalSec,
                      int declineSec,
                      std::string actionType,
                      RE::FormID originatorID,
                      Intent intent,
                      bool isSexual,
                      std::string activity,
                      std::string mainActorList,
                      std::string secondaryActorList)
            : _vm(vm), _stackID(stackID), _owner(owner),
              _keys(std::move(keys)), _normalSec(normalSec), _declineSec(declineSec),
              _actionType(std::move(actionType)), _originatorID(originatorID),
              _intent(intent), _isSexual(isSexual), _activity(std::move(activity)),
              _mainActorList(std::move(mainActorList)), _secondaryActorList(std::move(secondaryActorList)) {}

        void Run(Message a_msg) override {
            int result = static_cast<int>(a_msg);
            SKSE::log::info("ConfirmationModal::ModalCallback::Run - result={}", result);
            if      (result == 0) _owner->SetCooldownAll(_keys, _normalSec);
            else if (result == 1) _owner->SetCooldownAll(_keys, _declineSec);
            else if (result == 2) { _owner->SetCooldownAll(_keys, _declineSec); FireTriggerTextInput(); }
            if (result == 2) {
                std::string json = BuildDeclineJson(_actionType, _intent, _isSexual, _activity, _mainActorList, _secondaryActorList, result == 2);
                FireModEvent("ostimnet_decline", json.c_str(), 0.0f, RE::PlayerCharacter::GetSingleton());
            }
            _vm->ReturnLatentResult(_stackID, result);
        }

    private:
        RE::BSScript::Internal::VirtualMachine* _vm;
        RE::VMStackID      _stackID;
        ConfirmationModal* _owner;
        std::vector<std::string> _keys;
        int                _normalSec;
        int                _declineSec;
        std::string        _actionType;
        RE::FormID         _originatorID;
        Intent             _intent;
        bool               _isSexual;
        std::string        _activity;
        std::string        _mainActorList;
        std::string        _secondaryActorList;

        static std::string BuildDeclineMsg(const std::string& actionType,
                                           const std::string& mainActorList,
                                           const std::string& secondaryActorList,
                                           const std::string& activity) {
            auto* playerActor  = RE::PlayerCharacter::GetSingleton();
            std::string player = playerActor ? playerActor->GetDisplayFullName() : "the player";
            std::string msg;
            if (actionType == "stopsex")
                msg = mainActorList + " wanted to stop the sexual activity, but " + player + " refused to end it.";
            else if (actionType == "changesexscenepace")
                msg = mainActorList + " wanted to change the pace of sexual activity with " + secondaryActorList + ", but " + player + " refused the change.";
            else if (actionType == "joinongoingsex") {
                msg = mainActorList + " wanted to join " + secondaryActorList + "'s sexual scene, but " + player + " refused to let them in.";
            } else if (actionType == "invitetoyoursex")
                msg = mainActorList + " wanted to invite more people to the scene, but " + player + " refused.";
            else if (actionType == "changesexsceneposition")
                msg = mainActorList + " wanted to change the sexual activity with " + secondaryActorList + ", but " + player + " refused.";
            else if (actionType == "changesexsceneintent")
                msg = mainActorList + " wanted to change the sexual intent with " + secondaryActorList + ", but " + player + " refused.";
            else if (actionType == "startcarescene")
                msg = mainActorList + " attempted " + activity + " with " + secondaryActorList + ", " + player + " did not go through with it.";
            else  // StartNewSex and any unknown action
                msg = mainActorList + " wanted to start a new sexual encounter with " + secondaryActorList + ", but " + player + " declined the proposal.";
            msg += " " + secondaryActorList + " ";
            return msg;
        }

        static std::string BuildDeclineJson(const std::string& actionType,
                                             Intent intent,
                                             bool isSexual,
                                             const std::string& activity,
                                             const std::string& mainActorList,
                                             const std::string& secondaryActorList,
                                             bool skipTrigger = false) {
            std::string msg = BuildDeclineMsg(actionType, mainActorList, secondaryActorList, activity);
            return BuildBaseEventJson("action_decline", msg, 0, skipTrigger, actionType,
                                      intent, isSexual,
                                      mainActorList, secondaryActorList).dump();
        }

        static void FireTriggerTextInput() {
            if (auto* taskIF = SKSE::GetTaskInterface()) {
                taskIF->AddTask([]() {
                    auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
                    if (!vm) return;
                    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> nullCallback;
                    vm->DispatchStaticCall("SkyrimNetApi", "TriggerTextInput", RE::MakeFunctionArguments(), nullCallback);
                });
            }
        }
    };

    // -------------------------------------------------------------------------

    static bool IsPlayer(RE::Actor* actor) {
        if (!actor) return false;
        auto* player = RE::PlayerCharacter::GetSingleton();
        return player && actor->GetFormID() == player->GetFormID();
    }

    static std::string BuildCooldownKey(const char* actionType, RE::FormID formID) {
        std::string lower = actionType ? actionType : "";
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        // Treat both pre- and post-eval variants as the same cooldown bucket.
        if (lower == "startnewsexpreeval" || lower == "startnewsexposteval" || lower == "startnewsexafterscan")
            lower = "startnewsex";
        std::ostringstream oss;
        oss << lower << ":";
        oss << std::hex << formID;
        return oss.str();
    }

    void SetCooldown(const std::string& key, int seconds) {
        auto expiry = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
        std::unique_lock lock(_cooldownMutex);
        _cooldowns[key] = expiry;
        SKSE::log::info("ConfirmationModal::SetCooldown - key='{}' set ({}s)", key, seconds);
    }

    void SetCooldownAll(const std::vector<std::string>& keys, int seconds) {
        auto expiry = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
        std::unique_lock lock(_cooldownMutex);
        for (const auto& key : keys) {
            _cooldowns[key] = expiry;
            SKSE::log::info("ConfirmationModal::SetCooldownAll - key='{}' set ({}s)", key, seconds);
        }
    }

    // -------------------------------------------------------------------------

    static std::string BuildMessage(const std::string& actionType,
                                    const std::string& mainActorList,
                                    const std::string& secondaryActorList,
                                    bool        /*isSexual*/,
                                    const std::string& activityStr,
                                    const std::string& positionsStr,
                                    const std::string& furnitureStr = "",
                                    const std::string& excludedNamesStr = "",
                                    const std::string& intentStr = "") {
        std::string msg;

        if (actionType == "startnewsexposteval") {
            // Post-evaluation: actor list was changed by evaluator
            msg = "After evaluation, the participant list has been adjusted.";
            if (!excludedNamesStr.empty())
                msg += "\n" + excludedNamesStr + " chose not to participate.";
            msg += "\n" + mainActorList + " will proceed with " + secondaryActorList + ".";
            if (!activityStr.empty() && !positionsStr.empty()) {
                msg += "\nActivity: " + activityStr + " (" + positionsStr + ")";
            } else if (!activityStr.empty()) {
                msg += "\nActivity: " + activityStr;
            } else if (!positionsStr.empty()) {
                msg += "\nPosition: " + positionsStr;
            }
            msg += "\nDo you want to go ahead?";
            return msg;
        } else if(actionType == "startnewsexpreeval" || actionType == "startnewsexafterscan") {
            // Pre-evaluation: immediate consent check before anything starts
            msg = mainActorList + " wants to start a sexual encounter with " + secondaryActorList;
            if (!furnitureStr.empty() && furnitureStr != "floor" && furnitureStr != "none") {
                msg += " on a " + furnitureStr;
            }
            if (!intentStr.empty()) {
                msg += " (intent: " + intentStr + ")";
            }
            msg += ".";
            return msg;
        } else if (actionType == "joinongoingsex") {
            msg = mainActorList + " wants to join " + secondaryActorList + " sexual encounter.";
            if (!intentStr.empty())
                msg += "\nNew scene intent would be: " + intentStr + ".";
            msg += "\nDo you want to let them in?";
            return msg;
        } else if (actionType == "changesexsceneintent") {
            msg = mainActorList + " wants to change the intent of the sexual encounter with " + secondaryActorList;
            if (!intentStr.empty()) {
                msg += " to " + intentStr;
            }
            msg += ".";
            msg += "\nDo you accept this change?";
            return msg;
        } else if (actionType == "invitetoyoursex") {
            msg = mainActorList + " wants to invite " + secondaryActorList + " to the ongoing sexual encounter.";
            msg += "\nDo you want to accept them?";
            return msg;
        } else if (actionType == "startcarescene") {
            msg = mainActorList + " wants to start a care scene with " + secondaryActorList;
            if (!activityStr.empty()) {
                msg += " (" + activityStr + ")";
            }
            if (!intentStr.empty()) {
                msg += " (intent: " + intentStr + ")";
            }
            msg += ".";
            msg += "\nDo you accept this?";
            return msg;
        }

        // Generic fallback for all other action types
        msg = mainActorList;
        msg += " wants to start ";
        msg += !actionType.empty() ? actionType : "an activity";
        msg += " with ";
        msg += secondaryActorList;
        msg += ".";

        if (!activityStr.empty() && !positionsStr.empty()) {
            msg += "\nActivity: ";
            msg += activityStr;
            msg += " (";
            msg += positionsStr;
            msg += ")";
        } else if (!activityStr.empty()) {
            msg += "\nActivity: ";
            msg += activityStr;
        } else if (!positionsStr.empty()) {
            msg += "\nPosition: ";
            msg += positionsStr;
        }

        return msg;
    }
};

}  // namespace OStimNet
