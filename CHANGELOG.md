# Changelog

## [v0.7.0]

### Added
- on OStim end it will attempt to restore SkyrimNet's packages(before if NPC was following via SkyrimNet's package after OStim scene they cleared that package and didn't follow anymore - in this version it should work as expected)


### Changed
- Cleaned almost all decorators:
    - Moved actions descriptions to separate template files with gathering nearby potential participants via template cpp capabilities
- Removed Lover's Ledger requirement(moved its prompts to separate integration [Lover's Neural Ledger](https://github.com/tetherball88/Lovers-Neural-Ledger))

## [v0.6.0]

### Added
- SkyrimNet trigger system integration with 5 YAML trigger files for server-side configuration
  - `tton_sex_start.yaml` - NPC reactions when scenes begin
  - `tton_sex_change.yaml` - Position change commentary
  - `tton_sex_climax.yaml` - Orgasm reactions with high priority
  - `tton_sex_stop.yaml` - Scene end diary entries
  - `tton_action_decline.yaml` - Player refusal evaluations
- New `TTON_Storage.psc` script for caching decorator data with `TTONDec_` prefixed storage keys
- Direct PapyrusUtil access in Jinja2 templates via `papyrus_util()` helper function
- Pre-computed lover relationship data with human-readable recency strings ("3 days ago", etc.)
- Weighted speaker selection for events (orgasming NPC now comments on their own climax)
- Multi-climax support for group scenes
- Debug logging for OStim scene starts in `TTON_Actions.psc`
- MCM mute setting with hotkey support - silences trigger reactions while still logging events

### Changed
- Unified multiple event schemas into single `tton_event` schema with `type` field
- Migrated prompt templates to storage-based decorator system:
  - `0301_personality_lovers_ledger_insights.prompt` - Direct storage reads, removed `isTimePaused` check
  - `0601_relations_lovers_ledger_insights.prompt` - Form list iteration instead of JSON parsing
  - `0201_ostim_scenes.prompt` - Thread-based storage, simplified participant checking
  - `0500_sex.prompt` - Storage-based thread tracking
- Removed JSON-building decorators from `TTON_Decorators.psc` (~150 lines eliminated)
- Simplified `TTON_Utils.psc` by removing decorator helpers (~162 lines reduced)
- Refactored `TTON_JData.psc` for storage layer compatibility
- Expanded `TTON_MainController.psc` to orchestrate storage updates and event registration (~153 lines added)
- Updated `TTON_OStimIntegration.psc` to call storage layer on scene lifecycle
- Restructured `TTON_MCM.psc` - removed old comment cooldown/distance settings, reorganized layout
- Event registration now respects mute setting via `GetMuteSetting()`
- All events carry `threadID` field for storage correlation

### Removed
- MCM options for sex comment cooldown and hearing range (now controlled by trigger system)
- MCM option for "Continue narration chance" (removed from v0.6.0)

### Fixed
- SexLab compatibility - pausing scene tracking during SexLab scenes (PR #2)

### Performance
- Decorator calls reduced from O(n) JSON building to O(1) storage reads
- Template rendering no longer performs expensive calculations
- Event filtering handled by SkyrimNet trigger system instead of Papyrus

---

## [v0.5.0.dev] - 2025-11-16

### Breaking Changes
- Now requires Lover's Ledger v0.1.0.dev or newer.

### Added
- Advanced visibility system that considers distance, line of sight, and lighting when determining if NPCs can perceive sex scenes.
- Light-based sight distance modifiers that reduce visibility in darker environments (40% in pitch black, 100% in full light).
- Interior/exterior environmental sound propagation adjustments (quieter interiors allow 20% longer hearing range, noisier outdoors reduce by 20%).
- Granular visibility states: "canSeeAndHear", "canHearClose", "canHearFar", and "none" for more realistic NPC awareness.

### Changed
- MCM option renamed from "Comment hearing range" to "Sex hearing range" with improved description explaining dual purpose for comments and event logging.
- Default sex hearing range increased from 768 to 1152 units for better balance.
- Scene descriptions now only generated when NPCs have visual line of sight to the scene, improving performance and realism.
- Event logging now respects visibility system - if player can't see/hear the scene, no comment or log event is created.



## [v0.4.0] - 2025-11-06
### Added
- MCM slider for "Continue narration chance (%)" to control how often AI continues narrating after sex comments.
- Moved action declarations to `initialData.json` for easier editing without recompiling scripts.

### Changed
- MCM option changed from "Allow manual bed selection" to "Use default OStim start scene", and now makes start scene behaves same way as default OStim starts.
- Improved climax event messages with more natural language and better context. Enhanced orgasm detection with detailed tracking of ejaculation locations (on/in specific body parts).
- Event handling refactored to use direct narration requests instead of event schemas for more natural flow.
- Sex start, scene change, and climax events now trigger AI comments directly with rich context.
- Decline/rejection system unified across all action types with consistent cooldown handling.
- Improved confirmation dialogs with more natural language and better context for player choices.
- Declines now just log short lived event(present for 2 minutes in your recent events history) instead of triggering NPC reaction. It is up to you to explain your decline.

### Fixed
- Orgasm messages now properly exclude the climaxing actor from the "with" participants list.
- Removed debug trace calls that were cluttering logs.

## [v0.3.0.dev] - 2025-10-27
### Added
- MCM toggle for manual bed/furniture selection when LLM doesn't specify furniture preference.
- MCM slider for comment hearing range to control maximum distance for NPC comments (0 = unlimited, respects line of sight).

### Changed
- Action descriptions and parameters now stored in `initialData.json` for easier editing without recompiling scripts.
- Improved bed/furniture finding logic with better prioritization and distance checks.
- Scene search algorithm now better handles furniture selection and actor eligibility.
- Reorganized MCM settings into more logical groupings for better user experience.
- MCM default values refined: comment cooldown to 20 seconds, comment distance to 2048 units.
- Sex comment prompts now emphasize emotional reactions and dialogue variety over repetitive physical descriptions.
- Prompt file naming updated for better ordering (0301_personality, 0601_relations, 0201_ostim_scenes).
- Lover's Ledger insights now distinguish between "Character Sexual Behavior" and "Intimate Relationships" sections.
- Updated compatibility for SkyrimNet Beta 7, rc3.
- Changed "Character Insights from Intimate History" to "Intimate Relationships" prompt section showing detailed lover connections with relationship type, intensity, and recency.

### Fixed
- Scene selection now properly respects player's manual furniture choices when configured.
- Comment distance checks now correctly filter NPCs based on configured range and line of sight.

## [v0.2.1] - 2025-10-12
### Changed
- Temporarily disable the `BodyAnimation` tag when SkyrimNet Cuddle or SexLab integrations are present so encounters are not misclassified until tag handling is corrected.

## [v0.2.0] - 2025-10-12
### Added
- `StartAffectionScene` action so NPCs can initiate gentle, non-sexual encounters with curated interaction prompts.
- MCM toggle for confirming affectionate scenes before the player is involved.
- MCM slider to configure the duration of affection scenes on a per-save basis.

### Changed
- OStim start flow now routes through a non-sexual pipeline when affection scenes are requested, including furniture handling and builder settings.
- Affection scene builder uses the configured duration instead of a hard-coded timeout.

### Fixed
- Scene selection respects non-sexual requests to prevent sexual animations from being chosen for affection-only sequences.

## [v0.1.1] - 2025-10-11
### Added
- Body-animation tag detection for companion mods to advertise compatible actions.
- MCM storage flag allowing Start New Sex to be toggled per save.

### Changed
- Start New Sex eligibility now respects the new enable flag before registering an encounter.

### Fixed
- Default handling ensures the Start New Sex option stays enabled when migrating existing saves.

## [v0.1.0-dev] - 2025-09-29
### Added
- Dedicated OStim integration script with confirmation modals, sex comment system, and per-action registration helpers.
- Comprehensive MCM page with confirmation toggles, narration cooldown sliders, and denial cooldown tuning.
- Individual animation description catalogs for every supported author to improve scene context generation.

### Changed
- Prompt templates refreshed to surface furniture choices, action lists, and richer post-scene summaries.
- Scene selection utilities prefer author-specific metadata and smarter actor eligibility rules when adding participants.

### Fixed
- Invite/join logic now guards against duplicate actors and enforces OStim threading limits when expanding scenes.

## [v0.0.3] - 2025-09-24
### Added
- Automatic re-import of packaged data after clearing stored state to keep prompts and references populated.

### Changed
- MCM export/import help text updated to point at the `store.json` backup file.

### Fixed
- Clearing saved data no longer wipes required static state, preventing missing animation metadata after resets.

## [v0.0.2] - 2025-09-17
### Added
- Release packaging workflow for automated zip builds.
- Expanded event tracking around OStim lifecycle, including start, change, and climax reporting.
- Richer animation metadata and SkyrimNet prompt content for sexual-life, system, and instruction submodules.

### Changed
- Main controller now manages thread lifecycle events and global "OStim active" state for downstream consumers.
- Utility layer reworked to support furniture-aware scene selection and dynamic actor invitation checks.

### Fixed
- Join and invite actions now validate free slots and participant eligibility before restarting OStim threads.

## [v0.0.1] - 2025-09-17
Initial release.
