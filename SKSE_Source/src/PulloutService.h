#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include "RE/Skyrim.h"
#include "ThreadRegistry.h"

// Temporary workaround toggle:
// 1 = Workaround active: uses OActor.GetExcitementMultiplier / SetExcitementMultiplier
//     because native OThread.StallClimax / PermitClimax are broken in OStim NG.
// 0 = Native OStim stall restored: uses OThread.StallClimax / PermitClimax.
#define OSTIM_STALL_CLIMAX_WORKAROUND 1

namespace OStimNet {

class PulloutService {
public:
    static PulloutService& GetSingleton() {
        static PulloutService instance;
        return instance;
    }

    /// Evaluates whether pullout decision action is available for this actor.
    /// Checks: pullout enabled, actor in encounter faction, valid sexual OStimNet thread,
    /// and actor is either giving or receiving vaginal sex in the current scene.
    bool IsPulloutAvailable(RE::Actor* actor);

    struct VaginalPair {
        int giverSlot = -1;
        int receiverSlot = -1;
    };

    /// Resolves which actor slots in the scene are giving/receiving vaginal sex.
    static std::vector<VaginalPair> GetVaginalPairs(const char* sceneId);

    /// Returns true if the thread intent prohibits pullout according to configuration.
    bool IsIntentProhibited(Intent intent) const;

    /// Dispatches OThread.StallClimax(threadID) on game thread.
    void StallClimax(int threadID);

    /// Releases climax stall via OThread.PermitClimax(threadID) on game thread
    /// and clears stall flags in ThreadDataStore.
    void ReleasePulloutStall(int threadID);

    /// Dispatches an ostimnet_partner_near_edge sensory event to SkyrimNet.
    void FirePulloutSensoryCue(int threadID);

    /// Performs scene search via ONavFindPulloutScene, transitions the scene,
    /// and releases the climax stall.
    bool ExecutePulloutNavigation(int threadID);

    /// Sets pullout decision for actor (looks up threadID).
    void SetPulloutDecision(RE::Actor* actor, const std::string& decisionStr);

    /// Sets pullout decision directly for threadID and unblocks stall if active.
    void SetPulloutDecision(int threadID, PulloutDecision decision);

    /// Called by SkyrimNetIntegration when LLM responds to ostimnet_evaluate_pullout_decision.
    void OnLLMDecision(int threadID, const std::string& decision);

    /// Called by SkyrimNetIntegration when LLM request fails or cannot be queued.
    void OnLLMFailure(int threadID);

    /// Ticked by ScheduledEvalService: checks excitement levels and stall timeouts across active threads.
    void CheckActiveThreads(const std::vector<int>& activeThreads);

    /// Syncs OStim's native autoModePulloutChance (0xE37 in OStim.esp) with OStimNet's setting:
    /// - If OStimNet pullout is enabled -> sets OStim native chance to 0.
    /// - If OStimNet pullout is disabled -> sets OStim native chance to 75.
    void SyncOStimPulloutSetting();

    /// Gets OStim's configured pullout hotkey (from OStim API or 0xDEA in OStim.esp).
    int GetOStimPulloutHotkey() const;

    /// Triggers immediate pullout mechanic in the player's thread without waiting for actions or evaluations.
    /// Returns true if pullout navigation was successfully initiated.
    bool TriggerPlayerPullout();

    /// Cleans up state when a thread ends.
    void OnThreadEnd(int threadID);

#if OSTIM_STALL_CLIMAX_WORKAROUND
    // === Temporary Workaround: Excitement Multiplier Stall ===
    /// Queries actor's current excitement multiplier via OActor.GetExcitementMultiplier,
    /// saves it, and zeroes it via OActor.SetExcitementMultiplier.
    void StallActor(int threadID, RE::Actor* actor);

    /// Restores all saved excitement multipliers for actors stalled in this thread.
    void RevertStalledActors(int threadID);

    /// Returns true if the actor is already stalled or has a query pending in this thread.
    bool IsActorStalledOrPending(int threadID, RE::FormID actorFormID) const;

    /// Callback handler when OActor.GetExcitementMultiplier returns.
    void OnActorMultiplierReceived(int threadID, RE::FormID formID, float origMult);
#endif

private:
    PulloutService() = default;
    ~PulloutService() = default;
    PulloutService(const PulloutService&) = delete;
    PulloutService& operator=(const PulloutService&) = delete;

    RE::TESGlobal* m_ostimPulloutChanceGlobal{ nullptr };

#if OSTIM_STALL_CLIMAX_WORKAROUND
    // === Temporary Workaround: Excitement Multiplier Stall State ===
    mutable std::mutex m_stallMutex;
    // threadID -> { actorFormID -> originalMultiplier }
    std::unordered_map<int, std::unordered_map<RE::FormID, float>> m_stalledActorMultipliers;
    // Set of actors currently awaiting GetExcitementMultiplier callback
    std::unordered_set<RE::FormID> m_pendingStallActors;
#endif
};

} // namespace OStimNet
