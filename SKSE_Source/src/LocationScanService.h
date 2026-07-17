#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

namespace OStimNet {

/// Lightweight snapshot of the player's semantic location state.
/// Used to detect meaningful transitions (interior/exterior boundary,
/// worldspace change, named location change) and suppress the scan
/// on ordinary outdoor tile crossings where nothing meaningful changed.
struct LocationFingerprint {
    RE::FormID locationFormID{0};    ///< GetCurrentLocation()->GetFormID(), or 0
    RE::FormID worldspaceFormID{0};  ///< GetWorldspace()->GetFormID(), or 0 (interior)
    bool       isInterior{false};    ///< parentCell->IsInteriorCell()

    /// Returns true if this fingerprint represents a meaningful location
    /// change compared to @p other — i.e. a scan should be triggered.
    bool IsMeaningfullyDifferentFrom(const LocationFingerprint& other) const {
        return isInterior       != other.isInterior       ||
               worldspaceFormID != other.worldspaceFormID ||
               locationFormID   != other.locationFormID;
    }
};

/// Triggers an LLM "ostimnet_scan_location" evaluation when the player enters
/// a new cell or via manual hotkey press.
///
/// Listens to BGSActorCellEvent (sourced from PlayerCharacter; fires on every
/// cell boundary crossing, including outdoor transitions).
///
/// After a configurable delay (to allow NPCs to fully spawn and AI to
/// initialise), sends the prompt to SkyrimNet and fires the Papyrus mod event
/// "ostimnet_location_scan_result" with the result:
///   numArg = 1.0 — participants found; strArg = full result JSON
///   numArg = 2.0 — no willing participants (reason-only JSON)
///   numArg = 0.0 — LLM error or unparseable response
///
/// Scans are suppressed for pure outdoor tile crossings: they only fire when
/// the location fingerprint (named location, interior flag, worldspace) changes.
/// Manual hotkey scans always fire regardless of fingerprint.
///
/// Skips the scan silently if any OStim thread is already active.
class LocationScanService
    : public RE::BSTEventSink<RE::BGSActorCellEvent> {
public:
    static LocationScanService& GetSingleton() {
        static LocationScanService instance;
        return instance;
    }

    /// Register as a BGSActorCellEvent sink. Call from kPostLoadGame/kNewGame
    /// (PlayerCharacter must exist before adding the sink).
    void Register();

    /// Cancel any in-flight delayed scan and reset location tracking.
    /// Call from kPreLoadGame so stale scans never fire into a new session.
    void Reset();

    /// Schedule a scan after the configured delay for the initial game/load.
    /// TESActorLocationChangeEvent does not fire on save load, so call this
    /// from kPostLoadGame / kNewGame as a one-shot fallback.
    void OnGameReady();

    /// Cancel any pending delayed scan and run the LLM prompt immediately.
    /// Safe to call on the game thread from a hotkey handler.
    void TriggerManualScan();

protected:
    RE::BSEventNotifyControl ProcessEvent(
        const RE::BGSActorCellEvent* event,
        RE::BSTEventSource<RE::BGSActorCellEvent>*) override;

private:
    LocationScanService() = default;
    ~LocationScanService() { CancelDelay(); }

    LocationScanService(const LocationScanService&) = delete;
    LocationScanService& operator=(const LocationScanService&) = delete;

    /// Start (or restart) the delay timer. The delay thread signals AddTask
    /// to run RunScan on the game thread after Config::LocationScanDelay() seconds.
    void ScheduleScan();

    /// Signal the delay thread to exit and block until it has.
    void CancelDelay();

    /// Called on the game thread (via AddTask). Builds context and sends the
    /// LLM prompt via PublicSendCustomPromptToLLM.
    void RunScan(bool force = false);

    /// Build the JSON context object injected into the prompt template.
    std::string BuildContextJson() const;

    /// Returns true if the player's current location type is enabled for
    /// scanning per the Config location-type filter settings.
    bool IsLocationTypeAllowed() const;

    /// Snapshot the player's current semantic location into a fingerprint.
    /// Must be called on the game thread while PlayerCharacter is valid.
    LocationFingerprint SnapshotFingerprint() const;

    // -------------------------------------------------------------------------
    // Delay-thread synchronisation
    // -------------------------------------------------------------------------
    std::mutex              _mutex;
    std::condition_variable _cv;
    std::thread             _delayThread;
    bool                    _cancelFlag{false};

    /// Monotonically-increasing counter. Incremented on each ScheduleScan/Cancel
    /// so AddTask closures from superseded delay threads are discarded.
    std::atomic<uint64_t> _generation{0};

    /// True once the first post-load scan has been skipped.
    bool _initialScanDone{false};

    /// Time point of the last completed automatic scan. Used to enforce the
    /// LocationScanCooldown setting between consecutive automatic scans.
    std::chrono::steady_clock::time_point _lastScanTime{};

    /// Fingerprint of the location at which the last scan was triggered.
    /// Cell-enter events that do not change this fingerprint are silently
    /// suppressed (e.g. crossing outdoor tile boundaries in the wilderness).
    LocationFingerprint _lastFingerprint{};

    // -------------------------------------------------------------------------
    // Cached keyword pointers — resolved once at kDataLoaded via CacheKeywords().
    // Using BGSKeyword* instead of HasKeywordString() avoids dereferencing the
    // BSFixedString internal data pointer, which can be corrupted in SkyrimVR
    // (root cause of the HasKeywordString AVX2 access-violation crash).
    // -------------------------------------------------------------------------

    /// Look up and cache all location-type keyword pointers.
    /// Must be called on the game thread after kDataLoaded.
    void CacheKeywords();

    RE::BGSKeyword* _kwPlayerHouse{nullptr};
    RE::BGSKeyword* _kwTown{nullptr};
    RE::BGSKeyword* _kwCity{nullptr};
    RE::BGSKeyword* _kwSettlement{nullptr};
    RE::BGSKeyword* _kwInn{nullptr};
    RE::BGSKeyword* _kwGuild{nullptr};
    RE::BGSKeyword* _kwDwelling{nullptr};
    RE::BGSKeyword* _kwHouse{nullptr};
    RE::BGSKeyword* _kwDungeon{nullptr};
};

}  // namespace OStimNet
