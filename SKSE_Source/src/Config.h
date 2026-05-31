#pragma once
#include <optional>
#include <string>
#include <unordered_map>

namespace OStimNet {

/**
 * Runtime configuration backed by SkyrimNet's plugin config system.
 * Source: SKSE/Plugins/SkyrimNet/config/plugins/OStimNet/manifest.yaml
 *
 * Each getter calls PublicGetPluginConfigValue directly so values always
 * reflect the current state of the config (e.g., after the user edits it
 * in SkyrimNet's MCM/UI without reloading the game).
 */
struct Config {
    static Config& GetSingleton() {
        static Config instance;
        return instance;
    }

    // -------------------------------------------------------------------------
    // Settings
    // -------------------------------------------------------------------------

    /// When true, OStimNet tracks sexual encounter phases (undressing → foreplay → oral → sex).
    /// Phase lists are hardcoded per intent; intents like dom/aggressive always start at sex.
    /// When false, scenes use the original random selection behaviour.
    bool EnableThreadPhases() const;

    /// When true, scene descriptions from custom node YAML files are used.
    /// When false, only auto-generated descriptions are produced.
    bool UseCustomDescriptions() const;

    /// When true, ostimnet_scene_change only fires for the nearest thread to the player.
    /// If the player is in a thread, that thread always takes priority.
    bool CommentsPrioritizeNearestThread() const;

    /// Speaker gender probability for scene change events (0 = always male, 100 = always female).
    int CommentGenderPriority() const;

    /// Cooldown in seconds per action per originator actor before the same action can be shown again.
    int ActionsCooldown() const;

    /// Cooldown in seconds per action per originator actor when the action was declined by the player.
    int ActionsDeclineCooldown() const;

    /// When true, the spectator scanner runs while OStim threads are active,
    /// looking for nearby actors with romantic/marital relationships to actors in those threads.
    bool EnableSpectators() const;

    /// How often (in seconds) to scan for new spectators while scenes are running.
    int SpectatorScanIntervalSeconds() const;

    /// Search radius in game units to find potential spectators.
    float SpectatorScanRadius() const;

    /// When OStimNet receives a scene change, wait this many seconds of silence before firing.
    int OstimSceneChangeDebounce() const;

    /// Wait this many seconds after climax before dispatching the batch.
    int ClimaxDebounceSeconds() const;

    /// Wait this many seconds after speed changes before dispatching the change.
    int SpeedChangeDebounceSeconds() const;

    /// When true, the confirmation modal is shown even for aggressive/dom intent actions.
    /// When false, aggressive intent skips the modal and auto-accepts.
    bool ShowConfirmationModalForAggressiveIntent() const;

    /// When true, the confirmation modal is shown even when the player is not a
    /// participant in the scene (NPC-only scenes). When false, such scenes auto-accept.
    bool ShowConfirmationModalWithNoPlayer() const;

    /// When true, OStimNet considers aggressive intent when selecting scenes.
    bool EnableAggressiveIntent() const;

    // -------------------------------------------------------------------------
    // Controls
    // -------------------------------------------------------------------------

    /// When true, all NPC comments are muted (events are still logged, but skipTrigger is set to true).
    bool IsMuted() const;

    /// VK code for the toggle-mute hotkey. 0 means not configured.
    int ToggleMuteHotkey() const;

    /// Toggles the session-level mute override based on the current effective mute state.
    /// Persists until ResetMuteOverride() is called (e.g., on game reload).
    void ToggleMuteSession();

    /// Clears the session-level mute override so IsMuted() falls back to the config value.
    void ResetMuteOverride();

    /// Initialises m_muteOverride from the config value. Call on every game/new-game load.
    void InitFromConfig();

    /// Reads tton.settings.logLevel from config and applies it to the spdlog default logger.
    /// Safe to call at any time after SkyrimNet API is connected.
    void ApplyLogLevel() const;

    // -------------------------------------------------------------------------
    // Location Scan
    // -------------------------------------------------------------------------

    /// When true, OStimNet scans nearby NPCs for willing participants after
    /// every location change and fires ostimnet_location_scan_result.
    bool LocationScanEnabled() const;

    /// Seconds to wait after a loading screen closes before running the scan.
    /// Allows time for NPCs to fully spawn and AI to initialise.
    int LocationScanDelay() const;

    /// Minimum seconds that must pass between two automatic location scans.
    /// A value of 0 disables the cooldown.
    int LocationScanCooldown() const;

    // Location type filters — each controls whether the scan runs when the
    // player is in that category of location.
    bool LocationScanInSettlements() const;  // towns, cities, settlements
    bool LocationScanInInns() const;
    bool LocationScanInGuilds() const;
    bool LocationScanInPlayerHomes() const;  // LocTypePlayerHouse
    bool LocationScanInDwellings() const;   // LocTypeDwelling or LocTypeHouse
    bool LocationScanInDungeons() const;
    bool LocationScanInOther() const;  // unrecognised / no location

    /// VK code for the manual location-scan hotkey. 0 means not configured.
    int LocationScanHotkey() const;

    // -------------------------------------------------------------------------
    // Nearby Actors
    // -------------------------------------------------------------------------

    /// Search radius in game units used by get_nearby_actors_in_ostim and
    /// get_nearby_actors_not_in_ostim decorators.
    /// Default: 2000 units (≈ 28 m in Skyrim scale).
    float NearbyActorsRadius() const;

    // -------------------------------------------------------------------------
    // Game Master
    // -------------------------------------------------------------------------

    /// SkyrimNet LLM variant used for Game Master evaluations.
    std::string GmLlmVariant() const;

    bool ScheduledEvalPlayerThread() const;
    bool ScheduledEvalNpcThreads() const;
    int ScheduledEvalIntervalSeconds() const;
    float ProximityPauseRadius() const;

    // -------------------------------------------------------------------------
    // Undressing
    // -------------------------------------------------------------------------

    /// When false the Undressing phase is skipped for threads that include the player.
    bool EnableUndressingPlayer() const;

    /// When false the Undressing phase is skipped for NPC-only threads.
    bool EnableUndressingNpc() const;

    /// Undressing approach for player threads ("OStim", "OARE", "OSA", "OSAFirstFast").
    std::string UndressingApproachPlayer() const;

    /// Undressing approach for NPC-only threads.
    std::string UndressingApproachNpc() const;

private:
    std::optional<bool> m_muteOverride;
    std::unordered_map<std::string, std::string> m_defaults;

    // Parses manifest.yaml and populates m_defaults.
    void LoadManifestDefaults();

    // Returns GetPluginConfigValue(path), using m_defaults as the fallback
    // (falls back to hardcodedFallback only if the path isn't in m_defaults).
    std::string GetValue(const char* path, const char* hardcodedFallback) const;
};

}  // namespace OStimNet
