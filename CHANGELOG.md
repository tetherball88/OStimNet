# Changelog

## v2.7.4

### Fixed

- **Untied Magelight UI & OStim player control lifecycles** — Replaced control-mask snapshotting/restoring with clean lifecycle separation between Magelight UI modals, OStim scene transitions, and companion mods:
  - Deferred Magelight modal activation across OStim player-scene startup/migration transitions (`1500 ms` settle window and active blocking/carrier menus including `PhantasmActorCarrier`), preventing scene-start hooks from capturing Magelight's suspended control state and re-applying locked controls at scene end.
  - Serialized all modal callbacks (`ModalManager`, `ConfirmationModal`, `ThreadSetupModal`, `RetryModal`) to run strictly after `ReleaseUIMode` completes and `MagelightFocus` closes (including gamepad/VR button-release holds), preventing Papyrus actions (`OThreadBuilder.Start`, `OThread.Stop`, `OStimHotSwap`, `TriggerTextInput`) from overlapping active UI mode.
  - Cancelled active/queued thread-scoped modals automatically when their OStim thread ends, and suppressed OStimNet hotkeys while Magelight UI mode, text entry, or blocking menus own input.
- **Player scene stop after mid-scene actor invite/join (`0 -> 0` continuation)** — Fixed `ThreadRegistry::CopyStateForContinuation` and new-start state resets so same-ID player-thread continuations clear the `continuation` flag once consumed instead of suppressing the final `ostim_thread_end` / `ostimnet_stop` teardown.
- **Actor invite & join hot-swap reliability (`InviteToYourSex` / `JoinOngoingSex`)** — Filtered existing scene participants, duplicates, and busy actors out of `InviteToYourSex` / `JoinOngoingSex` before confirmation, evaluation, and `OStimHotSwap.AddActorsToThread` (fixing invitations silently aborting with `newThreadID = -1` when the LLM included current participants in optional invitee slots), excluded unwilling invitees on partial acceptance instead of adding them anyway, and cleared armed continuation state if `OStimHotSwap` fails to migrate the thread.

### Changed

- **Animation descriptions** — Updated custom scene descriptions in `Billyy.json`.

---

## v2.7.3

### Fixed

- **Player movement, camera & menu controls after Magelight UI modals and OStim scenes** — Fixed player controls (`Movement`, `Looking`, `Activate`, `Menu`, `POVSwitch`, `WheelZoom`, `Fighting`) or text-input locks remaining disabled after dismissing a Magelight UI modal or finishing an OStim scene, including when modals open mid-scene while the player character is AI-driven (`SetAIDriven`) or overlap with scene-end fade-outs.
- **Magelight UI focus handoff & exit recovery** — Captured Skyrim's main thread ID at SKSE plugin load, added support for Magelight UI `v0.9.1+` exit callbacks alongside `MagelightFocus` menu-close fallbacks, and prevented `UIModeExited` events from clearing focus ownership when back-to-back modals open consecutively.

---

## v2.7.2

### Fixed

- **OVirginity Reflowered v3.0.0 compatibility & female-only tracking** — Restricted virginity queries to female actors so male player characters and male NPCs (whose virginity status is initialized by `OVirginity Reflowered` v3.0.0 but never cleared during scenes) are no longer stuck with permanent virgin status, first-time prompt cues, or slow-speed clamping.
- **More reliable virginity-loss detection & hotkey refreshes** — Handled scenes that start directly in vaginal intercourse, replaced legacy event listeners with two-stage vaginal scene checks (`500 ms` + `1800 ms`) plus a scene-end fallback so script lag cannot drop deflowering reactions or diary entries, added immediate pre-evaluation refreshes so `OVirginity`'s toggle hotkey takes effect without cache delay, and added `virginity_lost_diary` formatting in event history prompts.
- **Intent- & role-accurate first-time memories, reactions & diary entries** — Rewrote permanent SkyrimNet first-person virginity-loss memories, mid-scene reaction events (`tton_virginity_lost`), and post-scene diary reflections (`tton_virginity_lost_diary`) to distinguish `aggressive` (non-consensual assault, tracking whether the virgin was the victim or the aggressor), `dom` (consensual dominance/submission, tracking dominant vs. submissive roles), `romantic`, `lustful`, and `transactional` (client vs. provider) encounters.

---

## v2.7.1

### Fixed

- **Crash to desktop (CTD) when ending scenes on Skyrim AE (`1.6.x`)** — Fixed an unmapped Address Library call (`Actor::IsInBleedout`) in scene-end interruption detection that caused an immediate crash to desktop whenever an OStim scene finished on Skyrim Anniversary Edition (`1.6.640` / `1.6.1170` / `1.6.1179` GOG).
- **Phase activity lists in ongoing scene prompts (`ostimnet_thread_phase`)** — Fixed `ONavGetThreadActions` array parsing so `foreplayActions`, `oralActions`, and `sexActions` populate properly instead of returning empty strings.
- **Thread safety & save-reload stability across AI evaluations and prompt decorators** — Synchronized `ThreadRegistry` access across SkyrimNet worker threads, marshaled async LLM/Decisions callbacks and pull-out scene transitions onto the SKSE main thread with save-session guards, and cached `OVirginityQuest` resolution once at startup to avoid repeated off-thread form lookups when `OVirginity.esp` is not installed.

---

## v2.7.0

### Added

- **Magelight UI modals** — Replaced Skyrim native message boxes and `UIExtensions` list menus with **Magelight UI** modals across all confirmation prompts, SkyrimNet Decisions retry dialogs, manual external scene setup menus, and in-scene OStim wheel intent/role menus:
  - Supports mouse clicks, keyboard navigation (`WASD` / `Arrows` / `Tab`, `Enter` / `Space` / `E`), and gamepads cleanly without the Skyrim engine camera fly-away bug caused by clicking native message boxes.
  - **Dependency change**: Replaces the previous `UIExtensions` requirement with **Magelight UI**.
- **Optional OVirginity Reflowered integration** — Added built-in support when `OVirginity.esp` is installed:
  - **Character & scene awareness** — Exposes virginity status (`ovirginity_is_virgin`) to Game Master character bios, pre-start/join/invite/location-scan evaluations, and ongoing scene dialogue cues (`0502_sex.prompt`).
  - **Gentle initial pacing** — Automatically starts penetrative animations at slow speed in `romantic` and `lustful` encounters involving a virgin participant.
  - **First-time reactions, memories & diary entries** — Detects virginity loss mid-scene to trigger an immediate in-character reaction (`tton_virginity_lost`), records permanent SkyrimNet relationship/trauma memories for participating NPCs, and supports an optional post-scene private diary reflection (`tton_virginity_lost_diary`, disabled by default).
- **Expanded non-sexual care intents (`lustful` & `transactional`)** — `StartCareScene` (hugging and kissing), non-sexual scene evaluations, manual external thread setup, and event logs now support `lustful` and `transactional` intents alongside `platonic` and `romantic`, including proper recipient/provider role assignment for `transactional` care scenes.
- **Animation pace awareness in dialogue** — Active sexual scenes now track normalized animation speed (`slow`, `moderate`, `fast`) and pass pace-specific delivery cues into `0502_sex.prompt` so NPC vocal intensity and breathlessness match the current rhythm of the animation.
- **Combat & guard arrest interruption reactions** — Detects when an ongoing intimate scene is abruptly broken up by combat, bleedout, or crime/guard arrest, cancels pending scene-change and speed-change narration, logs the interruption reason, and fires an immediate high-priority reaction (`tton_sex_interrupted`) instead of normal post-scene dialogue (`tton_sex_stop_comment`).

### Changed

- **Cleaner foreplay, oral & position filtering** — Scene searches (`SceneSexSearch`, `OStimChangeScene`, and `ChangeSexScenePosition`) now automatically blacklist higher-phase action types when searching for foreplay or oral activities (preventing multi-action intercourse scenes from matching earlier-stage requests) and clear incompatible intercourse position tags when switching to foreplay or oral animations.

### Fixed

- **`"0 starting animations found"` & scene start fallbacks** — Fixed several issues that could cause NPC- or AI-initiated scenes to fail at startup with `"no starting animation found"` or leave stale thread claims behind:
  - Fixed `SceneSexSearch` not relaxing `"intercourse"` to `"sexual"` during `"sex"`-phase starts (such as `dom` and `aggressive` encounters) when `actionType` was empty and participants only had non-intercourse animations available, and fixed bed subtype checks (`doublebed`, `singlebed`, `bedroll`) using `OFurniture.IsChildOf("bed", furnType)`.
  - Removed `OThreadBuilder.SetDominantActors` in `StartOstimSex`, which inverted `[Male, Female]` actor slot ordering on femdom fallback starts and caused animation lookup to fail.
  - Added pre-start and start-time fallbacks to avoid offering intercourse positions when no `"sex"`-phase activities exist for the participants, retry on the floor when selected furniture has no compatible animations, restrict default undressing starts to loaded 2-actor male+female pairs, and cleanly abort with a notification and actor cleanup if no compatible animation exists.
  - Hardened `ThreadRegistry` claim expiration, invalid `threadID` handling, actor-overlap validation, and starting-phase synchronization with the actual opening animation.

---

## v2.6.2

### Added

- **Billyy animation pack descriptions** — Added detailed scene descriptions and updated position metadata for **Billyy's** animation pack(covering male/female dom and spawned furniture modules), giving the AI a clearer understanding of what characters are doing during those scenes for more accurate dialogue and reactions.

### Fixed

- **Scene setup & progression popup errors with limited options** — Fixed a SkyrimNet Decisions error popup (*"offers 1 option(s); at least two are needed"*) that could interrupt scene starts, NPC location scans, or automatic scene progression when only a single compatible activity or position existed for the current participants and furniture. When only one valid option is available, OStimNet now selects it automatically.

---

## v2.6.1

### Fixed

- **Solo / masturbation scene support & phrasing across modals, evaluations, and events** — Fixed several issues where solo (1-person) sexual scenes were blocked or treated as partnered encounters with placeholder names:
  - **Confirmation modals & decline notifications** — Fixed confirmation popups and decline events displaying `"<Actor> wants to start a sexual encounter with Someone."` or `"<Actor> will proceed with Someone."` when no partner is present; solo scenes now use natural solo wording (e.g. `"<Actor> wants to start a solo sexual encounter."`, `"<Actor> will proceed alone."`) and no longer append stray partner text to decline messages.
  - **Furniture wait loop on solo player scenes** — Fixed an out-of-bounds array access in `TTON_OStimIntegration.StartOstimSex` when starting a solo player scene on furniture.
  - **In-scene intent change menu** — Fixed changing the scene intent from the OStim wheel menu (`TTON_OStimOptions.ChangeIntent`) failing with `"Not enough actors in the scene to change roles"` during solo scenes.
  - **Joining or inviting into an ongoing solo scene** — Fixed thread continuation detection in `OStimEventListener` when a partner joins or is invited into an active 1-person solo thread, preserving thread state and spectator tracking across the transition.
  - **AI scene evaluations & location scans** — Resolved contradictory validation rules in pre-start, external thread, and location scan evaluation prompts (and the chat evaluation fallback for location scans) that could reject 1-participant solo scenes, and skipped the redundant role-combination decision step when only one participant is involved.
  - **Ongoing scene prompts & event logs** — Fixed scheduled scene progression passing `"Partner"` as the secondary participant in solo scenes, added solo-scene handling to `intent_changed` event logs, and updated the ongoing sex voice prompt (`0502_sex.prompt`) to use solo-appropriate delivery instructions and omit two-person pregnancy/pullout cues during solo masturbation.

---

## v2.6.0

### Added

- **Faster, more reliable AI scene setup & progression (SkyrimNet Decisions support)** — OStimNet now uses SkyrimNet's **Decisions** system to evaluate and set up scenes step-by-step instead of relying solely on long, complex AI text responses:
  - Makes scene evaluations significantly faster, more consistent, and far less prone to AI formatting errors—especially with smaller or faster AI models.
  - Covers all scene evaluations: starting new sexual or affectionate scenes, inviting or joining ongoing scenes, automatic scene progression, near-climax pull-out decisions, spontaneous NPC location scans, and scenes started by external mods.
  - Enabled by default under **General → Use Jev Decisions for evaluations**. If no Decisions model is configured in SkyrimNet, OStimNet automatically falls back to the standard Game Master AI without any extra setup needed.

### Changed

- **Smarter scene initiation & furniture timing**:
  - When an NPC decides to initiate a sexual scene during conversation, the dialogue AI now focuses purely on *who* is involved and *why*, leaving the scene's mood (romantic, lustful, transactional, dominant, aggressive), roles, and furniture selection to the dedicated scene setup step.
  - Characters now wait until the scene evaluation is finished and everyone's willingness is confirmed before walking over to a bed or piece of furniture, rather than walking to a bed before knowing if the scene will actually happen.
  - Added safeguards so NPCs can no longer accidentally select themselves as their own partner when proposing a scene.
- **Better story continuity in scene evaluations** — When an NPC proposes a new sexual scene, initiates a non-sexual affection scene (hugging, kissing, cuddling), asks to join an ongoing scene, or invites someone else to join, their reasoning from the conversation is now shared with the scene evaluator so consent checks, mood changes, and role assignments match what just happened in dialogue.
- **More natural scene phases & animation selection**:
  - **Phase-aware scene starts** — When a scene begins, OStimNet now automatically matches the opening activity and position to the appropriate starting stage (undressing, foreplay, oral, or intercourse) based on the mood of the encounter, the number of participants, and whether furniture is being used.
  - **Compatible positions & activities only** — Using updated **OStim Navigator** integration, the AI is now only offered sexual positions and activities that actually exist in your installed animation packs for the current participants and furniture, while filtering out climax-only animations during normal progression.
  - **Improved animation search fallbacks** — If the AI picks a specific foreplay or oral activity that isn't available for a particular piece of furniture or character pairing, OStimNet now smoothly falls back to broader compatible animations for that stage rather than failing or skipping ahead prematurely.

---

## v2.5.7

### Fixed

- **Hotkeys triggering on mouse clicks & extended keys mapping** — Fixed an issue in `HotkeyInputSink::ProcessEvent` where configuring hotkeys (`Advance scene (player thread)`, `Location scan`, `Toggle mute`) to keys without a standard 1-byte `MapVirtualKeyA` scan code (such as `Pause/Break` / `VK_PAUSE = 19`, which mapped to `0`) caused every Left Mouse Click (`IDCode == 0`) to trigger the hotkey action and advance the player's scene:
  - Restricted keyboard hotkey evaluation strictly to `RE::INPUT_DEVICE::kKeyboard` events and required non-zero DirectInput scan codes.
  - Added `VkToDirectInputKeyCode` helper with explicit `VK_PAUSE` (`0xC5`) and `VK_NUMLOCK` (`0x45`) handling and `MAPVK_VK_TO_VSC_EX` translation for `0xE0`-extended keys (arrow keys, Insert/Delete/Home/End/PageUp/PageDown, Numpad Divide, right modifiers).

---

## v2.5.6

### Added

- **Session mute Papyrus API** — Added global native Papyrus functions on `OStimNet` (`ToggleMute()`, `IsMuted()`, and `SetMuted(bool akMuted)`) to query, toggle, or explicitly set the session-level mute state for NPC scene triggers and comments.

### Fixed

- **Spectator handling across thread continuations (join/swap)** — Fixed spectators being prematurely removed or stuck when an active scene transitions via thread continuation (e.g. when a spectator joins an ongoing scene):
  - Preserved non-participating spectators across continued threads via `TransferSpectatorsForContinuation`, only firing remove-spectator events for spectators who joined the scene as participants (or if a pending continuation expires).
  - Added a guard in `TTON_Spectators.TryMakeSpectator` to skip package overrides, faction assignment, and event dispatch if the spectator was already untracked before the Papyrus event was processed.
- **Thread continuation event prompt rendering** — Added `sex_continue_thread` event rendering (`{{msg}}`) to `ostimnet_event_compact.prompt` and `ostimnet_event_verbose.prompt` so scene continuation events are no longer omitted from SkyrimNet event logs.
- **Session mute toggle baseline** — Fixed `Config::ToggleMuteSession()` to invert the effective `IsMuted()` state (respecting the configured `muteAllActors` setting when no session override is active yet).

---

## v2.5.5

### Fixed

- **External thread start with AI evaluation** — Fixed third-party / external scene starts failing when delegating scene evaluation to AI (`EvaluateExternalSexualThread`):
  - Passed resolved participant `Actor[]` directly from `TTON_OStimOptions.SetupExternalThread` into `OStimNet.EvaluateExternalSexualThread` and stored participant FormIDs, pointers, and display names on the thread registry before running LLM evaluation.
  - Added module handle fallbacks for `OStim NG.dll` and `OStimNG.dll` alongside `OStim.dll` in `OstimNG_API::Thread::GetAPI`, and prevented missing `g_ostimThreadInterface` pointers from prematurely aborting `ClaimExternalThread` and `EvaluateExternalSexualThread`.

---

## v2.5.1 - 2026-09-20

### Fixed

- Fixed Skyrimnet Pre-25 folder structure

## v2.5.0 - 2026-09-20 (Ships both SkyrimNet pre 25 and 25+ versions)

### Added

- **Pull out mechanic** — Introduced an automated and interactive pull out system during vaginal intercourse, integrating excitement monitoring, climax stalling, AI dialogue reactions, and intelligent scene navigation:
  - **Climax stalling & excitement monitoring** — Automatically monitors excitement levels during vaginal sex scenes. When the giving partner reaches the excitement threshold (default: 85%), climax is stalled to give partners an opportunity to react and decide whether to pull out before ejaculation.
  - **Sensory cue (`tton_partner_near_edge`)** — Dispatches a sensory narration cue to SkyrimNet when the giver approaches climax, alerting the receiving partner that their partner is tensing up and on the verge of climax so they can react in dialogue.
  - **Pullout decision action (`PulloutDecision`)** — Enables receiving partners to make an action call deciding whether to request pulling out (`request_pullout`), allow finishing inside (`allow_finish_inside`), or leave it to their partner (`doesnt_care`). Includes automated LLM fallback evaluation (`ostimnet_evaluate_pullout_decision.prompt`) if no action is chosen before the configurable timeout (default: 25s).
  - **Smart pullout scene transition** — When pulling out is chosen, queries OStim Navigator (`ONavFindPulloutScene`) to seamlessly transition to a non-vaginal climax scene matching current actor pose height tiers (prioritizing handjob, boobjob, male masturbation, or cum-on animations), releases the climax stall, and dispatches a pullout event to SkyrimNet.
  - **OStim native pullout synchronization & hotkey** — Disables conflicting native OStim pullout chances while active (restoring them when disabled) and supports triggering immediate pullout in the player's scene via OStim's native pullout hotkey.
  - **Pullout settings** — Added dedicated **Pullout Mechanics** settings under the SkyrimNet mod menu:
    - `Enable pullout mechanics` (`tton.pullout.enabled`, default: `true`).
    - `Pullout excitement threshold` (`tton.pullout.excitementThreshold`, default: `85`).
    - `Pullout actor decision timeout (seconds)` (`tton.pullout.evalTimeoutSeconds`, default: `25`).
    - `Sensory narration cue at threshold` (`tton.pullout.narrateCue`, default: `true`).
    - `Prohibited intents` (`tton.pullout.prohibitedIntents`, default: `aggressive_dom`): disallows pullout mechanics in scenes with prohibited intents (`none`, `aggressive`, `aggressive_dom`).

- **Manual external thread setup** — Added interactive in-game menu setup for OStim scenes started outside of OStimNet (e.g. manually via OStim UI/hotkeys or third-party mods):
  - **Interactive configuration UI** — Prompts the player via UIExtensions (`UIListMenu`) to configure the scene type (`Sexual` vs `Non-Sexual`), scene intent (`Romantic`, `Lustful`, `Transactional`, `Dom`, `Aggressive`, or `Let AI decide`), and select Main and Secondary actor roles.
  - **External scenes settings** — Added settings under **External scenes** in the SkyrimNet mod menu to toggle manual setup prompts independently for player-involved and NPC-only scenes:
    - `Player thread` (`tton.externalScenes.playerThread`, default: `false`).
    - `NPC threads` (`tton.externalScenes.npcThreads`, default: `false`).
  - **Papyrus API & mod events** — Added `ClaimExternalThread`, `EvaluateExternalSexualThread`, and `CancelExternalThread` native Papyrus functions in `OStimNet.psc`, triggered via the new `ostimnet_manual_thread_setup` mod event.

### Changed

- **Conception & pregnancy prompt subtlety** — Refined ongoing sex scene prompts (`0502_sex.prompt`) and Game Master bio prompts to ensure pregnancy awareness serves as subtle background context that surfaces when climax is near, preventing repetitive dialogue and keeping focus on physical sensations and intimacy.

---

## v2.4.3 - 2026-09-16 (Requires SkyrimNet 25+)

### Added

- **Conception & pregnancy awareness prompt cue** — Added a toggleable prompt cue in ongoing sex scenes (`0502_sex.prompt`) reminding the LLM that sexual intercourse and finishing inside can lead to pregnancy and conceiving children, differentiated by gender:
  - **Female perspective & Fertility Mode support** — Integrates with `Fertility Mode.esm` (`ImmersiveEffectsFaction`) to dynamically reflect the character's cycle state and conception chances (peak fertile days / high risk, approaching fertile days, safe days / period, already pregnant, postpartum recovery, in labor), with natural fallback handling when untracked or if Fertility Mode is not installed.
  - **Male perspective** — Reminds that finishing inside can lead to fathering children and impregnating his partner (handling pulling out cautiously, taking the risk in the heat of passion, or deliberately aiming to conceive).
- **Internal climax prompt cue setting** — Added `Internal climax prompt cue` (`tton.settings.internalClimaxPromptCue`) toggle setting under **Comments** (defaults to `true`), with corresponding SKSE `ostimnet_settings` decorator integration.

---

## v2.4.2 - 2026-09-11 (Requires SkyrimNet 25+)

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
