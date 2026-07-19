#pragma once
#include <algorithm>
#include <cctype>
#include "src/SceneDescriptionUtils.h"
#include "src/ThreadRegistry.h"
#include "src/OStimEventListener.h"
#include "src/SkyrimNetIntegration.h"
#include "src/JsonService.h"
#include "src/ConfirmationModal.h"
#include "src/EventPayloadBuilder.h"
#include "src/LocationScanService.h"

namespace OStimNet::Papyrus::PapyrusFunctions {

// ---------------------------------------------------------------------------
// Workaround for a bug in CommonLibSSE-NG v4.14.0 NativeLatentFunction.h:
// Its constructor calls GetRawType<R>() (constructs functor) instead of
// GetRawType<R>{}() (constructs and invokes it) — C2679 compile error.
// We subclass BSScript::NativeFunction directly, which uses the correct form,
// and patch _retType + _isLatent ourselves. Then bind via BindNativeMethod.
// ---------------------------------------------------------------------------
template <class LatentR, class Fn, class Cls, class... Args>
class FixedNativeLatentFunction :
    public RE::BSScript::NativeFunction<
        /*IS_LONG=*/true,
        RE::BSScript::LatentStatus(RE::BSScript::Internal::VirtualMachine*, RE::VMStackID, Cls, Args...),
        std::underlying_type_t<RE::BSScript::LatentStatus>,
        Cls, Args...>
{
    using Super = RE::BSScript::NativeFunction<
        true,
        RE::BSScript::LatentStatus(RE::BSScript::Internal::VirtualMachine*, RE::VMStackID, Cls, Args...),
        std::underlying_type_t<RE::BSScript::LatentStatus>,
        Cls, Args...>;
public:
    FixedNativeLatentFunction(std::string_view fnName, std::string_view className, Fn* callback)
        : Super(fnName, className, callback)
    {
        this->_retType  = RE::BSScript::GetRawType<LatentR>{}();
        this->_isLatent = true;
    }
};

// Registers a latent Papyrus function. F must have the signature:
//   LatentStatus(VirtualMachine*, VMStackID, StaticFunctionTag*, Args...)
// LatentR is the Papyrus return type the script will receive.
template <class LatentR, class... Args>
static void RegisterLatentFixed(RE::BSScript::IVirtualMachine* vm,
                                 std::string_view fnName,
                                 std::string_view className,
                                 RE::BSScript::LatentStatus(*callback)(RE::BSScript::Internal::VirtualMachine*, RE::VMStackID, RE::StaticFunctionTag*, Args...)) {
    vm->BindNativeMethod(
        new FixedNativeLatentFunction<LatentR,
            RE::BSScript::LatentStatus(RE::BSScript::Internal::VirtualMachine*, RE::VMStackID, RE::StaticFunctionTag*, Args...),
            RE::StaticFunctionTag*,
            Args...>(fnName, className, callback));
}


    static std::string ToLower(const char* s) {
        std::string out(s ? s : "");
        std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return std::tolower(c); });
        return out;
    }

    // Papyrus native: int Function GetLocationGeneration() global native
    int32_t GetLocationGeneration(RE::StaticFunctionTag*) {
        return static_cast<int32_t>(OStimNet::LocationScanService::GetSingleton().GetGeneration());
    }

    // Papyrus native: string Function GetSceneDescription(int threadID) global
    RE::BSFixedString GetSceneDescription(RE::StaticFunctionTag*,
                                          uint32_t threadID) {
        std::string result = OStimNet::GetSceneDescription(threadID);
        return RE::BSFixedString(result.c_str());
    }

    // Papyrus native: string Function GetActorListString(Actor[] actors, Actor akExclude = None) global
    // Returns actor display names joined naturally, e.g. "Lydia", "Lydia and Farengar",
    // "Lydia, Farengar and Ulfric". akExclude is optional — pass None to include all actors.
    RE::BSFixedString GetActorListString(RE::StaticFunctionTag*,
                                         std::vector<RE::Actor*> actors,
                                         RE::Actor* exclude) {
        std::vector<std::string> names;
        for (RE::Actor* actor : actors) {
            if (!actor) continue;
            if (exclude && actor->GetFormID() == exclude->GetFormID()) continue;
            std::string name = OStimNet::ThreadDataStore::GetActorDisplayName(actor, "");
            if (!name.empty()) names.push_back(std::move(name));
        }
        if (names.empty()) return RE::BSFixedString("");
        return RE::BSFixedString(OStimNet::FormatActorList(names).c_str());
    }

    // Papyrus native: Function Log(string msg, int level) global
    // Levels: 0 = trace, 1 = debug, 2 = info, 3 = warn, 4 = error
    void Log(RE::StaticFunctionTag*, RE::BSFixedString msg, int32_t level) {
        switch (level) {
            case 0:  SKSE::log::trace("[OStimNet(trace)]: {}", msg); break;
            case 1:  SKSE::log::debug("[OStimNet(debug)]: {}", msg); break;
            case 2:  SKSE::log::info("[OStimNet(info)]: {}",   msg); break;
            case 3:  SKSE::log::warn("[OStimNet(warn)]: {}",   msg); break;
            case 4:  SKSE::log::error("[OStimNet(err)]: {}",   msg); break;
            default: SKSE::log::info("[OStimNet(info)]: {}",   msg); break;
        }
    }

    // Papyrus native: Function SetThreadContinuation(int threadID, bool isContinuation) global
    // Call this before triggering an add-actor operation that causes OStim to end and
    // restart the thread. While true, ostimnet_sex_stop is suppressed for that thread end.
    void SetThreadContinuation(RE::StaticFunctionTag*, int32_t threadID, bool isContinuation) {
        OStimNet::ThreadDataStore::GetSingleton().SetContinuation(threadID, isContinuation);
    }

    // Papyrus native: Function SetContinuationOverride(int threadID, string newIntent,
    //                                                   Actor[] main, Actor[] secondary) global
    // Call this before SetThreadContinuation + the hot-swap for actor-add scenarios.
    // Provides LLM-evaluated intent and actor roles so CopyStateForContinuation uses
    // them instead of copying stale data (sceneDescription, currentPosition) from the
    // old thread.  The swap path (no call to this) is unaffected.
    void SetContinuationOverride(RE::StaticFunctionTag*,
                                  int32_t threadID,
                                  RE::BSFixedString newIntent,
                                  std::vector<RE::Actor*> mainActors,
                                  std::vector<RE::Actor*> secondaryActors) {
        std::string intentLower = ToLower(newIntent.c_str());
        OStimNet::Intent intentEnum = OStimNet::IntentFromString(intentLower);
        SKSE::log::info("PapyrusFunctions: SetContinuationOverride threadID={} intent={} main={} secondary={}",
            threadID, intentLower,
            static_cast<int>(mainActors.size()), static_cast<int>(secondaryActors.size()));
        OStimNet::ThreadDataStore::GetSingleton().SetContinuationOverride(
            threadID, intentEnum, std::move(mainActors), std::move(secondaryActors));
    }

    // -------------------------------------------------------------------------
    // Two-call claim protocol (ClaimThread / ConfirmThread)
    // -------------------------------------------------------------------------

    // Papyrus native: int Function ClaimThread(string intent, bool isSexual,
    //                                          Actor[] mainActors, Actor[] secondaryActors) global
    // Call this BEFORE OThreadBuilder.Start(). Allocates a claim token in ThreadRegistry
    // so that when ostim_thread_start fires, HandleStart can immediately mark the thread
    // Active without waiting for any follow-up Papyrus calls. Returns the token (>0) that
    // must be passed to ConfirmThread() after OThreadBuilder.Start() returns the threadID.
    int32_t ClaimThread(RE::StaticFunctionTag*,
                        RE::BSFixedString intent,
                        bool isSexual,
                        std::vector<RE::Actor*> mainActors,
                        std::vector<RE::Actor*> secondaryActors) {
        std::string intentLower = ToLower(intent.c_str());
        OStimNet::Intent intentEnum = OStimNet::IntentFromString(intentLower);
        int token = OStimNet::ThreadRegistry::GetSingleton().AllocateClaimToken(
            intentEnum, isSexual, std::move(mainActors), std::move(secondaryActors));
        SKSE::log::info("PapyrusFunctions: ClaimThread intent={} isSexual={} -> token={}",
                        intentLower, isSexual, token);
        return token;
    }

    // Papyrus native: Function ConfirmThread(int claimToken, int threadID) global
    // Call this AFTER OThreadBuilder.Start() returns the real threadID.
    // Binds the claim token to the thread so HandleStart (which may have already fired)
    // can resolve the claim and mark the thread Active immediately.
    void ConfirmThread(RE::StaticFunctionTag*, int32_t claimToken, int32_t threadID) {
        SKSE::log::info("PapyrusFunctions: ConfirmThread token={} threadID={}", claimToken, threadID);
        bool ok = OStimNet::ThreadRegistry::GetSingleton().ConfirmClaim(claimToken, threadID);
        if (!ok) {
            SKSE::log::warn("PapyrusFunctions: ConfirmThread token={} not found (thread {} may have already ended)",
                            claimToken, threadID);
            return;
        }
        // HandleStart may have already fired before ConfirmThread arrived.
        // ConsumeActiveClaim checks: if the thread is Pending and a matching claim exists,
        // it applies the claim data and transitions the thread to Active.
        auto* listener = OStimNet::OStimEventListener::GetInstance();
        if (listener) listener->TryActivateClaimedThread(threadID);
    }

    // Papyrus native: Actor Function GetSpectatorTarget(Actor akSpectator) global
    // Returns the actor in an active OStim thread that the given spectator has a
    // romantic/marital relationship with, or None if not currently tracked.
    RE::Actor* GetSpectatorTarget(RE::StaticFunctionTag*, RE::Actor* spectator) {
        if (!spectator) return nullptr;
        RE::FormID targetID = OStimNet::ThreadRegistry::GetSingleton().GetSpectatorTargetFormID(spectator->GetFormID());
        if (!targetID) return nullptr;
        return RE::TESForm::LookupByID<RE::Actor>(targetID);
    }

    // Papyrus native: Function AddSpectator(Actor akSpectator, int ThreadID, Actor akTarget = None) global
    // Manually registers akSpectator as watching the given thread.
    // akTarget: the actor in the thread the spectator is focused on.
    //   Pass None to use the first actor in the thread as the default target.
    // Fires ostimnet_add_spectator mod event.
    void AddSpectator(RE::StaticFunctionTag*, RE::Actor* spectator, int32_t threadID, RE::Actor* target) {
        if (!spectator) return;
        RE::FormID spectatorID = spectator->GetFormID();

        RE::FormID targetID = 0;
        if (target) {
            targetID = target->GetFormID();
        } else {
            const auto& actors = OStimNet::ThreadRegistry::GetSingleton().GetActorPtrs(threadID);
            if (!actors.empty() && actors[0])
                targetID = actors[0]->GetFormID();
        }

        OStimNet::ThreadRegistry::GetSingleton().AddSpectator(threadID, spectatorID, targetID);

        std::string json;
        json += "{\"spectatorFormID\":";   json += std::to_string(spectatorID);
        json += ",\"targetActorFormID\":"; json += std::to_string(targetID);
        json += ",\"threadID\":";          json += std::to_string(threadID);
        json += "}";
        if (auto* source = SKSE::GetModCallbackEventSource()) {
            SKSE::ModCallbackEvent e{"ostimnet_add_spectator", json.c_str(), static_cast<float>(threadID), spectator};
            source->SendEvent(&e);
        }
        SKSE::log::info("PapyrusFunctions: AddSpectator spectator=0x{:08X} thread={} target=0x{:08X}",
            spectatorID, threadID, targetID);
    }

    // Papyrus native: Function RemoveSpectator(Actor akSpectator) global
    // Removes akSpectator from spectator tracking and fires ostimnet_remove_spectator.
    // Does nothing if the actor is not currently tracked as a spectator.
    void RemoveSpectator(RE::StaticFunctionTag*, RE::Actor* spectator) {
        if (!spectator) return;
        RE::FormID spectatorID = spectator->GetFormID();
        auto info = OStimNet::ThreadRegistry::GetSingleton().RemoveSpectator(spectatorID);
        if (info.threadID == -1) {
            SKSE::log::warn("PapyrusFunctions: RemoveSpectator 0x{:08X} not tracked", spectatorID);
            return;
        }
        OStimNet::DebounceQueue::FireRemoveSpectatorEvent(spectatorID, info.targetID, info.threadID);
        SKSE::log::info("PapyrusFunctions: RemoveSpectator spectator=0x{:08X} thread={}", spectatorID, info.threadID);
    }

    // Papyrus native: Function OcumApplied(int threadID, Actor orgasmer, Actor target, float amountML, string area, string sceneID) global
    // Called by the Papyrus listener for ocum_applied_cum / ocum_applied_cum_npc_scene.
    // Appends cum-applied data to the thread's pending climax batch and resets the debounce timer.
    void OcumApplied(RE::StaticFunctionTag*, int32_t threadID,
                     RE::Actor* orgasmer, RE::Actor* target,
                     float amountML, RE::BSFixedString area, RE::BSFixedString sceneID) {
        auto* listener = OStimNet::OStimEventListener::GetInstance();
        if (!listener) return;
        RE::FormID orgasmerID = orgasmer ? orgasmer->GetFormID() : 0;
        RE::FormID targetID   = target   ? target->GetFormID()   : 0;
        listener->RegisterOcumApplied(threadID, orgasmerID, targetID, amountML, ToLower(area.c_str()), ToLower(sceneID.c_str()));
    }

    // Papyrus native: Function OcumSquirt(int threadID, Actor actor, string sceneID, string type) global
    // Called by the Papyrus listener for ocum_squirt.
    // Appends a squirt entry to the thread's pending climax batch and resets the debounce timer.
    void OcumSquirt(RE::StaticFunctionTag*, int32_t threadID,
                    RE::Actor* actor, RE::BSFixedString sceneID, RE::BSFixedString squirtType) {
        auto* listener = OStimNet::OStimEventListener::GetInstance();
        if (!listener) return;
        RE::FormID actorID = actor ? actor->GetFormID() : 0;
        listener->RegisterOcumSquirt(threadID, actorID, ToLower(sceneID.c_str()), ToLower(squirtType.c_str()));
    }

    // Papyrus native: int Function EvaluatePreStartSexualScene(Actor[] akParticipants, string asIntent, string evalId) global native
    // Sends the "ostimnet_evaluate_prestart_sexual" prompt to the LLM before an OStim scene is created.
    // Returns 1 if the task was successfully queued, 0 on failure.
    // asIntent: optional intent hint for the LLM (e.g. "romantic", "dom"). Pass "" for no hint.
    // On completion fires mod event "ostimnet_sexual_evaluation_finished":
    //   numArg = 1.0 success, 2.0 insufficient participants, 0.0 error.
    int32_t EvaluatePreStartSexualScene(RE::StaticFunctionTag*,
                                        std::vector<RE::Actor*> participants,
                                        RE::BSFixedString intent,
                                        RE::BSFixedString evalId) {
        std::vector<RE::FormID> formIDs;
        formIDs.reserve(participants.size());
        for (RE::Actor* actor : participants) {
            if (actor) formIDs.push_back(actor->GetFormID());
        }
        bool queued = OStimNet::SkyrimNetIntegration::EvaluatePreStartSexualScene(
            formIDs, ToLower(intent.c_str()), evalId.c_str());
        return queued ? 1 : 0;
    }

    // Papyrus native: int Function EvaluateNonSexualScene(Actor[] akParticipants, string asActivity, string asIntent, Actor akInitiator, string evalId) global native
    // Sends the "ostimnet_evaluate_nonsexual_scene" prompt to the LLM before a non-sexual scene is started.
    // Returns 1 if the task was successfully queued, 0 on failure.
    // asActivity:   optional activity hint for the LLM (e.g. "spar", "meditate"). Pass "" for no hint.
    // asIntent:     optional intent hint for the LLM (e.g. "friendly", "hostile"). Pass "" for no hint.
    // akInitiator:  optional actor who initiated the scene. Pass None for no initiator.
    // On completion fires mod event "ostimnet_nonsexual_evaluation_finished":
    //   numArg = 1.0 start=true (strArg = JSON { "start": true, "intent": "...", "activity": "...", "main": [...], "secondary": [...] })
    //   numArg = 2.0 start=false (LLM decided scene should not start)
    //   numArg = 0.0 error
    int32_t EvaluateNonSexualScene(RE::StaticFunctionTag*,
                                   std::vector<RE::Actor*> participants,
                                   RE::BSFixedString activity,
                                   RE::BSFixedString intent,
                                   RE::Actor* initiator,
                                   RE::BSFixedString evalId) {
        std::vector<RE::FormID> formIDs;
        formIDs.reserve(participants.size());
        for (RE::Actor* actor : participants) {
            if (actor) formIDs.push_back(actor->GetFormID());
        }
        RE::FormID initiatorFormID = initiator ? initiator->GetFormID() : 0;
        bool queued = OStimNet::SkyrimNetIntegration::EvaluateNonSexualScene(
            formIDs, ToLower(activity.c_str()), ToLower(intent.c_str()), initiatorFormID, evalId.c_str());
        return queued ? 1 : 0;
    }

    // Papyrus native: int Function EvaluateJoinOngoingSex(Actor akJoiner, int threadID, string evalId) global native
    int32_t EvaluateJoinOngoingSex(RE::StaticFunctionTag*,
                                   RE::Actor* joiner,
                                   int32_t threadID,
                                   RE::BSFixedString evalId) {
        if (!joiner) return 0;
        bool queued = OStimNet::SkyrimNetIntegration::EvaluateJoinOngoingSex(
            joiner->GetFormID(), threadID, evalId.c_str());
        return queued ? 1 : 0;
    }

    // Papyrus native: int Function EvaluateInviteToSex(Actor akInviter, Actor[] akInvitees, int threadID, string evalId) global native
    int32_t EvaluateInviteToSex(RE::StaticFunctionTag*,
                                RE::Actor* inviter,
                                std::vector<RE::Actor*> invitees,
                                int32_t threadID,
                                RE::BSFixedString evalId) {
        if (!inviter) return 0;
        std::vector<RE::FormID> inviteeFormIDs;
        inviteeFormIDs.reserve(invitees.size());
        for (RE::Actor* actor : invitees) {
            if (actor) inviteeFormIDs.push_back(actor->GetFormID());
        }
        bool queued = OStimNet::SkyrimNetIntegration::EvaluateInviteToSex(
            inviter->GetFormID(), inviteeFormIDs, threadID, evalId.c_str());
        return queued ? 1 : 0;
    }

    // -------------------------------------------------------------------------
    // JSON utility natives — dot-notated path access into any JSON string.
    // Path examples:
    //   "reasoning"       -> top-level string key
    //   "agressor.0"      -> first element of the "agressor" array
    //   "nested.key.sub"  -> nested object traversal
    // -------------------------------------------------------------------------

    // Papyrus native: string Function JsonGetString(string akJson, string akPath, string akDefault = "") global
    RE::BSFixedString JsonGetString(RE::StaticFunctionTag*,
                                   RE::BSFixedString json,
                                   RE::BSFixedString path,
                                   RE::BSFixedString defaultVal) {
        std::string result = OStimNet::JsonService::GetString(
            json.c_str(), path.c_str(), defaultVal.c_str());
        return RE::BSFixedString(result.c_str());
    }

    // Papyrus native: int Function JsonGetInt(string akJson, string akPath, int akDefault = 0) global
    int32_t JsonGetInt(RE::StaticFunctionTag*,
                       RE::BSFixedString json,
                       RE::BSFixedString path,
                       int32_t defaultVal) {
        return OStimNet::JsonService::GetInt(json.c_str(), path.c_str(), defaultVal);
    }

    // Papyrus native: float Function JsonGetFloat(string akJson, string akPath, float akDefault = 0.0) global
    float JsonGetFloat(RE::StaticFunctionTag*,
                       RE::BSFixedString json,
                       RE::BSFixedString path,
                       float defaultVal) {
        return OStimNet::JsonService::GetFloat(json.c_str(), path.c_str(), defaultVal);
    }

    // Papyrus native: bool Function JsonGetBool(string akJson, string akPath, bool akDefault = false) global
    bool JsonGetBool(RE::StaticFunctionTag*,
                     RE::BSFixedString json,
                     RE::BSFixedString path,
                     bool defaultVal) {
        return OStimNet::JsonService::GetBool(json.c_str(), path.c_str(), defaultVal);
    }

    // Papyrus native: int Function JsonArrayLength(string akJson, string akPath = "") global
    // Returns the length of the array at akPath, or -1 if not found / not an array.
    // Pass akPath = "" to query the root value itself.
    int32_t JsonArrayLength(RE::StaticFunctionTag*,
                            RE::BSFixedString json,
                            RE::BSFixedString path) {
        return OStimNet::JsonService::GetArrayLength(json.c_str(), path.c_str());
    }

    // Papyrus native: Actor Function JsonGetActor(string akJson, string akPath = "") global
    // Resolves a single uint32 FormID at akPath to a live Actor reference.
    // Returns None if the value is missing, zero, or doesn't resolve to an Actor.
    RE::Actor* JsonGetActor(RE::StaticFunctionTag*,
                            RE::BSFixedString json,
                            RE::BSFixedString path) {
        SKSE::log::info("PapyrusFunctions: JsonGetActor path='{}'", path.c_str());
        auto* result = OStimNet::JsonService::GetActor(json.c_str(), path.c_str());
        SKSE::log::info("PapyrusFunctions: JsonGetActor path='{}' -> {}", path.c_str(), result ? "found" : "None");
        return result;
    }

    // Papyrus native: Actor[] Function JsonGetActorArray(string akJson, string akPath = "") global
    // Resolves a JSON array of uint32 FormIDs at akPath to live Actor references.
    // Elements that are zero, not integers, or don't resolve to an Actor are skipped.
    // Pass akPath = "" to treat the root value itself as the array.
    std::vector<RE::Actor*> JsonGetActorArray(RE::StaticFunctionTag*,
                                             RE::BSFixedString json,
                                             RE::BSFixedString path) {
        SKSE::log::info("PapyrusFunctions: JsonGetActorArray path='{}'", path.c_str());
        auto result = OStimNet::JsonService::GetActorArray(json.c_str(), path.c_str());
        SKSE::log::info("PapyrusFunctions: JsonGetActorArray path='{}' -> {} actor(s)", path.c_str(), result.size());
        return result;
    }

    // Papyrus native: int Function ShowConfirmationModal(string actionType, Actor[] mainActors, Actor[] secondaryActors, string paramsJson) global
    // LATENT — Papyrus script suspends until the player responds; game thread is never blocked.
    // Returns: 0 = accepted, 1 = declined, 2 = declined+explain, 3 = cooldown blocked, 4 = player not in actors, 5 = intent is aggressive/dom.
    // paramsJson fields vary by actionType. For StartNewSex / ChangeSexScenePosition:
    //   { "intent": "neutral", "isSexual": true, "activity": "...", "positions": "..." }
    RE::BSScript::LatentStatus ShowConfirmationModal(RE::BSScript::Internal::VirtualMachine* vm,
                                                     RE::VMStackID stackID,
                                                     RE::StaticFunctionTag*,
                                                     RE::BSFixedString actionType,
                                                     std::vector<RE::Actor*> mainActors,
                                                     std::vector<RE::Actor*> secondaryActors,
                                                     RE::BSFixedString paramsJson) {
        SKSE::log::info("PapyrusFunctions: ShowConfirmationModal called actionType='{}' mainActors={} paramsJson='{}'",
            actionType.c_str(), mainActors.size(), paramsJson.c_str());
        return OStimNet::ConfirmationModal::GetSingleton().Show(
            vm, stackID,
            actionType.c_str(), mainActors, secondaryActors,
            paramsJson.c_str());
    }

    // Papyrus native: int Function CheckAndSetActionCooldown(string actionType, Actor[] actors) global
    // Synchronous. Checks if the action is on cooldown for any actor; if not, sets the normal cooldown for all actors.
    // Returns: 0 = ok (cooldown set, action may proceed), 1 = blocked (cooldown still active).
    int32_t CheckAndSetActionCooldown(RE::StaticFunctionTag*,
                                      RE::BSFixedString actionType,
                                      std::vector<RE::Actor*> actors) {
        SKSE::log::info("PapyrusFunctions: CheckAndSetActionCooldown actionType='{}' actors={}",
            actionType.c_str(), actors.size());
        return OStimNet::ConfirmationModal::GetSingleton().CheckAndSet(actionType.c_str(), actors);
    }

    // Papyrus native: string Function GetThreadIntent(int threadID) global
    // Returns the intent string stored for the thread (e.g. "romantic", "dom").
    // Returns "" if no intent has been set for that thread.
    RE::BSFixedString GetThreadIntent(RE::StaticFunctionTag*, int32_t threadID) {
        OStimNet::Intent intent = OStimNet::ThreadDataStore::GetSingleton().GetIntent(threadID);
        return RE::BSFixedString(OStimNet::IntentToString(intent));
    }

    // Papyrus native: string Function GetThreadPhase(int threadID) global
    // Returns the current sexual phase name for a thread ("undressing", "foreplay", "oral", "sex").
    // Returns "" if phase tracking is disabled for this thread (e.g. dom/aggressive intent,
    // or EnableThreadPhases is off).
    RE::BSFixedString GetThreadPhase(RE::StaticFunctionTag*, int32_t threadID) {
        auto phase = OStimNet::ThreadDataStore::GetSingleton().GetSexualPhase(threadID);
        if (!phase.has_value()) return RE::BSFixedString("");
        return RE::BSFixedString(OStimNet::SexualPhaseToName(*phase));
    }

    // Papyrus native: string Function GetThreadPhaseActivityKeywords(int threadID) global
    // Returns a comma-separated list of OStim activity keywords for the thread's current phase,
    // suitable for passing as the activity filter in SceneSexSearch.
    // Returns "" if phase tracking is disabled for this thread.
    RE::BSFixedString GetThreadPhaseActivityKeywords(RE::StaticFunctionTag*, int32_t threadID) {
        std::string kw = OStimNet::ThreadDataStore::GetSingleton().GetSexualPhaseActivityKeywords(threadID);
        return RE::BSFixedString(kw.c_str());
    }

    // Papyrus native: string Function GetInitialPhaseByIntent(string intent, bool isPlayerThread) global
    // Returns the first phase name for the given intent ("undressing", "foreplay", "oral", or "sex"),
    // applying the EnableUndressingPlayer / EnableUndressingNpc config so Undressing is skipped
    // when it is disabled for this thread type.
    // Returns "" if the intent has no phase list (e.g. "platonic", or phases disabled for that intent).
    RE::BSFixedString GetInitialPhaseByIntent(RE::StaticFunctionTag*, RE::BSFixedString intent, bool isPlayerThread) {
        OStimNet::Intent intentEnum = OStimNet::IntentFromString(ToLower(intent.c_str()));
        const auto& list = OStimNet::IntentToPhaseList(intentEnum);
        if (list.empty()) return RE::BSFixedString("");

        // Respect the undressing config: if the first phase is Undressing but undressing
        // is disabled for this thread type, advance to the next phase.
        size_t startIdx = 0;
        if (list[0] == OStimNet::SexualPhase::Undressing) {
            bool undressingEnabled = isPlayerThread
                ? OStimNet::Config::GetSingleton().EnableUndressingPlayer()
                : OStimNet::Config::GetSingleton().EnableUndressingNpc();
            if (!undressingEnabled && list.size() > 1)
                startIdx = 1;
        }

        return RE::BSFixedString(OStimNet::SexualPhaseToName(list[startIdx]));
    }

    // Papyrus native: Function SetThreadIntent(int threadID, string intent, Actor[] mainActors) global
    // Updates the stored intent and reshuffles main/secondary actors for an active thread mid-scene.
    // mainActors:      actors to promote to the primary role for the new intent.
    //                  Pass an empty array to keep the existing actor roles unchanged.
    // secondaryActors: derived automatically as all thread participants NOT in mainActors.
    // Fires ostimnet_intent_changed with oldIntent, newIntent, mainActors, secondaryActors.
    void SetThreadIntent(RE::StaticFunctionTag*,
                         int32_t threadID,
                         RE::BSFixedString intent,
                         std::vector<RE::Actor*> mainActors) {
        auto& registry = OStimNet::ThreadRegistry::GetSingleton();
        std::string intentLower = ToLower(intent.c_str());
        OStimNet::Intent intentEnum = OStimNet::IntentFromString(intentLower);

        // Capture old state before overwriting
        const std::string oldIntentStr = OStimNet::IntentToString(
            OStimNet::ThreadDataStore::GetSingleton().GetIntent(threadID));
        const std::vector<RE::Actor*> oldMainActors = registry.GetMainActors(threadID);
        registry.SetIntent(threadID, intentEnum);

        bool mainActorsSame;
        if (mainActors.empty()) {
            SKSE::log::info("PapyrusFunctions: SetThreadIntent threadID={} intent={} — mainActors empty, keeping existing actor roles", threadID, intentLower);
            mainActorsSame = true;
        } else {
            // Derive secondary actors: all thread participants not listed in mainActors.
            const auto& allActors = registry.GetActorPtrs(threadID);
            std::unordered_set<RE::FormID> mainIDs;
            for (RE::Actor* a : mainActors) {
                if (a) mainIDs.insert(a->GetFormID());
            }
            std::vector<RE::Actor*> secondaryActors;
            for (RE::Actor* a : allActors) {
                if (a && !mainIDs.count(a->GetFormID()))
                    secondaryActors.push_back(a);
            }
            std::unordered_set<RE::FormID> oldMainIDs;
            for (auto* a : oldMainActors) if (a) oldMainIDs.insert(a->GetFormID());
            mainActorsSame = (mainIDs == oldMainIDs);

            registry.SetMainActors(threadID, std::move(mainActors));
            registry.SetSecondaryActors(threadID, std::move(secondaryActors));
        }

        // Build and fire ostimnet_intent_changed mod event
        const std::string json = OStimNet::EventPayloadBuilder::BuildIntentChanged(
            threadID, oldIntentStr, intentLower, mainActorsSame);
        if (auto* source = SKSE::GetModCallbackEventSource()) {
            SKSE::ModCallbackEvent e{"ostimnet_intent_changed", json.c_str(), static_cast<float>(threadID), nullptr};
            source->SendEvent(&e);
        }
        SKSE::log::info("PapyrusFunctions: SetThreadIntent fired ostimnet_intent_changed: {}", json);
    }

    // Papyrus native: bool Function IsValidPosition(string asPosition) global
    // Returns true if asPosition is a canonical position name or a known alias.
    // Case-insensitive. Use this to validate user/LLM-supplied position strings
    // before passing them to SceneSexSearch or OStim APIs.
    bool IsValidPosition(RE::StaticFunctionTag*, RE::BSFixedString position) {
        std::string lower = ToLower(position.c_str());
        std::string resolved;
        bool valid = OStimNavigatorAPI::ResolvePosition(lower, resolved);
        SKSE::log::trace("PapyrusFunctions: IsValidPosition '{}' -> {} (resolved='{}')", lower, valid, resolved);
        return valid;
    }

    // Papyrus native: string Function GetSexualPositionFromTags(string[] tags) global
    // Scans the provided scene-tag array and returns the first canonical sexual-position
    // string found (e.g. "missionary", "doggystyle"). Returns "" if no position tag is present.
    RE::BSFixedString GetSexualPositionFromTags(RE::StaticFunctionTag*, std::vector<std::string> tags) {
        for (const std::string& raw : tags) {
            std::string tag(raw);
            std::transform(tag.begin(), tag.end(), tag.begin(), [](unsigned char c) { return std::tolower(c); });
            std::string resolved;
            if (OStimNavigatorAPI::ResolvePosition(tag, resolved))
                return RE::BSFixedString(resolved.c_str());
        }
        return RE::BSFixedString("");
    }

    // Papyrus native: string Function BuildSpectatorAddedEventJson(Actor spectator) global native
    // Resolves threadID and target via ThreadRegistry, then delegates to EventPayloadBuilder::BuildSpectatorAdded.
    // Returns "" if the spectator is not currently tracked.
    RE::BSFixedString BuildSpectatorAddedEventJson(RE::StaticFunctionTag*, RE::Actor* spectator) {
        if (!spectator) return RE::BSFixedString("");
        const int threadID = OStimNet::ThreadRegistry::GetSingleton().GetSpectatorThreadID(spectator->GetFormID());
        if (threadID == -1) return RE::BSFixedString("");
        const RE::FormID targetID = OStimNet::ThreadRegistry::GetSingleton().GetSpectatorTargetFormID(spectator->GetFormID());
        RE::Actor* target = targetID ? RE::TESForm::LookupByID<RE::Actor>(targetID) : nullptr;
        const std::string s = OStimNet::EventPayloadBuilder::BuildSpectatorAdded(threadID, spectator, target);
        SKSE::log::info("BuildSpectatorAddedEventJson:event: {}", s);
        return RE::BSFixedString(s.c_str());
    }

    // Papyrus native: Actor[] Function GetMainActors(int threadID) global
    // Returns the stored main-actor array for the given thread.
    // Returns an empty array if the thread is not found or has no main actors set.
    std::vector<RE::Actor*> GetMainActors(RE::StaticFunctionTag*, int32_t threadID) {
        return OStimNet::ThreadRegistry::GetSingleton().GetMainActors(threadID);
    }

    // Papyrus native: Actor[] Function GetSecondaryActors(int threadID) global
    // Returns the stored secondary-actor array for the given thread.
    // Returns an empty array if the thread is not found or has no secondary actors set.
    std::vector<RE::Actor*> GetSecondaryActors(RE::StaticFunctionTag*, int32_t threadID) {
        return OStimNet::ThreadRegistry::GetSingleton().GetSecondaryActors(threadID);
    }

    // Papyrus native: string Function BuildSpectatorFledEventJson(Actor spectator) global native
    // Looks up the spectator's threadID from ThreadRegistry (-1 if no longer tracked).
    RE::BSFixedString BuildSpectatorFledEventJson(RE::StaticFunctionTag*, RE::Actor* spectator) {
        if (!spectator) return RE::BSFixedString("");
        const int threadID = OStimNet::ThreadRegistry::GetSingleton().GetSpectatorThreadID(spectator->GetFormID());
        const std::string s = OStimNet::EventPayloadBuilder::BuildSpectatorFled(threadID, spectator);
        return RE::BSFixedString(s.c_str());
    }

    bool Register(RE::BSScript::IVirtualMachine* vm) {
        vm->RegisterFunction("GetLocationGeneration", "OStimNet", GetLocationGeneration);
        vm->RegisterFunction("GetSceneDescription", "OStimNet", GetSceneDescription);
        vm->RegisterFunction("GetActorListString", "OStimNet", GetActorListString);
        vm->RegisterFunction("Log", "OStimNet", Log);
        vm->RegisterFunction("SetThreadContinuation", "OStimNet", SetThreadContinuation);
        vm->RegisterFunction("SetContinuationOverride", "OStimNet", SetContinuationOverride);
        vm->RegisterFunction("OcumApplied", "OStimNet", OcumApplied);
        vm->RegisterFunction("OcumSquirt", "OStimNet", OcumSquirt);
        vm->RegisterFunction("GetSpectatorTarget", "OStimNet", GetSpectatorTarget);
        vm->RegisterFunction("AddSpectator", "OStimNet", AddSpectator);
        vm->RegisterFunction("RemoveSpectator", "OStimNet", RemoveSpectator);
        vm->RegisterFunction("EvaluatePreStartSexualScene", "OStimNet", EvaluatePreStartSexualScene);
        vm->RegisterFunction("EvaluateNonSexualScene", "OStimNet", EvaluateNonSexualScene);
        vm->RegisterFunction("EvaluateJoinOngoingSex", "OStimNet", EvaluateJoinOngoingSex);
        vm->RegisterFunction("EvaluateInviteToSex", "OStimNet", EvaluateInviteToSex);
        vm->RegisterFunction("JsonGetString", "OStimNet", JsonGetString);
        vm->RegisterFunction("JsonGetInt", "OStimNet", JsonGetInt);
        vm->RegisterFunction("JsonGetFloat", "OStimNet", JsonGetFloat);
        vm->RegisterFunction("JsonGetBool", "OStimNet", JsonGetBool);
        vm->RegisterFunction("JsonArrayLength", "OStimNet", JsonArrayLength);
        vm->RegisterFunction("JsonGetActorArray", "OStimNet", JsonGetActorArray);
        vm->RegisterFunction("JsonGetActor", "OStimNet", JsonGetActor);
        RegisterLatentFixed<int32_t>(vm, "ShowConfirmationModal", "OStimNet", ShowConfirmationModal);
        vm->RegisterFunction("CheckAndSetActionCooldown", "OStimNet", CheckAndSetActionCooldown);
        vm->RegisterFunction("IsValidPosition", "OStimNet", IsValidPosition);
        vm->RegisterFunction("GetSexualPositionFromTags", "OStimNet", GetSexualPositionFromTags);
        vm->RegisterFunction("GetThreadIntent", "OStimNet", GetThreadIntent);
        vm->RegisterFunction("GetThreadPhase", "OStimNet", GetThreadPhase);
        vm->RegisterFunction("GetThreadPhaseActivityKeywords", "OStimNet", GetThreadPhaseActivityKeywords);
        vm->RegisterFunction("GetInitialPhaseByIntent", "OStimNet", GetInitialPhaseByIntent);
        vm->RegisterFunction("SetThreadIntent", "OStimNet", SetThreadIntent);
        vm->RegisterFunction("ClaimThread", "OStimNet", ClaimThread);
        vm->RegisterFunction("ConfirmThread", "OStimNet", ConfirmThread);
        vm->RegisterFunction("BuildSpectatorAddedEventJson", "OStimNet", BuildSpectatorAddedEventJson);
        vm->RegisterFunction("GetMainActors", "OStimNet", GetMainActors);
        vm->RegisterFunction("GetSecondaryActors", "OStimNet", GetSecondaryActors);
        vm->RegisterFunction("BuildSpectatorFledEventJson", "OStimNet", BuildSpectatorFledEventJson);
        SKSE::log::info("PapyrusDecorators: registered GetSceneDescription, GetActorListString, Log, SetThreadContinuation, OcumApplied, OcumSquirt, GetSpectatorTarget, EvaluatePreStartSexualScene, EvaluateNonSexualScene, JsonGetString, JsonGetInt, JsonGetFloat, JsonGetBool, JsonArrayLength, ShowConfirmationModal, CheckAndSetActionCooldown, GetSexualPositionFromTags, GetThreadIntent, SetThreadIntent, ClaimThread, ConfirmThread on OStimNet.");
        return true;
    }

}  // namespace OStimNet::Papyrus::PapyrusFunctions
