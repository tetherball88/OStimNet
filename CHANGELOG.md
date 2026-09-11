# Changelog

## v2.4.2 - 2026-09-11

### Changed

- **SkyrimNet Beta 25 folder structure migration** — Migrated content delivery to SkyrimNet Beta 25's external plugin package structure:
  - **External layer package** — Content is now delivered as an external plugin bundle located at `Data/SKSE/Plugins/SkyrimNet/external/tetherball88.ostimnet/` with its own `manifest.json`.
  - **Action filenames** — Renamed all action YAML files to match their internal `name` field directly (`ManageIntimateScenes.yaml`, `StartIntimateScenes.yaml`, `StartNewSex.yaml`, etc.), dropping the legacy `tton_` filename prefix per Beta 25 requirements.
  - **Triggers & prompts reorganization** — Reorganized triggers (`triggers/`) and prompts (`prompts/`) into the plugin folder, dropping legacy `config/` prefixes.
  - **Legacy folder cleanup** — Cleaned up loose files from legacy directories (`prompts/`, `config/actions/`, `config/triggers/`), while preserving the plugin settings UI schema.

---

## v2.4.1 - 2026-09-03

### Fixed

- **Bed search logic** — Replaced legacy `OSANative.FindBed` in `ScanBestBeds` with `OFurniture.FindFurnitureOfType` (`doublebed` > `singlebed` > `bedroll` priority). This ensures beds currently in use, reserved by NPCs, or engaged in active OStim scenes are properly skipped.

---

## v2.4.0 - 2026-08-26

### Added

- **Manual player scene advance hotkey** — Added `Advance scene (player thread)` (`tton.controls.advancePlayerSceneHotkey`) hotkey setting under **Hot Keys**. Allows manually triggering an immediate Game Master LLM evaluation to advance the player's active scene without waiting for scheduled timers or NPC actions.
- **Dedicated solo/masturbation scene descriptions** — Added specialized prompt phrasing and context handling for solo intimate encounters (1-actor scenes / masturbation) across event notifications (`ostimnet_event_compact.prompt`, `ostimnet_event_verbose.prompt`) and system context (`0201_ostim_scenes.prompt`), cleanly handling start, position change, and stop events without fictitious secondary participants.
- **Metadata for solo animations** — Added animation and node metadata support for solo/masturbation scenes.

### Changed

- **CommonLibSSE-NG update** — Updated `CommonLibSSE-NG` submodule to support newer versions of Skyrim 1.7.99.
- **Furniture travel package (ESP)** — Updated the participant follow-to-furniture AI package in `TT_OstimNet.esp` to allow actors to open doors while walking to beds/furniture when scenes start.
- **Actor formatting fallback** — Updated `FormatActorList` in SKSE source to support custom fallbacks rather than hardcoding `"someone"`, preventing solo scenes from erroneously reporting non-existent secondary actors.
- **Pace change trigger updates** — Adjusted `tton_sex_pace_change.yaml` trigger conditions for smoother event handling.

---

## v2.3.1 - 2026-08-19

### Fixed

- **Player selection mode in location scans** — Fixed a template syntax issue in `ostimnet_scan_location.prompt` where an invalid fallback filter prevented the configured `Player selection mode` setting (`never` / `regular`) from being respected during automatic location scans.

---

## v2.3.0 - 2026-08-14

### Added

- **Player selection mode in location scans** — Added `Player selection mode` (`tton.locationScan.playerSelectionChance`) config setting (defaults to `reduced`) under **Location Scan** settings. Allows configuring player involvement during automatic location scans:
  - `regular`: Player is treated as a regular candidate alongside NPCs.
  - `reduced` (default): NPC selection chances are elevated over the player, prioritizing NPC-NPC interactions and evaluating the player last.
  - `never`: Completely excludes the player character from location scans and omits the player profile from the scan prompt context.
- **Dedicated pace change trigger** — Added separate `tton_sex_pace_change.yaml` trigger to handle `sex_pace_change` events independently from position changes.

### Changed

- **Position change trigger cleanup** — Streamlined `tton_sex_position_change.yaml` to strictly handle `sex_change` position shifts now that pace changes have a dedicated trigger.

---

## v2.2.2 - 2026-08-01

### Added

- **Humanoid filtering toggle for nearby actors** — Added `Nearby actors humanoid only` (`tton.nearbyActors.humanoidOnly`) config setting (defaults to `true`) under General settings. When disabled, non-humanoid actors (animals, creatures) are included when retrieving nearby actors.

### Fixed

- **SexLab integration global lookup** — Fixed SexLab player mode detection by correcting the Form ID for the SexLab OStim player global variable (`0x001FC803` -> `0x808` in `SkyrimNet_Sexlab.esp`).
- **Game paused detection in scheduled evaluations** — Scheduled evaluation service now checks `UI::GameIsPaused()` in addition to `Main::gameActive`. This ensures timers slide forward accurately while game menus are open and prevents scheduled evaluations from triggering while paused.

---

## v2.2.1 - 2026-07-21

### Fixed

- **Skyrim VR startup & location scan crash** — Resolved the root cause of `EXCEPTION_ACCESS_VIOLATION` crashes (`vpcmpeqq` instruction) in Skyrim VR during plugin load / location scan registration. Fixed the `CommonLibSSE-NG` event source offset for `BSTEventSource<BGSActorCellEvent>` in VR (`0x2D0` -> `0x2E8`), preventing `AddEventSink` from misinterpreting `SkyrimVR.exe` vtables as an array of listeners.
- **Hotkey input sink unregistering after load** — Re-registered `HotkeyInputSink` on game load (`kPostLoadGame` / `kNewGame`) so hotkeys (e.g. location scan, mute toggle) remain active after loading a save file.

### Changed

- **Actor name sanitization** — Trimmed accidental leading and trailing whitespace from actor display names.

---

## v2.2.0 - 2026-07-19

### Changed

- **Location change events** — Improved logic to check meaningful location change events by utilizing the cell from the event instead of the player's current cell.
- **Scene starting guardrails** — Improved guardrails for starting OStim scenes to ensure all NPCs are still in the same location.

### Fixed

- **Scenes starting in abandoned cells** — Improved handling of scenes in the preparation phase when the player changes locations. Fixed an issue where scenes would still initiate or actors would continue attempting to reach furniture if the player changed locations during asynchronous LLM evaluations. OStimNet now safely aborts asynchronous actions if the player leaves the area before the evaluation or furniture walk finishes.
- **VR crash in location scan** — Attempt #3 to fix Skyrim VR crashes during location scans.
- **Transactional intent prompts** — Fixed an issue with transactional prompt roles.

---

## v2.1.3 - 2026-07-18

### Fixed

- **VR crash in location scan (incomplete fix)** — The v2.1.2 fix replaced `HasKeywordString()` with `HasKeyword(BGSKeyword*)`, but both methods cause MSVC to emit a 32-byte AVX2 `vpcmpeqq` loop over the keyword array. In Skyrim VR the array buffer can sit at a page boundary, so the over-read crashes identically. `LocationScanService` now iterates `BGSKeywordForm::keywords` directly through a `volatile` pointer, forcing individual 8-byte scalar loads and making the over-read impossible.
- **VR crash risk in nearby-actor race filter** — `GetNearbyActors` was calling `race->HasKeywordString("ActorTypeNPC")`, which has the same AVX2 page-boundary risk. The check now uses a one-time cached `BGSKeyword*` and the same volatile scalar loop.

### Changed

- **Proximity pause radius** — Minimum value lowered from 100 to 0, allowing the pause to be fully disabled via the config UI.
- **Minor prompt tweaks.**

---

## v2.1.2 - 2026-07-17

### Fixed

- **VR crash in location scan** — `LocationScanService` could crash in Skyrim VR due to unsafe form lookups during the location scan. The service now guards against VR-incompatible paths and caches results to avoid repeated unsafe calls.
- **Stuck scene-advance evaluation on thread end** — When an OStim thread ended while a scheduled scene-advance evaluation was in flight, the evaluation loop would never terminate, leaving the scheduler permanently occupied. The event listener and integration layer now signal a clean cancellation when a thread ends.
- **Animals included in nearby-actor results** — Non-humanoid actors (animals, creatures) were being returned by `GetNearbyActors`, causing them to appear as valid scene candidates. Humanoid filtering is now applied before any actor is added to the result list.
- **Incorrect placeholder values in intent prompts** — `ostimnet_intent_dom_description_short.prompt` and `ostimnet_intent_transactional_description.prompt` contained wrong variable references that caused the wrong description text to be injected into the LLM context.

---

## v2.1.1 - 2026-07-16

### Fixed

- **Incorrect prompt variable in scene advance evaluation** — `ostimnet_evaluate_scene_advance.prompt` was calling `ostimnet_thread_phase(actors[0])` (passing the first actor object) instead of `ostimnet_thread_phase(0)`. This caused phase-aware scene advancement logic to fail at runtime.

---

## v2.1.0 - 2026-07-15

### Added

- **Per-action confirmation toggles** — Each NPC-initiated action now has its own on/off switch in the new **Action Confirmations** settings category. Disabling a toggle causes that action to auto-accept silently instead of showing a popup. Applies to:
  - Start new sex
  - Join ongoing sex
  - Invite to your sex
  - Change sex scene position
  - Change sex scene intent
  - Change sex scene pace
  - Stop sex
  - Start care scene
- All three confirmation gates must be open for a modal to appear: the per-action toggle, the NPC-only gate, and the aggressive-intent gate. If any gate is closed the action auto-accepts.

### Changed

- **Action Confirmations settings category** — "Confirmation modal for NPC-only scenes" and "Show confirmation modal for actions with aggressive intent" have been moved from the Actions / Intents categories into the new unified **Action Confirmations** category.
- **Simplified oral phase activity keyword** — The `blowjob` and `cunnilingus` phase keywords have been merged into a single `oral` keyword that maps to the full set of oral actions (blowjob, deepthroat, lickingpenis, lickingtesticles, cunnilingus, lickingvagina, anilingus). The `foreplay` keyword similarly now maps through a single label.
- **Phase-aware scene change prompt** — When a scene is in the foreplay or oral phase, the ChangeSexScenePosition action prompt now explicitly tells the LLM which actions belong to the current phase, which belong to the next phase, and that it may also skip ahead entirely.
- **Phase-aware scene advancement prompt** — The scheduled scene advance (Game Master) prompt received the same phase guidance: current phase actions, next natural phase, and the option to skip.
- **Activity lists in prompts** updated across evaluate-prestart-sexual, evaluate-scene-advance, scan-location, and change-sex-scene-position to use the consolidated `foreplay` / `oral` keywords instead of individual act names.

### Fixed

- **Inverted boolean logic** in `ShowConfirmationModalForAggressiveIntent` and `EnableAggressiveIntent` config getters — both previously returned `true` when the config value was `"false"`, causing the settings to behave opposite to their descriptions. Aggressive intent and the aggressive confirmation modal now respect the configured values correctly.

---

## v2.0.0

Initial public release.
