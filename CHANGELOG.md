# Changelog

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
