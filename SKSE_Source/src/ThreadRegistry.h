#pragma once
#include <chrono>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "api/OstimNG-API-Thread.h"
#include "api/OStimNavigator_PublicAPI.h"
#include "Config.h"
#include "SexualPhase.h"

namespace OStimNet {

// Intent enum and string converters are defined once in OStimNavigator_PublicAPI.h
// (namespace OStimNavigatorAPI) and imported here so the rest of OStimNet can use
// the unqualified OStimNet::Intent, OStimNet::IntentFromString, OStimNet::IntentToString names.
using Intent = OStimNavigatorAPI::Intent;
using OStimNavigatorAPI::IntentFromString;
using OStimNavigatorAPI::IntentToString;

// Joins actor/label names into natural English:
//   {}  → "someone"  |  {"Lydia"} → "Lydia"
//   {"Lydia","Farengar"} → "Lydia and Farengar"
//   {"Lydia","Farengar","Ulfric"} → "Lydia, Farengar and Ulfric"
inline std::string FormatActorList(const std::vector<std::string>& names) {
    if (names.empty()) return "someone";
    if (names.size() == 1) return names[0];
    std::string result;
    for (size_t i = 0; i + 1 < names.size(); ++i) {
        if (i > 0) result += ", ";
        result += names[i];
    }
    result += " and " + names.back();
    return result;
}

// -------------------------------------------------------------------------
// Thread lifecycle phases.
//
//  Claiming — a claim token was allocated by ClaimThread before the OStim
//             thread ID is known.  All metadata is pre-stored under the token;
//             once ConfirmClaim() binds token → threadID the entry enters this
//             phase in the main _threads map.
//  Pending  — OStim thread is running but we are still waiting for the LLM
//             to evaluate roles (non-OStimNet / third-party thread path).
//             Times out after 30 s so a stale LLM response cannot leave a
//             thread stuck indefinitely.
//  Active   — fully initialised; accepts scene-change / climax / spectator events.
//  Ended    — thread has finished; entry is kept briefly for continuation
//             detection and then cleared.
// -------------------------------------------------------------------------
enum class ThreadPhase { Claiming, Pending, Active, Ended };

inline const char* ThreadPhaseToString(ThreadPhase p) {
    switch (p) {
        case ThreadPhase::Claiming: return "Claiming";
        case ThreadPhase::Pending:  return "Pending";
        case ThreadPhase::Active:   return "Active";
        case ThreadPhase::Ended:    return "Ended";
        default:                    return "Unknown";
    }
}

// -------------------------------------------------------------------------
// Per-thread data store and lifecycle manager.
//
// Everything is accessed on the game thread; no locking is needed.
// Actor pointers are cached at thread start (HandleStart) and remain valid
// for the entire thread lifetime — OStim ends all threads before actors are
// unloaded (e.g. on cell transition).  Always cleared at HandleEnd via
// ClearThread().
// -------------------------------------------------------------------------
class ThreadRegistry {
public:
    static ThreadRegistry& GetSingleton() {
        static ThreadRegistry instance;
        return instance;
    }

    // =========================================================================
    // Pre-start claim API (ClaimThread / ConfirmThread Papyrus natives)
    // =========================================================================

    // Allocates a claim token and stores all OStimNet thread metadata.
    // Call from the ClaimThread Papyrus native before OThreadBuilder.Start().
    // Returns a monotonically-increasing token that uniquely identifies this
    // pending start.
    int AllocateClaimToken(Intent intent, bool isSexual,
                           std::vector<RE::Actor*> mainActors,
                           std::vector<RE::Actor*> secondaryActors) {
        int token = _nextClaimToken++;
        auto& rec = _claimRecords[token];
        rec.intent          = intent;
        rec.isSexual        = isSexual;
        rec.mainActors      = std::move(mainActors);
        rec.secondaryActors = std::move(secondaryActors);
        SKSE::log::info("ThreadRegistry: AllocateClaimToken -> token={} intent={} isSexual={}",
            token, IntentToString(intent), isSexual);
        return token;
    }

    // Binds a previously allocated claim token to the actual OStim threadID.
    // Call from the ConfirmThread Papyrus native immediately after
    // OThreadBuilder.Start() returns.  Moves claim data into the main _threads
    // map with phase = Claiming so HandleStart can activate it atomically.
    // Returns false if the token was not found (already consumed or invalid).
    bool ConfirmClaim(int token, int threadID) {
        auto it = _claimRecords.find(token);
        if (it == _claimRecords.end()) {
            SKSE::log::warn("ThreadRegistry: ConfirmClaim token={} not found", token);
            return false;
        }
        auto& state         = _threads[threadID];
        state.phase         = ThreadPhase::Claiming;
        state.claimToken    = token;
        state.intent        = it->second.intent;
        state.intentHasBeenSet = true;
        state.sexual        = it->second.isSexual;
        state.mainActors    = std::move(it->second.mainActors);
        state.secondaryActors = std::move(it->second.secondaryActors);
        state.isOStimNet    = true;
        _claimRecords.erase(it);
        SKSE::log::info("ThreadRegistry: ConfirmClaim token={} -> threadID={} intent={} isSexual={}",
            token, threadID, IntentToString(state.intent), state.sexual.value_or(false));
        return true;
    }

    // Called from HandleStart: if this threadID has a confirmed claim
    // (phase == Claiming), transitions it to Active and returns true.
    // Returns false when no pre-claimed entry exists (non-OStimNet path).
    bool ConsumeActiveClaim(int threadID) {
        auto it = _threads.find(threadID);
        if (it == _threads.end() || it->second.phase != ThreadPhase::Claiming) {
            return false;
        }
        it->second.phase = ThreadPhase::Active;
        SKSE::log::info("ThreadRegistry: ConsumeActiveClaim threadID={} -> Active", threadID);
        return true;
    }

    // Returns true if any unconfirmed claim record (ClaimThread called but
    // ConfirmThread not yet arrived) contains actors whose FormIDs overlap with
    // the provided set.  Used by HandleStart to distinguish the race window
    // (OStimNet thread whose ConfirmThread is in-flight) from a genuine
    // third-party OStim thread that happened to start at the same time as an
    // unrelated claim.
    bool HasPendingClaimForActors(const std::vector<uint32_t>& threadActorFormIDs) const {
        for (const auto& [token, rec] : _claimRecords) {
            for (const auto* actor : rec.mainActors) {
                if (actor) {
                    uint32_t fid = static_cast<uint32_t>(actor->GetFormID());
                    if (std::find(threadActorFormIDs.begin(), threadActorFormIDs.end(), fid)
                            != threadActorFormIDs.end())
                        return true;
                }
            }
            for (const auto* actor : rec.secondaryActors) {
                if (actor) {
                    uint32_t fid = static_cast<uint32_t>(actor->GetFormID());
                    if (std::find(threadActorFormIDs.begin(), threadActorFormIDs.end(), fid)
                            != threadActorFormIDs.end())
                        return true;
                }
            }
        }
        return false;
    }

    // =========================================================================
    // Phase management
    // =========================================================================

    // Transitions a known thread to the given phase.  When transitioning to
    // Pending, records the current time so GetTimedOutPendingThreadIDs() works.
    void SetPhase(int threadID, ThreadPhase phase) {
        auto it = _threads.find(threadID);
        if (it == _threads.end()) return;
        it->second.phase = phase;
        if (phase == ThreadPhase::Pending)
            it->second.pendingSince = std::chrono::steady_clock::now();
        SKSE::log::info("ThreadRegistry: thread {} -> {}", threadID, ThreadPhaseToString(phase));
    }

    ThreadPhase GetPhase(int threadID) const {
        auto it = _threads.find(threadID);
        return it != _threads.end() ? it->second.phase : ThreadPhase::Pending;
    }

    bool IsActive(int threadID) const {
        return GetPhase(threadID) == ThreadPhase::Active;
    }

    bool IsPending(int threadID) const {
        return GetPhase(threadID) == ThreadPhase::Pending;
    }

    // Returns true only if the thread is actually registered in the map AND its
    // phase is Pending.  Unlike IsPending, this returns false for threads that
    // were never registered or have already been cleared by ClearThread().
    bool IsRegisteredAndPending(int threadID) const {
        auto it = _threads.find(threadID);
        return it != _threads.end() && it->second.phase == ThreadPhase::Pending;
    }

    // Returns threadIDs that have been in the Pending phase longer than
    // `timeout`.  Used by the event listener to fire ostimnet_start with empty
    // roles when the LLM never responds.
    std::vector<int> GetTimedOutPendingThreadIDs(std::chrono::seconds timeout) const {
        auto now = std::chrono::steady_clock::now();
        std::vector<int> result;
        for (const auto& [id, state] : _threads) {
            if (state.phase == ThreadPhase::Pending &&
                (now - state.pendingSince) >= timeout) {
                result.push_back(id);
            }
        }
        return result;
    }

    // =========================================================================
    // Sexual encounter phase (updated at thread start and on scene changes)
    // =========================================================================

    std::optional<SexualPhase> GetSexualPhase(int threadID) const {
        auto it = _threads.find(threadID);
        if (it == _threads.end()) return std::nullopt;
        // Non-sexual threads always report no phase.
        if (!it->second.sexual.value_or(false)) return std::nullopt;
        return it->second.sexualPhase;
    }

    void SetSexualPhase(int threadID, SexualPhase phase) {
        auto it = _threads.find(threadID);
        if (it == _threads.end()) return;
        SKSE::log::info("ThreadRegistry: thread {} SetSexualPhase -> {}", threadID, SexualPhaseToName(phase));
        it->second.sexualPhase = phase;
    }

    // Returns comma-separated activity keywords for the thread's current phase,
    // or an empty string if phase tracking is disabled for this thread.
    std::string GetSexualPhaseActivityKeywords(int threadID) const {
        auto it = _threads.find(threadID);
        if (it == _threads.end() || !it->second.sexualPhase.has_value()) return "";
        return SexualPhaseToActivityKeywordString(*it->second.sexualPhase);
    }

    // Returns true if the player character is one of the actors in this thread.
    bool IsPlayerThread(int threadID) const {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return false;
        for (auto* actor : GetActorPtrs(threadID))
            if (actor == static_cast<RE::Actor*>(player)) return true;
        return false;
    }

    // Computes the starting sexual phase for a newly-started thread, applying all
    // relevant config checks:
    //   - EnableThreadPhases (global toggle)          → nullopt if disabled
    //   - Intent phase list empty                      → nullopt
    //   - EnableUndressingPlayer / EnableUndressingNpc → skips Undressing if off
    //   - 2-actor requirement and no-furniture / must-be-standing requirement
    //
    // Returns the resolved starting SexualPhase, or nullopt if phase tracking
    // is disabled or not applicable for this thread. Does NOT call SetSexualPhase;
    // the caller is responsible for storing the result.
    //
    // Must be called from the SKSE game thread; reads the OStim API for scene info.
    std::optional<SexualPhase> ResolveStartingPhase(int threadID) const {
        if (!Config::GetSingleton().EnableThreadPhases()) return std::nullopt;

        // Non-sexual threads never get phase tracking.
        auto sexual = GetSexual(threadID);
        if (!sexual.value_or(false)) return std::nullopt;

        Intent intent = GetIntent(threadID);
        const auto& phaseList = IntentToPhaseList(intent);
        if (phaseList.empty()) return std::nullopt;

        // Undressing config check.
        bool isPlayerThread = IsPlayerThread(threadID);
        bool undressingEnabled = isPlayerThread
            ? Config::GetSingleton().EnableUndressingPlayer()
            : Config::GetSingleton().EnableUndressingNpc();
        SKSE::log::info("ThreadRegistry: ResolveStartingPhase thread={} undressingEnabled={}",
            threadID, undressingEnabled);

        // Undressing only makes sense for exactly 2 standing actors with no furniture.
        if (undressingEnabled) {
            bool twoActors = (GetActorPtrs(threadID).size() == 2);
            if (!twoActors) {
                SKSE::log::info("ThreadRegistry: thread {} has {} actors, disabling undressing",
                    threadID, GetActorPtrs(threadID).size());
            }
            bool noFurniture = true;
            if (g_ostimThreadInterface) {
                const char* curScene = g_ostimThreadInterface->GetCurrentSceneID(
                    static_cast<uint32_t>(threadID));
                if (curScene) {
                    if (ONavSceneHasFurniture && ONavSceneHasFurniture(curScene)) {
                        SKSE::log::info("ThreadRegistry: thread {} scene {} has furniture, disabling undressing",
                            threadID, curScene);
                        noFurniture = false;
                    } else if (ONavSceneHasActorWithTag && !ONavSceneHasActorWithTag(curScene, "standing")) {
                        SKSE::log::info("ThreadRegistry: thread {} scene {} has no 'standing' actor, disabling undressing",
                            threadID, curScene);
                        noFurniture = false;
                    }
                }
            }
            if (!twoActors || !noFurniture) undressingEnabled = false;
        }

        size_t startIdx = 0;
        if (!undressingEnabled &&
            phaseList[0] == SexualPhase::Undressing &&
            phaseList.size() > 1) {
            startIdx = 1;
        }

        return phaseList[startIdx];
    }

    // =========================================================================
    // Current sexual position (updated on every scene change in C++)
    // =========================================================================

    void SetCurrentPosition(int threadID, std::string position) {
        _threads[threadID].currentPosition = std::move(position);
    }

    // Returns the previously stored speed, then updates it to newSpeed.
    // Returns -1 if no speed was recorded for this thread yet.
    int32_t ExchangeCurrentSpeed(int threadID, int32_t newSpeed) {
        auto& state = _threads[threadID];
        int32_t old = state.currentSpeed;
        state.currentSpeed = newSpeed;
        return old;
    }

    const std::string& GetCurrentPosition(int threadID) const {

        static const std::string kEmpty;
        auto it = _threads.find(threadID);
        return it != _threads.end() ? it->second.currentPosition : kEmpty;
    }

    // =========================================================================
    // Actor data (set once at HandleStart, valid for the thread's lifetime)
    // =========================================================================

    // Single-pass fetch from the OStim API: resolves FormIDs, Actor* pointers,
    // and display names in one call, then stores them.  Returns the FormID list
    // so the caller (OStimEventListener) can use it for continuation detection
    // without needing to know about the API internals.
    //
    // If no pre-claimed entry exists for threadID this also initialises the
    // thread entry with phase = Pending and records pendingSince, ready for the
    // non-OStimNet LLM evaluation path.
    std::vector<uint32_t> FetchAndStoreActors(int threadID) {
        constexpr uint32_t kMaxActors = 8;
        std::vector<uint32_t> formIDs;
        std::vector<RE::Actor*> ptrs;
        std::vector<std::string> names;

        if (g_ostimThreadInterface) {
            OstimNG_API::Thread::ActorData buffer[kMaxActors];
            uint32_t count = g_ostimThreadInterface->GetActors(static_cast<uint32_t>(threadID), buffer, kMaxActors);
            formIDs.reserve(count);
            ptrs.reserve(count);
            names.reserve(count);
            for (uint32_t i = 0; i < count; ++i) {
                formIDs.push_back(buffer[i].formID);
                auto* actor = RE::TESForm::LookupByID<RE::Actor>(buffer[i].formID);
                ptrs.push_back(actor);
                names.push_back(GetActorDisplayName(actor, "Actor" + std::to_string(i)));
            }
        }

        // Initialise a Pending entry for new threads that don't have a claim.
        // Threads that went through ClaimThread/ConfirmThread already have a
        // Claiming entry — leave their phase untouched.
        auto& state = _threads[threadID];
        if (state.phase != ThreadPhase::Claiming) {
            if (state.phase != ThreadPhase::Pending) {
                state.phase        = ThreadPhase::Pending;
                state.pendingSince = std::chrono::steady_clock::now();
            }
        }

        state.actors.formIDs = formIDs;
        state.actors.ptrs   = std::move(ptrs);
        state.actors.names  = std::move(names);
        return formIDs;
    }

    // Returns the threadID of a Pending/Active thread that contains actorFormID,
    // or -1 if the actor is not in any running thread.
    int GetActorThreadID(RE::FormID formID) const {
        for (const auto& [threadID, state] : _threads) {
            if (state.phase != ThreadPhase::Pending && state.phase != ThreadPhase::Active)
                continue;
            for (uint32_t id : state.actors.formIDs)
                if (id == formID) return threadID;
        }
        return -1;
    }

    void SetActors(int threadID, std::vector<RE::Actor*> ptrs, std::vector<std::string> names) {
        auto& state = _threads[threadID];
        state.actors.ptrs  = std::move(ptrs);
        state.actors.names = std::move(names);
    }

    const std::vector<RE::Actor*>& GetActorPtrs(int threadID) const {
        static const std::vector<RE::Actor*> kEmpty;
        auto it = _threads.find(threadID);
        return it != _threads.end() ? it->second.actors.ptrs : kEmpty;
    }

    const std::vector<std::string>& GetActorNames(int threadID) const {
        static const std::vector<std::string> kEmpty;
        auto it = _threads.find(threadID);
        return it != _threads.end() ? it->second.actors.names : kEmpty;
    }

    const std::vector<uint32_t>& GetActorFormIDs(int threadID) const {
        static const std::vector<uint32_t> kEmpty;
        auto it = _threads.find(threadID);
        return it != _threads.end() ? it->second.actors.formIDs : kEmpty;
    }

    // Returns true once FetchAndStoreActors has been called for this thread
    // (i.e. HandleStart has fired). False for pre-start Claiming entries and
    // threads not yet registered.
    bool HasStarted(int threadID) const {
        auto it = _threads.find(threadID);
        return it != _threads.end() && !it->second.actors.formIDs.empty();
    }

    // Returns true if any thread is currently in the Pending or Active phase.
    bool HasActiveThreads() const {
        for (const auto& [id, state] : _threads)
            if (state.phase == ThreadPhase::Pending || state.phase == ThreadPhase::Active)
                return true;
        return false;
    }

    // Returns a {threadID → formIDs} snapshot for all Pending/Active threads.
    // Used by SelectPriorityThread and GetActiveScenes.
    std::unordered_map<int, std::vector<uint32_t>> GetActiveThreadActors() const {
        std::unordered_map<int, std::vector<uint32_t>> result;
        for (const auto& [id, state] : _threads)
            if (state.phase == ThreadPhase::Pending || state.phase == ThreadPhase::Active)
                result.emplace(id, state.actors.formIDs);
        return result;
    }

    // Snapshot of a single active thread for external consumers.
    struct ActiveSceneInfo {
        int                   threadID;
        std::vector<uint32_t> actorFormIDs;
    };

    // Returns a snapshot of all Pending/Active threads (actor FormIDs).
    std::vector<ActiveSceneInfo> GetActiveScenes() const {
        std::vector<ActiveSceneInfo> result;
        for (const auto& [id, state] : _threads) {
            if (state.phase != ThreadPhase::Pending && state.phase != ThreadPhase::Active)
                continue;
            result.push_back({id, state.actors.formIDs});
        }
        return result;
    }

    // =========================================================================
    // Actor name utilities (static helpers, no thread state)
    // =========================================================================

    static std::string GetActorDisplayName(RE::Actor* actor, const std::string& fallback = "someone") {
        if (actor) {
            const char* n = actor->GetDisplayFullName();
            if (n && n[0]) {
                std::string name = n;
                size_t first = name.find_first_not_of(" \t\n\r");
                if (first == std::string::npos) return fallback;
                size_t last = name.find_last_not_of(" \t\n\r");
                return name.substr(first, (last - first + 1));
            }
        }
        return fallback;
    }

    static std::string FormatActorNames(const std::vector<RE::Actor*>& actors) {
        std::vector<std::string> names;
        names.reserve(actors.size());
        for (auto* actor : actors) {
            std::string name = GetActorDisplayName(actor, "");
            if (!name.empty()) names.push_back(std::move(name));
        }
        return FormatActorList(names);
    }

    // =========================================================================
    // Papyrus-set metadata (preserved for backward compat; Phase 2 replaces
    // SetIntent/SetSexual/SetMainActors/SetSecondaryActors with ClaimThread)
    // =========================================================================

    void SetIntent(int threadID, Intent intent) {
        SKSE::log::info("ThreadRegistry: thread {} SetIntent -> {}", threadID, IntentToString(intent));
        auto& state         = _threads[threadID];
        state.intent        = intent;
        state.intentHasBeenSet = true;
    }

    Intent GetIntent(int threadID) const {
        auto it = _threads.find(threadID);
        return it != _threads.end() ? it->second.intent : Intent::Unset;
    }

    bool HasIntentBeenSet(int threadID) const {
        auto it = _threads.find(threadID);
        return it != _threads.end() && it->second.intentHasBeenSet;
    }

    void SetSexual(int threadID, bool isSexual) {
        SKSE::log::info("ThreadRegistry: thread {} SetSexual -> {}", threadID, isSexual);
        _threads[threadID].sexual = isSexual;
    }

    std::optional<bool> GetSexual(int threadID) const {
        auto it = _threads.find(threadID);
        return it != _threads.end() ? it->second.sexual : std::nullopt;
    }

    void SetContinuation(int threadID, bool isContinuation) {
        _threads[threadID].continuation = isContinuation;
    }

    bool IsContinuation(int threadID) const {
        auto it = _threads.find(threadID);
        return it != _threads.end() && it->second.continuation;
    }

    // Stores LLM-evaluated actor roles and intent to use when the continuation
    // thread starts, instead of copying stale data from the old thread.
    // Call before SetThreadContinuation + the hot-swap that ends the thread.
    void SetContinuationOverride(int threadID, Intent intent,
                                  std::vector<RE::Actor*> mainActors,
                                  std::vector<RE::Actor*> secondaryActors) {
        auto& state = _threads[threadID];
        state.overrideIntent          = intent;
        state.overrideMainActors      = std::move(mainActors);
        state.overrideSecondaryActors = std::move(secondaryActors);
        SKSE::log::info("ThreadRegistry: SetContinuationOverride thread={} intent={} main={} secondary={}",
            threadID, IntentToString(intent),
            FormatActorNames(state.overrideMainActors),
            FormatActorNames(state.overrideSecondaryActors));
    }

    bool HasContinuationOverride(int threadID) const {
        auto it = _threads.find(threadID);
        return it != _threads.end() && !it->second.overrideMainActors.empty();
    }

    void SetOStimNet(int threadID, bool value) {
        SKSE::log::info("ThreadRegistry: thread {} SetOStimNet -> {}", threadID, value);
        _threads[threadID].isOStimNet = value;
    }

    bool IsOStimNet(int threadID) const {
        auto it = _threads.find(threadID);
        return it != _threads.end() && it->second.isOStimNet;
    }

    void SetMainActors(int threadID, std::vector<RE::Actor*> actors) {
        SKSE::log::info("ThreadRegistry: thread {} SetMainActors count={}", threadID, actors.size());
        for (auto* a : actors)
            SKSE::log::info("ThreadRegistry: thread {} SetMainActors actor 0x{:08X} ('{}')",
                threadID, a ? a->GetFormID() : 0,
                (a && a->GetDisplayFullName()) ? a->GetDisplayFullName() : "(null)");
        _threads[threadID].mainActors = std::move(actors);
    }

    const std::vector<RE::Actor*>& GetMainActors(int threadID) const {
        static const std::vector<RE::Actor*> kEmpty;
        auto it = _threads.find(threadID);
        return it != _threads.end() ? it->second.mainActors : kEmpty;
    }

    void SetSecondaryActors(int threadID, std::vector<RE::Actor*> actors) {
        SKSE::log::info("ThreadRegistry: thread {} SetSecondaryActors count={}", threadID, actors.size());
        for (auto* a : actors)
            SKSE::log::info("ThreadRegistry: thread {} SetSecondaryActors actor 0x{:08X} ('{}')",
                threadID, a ? a->GetFormID() : 0,
                (a && a->GetDisplayFullName()) ? a->GetDisplayFullName() : "(null)");
        _threads[threadID].secondaryActors = std::move(actors);
    }

    const std::vector<RE::Actor*>& GetSecondaryActors(int threadID) const {
        static const std::vector<RE::Actor*> kEmpty;
        auto it = _threads.find(threadID);
        return it != _threads.end() ? it->second.secondaryActors : kEmpty;
    }

    std::string GetFormattedMainActorNames(int threadID) const {
        const auto& actors = GetMainActors(threadID);
        SKSE::log::debug("ThreadRegistry: thread {} GetFormattedMainActorNames: {} actor(s) stored",
            threadID, actors.size());
        std::string formatted = FormatActorNames(actors);
        SKSE::log::debug("ThreadRegistry: thread {} GetFormattedMainActorNames -> '{}'", threadID, formatted);
        return formatted;
    }

    std::string GetFormattedSecondaryActorNames(int threadID) const {
        return FormatActorNames(GetSecondaryActors(threadID));
    }

    // =========================================================================
    // Spectator state — game-thread only
    // Spectators are stored per-thread; _alreadyNotified is a global scan-dedup
    // set so the periodic scan doesn't re-add a spectator that was manually added.
    // =========================================================================

    struct SpectatorInfo { RE::FormID targetID; int threadID; };

    // Stores a spectator assignment and marks them as notified so the scan skips them.
    void AddSpectator(int threadID, RE::FormID spectatorID, RE::FormID targetID) {
        _threads[threadID].spectators[spectatorID] = targetID;
        _alreadyNotified.insert(spectatorID);
    }

    // Removes a spectator from whichever thread they're assigned to.
    // Returns {targetID, threadID} if found, {0, -1} if not tracked.
    SpectatorInfo RemoveSpectator(RE::FormID spectatorID) {
        _alreadyNotified.erase(spectatorID);
        for (auto& [tid, state] : _threads) {
            auto it = state.spectators.find(spectatorID);
            if (it != state.spectators.end()) {
                SpectatorInfo info{it->second, tid};
                state.spectators.erase(it);
                return info;
            }
        }
        return {0, -1};
    }

    // Removes all spectators for a thread and returns them as {spectatorID, targetID} pairs.
    // Also removes each from _alreadyNotified so they can be re-detected by the scan.
    std::vector<std::pair<RE::FormID, RE::FormID>> TakeSpectators(int threadID) {
        auto it = _threads.find(threadID);
        if (it == _threads.end()) return {};
        std::vector<std::pair<RE::FormID, RE::FormID>> result;
        for (const auto& [specID, targetID] : it->second.spectators) {
            _alreadyNotified.erase(specID);
            result.emplace_back(specID, targetID);
        }
        it->second.spectators.clear();
        return result;
    }

    RE::FormID GetSpectatorTargetFormID(RE::FormID spectatorID) const {
        for (const auto& [tid, state] : _threads) {
            auto it = state.spectators.find(spectatorID);
            if (it != state.spectators.end()) return it->second;
        }
        return 0;
    }

    int GetSpectatorThreadID(RE::FormID spectatorID) const {
        for (const auto& [tid, state] : _threads) {
            if (state.spectators.count(spectatorID)) return tid;
        }
        return -1;
    }

    std::vector<RE::FormID> GetSpectatorFormIDs(int threadID) const {
        auto it = _threads.find(threadID);
        if (it == _threads.end()) return {};
        std::vector<RE::FormID> result;
        for (const auto& [specID, targetID] : it->second.spectators)
            result.push_back(specID);
        return result;
    }

    // Returns all tracked spectator FormIDs (across all threads) for scan pruning.
    std::vector<RE::FormID> GetAllTrackedSpectatorIDs() const {
        return std::vector<RE::FormID>(_alreadyNotified.begin(), _alreadyNotified.end());
    }

    bool IsSpectatorAlreadyNotified(RE::FormID spectatorID) const {
        return _alreadyNotified.count(spectatorID) > 0;
    }

    // Clears all spectator state across all threads (called on Stop/Reset).
    void ClearAllSpectators() {
        for (auto& [tid, state] : _threads)
            state.spectators.clear();
        _alreadyNotified.clear();
    }


    // =========================================================================
    // Lifecycle
    // =========================================================================

    // Called from HandleStart when a continuation is detected.  Copies intent,
    // sexual, isOStimNet, mainActors, and currentPosition from the old thread to
    // the new thread; appends newly-joined actors to secondaryActors; sets the new
    // thread's phase to Active.  Description is always computed live from the
    // OStim API and never stored.
    //
    // Three cases:
    //   override present  — actor-add path; applies LLM override for both same-ID
    //                       (player thread) and different-ID continuations.
    //   no override, different IDs — actor-swap path; copies state from old thread.
    //   no override, same ID       — same-ID restart; state is preserved in place,
    //                               joiner detection appends new actors.
    //
    // preUpdateOldFormIDs: FormIDs of the old thread captured BEFORE
    // FetchAndStoreActors overwrote the store (required for same-ID restarts).
    void CopyStateForContinuation(int oldThreadID, int newThreadID,
                                  const std::vector<uint32_t>& preUpdateOldFormIDs = {}) {
        SKSE::log::info("ThreadRegistry: CopyStateForContinuation old={} new={}", oldThreadID, newThreadID);
        auto oldIt = _threads.find(oldThreadID);
        if (oldIt == _threads.end()) {
            SKSE::log::warn("ThreadRegistry: CopyStateForContinuation old thread {} not found", oldThreadID);
            return;
        }
        const ThreadState& old = oldIt->second;
        ThreadState& nw        = _threads[newThreadID];

        // Check whether Papyrus pre-set override actors (actor-add path).
        // If so, use those instead of copying stale data from the old thread.
        bool hasOverride = !nw.overrideMainActors.empty();

        if (hasOverride) {
            // Actor-add path (same-ID or different-ID): use LLM-evaluated values.
            // sexual and isOStimNet are already correct on the thread entry —
            // for same-ID they were never cleared; for different-ID copy from old.
            nw.intent           = nw.overrideIntent != Intent::Unset ? nw.overrideIntent : old.intent;
            nw.intentHasBeenSet = true;
            if (oldThreadID != newThreadID) {
                nw.sexual     = old.sexual;
                nw.isOStimNet = old.isOStimNet;
            }
            nw.mainActors      = std::move(nw.overrideMainActors);
            nw.secondaryActors = std::move(nw.overrideSecondaryActors);
            // currentPosition intentionally NOT copied —
            // HandleStart will compute a fresh description from the new thread.
            SKSE::log::info("ThreadRegistry: CopyStateForContinuation (override{}) intent={} sexual={} isOStimNet={}",
                oldThreadID == newThreadID ? " same-ID" : "",
                IntentToString(nw.intent),
                nw.sexual.has_value() ? (nw.sexual.value() ? "true" : "false") : "unset",
                nw.isOStimNet);
            SKSE::log::info("ThreadRegistry: CopyStateForContinuation (override) mainActors={}", FormatActorNames(nw.mainActors));
            SKSE::log::info("ThreadRegistry: CopyStateForContinuation (override) secondaryActors={}", FormatActorNames(nw.secondaryActors));
        } else if (oldThreadID != newThreadID) {
            // Swap path (different IDs, no override): copy state from old thread.
            nw.intent              = old.intent;
            nw.intentHasBeenSet    = old.intentHasBeenSet;
            nw.sexual              = old.sexual;
            nw.isOStimNet          = old.isOStimNet;
            nw.mainActors          = old.mainActors;
            nw.currentPosition     = old.currentPosition;
            // currentPosition intentionally NOT copied — always refetched by HandleStart.
            SKSE::log::info("ThreadRegistry: CopyStateForContinuation (swap) intent={} sexual={} isOStimNet={} position='{}'",
                IntentToString(nw.intent),
                nw.sexual.has_value() ? (nw.sexual.value() ? "true" : "false") : "unset",
                nw.isOStimNet,
                nw.currentPosition);
            SKSE::log::info("ThreadRegistry: CopyStateForContinuation (swap) mainActors={}", FormatActorNames(nw.mainActors));
        } else {
            SKSE::log::info("ThreadRegistry: CopyStateForContinuation same-ID restart, no override, checking for joiners");
        }

        // Copy sexual phase — continuation always inherits the predecessor's phase.
        nw.sexualPhase = old.sexualPhase;

        // Clear override fields — consumed above or irrelevant for same-ID restarts.
        nw.overrideIntent         = Intent::Unset;
        nw.overrideMainActors.clear();
        nw.overrideSecondaryActors.clear();

        // Continuation threads are considered Active immediately.
        nw.phase = ThreadPhase::Active;

        // For the override (add) path, secondaryActors were already set above.
        // For swap and same-ID paths, run the joiner-detection loop as before.
        if (!hasOverride) {
            // Build old FormID set.
            std::unordered_set<uint32_t> oldFormIDs;
            if (!preUpdateOldFormIDs.empty()) {
                for (uint32_t fid : preUpdateOldFormIDs) {
                    SKSE::log::info("ThreadRegistry: CopyStateForContinuation old actor FormID {:08X}", fid);
                    oldFormIDs.insert(fid);
                }
            } else {
                for (auto* a : old.actors.ptrs) {
                    SKSE::log::info("ThreadRegistry: CopyStateForContinuation old actor {} ({:08X})",
                        GetActorDisplayName(a), a ? a->GetFormID() : 0);
                    if (a) oldFormIDs.insert(a->GetFormID());
                }
            }

            // secondaryActors = inherited + newly joined actors.
            std::vector<RE::Actor*> secondary = old.secondaryActors;
            for (auto* a : nw.actors.ptrs) {
                SKSE::log::info("ThreadRegistry: CopyStateForContinuation checking actor {} ({:08X}) for joiner status",
                    GetActorDisplayName(a), a ? a->GetFormID() : 0);
                if (a && !oldFormIDs.count(a->GetFormID())) {
                    SKSE::log::info("ThreadRegistry: CopyStateForContinuation new joiner: {} ({:08X})",
                        GetActorDisplayName(a), a->GetFormID());
                    secondary.push_back(a);
                }
            }
            nw.secondaryActors = std::move(secondary);
            SKSE::log::info("ThreadRegistry: CopyStateForContinuation secondaryActors={}", FormatActorNames(nw.secondaryActors));
        }
    }

    // Call from HandleEnd to free all per-thread state.
    // Also discards any orphaned claim record for this threadID.
    void ClearThread(int threadID) {
        auto it = _threads.find(threadID);
        if (it != _threads.end()) {
            int tok = it->second.claimToken;
            if (tok != -1) _claimRecords.erase(tok);
            for (const auto& [specID, targetID] : it->second.spectators)
                _alreadyNotified.erase(specID);
            _threads.erase(it);
        }
    }

    // Call from OStimEventListener::Reset() on game load to wipe all state.
    void ClearAll() {
        _threads.clear();
        _claimRecords.clear();
        _alreadyNotified.clear();
    }

private:
    // -----------------------------------------------------------------
    // Pre-start claim record — lives in _claimRecords until ConfirmClaim
    // moves it into _threads keyed by the real threadID.
    // -----------------------------------------------------------------
    struct ClaimRecord {
        Intent                  intent          = Intent::Unset;
        bool                    isSexual        = false;
        std::vector<RE::Actor*> mainActors;
        std::vector<RE::Actor*> secondaryActors;
    };

    struct ActorEntry {
        std::vector<uint32_t>    formIDs;
        std::vector<RE::Actor*>  ptrs;
        std::vector<std::string> names;
    };

    struct ThreadState {
        // --- lifecycle ---
        ThreadPhase             phase            = ThreadPhase::Pending;
        int                     claimToken       = -1;
        std::string             currentPosition;
        int32_t                 currentSpeed     = -1;
        std::chrono::steady_clock::time_point pendingSince;

        // --- sexual phase (nullopt = phases disabled for this thread) ---
        std::optional<SexualPhase> sexualPhase;

        // --- existing fields ---
        ActorEntry              actors;
        Intent                  intent           = Intent::Unset;
        bool                    intentHasBeenSet = false;
        std::optional<bool>     sexual;
        bool                    continuation     = false;
        bool                    isOStimNet       = false;
        std::vector<RE::Actor*> mainActors;
        std::vector<RE::Actor*> secondaryActors;
        // Continuation override: set by SetContinuationOverride() before an actor-add
        // hot-swap. When non-empty, CopyStateForContinuation uses these values instead
        // of copying from the old thread, and skips stale currentPosition.
        Intent                  overrideIntent         = Intent::Unset;
        std::vector<RE::Actor*> overrideMainActors;
        std::vector<RE::Actor*> overrideSecondaryActors;
        // Per-thread spectators: spectatorFormID → targetActorFormID
        std::unordered_map<RE::FormID, RE::FormID> spectators;
    };

    int _nextClaimToken = 1;
    std::unordered_map<int, ClaimRecord>  _claimRecords;  // token  → pre-start data
    std::unordered_map<int, ThreadState>  _threads;       // threadID → runtime state
    // Global scan-dedup: spectatorFormIDs that have been assigned (ostimnet_add_spectator fired).
    // Not per-thread because a spectator can only watch one thread at a time.
    std::unordered_set<RE::FormID>        _alreadyNotified;
};

// Backward-compatibility alias — callers use ThreadDataStore::GetSingleton().
using ThreadDataStore = ThreadRegistry;

}  // namespace OStimNet
