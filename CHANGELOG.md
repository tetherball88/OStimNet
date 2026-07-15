# Changelog

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
