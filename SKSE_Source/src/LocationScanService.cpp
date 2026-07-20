#include "PCH.h"

#include "LocationScanService.h"
#include "Config.h"
#include "KeywordUtils.h"
#include "ModEventDispatch.h"
#include "SkyrimNetIntegration.h"
#include "ThreadRegistry.h"

#include <nlohmann/json.hpp>

namespace OStimNet {

void LocationScanService::Register() {
    CacheKeywords();  // resolve keyword pointers before we start receiving events

    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) {
        SKSE::log::warn("LocationScanService: PlayerCharacter unavailable, could not register event sink");
        return;
    }

    RE::BSTEventSource<RE::BGSActorCellEvent>* source = nullptr;
    if (REL::Module::IsVR()) {
        source = reinterpret_cast<RE::BSTEventSource<RE::BGSActorCellEvent>*>(
            reinterpret_cast<uintptr_t>(player) + 0x2E8);
    } else {
        source = player->AsBGSActorCellEventSource();
    }

    if (source) {
        source->AddEventSink(this);
        SKSE::log::info("LocationScanService: registered for BGSActorCellEvent");
    } else {
        SKSE::log::warn("LocationScanService: BGSActorCellEvent source unavailable, could not register event sink");
    }
}

void LocationScanService::Reset() {
    CancelDelay();
    _initialScanDone = false;
    _lastScanTime = {};
    _lastFingerprint = {};
    SKSE::log::info("LocationScanService: reset");
}

void LocationScanService::OnGameReady() {
    if (Config::GetSingleton().LocationScanEnabled()) {
        SKSE::log::info("LocationScanService: OnGameReady — skipping initial scan");
        _initialScanDone = false;  // will be set true when the first real scan fires
    }
}

void LocationScanService::TriggerManualScan() {
    SKSE::log::info("LocationScanService: manual scan triggered via hotkey");
    CancelDelay();
    RunScan(true);
}

// -----------------------------------------------------------------------------
// TESActorLocationChangeEvent sink
// -----------------------------------------------------------------------------

RE::BSEventNotifyControl LocationScanService::ProcessEvent(
    const RE::BGSActorCellEvent* event,
    RE::BSTEventSource<RE::BGSActorCellEvent>*) {
    if (!event) return RE::BSEventNotifyControl::kContinue;
    if (!Config::GetSingleton().LocationScanEnabled()) return RE::BSEventNotifyControl::kContinue;

    // Only care about cell-enter events.
    if (event->flags == RE::BGSActorCellEvent::CellFlag::kLeave)
        return RE::BSEventNotifyControl::kContinue;

    // Only care about the player.
    auto* player = RE::PlayerCharacter::GetSingleton();
    RE::Actor* actor = event->actor.get().get();
    if (!player || !actor || actor != static_cast<RE::Actor*>(player))
        return RE::BSEventNotifyControl::kContinue;

    // Skip the very first cell event that fires on save-load (location hasn't
    // changed; we capture the baseline fingerprint here instead).
    if (!_initialScanDone) {
        _initialScanDone = true;
        _lastFingerprint = SnapshotFingerprint(event->cellID);
        SKSE::log::info(
            "LocationScanService: first post-load cell (0x{:08X}) event — captured baseline "
            "(loc=0x{:08X} ws=0x{:08X} interior={})",
            event->cellID,
            _lastFingerprint.locationFormID,
            _lastFingerprint.worldspaceFormID,
            _lastFingerprint.isInterior);
        return RE::BSEventNotifyControl::kContinue;
    }

    // Snapshot current semantic location and compare to the last scan.
    LocationFingerprint current = SnapshotFingerprint(event->cellID);
    if (!current.IsMeaningfullyDifferentFrom(_lastFingerprint)) {
        SKSE::log::debug(
            "LocationScanService: cell 0x{:08X} — fingerprint unchanged, scan suppressed "
            "(loc=0x{:08X} ws=0x{:08X} interior={})",
            event->cellID,
            current.locationFormID,
            current.worldspaceFormID,
            current.isInterior);
        return RE::BSEventNotifyControl::kContinue;
    }

    SKSE::log::info(
        "LocationScanService: cell 0x{:08X} meaningful location change detected "
        "(loc 0x{:08X}->0x{:08X}  ws 0x{:08X}->0x{:08X}  interior {}->{}) scheduling scan",
        event->cellID,
        _lastFingerprint.locationFormID, current.locationFormID,
        _lastFingerprint.worldspaceFormID, current.worldspaceFormID,
        _lastFingerprint.isInterior, current.isInterior);

    // Advance the baseline NOW so that future cell events compare against the
    // actual current location — not the one from the last *completed* scan.
    // If the scan is later filtered out (e.g. settlement) or cancelled, the
    // fingerprint is already correct and won't produce phantom "changes".
    _lastFingerprint = current;

    // Early-out: if the location type is already known to be excluded, skip
    // scheduling the delay thread entirely. GetCurrentLocation() can be null
    // or unresolved immediately after a cell transition, so RunScan() retains
    // its own authoritative check to catch cases that resolve during the delay.
    if (!IsLocationTypeAllowed()) {
        SKSE::log::debug(
            "LocationScanService: location type excluded at cell-enter, scan suppressed");
        return RE::BSEventNotifyControl::kContinue;
    }

    ScheduleScan();
    return RE::BSEventNotifyControl::kContinue;
}

// -----------------------------------------------------------------------------
// Delay logic
// -----------------------------------------------------------------------------

void LocationScanService::ScheduleScan() {
    // Cancel any existing delay first (blocks until old thread exits — fast
    // because we signal it immediately).
    CancelDelay();

    int      delaySec = Config::GetSingleton().LocationScanDelay();
    uint64_t gen      = ++_generation;
    SKSE::log::info("LocationScanService: scan scheduled in {} second(s) (gen={})", delaySec, gen);

    std::lock_guard<std::mutex> lk(_mutex);
    _cancelFlag = false;
    _delayThread = std::thread([this, delaySec, gen]() {
        std::unique_lock<std::mutex> threadLock(_mutex);
        _cv.wait_for(threadLock, std::chrono::seconds(delaySec),
                     [this]() { return _cancelFlag; });
        bool wasCancelled = _cancelFlag;
        threadLock.unlock();  // release before AddTask so CancelDelay can join cleanly

        if (wasCancelled) {
            SKSE::log::debug("LocationScanService: delay cancelled (gen={})", gen);
            return;
        }

        if (auto* taskIF = SKSE::GetTaskInterface()) {
            taskIF->AddTask([this, gen]() {
                // Discard if a newer scan was scheduled between AddTask and execution.
                if (gen != _generation.load(std::memory_order_relaxed)) {
                    SKSE::log::debug("LocationScanService: stale scan discarded (gen={})", gen);
                    return;
                }
                RunScan();
            });
        }
    });
}

void LocationScanService::CancelDelay() {
    {
        std::lock_guard<std::mutex> lk(_mutex);
        _cancelFlag = true;
    }
    _cv.notify_all();
    if (_delayThread.joinable()) {
        _delayThread.join();
    }
}

// -----------------------------------------------------------------------------
// Fingerprint helper
// -----------------------------------------------------------------------------

LocationFingerprint LocationScanService::SnapshotFingerprint(RE::FormID cellID) const {
    LocationFingerprint fp;

    RE::TESObjectCELL* cell = nullptr;
    if (cellID != 0) {
        cell = RE::TESForm::LookupByID<RE::TESObjectCELL>(cellID);
    }
    
    if (!cell) {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (player) {
            cell = player->GetParentCell();
        }
    }

    if (!cell) return fp;

    if (auto* loc = cell->GetLocation())
        fp.locationFormID = loc->GetFormID();

    fp.isInterior = cell->IsInteriorCell();
    if (!fp.isInterior) {
        if (auto* ws = cell->GetRuntimeData().worldSpace)
            fp.worldspaceFormID = ws->GetFormID();
    }

    return fp;
}

// -----------------------------------------------------------------------------
// LLM scan
// -----------------------------------------------------------------------------

void LocationScanService::RunScan(bool force) {
    if (!Config::GetSingleton().LocationScanEnabled()) {
        SKSE::log::debug("LocationScanService: scan disabled in config, aborting");
        return;
    }

    if (!force) {
        int cooldownSec = Config::GetSingleton().LocationScanCooldown();
        if (cooldownSec > 0 && _lastScanTime.time_since_epoch().count() != 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - _lastScanTime).count();
            if (elapsed < cooldownSec) {
                SKSE::log::info("LocationScanService: cooldown active ({}/{}s elapsed), skipping scan",
                                elapsed, cooldownSec);
                return;
            }
        }
    }

    if (!IsLocationTypeAllowed()) {
        SKSE::log::info("LocationScanService: location type excluded by filter, skipping scan");
        return;
    }

    if (ThreadRegistry::GetSingleton().HasActiveThreads()) {
        SKSE::log::info("LocationScanService: OStim threads active, skipping location scan");
        return;
    }

    // Build a name→FormID snapshot of the player and all nearby actors.
    // Captured by value into the LLM callback so name resolution is safe
    // without calling RE:: functions from the worker thread.
    std::map<std::string, RE::FormID> nameToFormID;
    if (auto* player = RE::PlayerCharacter::GetSingleton()) {
        const char* pName = player->GetName();
        if (pName && pName[0] != '\0')
            nameToFormID[pName] = player->GetFormID();

        if (auto* pl = RE::ProcessLists::GetSingleton()) {
            const RE::NiPoint3 playerPos = player->GetPosition();
            constexpr float kRadiusSq = 2000.0f * 2000.0f;
            for (auto& handle : pl->highActorHandles) {
                RE::Actor* actor = handle.get().get();
                if (!actor || actor == static_cast<RE::Actor*>(player)) continue;
                const RE::NiPoint3 pos = actor->GetPosition();
                float dx = pos.x - playerPos.x, dy = pos.y - playerPos.y, dz = pos.z - playerPos.z;
                if (dx * dx + dy * dy + dz * dz > kRadiusSq) continue;
                const char* name = actor->GetName();
                if (name && name[0] != '\0')
                    nameToFormID[name] = actor->GetFormID();
            }
        }
    }
    SKSE::log::info("LocationScanService: built name snapshot with {} actor(s)", nameToFormID.size());

    // Re-snapshot fingerprint immediately before the LLM call to pick up any
    // location refinement that happened during the delay (e.g. GetCurrentLocation()
    // returning a more-specific value after NPCs finish spawning).  In the normal
    // case this is already up-to-date from ProcessEvent, but the manual scan path
    // and any edge cases are covered here too.
    _lastFingerprint = SnapshotFingerprint();

    std::string contextJson = BuildContextJson();
    _lastScanTime = std::chrono::steady_clock::now();
    SkyrimNetIntegration::EvaluateLocationScan(contextJson, nameToFormID, _generation.load(std::memory_order_relaxed));
}

// -----------------------------------------------------------------------------
// Context builder
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Keyword cache
// -----------------------------------------------------------------------------

void LocationScanService::CacheKeywords() {
    auto Lookup = [](const char* editorID) -> RE::BGSKeyword* {
        try {
            auto* kw = RE::TESForm::LookupByEditorID<RE::BGSKeyword>(editorID);
            if (!kw) {
                SKSE::log::warn("LocationScanService: Keyword '{}' not found via EditorID lookup.", editorID);
            }
            return kw;
        } catch (const std::exception& e) {
            SKSE::log::error("LocationScanService: Exception looking up '{}': {}", editorID, e.what());
            return nullptr;
        } catch (...) {
            SKSE::log::error("LocationScanService: Unknown exception looking up '{}'", editorID);
            return nullptr;
        }
    };

    _kwPlayerHouse = Lookup("LocTypePlayerHouse");
    _kwTown        = Lookup("LocTypeTown");
    _kwCity        = Lookup("LocTypeCity");
    _kwSettlement  = Lookup("LocTypeSettlement");
    _kwInn         = Lookup("LocTypeInn");
    _kwGuild       = Lookup("LocTypeGuild");
    _kwDwelling    = Lookup("LocTypeDwelling");
    _kwHouse       = Lookup("LocTypeHouse");
    _kwDungeon     = Lookup("LocTypeDungeon");

    SKSE::log::info(
        "LocationScanService: CacheKeywords — PlayerHouse={} Town={} City={} Settlement={} "
        "Inn={} Guild={} Dwelling={} House={} Dungeon={}",
        _kwPlayerHouse != nullptr, _kwTown != nullptr, _kwCity != nullptr,
        _kwSettlement  != nullptr, _kwInn  != nullptr, _kwGuild != nullptr,
        _kwDwelling    != nullptr, _kwHouse != nullptr, _kwDungeon != nullptr);
}

// -----------------------------------------------------------------------------
// Location type filter
// -----------------------------------------------------------------------------

bool LocationScanService::IsLocationTypeAllowed() const {
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return true;

    auto* loc = player->GetCurrentLocation();
    if (!loc) {
        bool allow = Config::GetSingleton().LocationScanInOther();
        SKSE::log::debug("LocationScanService: no named location — scanInOther={}", allow);
        return allow;
    }

    const auto& cfg = Config::GetSingleton();

    if (KeywordUtils::FormHasKeyword(loc, _kwPlayerHouse)) {
        bool allow = cfg.LocationScanInPlayerHomes();
        SKSE::log::debug("LocationScanService: location is player home — scanInPlayerHomes={}", allow);
        return allow;
    }

    if (KeywordUtils::FormHasKeyword(loc, _kwTown)       ||
        KeywordUtils::FormHasKeyword(loc, _kwCity)       ||
        KeywordUtils::FormHasKeyword(loc, _kwSettlement)) {
        bool allow = cfg.LocationScanInSettlements();
        SKSE::log::debug("LocationScanService: location is settlement — scanInSettlements={}", allow);
        return allow;
    }

    if (KeywordUtils::FormHasKeyword(loc, _kwInn)) {
        bool allow = cfg.LocationScanInInns();
        SKSE::log::debug("LocationScanService: location is inn — scanInInns={}", allow);
        return allow;
    }

    if (KeywordUtils::FormHasKeyword(loc, _kwGuild)) {
        bool allow = cfg.LocationScanInGuilds();
        SKSE::log::debug("LocationScanService: location is guild — scanInGuilds={}", allow);
        return allow;
    }

    if (KeywordUtils::FormHasKeyword(loc, _kwDwelling) || KeywordUtils::FormHasKeyword(loc, _kwHouse)) {
        bool allow = cfg.LocationScanInDwellings();
        SKSE::log::debug("LocationScanService: location is dwelling — scanInDwellings={}", allow);
        return allow;
    }

    if (KeywordUtils::FormHasKeyword(loc, _kwDungeon)) {
        bool allow = cfg.LocationScanInDungeons();
        SKSE::log::debug("LocationScanService: location is dungeon — scanInDungeons={}", allow);
        return allow;
    }

    bool allow = cfg.LocationScanInOther();
    SKSE::log::debug("LocationScanService: location type unrecognised — scanInOther={}", allow);
    return allow;
}

std::string LocationScanService::BuildContextJson() const {
    std::string locationName = "Unknown";

    if (auto* player = RE::PlayerCharacter::GetSingleton()) {
        if (auto* loc = player->GetCurrentLocation()) {
            const char* name = loc->GetFullName();
            if (name && name[0] != '\0') locationName = name;
        } else if (auto* cell = player->GetParentCell()) {
            const char* name = cell->GetFullName();
            if (name && name[0] != '\0') locationName = name;
        }
    }

    nlohmann::json ctx;
    ctx["location"]              = locationName;
    ctx["enableAggressiveIntent"] = Config::GetSingleton().EnableAggressiveIntent();
    return ctx.dump();
}

}  // namespace OStimNet
