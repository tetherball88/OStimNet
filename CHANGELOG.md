# Changelog

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
