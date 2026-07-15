#include "PCH.h"

#include "Config.h"
#include "SkyrimNetIntegration.h"

#include <filesystem>
#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>

namespace OStimNet {

// -----------------------------------------------------------------------------
// Manifest defaults loader
// -----------------------------------------------------------------------------

void Config::LoadManifestDefaults() {
    const std::filesystem::path manifestPath =
        "Data/SKSE/Plugins/SkyrimNet/config/plugins/OStimNet/manifest.yaml";

    if (!std::filesystem::exists(manifestPath)) {
        SKSE::log::warn("OStimNet: manifest.yaml not found at '{}', using hardcoded defaults",
                        manifestPath.string());
        return;
    }

    try {
        YAML::Node root   = YAML::LoadFile(manifestPath.string());
        YAML::Node fields = root["schema"]["fields"];
        if (!fields || !fields.IsSequence()) {
            SKSE::log::warn("OStimNet: manifest.yaml has no schema.fields sequence");
            return;
        }

        m_defaults.clear();
        for (const auto& field : fields) {
            if (!field["path"] || !field["defaultValue"]) continue;
            m_defaults[field["path"].as<std::string>()] =
                field["defaultValue"].as<std::string>();
        }

        SKSE::log::info("OStimNet: loaded {} default(s) from manifest.yaml", m_defaults.size());
    } catch (const YAML::Exception& e) {
        SKSE::log::error("OStimNet: failed to parse manifest.yaml: {}", e.what());
    }
}

std::string Config::GetValue(const char* path, const char* hardcodedFallback) const {
    auto it  = m_defaults.find(path);
    const char* def = (it != m_defaults.end()) ? it->second.c_str() : hardcodedFallback;
    return SkyrimNetIntegration::GetPluginConfigValue("OStimNet", path, def);
}

// -----------------------------------------------------------------------------
// Nearby Actors
// -----------------------------------------------------------------------------

float Config::NearbyActorsRadius() const {
    try { return std::stof(GetValue("tton.nearbyActors.radius", "2000")); }
    catch (...) { return 2000.f; }
}


// -----------------------------------------------------------------------------
// Settings
// -----------------------------------------------------------------------------

bool Config::EnableThreadPhases() const {
    std::string val = GetValue("tton.settings.enableThreadPhases", "true");
    return val == "true" || val == "1";
}

bool Config::UseCustomDescriptions() const {
    std::string val = GetValue("tton.settings.useCustomDescriptions", "true");
    return val == "true" || val == "1";
}

std::string Config::GmLlmVariant() const {
    return GetValue("tton.gm.gmLlmVariant", "gamemaster_evaluation");
}

bool Config::ScheduledEvalPlayerThread() const {
    std::string val = GetValue("tton.gameMaster.scheduledEvalPlayerThread", "false");
    return val == "true" || val == "1";
}

bool Config::ScheduledEvalNpcThreads() const {
    std::string val = GetValue("tton.gameMaster.scheduledEvalNpcThreads", "true");
    return val == "true" || val == "1";
}

int Config::ScheduledEvalIntervalSeconds() const {
    try { return std::stoi(GetValue("tton.gameMaster.scheduledEvalIntervalSeconds", "60")); }
    catch (...) { return 60; }
}

float Config::ProximityPauseRadius() const {
    try { return std::stof(GetValue("tton.settings.proximityPauseRadius", "200")); }
    catch (...) { return 200.f; }
}

bool Config::CommentsPrioritizeNearestThread() const {
    std::string val = GetValue("tton.settings.commentsPrioritizeNearestThread", "true");
    return val == "true" || val == "1";
}

int Config::CommentGenderPriority() const {
    try { return std::stoi(GetValue("tton.settings.commentGenderPriority", "50")); }
    catch (...) { return 50; }
}

bool Config::EnableSpectators() const {
    std::string val = GetValue("tton.settings.enableSpectators", "true");
    return val == "true" || val == "1";
}

int Config::SpectatorScanIntervalSeconds() const {
    try { return std::stoi(GetValue("tton.spectators.scanIntervalSeconds", "5")); }
    catch (...) { return 5; }
}

float Config::SpectatorScanRadius() const {
    try { return std::stof(GetValue("tton.spectators.scanRadius", "3000")); }
    catch (...) { return 3000.f; }
}

int Config::OstimSceneChangeDebounce() const {
    try { return std::stoi(GetValue("tton.settings.ostimSceneChangeDebounce", "2")); }
    catch (...) { return 2; }
}

int Config::ClimaxDebounceSeconds() const {
    try { return std::stoi(GetValue("tton.settings.climaxDebounce", "2")); }
    catch (...) { return 2; }
}

int Config::SpeedChangeDebounceSeconds() const {
    try { return std::stoi(GetValue("tton.settings.speedChangeDebounce", "5")); }
    catch (...) { return 5; }
}

int Config::ActionsCooldown() const {
    try { return std::stoi(GetValue("tton.settings.actionsCooldown", "20")); }
    catch (...) { return 20; }
}

int Config::ActionsDeclineCooldown() const {
    try { return std::stoi(GetValue("tton.settings.actionsDeclineCooldown", "40")); }
    catch (...) { return 40; }
}

bool Config::ShowConfirmationModalForAggressiveIntent() const {
    std::string val = GetValue("tton.settings.showConfirmationModalForAggressiveIntent", "false");
    return val == "true" || val == "1";
}

bool Config::ShowConfirmationModalWithNoPlayer() const {
    std::string val = GetValue("tton.settings.showConfirmationModalWithNoPlayer", "false");
    return val == "true" || val == "1";
}

bool Config::EnableAggressiveIntent() const {
    std::string val = GetValue("tton.settings.enableAggressiveIntent", "false");
    return val == "true" || val == "1";
}

// -----------------------------------------------------------------------------
// Controls
// -----------------------------------------------------------------------------

bool Config::IsMuted() const {
    if (m_muteOverride.has_value()) return *m_muteOverride;
    std::string val = GetValue("tton.controls.toggleMute", "false");
    return val == "true" || val == "1";
}

void Config::ToggleMuteSession() {
    m_muteOverride = !m_muteOverride.value_or(false);
    SKSE::log::info("OStimNet: mute toggled (session override = {})", *m_muteOverride);
    RE::SendHUDMessage::ShowHUDMessage(*m_muteOverride ? "OStimNet: all actors muted" : "OStimNet: all actors unmuted");
}

void Config::ResetMuteOverride() {
    m_muteOverride.reset();
    SKSE::log::info("OStimNet: mute session override cleared (config value takes effect).");
}

void Config::InitFromConfig() {
    LoadManifestDefaults();

    std::string val = GetValue("tton.controls.toggleMute", "false");
    m_muteOverride  = (val == "true" || val == "1");
    SKSE::log::info("OStimNet: mute initialised from config (muted = {})", *m_muteOverride);

    ApplyLogLevel();
}

void Config::ApplyLogLevel() const {
    std::string level = GetValue("tton.settings.logLevel", "info");
    spdlog::level::level_enum spdLevel = spdlog::level::info;
    if      (level == "debug") spdLevel = spdlog::level::debug;
    else if (level == "warn")  spdLevel = spdlog::level::warn;
    else if (level == "error") spdLevel = spdlog::level::err;
    spdlog::default_logger()->set_level(spdLevel);
    SKSE::log::info("OStimNet: log level set to '{}'", level);
}

int Config::ToggleMuteHotkey() const {
    try { return std::stoi(GetValue("tton.controls.toggleMuteHotkey", "0")); }
    catch (...) { return 0; }
}

// -----------------------------------------------------------------------------
// Action Confirmations
// -----------------------------------------------------------------------------

bool Config::ConfirmStartNewSex() const {
    std::string val = GetValue("tton.confirmations.startNewSex", "true");
    return val == "true" || val == "1";
}

bool Config::ConfirmJoinOngoingSex() const {
    std::string val = GetValue("tton.confirmations.joinOngoingSex", "true");
    return val == "true" || val == "1";
}

bool Config::ConfirmInviteToYourSex() const {
    std::string val = GetValue("tton.confirmations.inviteToYourSex", "true");
    return val == "true" || val == "1";
}

bool Config::ConfirmChangeSexScenePosition() const {
    std::string val = GetValue("tton.confirmations.changeSexScenePosition", "true");
    return val == "true" || val == "1";
}

bool Config::ConfirmChangeSexSceneIntent() const {
    std::string val = GetValue("tton.confirmations.changeSexSceneIntent", "true");
    return val == "true" || val == "1";
}

bool Config::ConfirmChangeSexScenePace() const {
    std::string val = GetValue("tton.confirmations.changeSexScenePace", "true");
    return val == "true" || val == "1";
}

bool Config::ConfirmStopSex() const {
    std::string val = GetValue("tton.confirmations.stopSex", "true");
    return val == "true" || val == "1";
}

bool Config::ConfirmStartCareScene() const {
    std::string val = GetValue("tton.confirmations.startCareScene", "true");
    return val == "true" || val == "1";
}

// -----------------------------------------------------------------------------
// Location Scan
// -----------------------------------------------------------------------------

bool Config::LocationScanEnabled() const {
    std::string val = GetValue("tton.locationScan.enabled", "true");
    return val == "true" || val == "1";
}

int Config::LocationScanDelay() const {
    try { return std::stoi(GetValue("tton.locationScan.delaySeconds", "8")); }
    catch (...) { return 8; }
}

int Config::LocationScanCooldown() const {
    try { return std::stoi(GetValue("tton.locationScan.cooldownSeconds", "120")); }
    catch (...) { return 120; }
}

bool Config::LocationScanInSettlements() const {
    std::string v = GetValue("tton.locationScan.scanInSettlements", "true");
    return v == "true" || v == "1";
}

bool Config::LocationScanInInns() const {
    std::string v = GetValue("tton.locationScan.scanInInns", "true");
    return v == "true" || v == "1";
}

bool Config::LocationScanInGuilds() const {
    std::string v = GetValue("tton.locationScan.scanInGuilds", "true");
    return v == "true" || v == "1";
}

bool Config::LocationScanInPlayerHomes() const {
    std::string v = GetValue("tton.locationScan.scanInPlayerHomes", "true");
    return v == "true" || v == "1";
}

bool Config::LocationScanInDwellings() const {
    std::string v = GetValue("tton.locationScan.scanInDwellings", "true");
    return v == "true" || v == "1";
}

bool Config::LocationScanInDungeons() const {
    std::string v = GetValue("tton.locationScan.scanInDungeons", "false");
    return v == "true" || v == "1";
}

bool Config::LocationScanInOther() const {
    std::string v = GetValue("tton.locationScan.scanInOther", "true");
    return v == "true" || v == "1";
}

int Config::LocationScanHotkey() const {
    try { return std::stoi(GetValue("tton.controls.locationScanHotkey", "0")); }
    catch (...) { return 0; }
}

// -----------------------------------------------------------------------------
// Undressing
// -----------------------------------------------------------------------------

bool Config::EnableUndressingPlayer() const {
    std::string val = GetValue("tton.undress.enableUndressingPlayer", "true");
    return val == "true" || val == "1";
}

bool Config::EnableUndressingNpc() const {
    std::string val = GetValue("tton.undress.enableUndressingNpc", "true");
    return val == "true" || val == "1";
}

std::string Config::UndressingApproachPlayer() const {
    return GetValue("tton.undress.undressingApproachPlayer", "OStim");
}

std::string Config::UndressingApproachNpc() const {
    return GetValue("tton.undress.undressingApproachNpc", "OStim");
}

}  // namespace OStimNet

