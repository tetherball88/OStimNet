#pragma once

#include <chrono>
#include <functional>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "RE/Skyrim.h"

namespace OStimNet {

/// Bridges OVirginity Reflowered (`OVirginity.esp`) state into SkyrimNet decorators,
/// GM evaluations, gentle speed pacing, mid-scene deflowering events, and permanent memories.
class OVirginityService {
public:
    static OVirginityService& GetSingleton() {
        static OVirginityService instance;
        return instance;
    }

    /// Returns true if OVirginity.esp (`OVirginityQuest`) is loaded in the active game.
    bool IsLoaded() const;

    /// Returns cached virginity state for an eligible actor (`true` = virgin, `false` = non-virgin),
    /// or `std::nullopt` if OVirginity is unavailable, the actor is ineligible, or not yet cached.
    /// Schedules an asynchronous refresh if missing or stale.
    std::optional<bool> GetCachedVirginity(RE::Actor* actor);

    /// Asynchronously invokes `OVirginityScript.isVirgin(actor)` on `OVirginityQuest` for any
    /// uncached/stale (or all when `forceRefresh` is true) eligible actors in `formIDs`, then
    /// invokes `onReady` on the game thread (bounded by a 1500 ms failsafe timeout).
    void EnsureActorsResolved(const std::vector<RE::FormID>& formIDs,
                              std::function<void()> onReady,
                              bool forceRefresh = false);

    /// Resolves virginity state at thread start and records any participating virgins.
    void OnThreadStart(int threadID, const std::vector<uint32_t>& actorFormIDs);

    /// Transfers active virgin and deflowered tracking across an OStim continuation (actor swap/add)
    /// and resolves any newly joined actors.
    void TransferForContinuation(int oldThreadID,
                                 int newThreadID,
                                 const std::vector<uint32_t>& newActorFormIDs);

    /// Returns true if this thread currently has at least one tracked active virgin.
    bool ThreadHasActiveVirgin(int threadID) const;

    /// Clamps initial intercourse speed to slow (`0`) for romantic/lustful threads involving a virgin
    /// and schedules a delayed deflowering check (450 ms) to catch `OVirginity` state updates.
    void OnSceneChanged(int threadID, const std::string& sceneID);

    /// Called when an `ovirginity_lost_*` mod event fires; re-evaluates active threads with tracked virgins.
    void OnOVirginityLostModEvent();

    /// Queries `OVirginityScript.isVirgin` for tracked virgins in `threadID` and dispatches
    /// `virginity_lost` + permanent vector memories for any actor whose virginity was just taken.
    void CheckThreadVirginityLoss(int threadID, const std::string& sceneID);

    /// Fires `virginity_lost_diary` for each deflowered entry recorded during `threadID` and clears thread state.
    void OnThreadEnd(int threadID);

    /// Clears per-thread tracking state without firing diary events (e.g. cancelled pending thread).
    void ClearThread(int threadID);

    /// Clears all cached and per-thread state (called on game load / new game).
    void Reset();

    /// Internal callback hook used by `IsVirginCallback` to store a resolved value.
    void OnVirginityResolved(RE::FormID formID, bool isVirgin);

private:
    OVirginityService() = default;
    ~OVirginityService() = default;
    OVirginityService(const OVirginityService&) = delete;
    OVirginityService& operator=(const OVirginityService&) = delete;

    struct CacheEntry {
        bool isVirgin{false};
        std::chrono::steady_clock::time_point timestamp;
    };

    struct DefloweredRecord {
        RE::FormID  virginFormID{0};
        RE::FormID  partnerFormID{0};
        std::string sceneID;
    };

    static bool IsActorEligible(RE::Actor* actor);
    static RE::TESQuest* GetOVirginityQuest();
    bool IsActorInTrackedThread(RE::FormID formID) const;

    mutable std::shared_mutex _cacheMutex;
    std::unordered_map<RE::FormID, CacheEntry> _cache;
    std::unordered_set<RE::FormID> _inFlightQueries;

    mutable std::mutex _threadMutex;
    std::unordered_map<int, std::unordered_set<RE::FormID>> _threadVirgins;
    std::unordered_map<int, std::vector<DefloweredRecord>> _threadDeflowered;
};

}  // namespace OStimNet
