# Changelog

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
