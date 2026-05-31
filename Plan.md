# Plan: Player Proximity Pause for NPC Threads

## TL;DR
When the player enters within 700 units of an active NPC-only thread's first actor, pause that thread's scheduled eval timer and stop its OStim auto mode. When the player moves away, reset the timer and restore auto mode. Implemented as a new `ProximityCheck` event type piggybacking on the existing `DebounceQueue` tick, with auto mode controlled via Papyrus VM dispatch from C++.

## Decisions
- **Detection**: C++ tick on DebounceQueue, every `SpectatorScanIntervalSeconds` (5s)
- **Auto mode control**: Papyrus VM dispatch (`OThread.StopAutoMode` / `OThread.StartAutoMode`) — these don't exist in OStimNG C++ API
- **Auto mode query**: C++ OStim API `OstimNG-API-Thread.h` `IsAutoMode(uint32_t threadID)`
- **Eligibility**: Any NPC-only thread (no player in actors[]), regardless of isOStimNet
- **Distance target**: `actors[0]` (first actor, index 0)
- **Scheduler pause semantics**: Reset timer (`m_lastSceneChange[id] = now`) on resume
- **State storage**: `_proximityPaused: std::unordered_map<int, ProximityPauseState>` on DebounceQueue
- **New thread start lag**: Accept up to 5s (no OnThreadStart hook)
- **Config key**: `tton.gameMaster.proximityPauseRadius` (float, default 700) — no enable/disable toggle
- **Thread ends while paused**: Clean up state, no restore needed

## Steps

### Phase 1: Config (no dependencies)
1. **Config.h** — Add `float ProximityPauseRadius() const;` declaration (follow existing pattern, PascalCase, no Get prefix, `const`)
2. **Config.cpp** — Implement: `return std::stof(GetValue("tton.gameMaster.proximityPauseRadius", "700"));`
3. **manifest.yaml** — Add new field under "Game Master - Scheduled" category:
   - name: "Proximity pause radius"
   - path: "tton.gameMaster.proximityPauseRadius"
   - type: "float", min: 100, max: 5000, defaultValue: 700
   - description: "Units from an NPC thread actor at which scene advancement is paused"

### Phase 2: ScheduledEvalService pause/resume API (no dependencies)
4. **ScheduledEvalService.h** — Add:
   - `void PauseThread(int threadID);`
   - `void ResumeThread(int threadID);`
   - `std::unordered_set<int> m_pausedThreads;` (private member, guarded by existing `m_mutex`)
5. **ScheduledEvalService.cpp** — Implement:
   - `PauseThread(int id)`: lock `m_mutex`, insert into `m_pausedThreads`
   - `ResumeThread(int id)`: lock `m_mutex`, erase from `m_pausedThreads`, set `m_lastSceneChange[id] = std::chrono::steady_clock::now()`
   - In `RunLoop()` after the in-flight check: `if (m_pausedThreads.contains(threadID)) continue;`
   - In `OnThreadEnd()`: also erase from `m_pausedThreads` (cleanup)

### Phase 3: DebounceQueue proximity logic (depends on Phase 1 & 2)
6. **DebounceQueue.h** — Add `ProximityCheck` to `EventType` enum (line 258, after SpectatorScan)
7. **DebounceQueue.h** — Add struct above or near existing members:
   ```
   struct ProximityPauseState { bool wasInAutoMode; };
   std::unordered_map<int, ProximityPauseState> _proximityPaused;
   ```
8. **DebounceQueue.h** — Add `ScheduleNextProximityCheckUnderLock()` private method (mirrors `ScheduleNextScanUnderLock()` pattern, uses same `SpectatorScanIntervalSeconds` interval, pushes `EventType::ProximityCheck`)
9. **DebounceQueue.h** — Add `PerformProximityCheck()` method (called from `DispatchFireData` when `EventType::ProximityCheck` fires). Logic:
   - Get player position
   - Get `radius = Config::GetSingleton().ProximityPauseRadius()`
   - Iterate active NPC threads (same pattern as ScheduledEvalService: GetActorPtrs per thread)
   - Filter: skip if player in actors[], skip if actors[0] is null
   - Distance check: `dx*dx + dy*dy + dz*dz <= r*r` against actors[0] (matches existing spectator math pattern from PerformScan() lines 604-606)
   - **Entering proximity** (near && !paused): query `OStimInterface->IsAutoMode(threadID)`, store in `_proximityPaused[threadID]`, VM dispatch `OThread.StopAutoMode(threadID)` if wasAutoMode, call `ScheduledEvalService::GetSingleton().PauseThread(threadID)`
   - **Leaving proximity** (not near && paused): if `state.wasInAutoMode`, VM dispatch `OThread.StartAutoMode(threadID)`, call `ScheduledEvalService::GetSingleton().ResumeThread(threadID)`, erase from `_proximityPaused`
   - Re-schedule: call `ScheduleNextProximityCheckUnderLock()` under lock
10. **DebounceQueue.h** — In `DispatchFireData()`: add `case EventType::ProximityCheck: PerformProximityCheck(); break;`
11. **DebounceQueue.h** — In existing `ScheduleNextScanUnderLock()` (or wherever the first spectator scan is scheduled on thread start): also schedule first `ProximityCheck` — mirror the exact same site where SpectatorScan is first queued
12. **DebounceQueue.h** — In `OnThreadEnd(int threadID)`: if thread is in `_proximityPaused`, erase it (no restore — thread is gone)
13. **DebounceQueue.h** — Add VM dispatch helpers (private): `DispatchStopAutoMode(int threadID)` and `DispatchStartAutoMode(int threadID)` using `RE::BSScript::Internal::VirtualMachine::GetSingleton()` static dispatch to `"OThread"` script

## Relevant Files
- `SKSE_Source/src/DebounceQueue.h` — Main implementation; EventType enum (L258), PerformScan() pattern (L468-625), spectator distance math (L604-606), ScheduleNextScanUnderLock() pattern (L344-349), DispatchFireData routing (L454-461)
- `SKSE_Source/src/ScheduledEvalService.h` — Add PauseThread/ResumeThread declarations, m_pausedThreads (L16-20, L31-34)
- `SKSE_Source/src/ScheduledEvalService.cpp` — Add pause skip in RunLoop interval check (L107-130), OnThreadEnd cleanup (find existing call)
- `SKSE_Source/src/Config.h` — Add `float ProximityPauseRadius() const;` (follow pattern of `float SpectatorScanRadius() const;`)
- `SKSE_Source/src/Config.cpp` — Implement getter near existing gameMaster getters
- `SKSE/Plugins/SkyrimNet/config/plugins/OStimNet/manifest.yaml` — Add field under "Game Master - Scheduled" (L265-281)
- `SKSE_Source/src/api/OstimNG-API-Thread.h` — `IsAutoMode(uint32_t threadID)` at L175 (read-only, use for query)

## Verification
1. Build the SKSE plugin (cmake Build Plugin Release) — zero compile errors
2. In-game: start NPC×NPC thread, walk player within 700 units → verify scene stops advancing and auto-mode is off
3. Walk player away → verify scene resumes advancing after ~60s and auto-mode is restored
4. In-game: start NPC×NPC thread with auto mode OFF → walk close → walk away → verify auto mode was NOT turned on (wasInAutoMode=false respected)
5. In-game: thread ends while player nearby → verify no crash, no spurious resume attempt
6. Check logs for proximity pause/resume trace messages
