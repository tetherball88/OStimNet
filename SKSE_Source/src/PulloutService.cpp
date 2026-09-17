#include "PCH.h"

#include "PulloutService.h"
#include "ActorUtils.h"
#include "Config.h"
#include "ModEventDispatch.h"
#include "SkyrimNetIntegration.h"
#include "EventPayloadBuilder.h"
#include "api/OStimNavigator_PublicAPI.h"
#include "api/OstimNG-API-Thread.h"
#include "nlohmann/json.hpp"

namespace OStimNet {

std::vector<PulloutService::VaginalPair> PulloutService::GetVaginalPairs(const char* sceneId) {
    std::vector<VaginalPair> pairs;
    if (!sceneId || sceneId[0] == '\0') return pairs;

    if (ONavGetSceneActionsDetailed) {
        const char* detailedJson = ONavGetSceneActionsDetailed(sceneId);
        if (detailedJson && detailedJson[0] != '\0') {
            try {
                auto j = nlohmann::json::parse(detailedJson);
                if (j.is_array()) {
                    for (const auto& item : j) {
                        if (item.is_object() && item.contains("type")) {
                            std::string actionType = item["type"].get<std::string>();
                            if (actionType == "vaginalsex") {
                                VaginalPair pair;
                                if (item.contains("actor") && item["actor"].is_number_integer()) {
                                    pair.giverSlot = item["actor"].get<int>();
                                }
                                if (item.contains("target") && item["target"].is_number_integer()) {
                                    pair.receiverSlot = item["target"].get<int>();
                                }
                                if (pair.giverSlot >= 0 && pair.receiverSlot >= 0) {
                                    bool exists = false;
                                    for (const auto& existing : pairs) {
                                        if (existing.giverSlot == pair.giverSlot && existing.receiverSlot == pair.receiverSlot) {
                                            exists = true;
                                            break;
                                        }
                                    }
                                    if (!exists) {
                                        pairs.push_back(pair);
                                    }
                                }
                            }
                        }
                    }
                }
            } catch (const std::exception& e) {
                SKSE::log::error("PulloutService::GetVaginalPairs: failed to parse detailed actions for '{}': {}", sceneId, e.what());
            }
        }
    }

    // Fallback: If detailed actions unavailable or returned no pairs with slots, but scene has "vaginalsex" in ONavGetSceneActions
    if (pairs.empty() && ONavGetSceneActions) {
        const char* actionsJson = ONavGetSceneActions(sceneId);
        if (actionsJson && std::string(actionsJson).find("vaginalsex") != std::string::npos) {
            // Standard 2-actor fallback: slot 1 is giver (male penetrator) and slot 0 is receiver (female)
            pairs.push_back({ 1, 0 });
        }
    }

    return pairs;
}

bool PulloutService::IsPulloutAvailable(RE::Actor* actor) {
    if (!actor) return false;
    if (!Config::GetSingleton().PulloutEnabled()) return false;
    if (!ActorUtils::IsInFactionByEditorID(actor, "OStimActorCountFaction")) return false;

    int threadID = ThreadDataStore::GetSingleton().GetActorThreadID(actor->GetFormID());
    if (threadID == -1) return false;

    auto sexual = ThreadDataStore::GetSingleton().GetSexual(threadID);
    if (!sexual.value_or(false)) return false;

    Intent intent = ThreadDataStore::GetSingleton().GetIntent(threadID);
    if (IsIntentProhibited(intent)) return false;

    if (!g_ostimThreadInterface || !g_ostimThreadInterface->IsThreadValid(static_cast<uint32_t>(threadID))) return false;

    const char* curScene = g_ostimThreadInterface->GetCurrentSceneID(static_cast<uint32_t>(threadID));
    if (!curScene || curScene[0] == '\0') return false;

    auto pairs = GetVaginalPairs(curScene);
    if (pairs.empty()) return false;

    // Resolve actor's slot in the current thread
    constexpr uint32_t kMaxActors = 8;
    OstimNG_API::Thread::ActorData buffer[kMaxActors];
    uint32_t count = g_ostimThreadInterface->GetActors(static_cast<uint32_t>(threadID), buffer, kMaxActors);
    int actorSlot = -1;
    for (uint32_t i = 0; i < count; ++i) {
        if (buffer[i].formID == actor->GetFormID()) {
            actorSlot = static_cast<int>(i);
            break;
        }
    }
    if (actorSlot == -1) return false;

    // Check if this actor is giving or receiving vaginal sex in any pair
    for (const auto& pair : pairs) {
        if (actorSlot == pair.giverSlot || actorSlot == pair.receiverSlot) {
            return true;
        }
    }

    return false;
}

bool PulloutService::IsIntentProhibited(Intent intent) const {
    std::string val = Config::GetSingleton().PulloutProhibitedIntents();
    for (char& c : val) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (val == "none" || val == "all") {
        return false;
    }
    if (val == "aggressive") {
        return intent == Intent::Aggressive;
    }
    // "aggressive_dom" (default)
    return intent == Intent::Aggressive || intent == Intent::Dom;
}

void PulloutService::StallClimax(int threadID) {
#if OSTIM_STALL_CLIMAX_WORKAROUND
    // Native OThread.StallClimax is broken in OStim NG.
    // Stall is handled per-actor via StallActor() using excitement multipliers.
    SKSE::log::debug("PulloutService: StallClimax({}) bypassed (workaround active)", threadID);
#else
    if (auto* taskIF = SKSE::GetTaskInterface()) {
        taskIF->AddTask([threadID]() {
            auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
            if (vm) {
                RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> nullCallback;
                auto arg = threadID;
                vm->DispatchStaticCall("OThread", "StallClimax", RE::MakeFunctionArguments(std::move(arg)), nullCallback);
                SKSE::log::info("PulloutService: applied OThread.StallClimax({})", threadID);
            }
        });
    }
#endif
}

void PulloutService::ReleasePulloutStall(int threadID) {
    ThreadDataStore::GetSingleton().SetPulloutStallActive(threadID, false);
    ThreadDataStore::GetSingleton().SetPulloutEvaluationPending(threadID, false);
#if OSTIM_STALL_CLIMAX_WORKAROUND
    // Revert all actors whose excitement multipliers were zeroed for this thread
    RevertStalledActors(threadID);
#else
    if (auto* taskIF = SKSE::GetTaskInterface()) {
        taskIF->AddTask([threadID]() {
            auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
            if (vm) {
                RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> nullCallback;
                auto argID = threadID;
                auto argPermitActors = true;
                vm->DispatchStaticCall("OThread", "PermitClimax",
                    RE::MakeFunctionArguments(std::move(argID), std::move(argPermitActors)),
                    nullCallback);
                SKSE::log::info("PulloutService: released OThread.PermitClimax({}, true)", threadID);
            }
        });
    }
#endif
}

#if OSTIM_STALL_CLIMAX_WORKAROUND
// === Temporary Workaround: Excitement Multiplier Stall Implementation ===
namespace {
    struct GetExcitementMultiplierCallback : public RE::BSScript::IStackCallbackFunctor {
        int m_threadID;
        RE::FormID m_formID;

        GetExcitementMultiplierCallback(int threadID, RE::FormID formID)
            : m_threadID(threadID), m_formID(formID) {}

        void operator()(RE::BSScript::Variable a_result) override {
            float origMult = a_result.IsFloat() ? a_result.GetFloat() : 1.0f;
            if (origMult <= 0.001f) {
                origMult = 1.0f; // Safety fallback
            }

            int threadID = m_threadID;
            RE::FormID formID = m_formID;

            if (auto* taskIF = SKSE::GetTaskInterface()) {
                taskIF->AddTask([threadID, formID, origMult]() {
                    PulloutService::GetSingleton().OnActorMultiplierReceived(threadID, formID, origMult);
                });
            }
        }

        void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}
    };
}

void PulloutService::OnActorMultiplierReceived(int threadID, RE::FormID formID, float origMult) {
    {
        std::lock_guard<std::mutex> lock(m_stallMutex);
        m_pendingStallActors.erase(formID);

        // Check if the thread stall is still active
        if (!ThreadDataStore::GetSingleton().IsPulloutStallActive(threadID)) {
            SKSE::log::info("PulloutService: thread {} stall was released before multiplier callback for actor 0x{:08X} returned (val={})",
                threadID, formID, origMult);
            return;
        }

        m_stalledActorMultipliers[threadID][formID] = origMult;
    }

    auto* actor = RE::TESForm::LookupByID<RE::Actor>(formID);
    if (!actor) return;

    auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
    if (vm) {
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> nullCallback;
        auto argActor = actor;
        auto argMult = 0.0f;
        vm->DispatchStaticCall("OActor", "SetExcitementMultiplier",
            RE::MakeFunctionArguments(std::move(argActor), std::move(argMult)), nullCallback);
        SKSE::log::info("PulloutService: actor 0x{:08X} in thread {} excitement multiplier saved ({}) and set to 0.0",
            formID, threadID, origMult);
    }
}

bool PulloutService::IsActorStalledOrPending(int threadID, RE::FormID actorFormID) const {
    std::lock_guard<std::mutex> lock(m_stallMutex);
    if (m_pendingStallActors.contains(actorFormID)) return true;
    auto it = m_stalledActorMultipliers.find(threadID);
    if (it != m_stalledActorMultipliers.end() && it->second.contains(actorFormID)) {
        return true;
    }
    return false;
}

void PulloutService::StallActor(int threadID, RE::Actor* actor) {
    if (!actor) return;
    RE::FormID formID = actor->GetFormID();

    {
        std::lock_guard<std::mutex> lock(m_stallMutex);
        if (m_pendingStallActors.contains(formID)) return;
        auto it = m_stalledActorMultipliers.find(threadID);
        if (it != m_stalledActorMultipliers.end() && it->second.contains(formID)) {
            return;
        }
        m_pendingStallActors.insert(formID);
    }

    if (auto* taskIF = SKSE::GetTaskInterface()) {
        taskIF->AddTask([threadID, formID, actor]() {
            auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
            if (vm) {
                RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback =
                    RE::make_smart<GetExcitementMultiplierCallback>(threadID, formID);
                auto argActor = actor;
                vm->DispatchStaticCall("OActor", "GetExcitementMultiplier",
                    RE::MakeFunctionArguments(std::move(argActor)), callback);
                SKSE::log::info("PulloutService: querying OActor.GetExcitementMultiplier for actor 0x{:08X} in thread {}",
                    formID, threadID);
            }
        });
    }
}

void PulloutService::RevertStalledActors(int threadID) {
    std::unordered_map<RE::FormID, float> toRevert;
    {
        std::lock_guard<std::mutex> lock(m_stallMutex);
        auto it = m_stalledActorMultipliers.find(threadID);
        if (it != m_stalledActorMultipliers.end()) {
            toRevert = std::move(it->second);
            m_stalledActorMultipliers.erase(it);
        }
    }

    if (toRevert.empty()) return;

    if (auto* taskIF = SKSE::GetTaskInterface()) {
        taskIF->AddTask([threadID, toRevert = std::move(toRevert)]() {
            auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
            if (!vm) return;

            for (const auto& [formID, savedMult] : toRevert) {
                auto* actor = RE::TESForm::LookupByID<RE::Actor>(formID);
                if (actor) {
                    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> nullCallback;
                    auto argActor = actor;
                    auto argMult = savedMult;
                    vm->DispatchStaticCall("OActor", "SetExcitementMultiplier",
                        RE::MakeFunctionArguments(std::move(argActor), std::move(argMult)), nullCallback);
                    SKSE::log::info("PulloutService: restored OActor.SetExcitementMultiplier for actor 0x{:08X} in thread {} to {}",
                        formID, threadID, savedMult);
                } else {
                    SKSE::log::warn("PulloutService: could not lookup actor 0x{:08X} to restore multiplier ({}) in thread {}",
                        formID, savedMult, threadID);
                }
            }
        });
    }
}
#endif

void PulloutService::FirePulloutSensoryCue(int threadID) {
    if (!g_ostimThreadInterface || !g_ostimThreadInterface->IsThreadValid(static_cast<uint32_t>(threadID))) return;

    const char* curScene = g_ostimThreadInterface->GetCurrentSceneID(static_cast<uint32_t>(threadID));
    if (!curScene || curScene[0] == '\0') return;

    auto pairs = GetVaginalPairs(curScene);
    if (pairs.empty()) return;

    constexpr uint32_t kMax = 8;
    OstimNG_API::Thread::ActorData buf[kMax];
    uint32_t count = g_ostimThreadInterface->GetActors(static_cast<uint32_t>(threadID), buf, kMax);
    if (count == 0) return;

    // Find the vaginal pair whose giver has the highest excitement
    int peakingGiverSlot = -1;
    int matchingReceiverSlot = -1;
    float maxEx = -1.0f;

    for (const auto& pair : pairs) {
        if (pair.giverSlot >= 0 && static_cast<uint32_t>(pair.giverSlot) < count) {
            float ex = buf[pair.giverSlot].excitement;
            if (ex > maxEx) {
                maxEx = ex;
                peakingGiverSlot = pair.giverSlot;
                matchingReceiverSlot = pair.receiverSlot;
            }
        }
    }

    if (peakingGiverSlot < 0 || matchingReceiverSlot < 0) return;

    // Target is the male giving vaginal sex who is approaching climax
    RE::Actor* target = RE::TESForm::LookupByID<RE::Actor>(buf[peakingGiverSlot].formID);
    // Speaker is the partner receiving vaginal sex from him (reacts and makes pullout decision)
    RE::Actor* speaker = (static_cast<uint32_t>(matchingReceiverSlot) < count)
        ? RE::TESForm::LookupByID<RE::Actor>(buf[matchingReceiverSlot].formID)
        : nullptr;

    if (!speaker && !target) return;
    if (!speaker) speaker = target;
    if (!target) target = speaker;

    std::string jsonStr = EventPayloadBuilder::BuildPartnerNearEdge(threadID, speaker);

    // Dispatch directly to SkyrimNet via SkyrimNetApi.RegisterEvent
    if (auto* taskIF = SKSE::GetTaskInterface()) {
        taskIF->AddTask([jsonStr, speaker, target]() {
            auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
            if (vm) {
                RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> nullCallback;
                auto eventType = RE::BSFixedString("tton_event");
                auto msg = RE::BSFixedString(jsonStr);
                auto spk = speaker;
                auto tgt = target;
                vm->DispatchStaticCall("SkyrimNetApi", "RegisterEvent",
                    RE::MakeFunctionArguments(std::move(eventType), std::move(msg), std::move(spk), std::move(tgt)),
                    nullCallback);
                SKSE::log::info("PulloutService: dispatched tton_event (partner_near_edge) to SkyrimNetApi (speaker=0x{:08X}, target=0x{:08X})",
                    speaker ? speaker->GetFormID() : 0, target ? target->GetFormID() : 0);
            }
        });
    }
}

bool PulloutService::ExecutePulloutNavigation(int threadID) {
    if (!g_ostimThreadInterface || !g_ostimThreadInterface->IsThreadValid(static_cast<uint32_t>(threadID))) {
        ReleasePulloutStall(threadID);
        return false;
    }

    const char* curScene = g_ostimThreadInterface->GetCurrentSceneID(static_cast<uint32_t>(threadID));
    if (!curScene || curScene[0] == '\0') {
        SKSE::log::warn("PulloutService: thread {} has no current scene", threadID);
        ReleasePulloutStall(threadID);
        return false;
    }

    std::string targetScene;
    if (ONavFindPulloutScene) {
        const char* res = ONavFindPulloutScene(curScene, static_cast<uint32_t>(threadID));
        if (res && res[0] != '\0') {
            targetScene = res;
        }
    }

    if (!targetScene.empty()) {
        SKSE::log::info("PulloutService: transitioning thread {} from '{}' to pullout scene '{}'",
            threadID, curScene, targetScene);
        g_ostimThreadInterface->NavigateToSearchResult(static_cast<uint32_t>(threadID), targetScene.c_str());

        // Release climax stall now that transition is queued
        ReleasePulloutStall(threadID);
        ThreadDataStore::GetSingleton().SetPulloutEvaluatedForCycle(threadID, true);
        return true;
    } else {
        SKSE::log::warn("PulloutService: no suitable pullout scene found for scene '{}', thread {}", curScene, threadID);
        ReleasePulloutStall(threadID);
        ThreadDataStore::GetSingleton().SetPulloutEvaluatedForCycle(threadID, true);
        return false;
    }
}

void PulloutService::SetPulloutDecision(RE::Actor* actor, const std::string& decisionStr) {
    if (!actor) return;
    int threadID = ThreadDataStore::GetSingleton().GetActorThreadID(actor->GetFormID());
    if (threadID == -1) return;
    PulloutDecision decision = PulloutDecisionFromString(decisionStr);
    SetPulloutDecision(threadID, decision);
}

void PulloutService::SetPulloutDecision(int threadID, PulloutDecision decision) {
    ThreadDataStore::GetSingleton().SetPulloutDecision(threadID, decision);
    SKSE::log::info("PulloutService: thread {} decision set to '{}'", threadID, PulloutDecisionToString(decision));

    // If stall is currently active, unblock immediately
    if (ThreadDataStore::GetSingleton().IsPulloutStallActive(threadID)) {
        if (decision == PulloutDecision::RequestPullout) {
            ExecutePulloutNavigation(threadID);
        } else if (decision == PulloutDecision::AllowFinishInside) {
            ReleasePulloutStall(threadID);
            ThreadDataStore::GetSingleton().SetPulloutEvaluatedForCycle(threadID, true);
        }
    }
}

void PulloutService::OnLLMDecision(int threadID, const std::string& decisionStr) {
    if (!g_ostimThreadInterface || !g_ostimThreadInterface->IsThreadValid(static_cast<uint32_t>(threadID)) ||
        !ThreadDataStore::GetSingleton().IsOStimNet(threadID)) {
        SKSE::log::info("PulloutService::OnLLMDecision: thread {} ended or invalid during LLM wait", threadID);
        ReleasePulloutStall(threadID);
        return;
    }

    SKSE::log::info("PulloutService::OnLLMDecision: decision='{}' for thread {}", decisionStr, threadID);

    if (decisionStr == "pullout" || decisionStr == "request_pullout") {
        ThreadDataStore::GetSingleton().SetPulloutDecision(threadID, PulloutDecision::RequestPullout);
        ExecutePulloutNavigation(threadID);
    } else {
        ThreadDataStore::GetSingleton().SetPulloutDecision(threadID, PulloutDecision::AllowFinishInside);
        ReleasePulloutStall(threadID);
        ThreadDataStore::GetSingleton().SetPulloutEvaluatedForCycle(threadID, true);
    }
}

void PulloutService::OnLLMFailure(int threadID) {
    SKSE::log::warn("PulloutService::OnLLMFailure: releasing stall for thread {}", threadID);
    ReleasePulloutStall(threadID);
    ThreadDataStore::GetSingleton().SetPulloutEvaluatedForCycle(threadID, true);
}

void PulloutService::SyncOStimPulloutSetting() {
    if (!m_ostimPulloutChanceGlobal) {
        if (auto* dataHandler = RE::TESDataHandler::GetSingleton()) {
            m_ostimPulloutChanceGlobal = dataHandler->LookupForm<RE::TESGlobal>(0xE37, "OStim.esp");
            if (m_ostimPulloutChanceGlobal) {
                SKSE::log::info("PulloutService: resolved OStim.esp global 0xE37 (autoModePulloutChance)");
            }
        }
    }

    if (!m_ostimPulloutChanceGlobal) return;

    bool enabled = Config::GetSingleton().PulloutEnabled();
    float targetChance = enabled ? 0.0f : 75.0f;

    if (m_ostimPulloutChanceGlobal->value != targetChance) {
        SKSE::log::info("PulloutService: syncing OStim native autoModePulloutChance (0xE37) from {} to {} (OStimNet pullout enabled = {})",
            m_ostimPulloutChanceGlobal->value, targetChance, enabled);
        m_ostimPulloutChanceGlobal->value = targetChance;
    }
}

int PulloutService::GetOStimPulloutHotkey() const {
    if (g_ostimThreadInterface) {
        OstimNG_API::Thread::KeyData keys{};
        g_ostimThreadInterface->GetKeyData(&keys);
        if (keys.keyPullOut > 0) {
            return keys.keyPullOut;
        }
    }

    if (auto* dataHandler = RE::TESDataHandler::GetSingleton()) {
        auto* global = dataHandler->LookupForm<RE::TESGlobal>(0xDEA, "OStim.esp");
        if (global) {
            return static_cast<int>(global->value);
        }
    }

    return 79; // Default OStim pullout key: DIK_NUMPAD1
}

bool PulloutService::TriggerPlayerPullout() {
    if (!Config::GetSingleton().PulloutEnabled()) return false;
    if (!g_ostimThreadInterface) return false;

    uint32_t threadID = g_ostimThreadInterface->GetPlayerThreadID();
    if (!g_ostimThreadInterface->IsThreadValid(threadID)) return false;

    auto& store = ThreadDataStore::GetSingleton();
    if (!store.IsOStimNet(static_cast<int>(threadID))) return false;

    auto sexual = store.GetSexual(static_cast<int>(threadID));
    if (!sexual.value_or(false)) return false;

    const char* curScene = g_ostimThreadInterface->GetCurrentSceneID(threadID);
    if (!curScene || curScene[0] == '\0') return false;

    auto pairs = GetVaginalPairs(curScene);
    if (pairs.empty()) return false; // Only vaginal intercourse scenes have pullout mechanics

    // Check that the player is actually giving or receiving vaginal sex in this scene!
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return false;
    uint32_t playerFormID = player->GetFormID();

    constexpr uint32_t kMaxActors = 8;
    OstimNG_API::Thread::ActorData buffer[kMaxActors];
    uint32_t count = g_ostimThreadInterface->GetActors(threadID, buffer, kMaxActors);

    int playerSlot = -1;
    for (uint32_t i = 0; i < count; ++i) {
        if (buffer[i].formID == playerFormID) {
            playerSlot = static_cast<int>(i);
            break;
        }
    }
    if (playerSlot == -1) return false;

    bool playerInVaginalPair = false;
    for (const auto& pair : pairs) {
        if (playerSlot == pair.giverSlot || playerSlot == pair.receiverSlot) {
            playerInVaginalPair = true;
            break;
        }
    }
    if (!playerInVaginalPair) {
        SKSE::log::info("PulloutService: player pressed pullout hotkey, but is neither giving nor receiving vaginal sex in scene '{}' (thread {})",
            curScene, threadID);
        return false;
    }

    SKSE::log::info("PulloutService: player hotkey triggered immediate pullout for thread {}", threadID);

    // Cancel any waiting actor dialogue/action or pending LLM evaluation
    store.SetPulloutEvaluationPending(static_cast<int>(threadID), false);
    store.SetPulloutDecision(static_cast<int>(threadID), PulloutDecision::RequestPullout);

#if !OSTIM_STALL_CLIMAX_WORKAROUND
    // Stall climax immediately to prevent finish inside while navigating
    StallClimax(static_cast<int>(threadID));
    store.SetPulloutStallActive(static_cast<int>(threadID), true);
#endif

    // Navigate to pullout scene immediately without waiting for actions or evaluations
    return ExecutePulloutNavigation(static_cast<int>(threadID));
}

void PulloutService::CheckActiveThreads(const std::vector<int>& activeThreads) {
    SyncOStimPulloutSetting();

    if (!Config::GetSingleton().PulloutEnabled()) return;
    if (!g_ostimThreadInterface) return;

    auto now = std::chrono::steady_clock::now();
    auto& store = ThreadDataStore::GetSingleton();
    float threshold = Config::GetSingleton().PulloutExcitementThreshold();
    float timeoutSecs = Config::GetSingleton().PulloutEvalTimeoutSeconds();

    for (int threadID : activeThreads) {
        if (!g_ostimThreadInterface->IsThreadValid(static_cast<uint32_t>(threadID))) continue;
        if (!store.IsOStimNet(threadID)) continue;

        auto sexual = store.GetSexual(threadID);
        if (!sexual.value_or(false)) continue;

        Intent intent = store.GetIntent(threadID);
        if (IsIntentProhibited(intent)) continue;

        // ---------------------------------------------------------------------
        // Case 1: Thread is currently stalled for pullout mechanics
        // ---------------------------------------------------------------------
        if (store.IsPulloutStallActive(threadID)) {
            auto stallStart = store.GetPulloutStallStartTime(threadID);
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - stallStart).count();

            // Check if an actor has made a decision (e.g. via PulloutDecision action call)
            auto currentDecision = store.GetPulloutDecision(threadID);
            if (currentDecision == PulloutDecision::RequestPullout) {
                SKSE::log::info("PulloutService: thread {} received RequestPullout decision during stall, executing navigation", threadID);
                ExecutePulloutNavigation(threadID);
                continue;
            } else if (currentDecision == PulloutDecision::AllowFinishInside || currentDecision == PulloutDecision::DoesntCare) {
                SKSE::log::info("PulloutService: thread {} received finish inside decision during stall, releasing stall", threadID);
                ReleasePulloutStall(threadID);
                store.SetPulloutEvaluatedForCycle(threadID, true);
                continue;
            }

            // Still undecided: check if we are waiting for the actor's action or if LLM eval is already pending
            if (!store.IsPulloutEvaluationPending(threadID)) {
                // Waiting for the actor to make a decision via dialogue + action call
                if (elapsed >= timeoutSecs) {
                    SKSE::log::info("PulloutService: actor decision timeout ({}s >= {}s) elapsed without decision for thread {}, firing custom LLM evaluation",
                        elapsed, timeoutSecs, threadID);
                    store.SetPulloutEvaluationPending(threadID, true);
                    SkyrimNetIntegration::EvaluatePulloutDecision(threadID);
                }
            }

#if OSTIM_STALL_CLIMAX_WORKAROUND
            // Multi-actor dynamic check while thread stall is active:
            // If another actor in this thread (e.g. Actor 1 in DP / threesome) reaches the excitement threshold,
            // freeze their multiplier too, joining this thread's existing stall.
            const char* curScene = g_ostimThreadInterface->GetCurrentSceneID(static_cast<uint32_t>(threadID));
            if (curScene && curScene[0] != '\0') {
                auto pairs = GetVaginalPairs(curScene);
                if (!pairs.empty()) {
                    constexpr uint32_t kMaxActors = 8;
                    OstimNG_API::Thread::ActorData buffer[kMaxActors];
                    uint32_t count = g_ostimThreadInterface->GetActors(static_cast<uint32_t>(threadID), buffer, kMaxActors);
                    for (const auto& pair : pairs) {
                        if (pair.giverSlot >= 0 && static_cast<uint32_t>(pair.giverSlot) < count) {
                            const auto& giverData = buffer[pair.giverSlot];
                            if (giverData.excitement >= threshold && !IsActorStalledOrPending(threadID, giverData.formID)) {
                                auto* giverActor = RE::TESForm::LookupByID<RE::Actor>(giverData.formID);
                                if (giverActor) {
                                    SKSE::log::info("PulloutService: secondary giver 0x{:08X} in thread {} reached threshold ({}) during active stall, stalling actor",
                                        giverData.formID, threadID, giverData.excitement);
                                    StallActor(threadID, giverActor);
                                }
                            }
                        }
                    }
                }
            }
#endif
            continue;
        }

        // If this cycle was already evaluated (e.g. climax cycle resolved), skip
        if (store.IsPulloutEvaluatedForCycle(threadID)) continue;

        // Check if current scene has vaginalsex
        const char* curScene = g_ostimThreadInterface->GetCurrentSceneID(static_cast<uint32_t>(threadID));
        if (!curScene || curScene[0] == '\0') continue;

        auto pairs = GetVaginalPairs(curScene);
        if (pairs.empty()) continue; // Not a vaginal sex scene or no vaginal pairs

        // Get excitement of actors
        constexpr uint32_t kMaxActors = 8;
        OstimNG_API::Thread::ActorData buffer[kMaxActors];
        uint32_t count = g_ostimThreadInterface->GetActors(static_cast<uint32_t>(threadID), buffer, kMaxActors);
        if (count == 0) continue;

        // ONLY check excitement for actors GIVING vaginal sex!
        // If an actor is receiving a blowjob/handjob, we do NOT care about his excitement!
        float giverExcitement = 0.0f;
#if OSTIM_STALL_CLIMAX_WORKAROUND
        std::vector<int> readyGiverSlots;
#endif
        for (const auto& pair : pairs) {
            if (pair.giverSlot >= 0 && static_cast<uint32_t>(pair.giverSlot) < count) {
                float ex = buffer[pair.giverSlot].excitement;
                if (ex > giverExcitement) {
                    giverExcitement = ex;
                }
#if OSTIM_STALL_CLIMAX_WORKAROUND
                if (ex >= threshold) {
                    if (std::find(readyGiverSlots.begin(), readyGiverSlots.end(), pair.giverSlot) == readyGiverSlots.end()) {
                        readyGiverSlots.push_back(pair.giverSlot);
                    }
                }
#endif
            }
        }

        if (giverExcitement < threshold) continue;

        // ---------------------------------------------------------------------
        // Case 2: Vaginal giver reached excitement threshold!
        // ---------------------------------------------------------------------
        auto decision = store.GetPulloutDecision(threadID);
        if (decision == PulloutDecision::RequestPullout) {
            // Decision was already invoked earlier in the scene!
            SKSE::log::info("PulloutService: thread {} reached threshold ({}) with prior RequestPullout decision, stalling and navigating",
                threadID, threshold);
#if OSTIM_STALL_CLIMAX_WORKAROUND
            for (int slot : readyGiverSlots) {
                auto* giverActor = RE::TESForm::LookupByID<RE::Actor>(buffer[slot].formID);
                if (giverActor) StallActor(threadID, giverActor);
            }
#else
            StallClimax(threadID);
#endif
            store.SetPulloutStallActive(threadID, true);
            ExecutePulloutNavigation(threadID);
        } else if (decision == PulloutDecision::AllowFinishInside || decision == PulloutDecision::DoesntCare) {
            // Actor already decided earlier to finish inside!
            SKSE::log::info("PulloutService: thread {} reached threshold ({}) with prior AllowFinishInside decision, permitting climax",
                threadID, threshold);
            store.SetPulloutEvaluatedForCycle(threadID, true);
        } else {
            // Decision was NOT made yet during the thread!
            // 1. Stall climax immediately
            SKSE::log::info("PulloutService: thread {} giver excitement ({}) reached threshold ({}) without decision — stalling climax and waiting up to {}s for actor action call",
                threadID, giverExcitement, threshold, timeoutSecs);
#if OSTIM_STALL_CLIMAX_WORKAROUND
            for (int slot : readyGiverSlots) {
                auto* giverActor = RE::TESForm::LookupByID<RE::Actor>(buffer[slot].formID);
                if (giverActor) StallActor(threadID, giverActor);
            }
#else
            StallClimax(threadID);
#endif
            store.SetPulloutStallActive(threadID, true);

            // 2. Fire event + direct narration trigger for actor's dialogue + action call
            if (Config::GetSingleton().PulloutNarrateCue() && !store.HasPulloutCueFired(threadID)) {
                store.SetPulloutCueFired(threadID, true);
                FirePulloutSensoryCue(threadID);
            }
            // 3. We do NOT fire LLM evaluation immediately. We wait during PulloutEvalTimeoutSeconds for actor's action decision!
        }
    }
}

void PulloutService::OnThreadEnd(int threadID) {
    if (ThreadDataStore::GetSingleton().IsPulloutStallActive(threadID)) {
        ReleasePulloutStall(threadID);
    }
#if OSTIM_STALL_CLIMAX_WORKAROUND
    else {
        // Safety: ensure any stalled actors in this thread are reverted even if stall active flag was cleared
        RevertStalledActors(threadID);
    }
#endif
}

} // namespace OStimNet
