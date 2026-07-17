#pragma once
#include <optional>
#include <unordered_set>
#include "api/OstimNG-API-Thread.h"
#include "api/OStimNavigator_PublicAPI.h"
#include "DebounceQueue.h"
#include "EventPayloadBuilder.h"
#include "OStimEventHelpers.h"
#include "SceneDescriptionUtils.h"
#include "SkyrimNetIntegration.h"
#include "SpeakerSelector.h"
#include "ThreadRegistry.h"
#include "OCumOverlayReader.h"
#include "ScheduledEvalService.h"
namespace OStimNet {

class OStimEventListener : public RE::BSTEventSink<SKSE::ModCallbackEvent> {
public:
    static void Register() {
        static OStimEventListener instance;
        s_instance = &instance;
        if (auto* source = SKSE::GetModCallbackEventSource()) {
            source->AddEventSink(&instance);
            SKSE::log::info("OStimEventListener: registered for OStim mod callback events.");
        } else {
            SKSE::log::error("OStimEventListener: ModCallbackEventSource unavailable.");
        }
    }

    static OStimEventListener* GetInstance() { return s_instance; }

    // Called when the LLM callback determines intent+roles for a non-OStimNet thread.
    // Transitions the thread to Active and fires ostimnet_start.
    void ClaimPendingNonOStimNetThread(int threadID) {
        ThreadRegistry::GetSingleton().SetPhase(threadID, ThreadPhase::Active);
        SKSE::log::info("OStimEventListener: thread {} claimed as OStimNet, firing ostimnet_start", threadID);
        OnNewThreadStart(threadID);
    }

    // Called from ConfirmThread native after ConfirmClaim() succeeds.
    // If HandleStart already ran and left the thread Pending (waiting for claim binding),
    // this applies the claim data to the thread and fires ostimnet_start immediately.
    // If HandleStart hasn't run yet, the claim will be consumed there instead.
    void TryActivateClaimedThread(int threadID) {
        // If HandleStart hasn't fired yet, the thread won't have actor data in the registry.
        // Do nothing — ConsumeActiveClaim will be called from HandleStart.
        if (!ThreadRegistry::GetSingleton().HasStarted(threadID)) {
            SKSE::log::info("OStimEventListener: TryActivateClaimedThread thread {} HandleStart hasn't fired yet, claim will be consumed there", threadID);
            return;
        }
        auto& registry = OStimNet::ThreadRegistry::GetSingleton();
        if (!registry.ConsumeActiveClaim(threadID)) {
            SKSE::log::warn("OStimEventListener: TryActivateClaimedThread thread {} no Claiming entry to consume", threadID);
            return;
        }
        SKSE::log::info("OStimEventListener: TryActivateClaimedThread thread {} claim consumed, firing ostimnet_start", threadID);
        OnNewThreadStart(threadID);
    }

    // Called from Papyrus OcumApplied native. Appends cum-applied data to the thread's
    // pending climax batch and resets the debounce timer.
    void RegisterOcumApplied(int threadID, RE::FormID orgasmerID, RE::FormID targetID,
                             float amountML, const std::string& area, const std::string& sceneID) {
        _dq.PostClimaxOcumApplied(threadID, orgasmerID, targetID, amountML, area, sceneID);
    }

    // Called from Papyrus OcumSquirt native. Appends a squirt entry to the thread's
    // pending climax batch and resets the debounce timer.
    void RegisterOcumSquirt(int threadID, RE::FormID actorID, const std::string& sceneID, const std::string& squirtType) {
        _dq.PostClimaxSquirt(threadID, actorID, sceneID, squirtType);
    }

    // Called on kPreLoadGame to discard all in-flight state. OStim threads are
    // dead after a game load, so we must not carry over actors, pending sets, or
    // continuation data from the previous session.
    void Reset() {
        _dq.Stop();
        _pendingContinuation.reset();
        _suppressNextSceneChange.clear();
        _suppressNextSpeedChange.clear();
        _skipNextSceneTrigger.clear();
        ThreadDataStore::GetSingleton().ClearAll();
        SKSE::log::info("OStimEventListener: reset on game load.");
    }

protected:
    // ProcessEvent must return as fast as possible — OStim's BSTEventSource holds
    // its BSSpinLock for the entire duration of the notification, and any re-entry
    // into OStim (GetActors, GetCurrentSceneID, etc.) or back into SendEvent will
    // deadlock. Copy all event data here and dispatch everything via AddTask so
    // our handlers run on the next game frame, outside the lock.
    RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* event,
                                          RE::BSTEventSource<SKSE::ModCallbackEvent>*) override {
        if (!event) return RE::BSEventNotifyControl::kContinue;

        const std::string name(event->eventName);
        if (name != "ostim_thread_start" &&
            name != "ostim_thread_scenechanged" &&
            name != "ostim_thread_speedchanged" &&
            name != "ostim_actor_orgasm" &&
            name != "ostim_thread_end") {
            return RE::BSEventNotifyControl::kContinue;
        }

        const int      numArg   = static_cast<int>(event->numArg);
        const std::string strArg(event->strArg.c_str());
        const RE::FormID senderID = event->sender ? event->sender->GetFormID() : 0;

        if (auto* taskIF = SKSE::GetTaskInterface()) {
            taskIF->AddTask([this, name, numArg, strArg, senderID]() {
                if (name == "ostim_thread_start") {
                    HandleStart(numArg);
                } else if (name == "ostim_thread_scenechanged") {
                    HandleSceneChange(numArg, strArg.c_str());
                } else if (name == "ostim_thread_speedchanged") {
                    int speed = strArg.empty() ? 0 : std::stoi(strArg);
                    OnSpeedChanged(numArg, speed);
                } else if (name == "ostim_actor_orgasm") {
                    auto* actor = senderID ? RE::TESForm::LookupByID<RE::Actor>(senderID) : nullptr;
                    OnClimax(numArg, strArg.c_str(), actor);
                } else if (name == "ostim_thread_end") {
                    HandleEnd(numArg);
                }
            });
        }

        return RE::BSEventNotifyControl::kContinue;
    }

private:
    // Tracks the most recently ended thread so HandleStart can detect continuations
    // (actor swap/add) without a separate class. Only threads with \u22652 actors are recorded.
    struct ContinuationPending {
        int                                       oldThreadID;
        std::vector<uint32_t>                     oldFormIDs;
        std::chrono::steady_clock::time_point     recordedAt;
    };
    std::optional<ContinuationPending> _pendingContinuation;

    // Threads whose first ostim_thread_scenechanged should be suppressed
    // (fired automatically by OStim right after thread start).
    std::unordered_set<int> _suppressNextSceneChange;

    // Threads whose first ostim_thread_speedchanged should be suppressed
    // (fired automatically by OStim right after thread start).
    std::unordered_set<int> _suppressNextSpeedChange;

    // Threads whose next scene change should have skipTrigger forced to true.
    // Set when a thread transitions through a scene where at least one actor
    // has the 'climaxing' tag (i.e. the post-climax transition scene).
    // Consumed (and cleared) the next time OnSceneChanged fires for that thread.
    std::unordered_set<int> _skipNextSceneTrigger;

    // Unified background service: debounces scene changes and climax batches, and
    // runs the periodic spectator scan. Replaces SceneChangeDebouncer, ClimaxDebouncer,
    // and SpectatorScanner.
    DebounceQueue _dq{
        [this](int tid, const std::string& sid) { OnSceneChanged(tid, sid); },
        [this](int tid, const DebounceQueue::ClimaxBatchData& data) { OnClimaxBatch(tid, data); },
        [this](int tid, int32_t speed) { OnSpeedChangeFired(tid, speed); },
        []() {
            auto activeActors = ThreadRegistry::GetSingleton().GetActiveThreadActors();
            DebounceQueue::ActiveThreadList result;
            result.reserve(activeActors.size());
            for (const auto& [tid, formIDs] : activeActors)
                result.emplace_back(tid, formIDs);
            return result;
        }
    };

    inline static OStimEventListener* s_instance = nullptr;

    // Called when ostim_thread_start fires. Resolves genuine start vs continuation.
    void HandleStart(int threadID) {
        bool wasEmpty = !ThreadRegistry::GetSingleton().HasActiveThreads();
        // Snapshot old FormIDs BEFORE FetchAndStoreActors overwrites the store.
        // This is critical for same-ID continuations: the old actor list must be
        // preserved so CopyStateForContinuation can correctly identify joiners.
        std::vector<uint32_t> preUpdateFormIDs = ThreadRegistry::GetSingleton().GetActorFormIDs(threadID);

        auto newActors = ThreadDataStore::GetSingleton().FetchAndStoreActors(threadID);
        if (wasEmpty) {
            _dq.Start();
        }

        // Continuation detection: check whether this start is a continuation of the
        // most recently ended thread (actor swap/add within the 5s window).
        int sourceID = -1;
        if (_pendingContinuation.has_value()) {
            auto& pending = *_pendingContinuation;
            auto age = std::chrono::duration<float>(
                std::chrono::steady_clock::now() - pending.recordedAt).count();
            if (age > 5.0f) {
                _pendingContinuation.reset();  // expired
            } else {
                const auto& old = pending.oldFormIDs;
                uint32_t required = static_cast<uint32_t>(old.size()) - 1;
                uint32_t matches  = 0;
                for (uint32_t fid : old)
                    if (std::find(newActors.begin(), newActors.end(), fid) != newActors.end())
                        ++matches;
                if (matches >= required) {
                    sourceID = pending.oldThreadID;
                    _pendingContinuation.reset();  // consumed
                }
            }
        }
        if (sourceID != -1) {
            SKSE::log::info("OStimEventListener: thread {} is continuation of thread {}", threadID, sourceID);
            ThreadDataStore::GetSingleton().CopyStateForContinuation(sourceID, threadID, preUpdateFormIDs);
            OnThreadContinued(sourceID, threadID);
            if (sourceID != threadID) {
                for (auto [specID, targetID] : ThreadDataStore::GetSingleton().TakeSpectators(sourceID))
                    DebounceQueue::FireRemoveSpectatorEvent(specID, targetID, sourceID);
                ThreadDataStore::GetSingleton().ClearThread(sourceID);
            }
            return;
        }

        SKSE::log::info("OStimEventListener: thread {} is a new start", threadID);

        // Suppress the automatic post-start scene change and speed change now, regardless of
        // deferral, so the events don't slip through while we wait for Papyrus metadata.
        _suppressNextSceneChange.insert(threadID);
        _suppressNextSpeedChange.insert(threadID);

        // --- Fast path: ClaimThread/ConfirmThread already arrived (Phase-2 protocol) ---
        // ConfirmClaim binds a pre-allocated claim token to this threadID.
        // If that already happened before HandleStart fired, ConsumeActiveClaim applies
        // the pre-set intent/sexual/actors and transitions the thread to Active immediately.
        if (ThreadRegistry::GetSingleton().ConsumeActiveClaim(threadID)) {
            SKSE::log::info("OStimEventListener: thread {} claim consumed in HandleStart, firing ostimnet_start immediately", threadID);
            OnNewThreadStart(threadID);
            return;
        }

        // Non-OStimNet path: FetchAndStoreActors already set the phase to Pending.
        // Queue LLM evaluation to determine intent + actor roles.
        // ostimnet_start fires when ClaimPendingNonOStimNetThread() is called by the callback.
        std::vector<RE::FormID> actorFormIDs;
        for (uint32_t fid : newActors) actorFormIDs.push_back(static_cast<RE::FormID>(fid));

        if (ThreadRegistry::GetSingleton().HasPendingClaimForActors(newActors)) {
            // The incoming actors match an unconfirmed claim token — this is an OStimNet-owned
            // thread in the race window between OThreadBuilder.Start() firing ostim_thread_start
            // and Papyrus calling ConfirmThread.  Do not queue a non-OStimNet LLM evaluation;
            // ConfirmThread -> TryActivateClaimedThread will consume the claim and fire
            // ostimnet_start when it arrives.
            SKSE::log::info("OStimEventListener: thread {} waiting for ConfirmThread — actors match pending claim, skipping non-OStimNet LLM eval", threadID);
            return;
        }

        SKSE::log::info("OStimEventListener: thread {} is non-OStimNet, queuing LLM evaluation", threadID);
        SkyrimNetIntegration::EvaluateExternalSexualThread(actorFormIDs, threadID);
    }

    // Called when ostim_thread_end fires.
    void HandleEnd(int threadID) {
        // Capture formIDs before marking the thread ended.
        // Record as pending continuation if the thread had \u22652 actors.
        // Covers both explicit OStimNet continuations and the fallback
        // FormID-overlap detection for non-OStimNet continuations.
        auto formIDs = ThreadRegistry::GetSingleton().GetActorFormIDs(threadID);
        if (formIDs.size() >= 2) {
            _pendingContinuation = ContinuationPending{
                threadID, std::move(formIDs), std::chrono::steady_clock::now()};
        }
        ThreadRegistry::GetSingleton().SetPhase(threadID, ThreadPhase::Ended);
        if (!ThreadRegistry::GetSingleton().HasActiveThreads()) {
            _dq.Stop();
        }

        // Always stop the scheduled eval timer when a thread physically ends —
        // the timer is independent of continuation data and must not keep firing
        // for a dead OStim thread.
        ScheduledEvalService::GetSingleton().OnThreadEnd(threadID);

        if (ThreadDataStore::GetSingleton().IsContinuation(threadID)) {
            SKSE::log::info("OStimEventListener: thread {} end suppressed (continuation)", threadID);
            // Do not clear — data is preserved for CopyStateForContinuation in HandleStart.
            for (auto [specID, targetID] : ThreadRegistry::GetSingleton().TakeSpectators(threadID))
                DebounceQueue::FireRemoveSpectatorEvent(specID, targetID, threadID);
        } else {
            SKSE::log::info("OStimEventListener: thread {} ended", threadID);
            OnThreadEnd(threadID);
            ThreadDataStore::GetSingleton().ClearThread(threadID);
        }
    }

    // Fired when the scene (node) of a thread changes.
    // Fires papyrus mod event: ostimnet_scene_change
    //   numArg = threadID  |  strArg = sceneID  |  sender = None
    // Suppressed once per thread immediately after ostimnet_start.
    // Debounced: registers a pending entry; the poll thread calls OnSceneChanged after 2s of silence.
    void HandleSceneChange(int threadID, const char* sceneID) {
        if (_suppressNextSceneChange.erase(threadID) > 0) {
            SKSE::log::info("OStimEventListener: suppressing initial scene change for thread {}", threadID);
            return;
        }
        // Skip idle/intro/transition scenes — they are not meaningful scene changes.
        // However, if this transition has at least one actor with the 'climaxing' tag,
        // mark the thread so the *next* real scene change will have skipTrigger=true.
        if (ONavIsIdle && ONavIsIntro && ONavIsTransit && sceneID) {
            bool isIdleOrTransit = ONavIsIdle(sceneID) || ONavIsTransit(sceneID);
            SKSE::log::info("OStimEventListener: thread {} isIdleOrTransit({}) = {}", threadID, sceneID, isIdleOrTransit);
            if (isIdleOrTransit) {
                _dq.CancelSceneChange(threadID);
                if (ONavSceneHasActorWithTag && ONavSceneHasActorWithTag(sceneID, "climaxing")) {
                    _skipNextSceneTrigger.insert(threadID);
                    SKSE::log::info("OStimEventListener: thread {} transition scene has 'climaxing' actor, marking skipTrigger for next scene change", threadID);
                }
                return;
            }
        } else {
            SKSE::log::info("OStimEventListener: thread {} ONavIsIdle/ONavIsIntro/ONavIsTransit not resolved or sceneID null, skipping filter", threadID);
        }
        _dq.PostSceneChange(threadID, sceneID ? sceneID : "");
        SKSE::log::info("OStimEventListener: thread {} scene change pending ({})", threadID, sceneID ? sceneID : "");
    }

    // --- event handlers -------------------------------------------------------

    // Fires ostimnet_start for a new (non-continuation) thread.
    // Called from HandleStart when the claim is consumed (OStimNet path), or from
    // ClaimPendingNonOStimNetThread when the LLM callback resolves (non-OStimNet path).
    void OnNewThreadStart(int threadID) {
        SKSE::log::info("OStimEventListener: firing ostimnet_start for thread {}", threadID);
        ScheduledEvalService::GetSingleton().OnThreadStart(threadID);

        auto startingPhase = ThreadRegistry::GetSingleton().ResolveStartingPhase(threadID);
        if (startingPhase.has_value()) {
            OStimNet::ThreadDataStore::GetSingleton().SetSexualPhase(threadID, *startingPhase);
            SKSE::log::info("OStimEventListener: thread {} sexualPhase initialised -> {}",
                threadID, OStimNet::SexualPhaseToName(*startingPhase));
        }
        bool startsWithUndressing = startingPhase == OStimNet::SexualPhase::Undressing;


        // Build start event — use approach-specific description when opening with undressing.
        std::string startJson;
        if (startsWithUndressing) {
            const auto& actorPtrs = ThreadRegistry::GetSingleton().GetActorPtrs(threadID);
            std::string name0 = actorPtrs.size() > 0
                ? ThreadRegistry::GetActorDisplayName(actorPtrs[0], "Actor 1") : "Actor 1";
            std::string name1 = actorPtrs.size() > 1
                ? ThreadRegistry::GetActorDisplayName(actorPtrs[1], "Actor 2") : "Actor 2";
            bool isOStimApproach = (ThreadRegistry::GetSingleton().IsPlayerThread(threadID)
                ? OStimNet::Config::GetSingleton().UndressingApproachPlayer()
                : OStimNet::Config::GetSingleton().UndressingApproachNpc()) == "OStim";
            std::string msg;
            if (isOStimApproach)
                msg = name1 + " undresses themselves. " + name1 + " undresses " + name0 + ". Both actors are standing.";
            else
                msg = name0 + " undresses " + name1 + " and " + name1 + " undresses " + name0 + ". Both actors are standing.";
            SKSE::log::info("OStimEventListener: thread {} undressing start msg (approach={}): {}",
                threadID, isOStimApproach ? "OStim" : "non-OStim", msg);
            startJson = EventPayloadBuilder::BuildStart(threadID, msg);
        } else {
            startJson = EventPayloadBuilder::BuildStart(threadID);
        }

        RE::Actor* speaker = SelectSpeakerActor(threadID, Config::GetSingleton().CommentGenderPriority());
        FireModEvent("ostimnet_start", startJson.c_str(), static_cast<float>(threadID), speaker);
    }

    // Fired when a thread starts that continues an older thread (actor was added/removed).
    // oldThreadID: the thread that just ended
    // newThreadID: the new thread that replaced it
    // Main actors = participants of the old thread; secondary actors = newly joined actors.
    void OnThreadContinued(int oldThreadID, int newThreadID) {
        SKSE::log::info("OStimEventListener: thread {} continued as {}, firing ostimnet_continue_thread", oldThreadID, newThreadID);
        RE::Actor* speaker = SelectSpeakerActor(newThreadID, Config::GetSingleton().CommentGenderPriority());
        std::string json = EventPayloadBuilder::BuildContinueThread(oldThreadID, newThreadID);
        FireModEvent("ostimnet_continue_thread", json.c_str(), static_cast<float>(newThreadID), speaker);
    }

    // Called by the poll thread (via AddTask) and by OnThreadEnd flush when debounce resolves.
    // Applies thread priority and speaker selection filters, then fires ostimnet_scene_change.
    void OnSceneChanged(int threadID, const std::string& sceneID) {
        SKSE::log::info("OStimEventListener: thread {} scene change fired ({})", threadID, sceneID);

        // Extract position from scene tags and persist to ThreadRegistry so
        // EventPayloadBuilder can include it in the payload without a re-call.
        std::string position = ExtractPositionFromSceneTags(sceneID.c_str());
        ThreadDataStore::GetSingleton().SetCurrentPosition(threadID, position);

        // Detect sexual phase from new scene's action types and advance forward if higher.
        {
            auto currentPhase = OStimNet::ThreadDataStore::GetSingleton().GetSexualPhase(threadID);
            if (currentPhase.has_value()) {
                auto detectedPhase = OStimNet::DetectPhaseFromSceneID(sceneID.c_str());
                if (detectedPhase.has_value()) {
                    Intent intent    = OStimNet::ThreadDataStore::GetSingleton().GetIntent(threadID);
                    int currentIdx   = OStimNet::GetPhaseIndex(intent, *currentPhase);
                    int detectedIdx  = OStimNet::GetPhaseIndex(intent, *detectedPhase);
                    if (detectedIdx > currentIdx && detectedIdx >= 0) {
                        OStimNet::ThreadDataStore::GetSingleton().SetSexualPhase(threadID, *detectedPhase);
                        SKSE::log::info("OStimEventListener: thread {} phase advanced {} -> {}",
                            threadID,
                            OStimNet::SexualPhaseToName(*currentPhase),
                            OStimNet::SexualPhaseToName(*detectedPhase));
                    }
                }
            }
        }

        ScheduledEvalService::GetSingleton().OnSceneChanged(threadID);

        if (!position.empty())
            SKSE::log::info("OStimEventListener: thread {} position = '{}'", threadID, position);

        // Thread priority filter: set skipTrigger=true for non-priority threads.
        bool skipTrigger = Config::GetSingleton().IsMuted();
        if (!skipTrigger && Config::GetSingleton().CommentsPrioritizeNearestThread()) {
            int priorityThread = SelectPriorityThread(ThreadRegistry::GetSingleton().GetActiveThreadActors());
            if (priorityThread != -1 && priorityThread != threadID) {
                SKSE::log::info("OStimEventListener: thread {} scene change skipTrigger=true (priority thread is {})",
                                threadID, priorityThread);
                skipTrigger = true;
            }
        }
        // Post-climax transition: force skipTrigger for the scene immediately following
        // a transition where an actor had the 'climaxing' tag.
        if (!skipTrigger && _skipNextSceneTrigger.erase(threadID) > 0) {
            SKSE::log::info("OStimEventListener: thread {} scene change skipTrigger=true (post-climax transition)", threadID);
            skipTrigger = true;
        }

        // Speaker selection: pick actor based on gender priority config.
        RE::Actor* speaker = SelectSpeakerActor(threadID, Config::GetSingleton().CommentGenderPriority());
        std::string json = EventPayloadBuilder::BuildSceneChange(threadID, sceneID, skipTrigger);
        FireModEvent("ostimnet_scene_change", json.c_str(), static_cast<float>(threadID), speaker);
    }

    // Fired when the animation speed of a thread changes.
    // Posts to the debounce queue — only the last speed in a burst is fired.
    void OnSpeedChanged(int threadID, int speed) {
        if (_suppressNextSpeedChange.erase(threadID) > 0) {
            SKSE::log::info("OStimEventListener: suppressing initial speed change for thread {}", threadID);
            return;
        }
        SKSE::log::info("OStimEventListener: thread {} speed change posted (speed={})", threadID, speed);
        _dq.PostSpeedChange(threadID, static_cast<int32_t>(speed));
    }

    // Called by DebounceQueue (via AddTask) when the speed-change debounce resolves.
    void OnSpeedChangeFired(int threadID, int32_t speed) {
        if (speed == 0) {
            SKSE::log::info("OStimEventListener: thread {} speed change ignored (speed=0)", threadID);
            return;
        }
        int32_t oldSpeed = ThreadDataStore::GetSingleton().ExchangeCurrentSpeed(threadID, speed);
        SKSE::log::info("OStimEventListener: thread {} speed change fired {} -> {}", threadID, oldSpeed, speed);

        bool skipTrigger = Config::GetSingleton().IsMuted();
        if (!skipTrigger && Config::GetSingleton().CommentsPrioritizeNearestThread()) {
            int priorityThread = SelectPriorityThread(ThreadRegistry::GetSingleton().GetActiveThreadActors());
            if (priorityThread != -1 && priorityThread != threadID) {
                SKSE::log::info("OStimEventListener: thread {} speed change skipTrigger=true (priority thread is {})",
                                threadID, priorityThread);
                skipTrigger = true;
            }
        }

        RE::Actor* speaker = SelectSpeakerActor(threadID, Config::GetSingleton().CommentGenderPriority());
        auto json = EventPayloadBuilder::BuildSpeedChange(threadID, oldSpeed, speed, skipTrigger);
        if (!json.has_value()) {
            SKSE::log::info("OStimEventListener: thread {} speed change ignored (no direction change)", threadID);
            return;
        }
        FireModEvent("ostimnet_speed_change", json->c_str(), static_cast<float>(threadID), speaker);
    }

    // Fired when an actor climaxes. Registers the event with the climax debouncer;
    // the batch handler fires ostimnet_climax after 2s of silence on this thread.
    //   numArg = threadID  |  strArg = sceneID  |  sender = orgasming actor
    void OnClimax(int threadID, const char* sceneID, RE::Actor* actor) {
        SKSE::log::info("OStimEventListener: actor climax registered on thread {} scene {}", threadID, sceneID ? sceneID : "");
        _dq.PostClimaxEvent(threadID, actor ? actor->GetFormID() : 0, sceneID ? sceneID : "");
        RefreshOCumCacheForThread(threadID);

        bool isTransition = false;
        if (g_ostimThreadInterface && g_ostimThreadInterface->IsThreadValid(threadID)) {
            isTransition = g_ostimThreadInterface->IsTransition(threadID);
        }

        if (isTransition) {
            // Suppress the scene change comment that immediately follows a climax on a transition.
            _skipNextSceneTrigger.insert(threadID);
            SKSE::log::info("OStimEventListener: thread {} climax on transition — marking skipTrigger for next scene change", threadID);
        } else {
            SKSE::log::info("OStimEventListener: thread {} climax on main scene — NOT marking skipTrigger", threadID);
        }
    }

    // Called by ClimaxDebouncer (via AddTask) and by OnThreadEnd flush when the debounce resolves.
    // Builds a description from all collected data and fires ostimnet_climax.
    void OnClimaxBatch(int threadID, const DebounceQueue::ClimaxBatchData& data) {
        SKSE::log::info("OStimEventListener: thread {} climax batch fired ({} climax, {} cum applied, {} squirts)",
                        threadID, data.climaxEvents.size(), data.cumApplied.size(), data.squirts.size());
        RE::Actor* speaker = SelectClimaxSpeakerActor(threadID, data);
        std::string json = EventPayloadBuilder::BuildClimax(threadID, data);
        FireModEvent("ostimnet_climax", json.c_str(), static_cast<float>(threadID), speaker);
    }

    // Fired when any OStim thread ends (after continuation check).
    // Fires papyrus mod event: ostimnet_stop
    //   numArg = threadID  |  strArg = ""  |  sender = None
    void OnThreadEnd(int threadID) {
        SKSE::log::info("OStimEventListener: thread {} ended", threadID);
        _suppressNextSceneChange.erase(threadID);  // clean up if thread ended before first scene change
        _suppressNextSpeedChange.erase(threadID);   // clean up if thread ended before first speed change
        _skipNextSceneTrigger.erase(threadID);     // clean up if thread ended before the marked scene change
        _dq.FlushSceneChange(threadID);
        _dq.FlushClimax(threadID);
        _dq.FlushSpeedChange(threadID);
        for (auto [specID, targetID] : ThreadRegistry::GetSingleton().TakeSpectators(threadID))
            DebounceQueue::FireRemoveSpectatorEvent(specID, targetID, threadID);
        RE::Actor* speaker = SelectSpeakerActor(threadID, Config::GetSingleton().CommentGenderPriority());
        std::string json = EventPayloadBuilder::BuildStop(threadID);
        FireModEvent("ostimnet_stop", json.c_str(), static_cast<float>(threadID), speaker);
    }
};

}  // namespace OStimNet
