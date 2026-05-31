#pragma once
#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Config.h"
#include "ModEventDispatch.h"
#include "ThreadRegistry.h"
#include "ScheduledEvalService.h"
#include "api/OstimNG-API-Thread.h"

namespace OStimNet {

// Unified background service replacing SceneChangeDebouncer, ClimaxDebouncer,
// and SpectatorScanner.  One thread, one mutex, std::priority_queue sorted by
// fire time with lazy-deletion for debounce resets.
//
// Debounced events (SceneChange, Climax): posting again before expiry resets
// the timer; stale queue entries are skipped via a generation counter.
// Periodic events (SpectatorScan): self-reschedule on every fire.
class DebounceQueue {
public:
    // =========================================================================
    // Climax data types  (previously in ClimaxDebouncer.h)
    // =========================================================================
    struct ClimaxEntry {
        RE::FormID  actorFormID;
        std::string sceneID;
    };
    struct OcumAppliedEntry {
        RE::FormID  orgasmerFormID;
        RE::FormID  targetFormID;
        float       amountML;
        std::string area;
        std::string sceneID;
    };
    struct OcumSquirtEntry {
        RE::FormID  actorFormID;
        std::string sceneID;
        std::string squirtType;
    };
    struct ClimaxBatchData {
        std::vector<ClimaxEntry>      climaxEvents;
        std::vector<OcumAppliedEntry> cumApplied;
        std::vector<OcumSquirtEntry>  squirts;
    };

    // =========================================================================
    // Spectator types  (previously in SpectatorScanner.h)
    // =========================================================================
    using ActiveThreadList     = std::vector<std::pair<int, std::vector<uint32_t>>>;
    using GetActiveThreadsFunc = std::function<ActiveThreadList()>;

    // =========================================================================
    // Callback types
    // =========================================================================
    using SceneChangeCb = std::function<void(int threadID, const std::string& sceneID)>;
    using ClimaxCb      = std::function<void(int threadID, const ClimaxBatchData&)>;
    using SpeedChangeCb = std::function<void(int threadID, int32_t speed)>;

    // Fires ostimnet_remove_spectator for the given spectator (game-thread only).
    static void FireRemoveSpectatorEvent(RE::FormID spectatorID, RE::FormID targetActorFormID, int threadID) {
        std::string json;
        json += "{\"spectatorFormID\":";
        json += std::to_string(spectatorID);
        json += ",\"targetActorFormID\":";
        json += std::to_string(targetActorFormID);
        json += ",\"threadID\":";
        json += std::to_string(threadID);
        json += "}";
        auto* spectator = RE::TESForm::LookupByID<RE::Actor>(spectatorID);
        if (auto* source = SKSE::GetModCallbackEventSource()) {
            SKSE::ModCallbackEvent e{
                "ostimnet_remove_spectator", json.c_str(),
                static_cast<float>(threadID), spectator};
            source->SendEvent(&e);
        }
    }

    DebounceQueue(SceneChangeCb sceneChangeCb,
                  ClimaxCb      climaxCb,
                  SpeedChangeCb speedChangeCb,
                  GetActiveThreadsFunc getActiveThreads)
        : _sceneChangeCb(std::move(sceneChangeCb))
        , _climaxCb(std::move(climaxCb))
        , _speedChangeCb(std::move(speedChangeCb))
        , _getActiveThreads(std::move(getActiveThreads))
    {}

    ~DebounceQueue() { Stop(); }

    // =========================================================================
    // Scene change API
    // =========================================================================

    // Register (or reset) a pending scene change for a thread.
    void PostSceneChange(int threadID, std::string sceneID) {
        std::lock_guard<std::mutex> lk(_mutex);
        _pendingSceneID[threadID] = std::move(sceneID);
        int debounceSec = Config::GetSingleton().OstimSceneChangeDebounce();
        RescheduleUnderLock({EventType::SceneChange, threadID}, std::chrono::seconds(debounceSec));
    }

    // Flush any pending scene change for a thread immediately (e.g. on thread end).
    // Calls the callback directly on the calling (game) thread if there was a pending entry.
    void FlushSceneChange(int threadID) {
        std::string sceneID;
        {
            std::lock_guard<std::mutex> lk(_mutex);
            auto it = _pendingSceneID.find(threadID);
            if (it == _pendingSceneID.end()) return;
            sceneID = std::move(it->second);
            _pendingSceneID.erase(it);
            CancelUnderLock({EventType::SceneChange, threadID});
        }
        _sceneChangeCb(threadID, sceneID);
    }

    // Cancel any pending scene change for a thread without firing it.
    void CancelSceneChange(int threadID) {
        std::lock_guard<std::mutex> lk(_mutex);
        _pendingSceneID.erase(threadID);
        CancelUnderLock({EventType::SceneChange, threadID});
    }

    // =========================================================================
    // Climax API
    // =========================================================================

    // Append a climax entry and reset the debounce timer for this thread.
    void PostClimaxEvent(int threadID, RE::FormID actorFormID, std::string sceneID) {
        std::lock_guard<std::mutex> lk(_mutex);
        _pendingClimax[threadID].events.push_back({actorFormID, std::move(sceneID)});
        int debounceSec = Config::GetSingleton().ClimaxDebounceSeconds();
        RescheduleUnderLock({EventType::Climax, threadID}, std::chrono::seconds(debounceSec));
    }

    // Append cum-applied data and reset the debounce timer.
    void PostClimaxOcumApplied(int threadID, RE::FormID orgasmer, RE::FormID target,
                               float ml, std::string area, std::string sceneID) {
        std::lock_guard<std::mutex> lk(_mutex);
        _pendingClimax[threadID].cumApplied.push_back(
            {orgasmer, target, ml, std::move(area), std::move(sceneID)});
        int debounceSec = Config::GetSingleton().ClimaxDebounceSeconds();
        RescheduleUnderLock({EventType::Climax, threadID}, std::chrono::seconds(debounceSec));
    }

    // Append a squirt entry and reset the debounce timer.
    void PostClimaxSquirt(int threadID, RE::FormID actorID, std::string sceneID, std::string squirtType) {
        std::transform(squirtType.begin(), squirtType.end(), squirtType.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        std::lock_guard<std::mutex> lk(_mutex);
        _pendingClimax[threadID].squirts.push_back({actorID, std::move(sceneID), std::move(squirtType)});
        int debounceSec = Config::GetSingleton().ClimaxDebounceSeconds();
        RescheduleUnderLock({EventType::Climax, threadID}, std::chrono::seconds(debounceSec));
    }

    // Flush all accumulated climax data for a thread immediately (e.g. on thread end).
    // Calls the callback directly on the calling (game) thread.
    void FlushClimax(int threadID) {
        ClimaxBatchData data;
        {
            std::lock_guard<std::mutex> lk(_mutex);
            auto it = _pendingClimax.find(threadID);
            if (it == _pendingClimax.end()) return;
            data.climaxEvents = std::move(it->second.events);
            data.cumApplied   = std::move(it->second.cumApplied);
            data.squirts      = std::move(it->second.squirts);
            _pendingClimax.erase(it);
            CancelUnderLock({EventType::Climax, threadID});
        }
        _climaxCb(threadID, data);
    }

    // =========================================================================
    // Speed change API
    // =========================================================================

    // Register (or overwrite) a pending speed change for a thread.
    // Only the last speed posted within the debounce window is fired.
    void PostSpeedChange(int threadID, int32_t speed) {
        std::lock_guard<std::mutex> lk(_mutex);
        _pendingSpeed[threadID] = speed;
        int debounceSec = Config::GetSingleton().SpeedChangeDebounceSeconds();
        RescheduleUnderLock({EventType::SpeedChange, threadID}, std::chrono::seconds(debounceSec));
    }

    // Flush any pending speed change for a thread immediately (e.g. on thread end).
    void FlushSpeedChange(int threadID) {
        int32_t speed = 0;
        {
            std::lock_guard<std::mutex> lk(_mutex);
            auto it = _pendingSpeed.find(threadID);
            if (it == _pendingSpeed.end()) return;
            speed = it->second;
            _pendingSpeed.erase(it);
            CancelUnderLock({EventType::SpeedChange, threadID});
        }
        _speedChangeCb(threadID, speed);
    }

    // =========================================================================
    // Cancel all pending debounced events for a thread (no firing).
    // Call from HandleEnd when you want to suppress stale events without firing.
    // Normally you want FlushSceneChange + FlushClimax instead.
    // =========================================================================
    void CancelAll(int threadID) {
        std::lock_guard<std::mutex> lk(_mutex);
        _pendingSceneID.erase(threadID);
        _pendingClimax.erase(threadID);
        _pendingSpeed.erase(threadID);
        _proximityPaused.erase(threadID);
        CancelUnderLock({EventType::SceneChange, threadID});
        CancelUnderLock({EventType::Climax, threadID});
        CancelUnderLock({EventType::SpeedChange, threadID});
    }

    // =========================================================================
    // Lifecycle
    // =========================================================================

    void Start() {
        bool expected = false;
        if (!_active.compare_exchange_strong(expected, true)) return;
        {
            std::lock_guard<std::mutex> lk(_mutex);
            ScheduleNextScanUnderLock();
            ScheduleNextProximityCheckUnderLock();
        }
        _thread = std::thread([this]() { ThreadProc(); });
        SKSE::log::info("DebounceQueue: started");
    }

    void Stop() {
        _active.store(false, std::memory_order_relaxed);
        _cv.notify_one();
        if (_thread.joinable()) _thread.join();
        // Reset faction resolution flag for next Start()
        _factionsResolved = false;
        SKSE::log::info("DebounceQueue: stopped");
    }

private:
    using Clock     = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    // -------------------------------------------------------------------------
    // Priority queue internals
    // -------------------------------------------------------------------------

    enum class EventType : uint8_t { SceneChange, Climax, SpeedChange, SpectatorScan, ProximityCheck };

    struct EventKey {
        EventType type;
        int       threadID;
        bool operator==(const EventKey& o) const {
            return type == o.type && threadID == o.threadID;
        }
    };

    struct EventKeyHash {
        size_t operator()(const EventKey& k) const {
            return std::hash<int>()(static_cast<int>(k.type)) ^
                   (std::hash<int>()(k.threadID) * 2654435761u);
        }
    };

    struct QueueEntry {
        TimePoint  fireAt;
        EventType  type;
        int        threadID;
        uint64_t   generation;
        bool operator>(const QueueEntry& o) const { return fireAt > o.fireAt; }
    };

    struct PendingClimax {
        std::vector<ClimaxEntry>      events;
        std::vector<OcumAppliedEntry> cumApplied;
        std::vector<OcumSquirtEntry>  squirts;
    };

    // -------------------------------------------------------------------------
    // Spectator scanner state — game-thread only, no locking needed
    // (spectator data itself is now in ThreadRegistry; only faction/resolve flags live here)
    // -------------------------------------------------------------------------

    // -------------------------------------------------------------------------
    // Members
    // -------------------------------------------------------------------------

    SceneChangeCb        _sceneChangeCb;
    ClimaxCb             _climaxCb;
    SpeedChangeCb        _speedChangeCb;
    GetActiveThreadsFunc _getActiveThreads;

    std::mutex              _mutex;
    std::condition_variable _cv;
    std::thread             _thread;
    std::atomic<bool>       _active{false};

    // Priority queue sorted by fire time (min at top)
    std::priority_queue<QueueEntry, std::vector<QueueEntry>, std::greater<QueueEntry>> _queue;
    std::unordered_map<EventKey, uint64_t, EventKeyHash> _currentGeneration;

    // Per-thread pending data — accessed under _mutex
    std::unordered_map<int, std::string>   _pendingSceneID;
    std::unordered_map<int, PendingClimax> _pendingClimax;
    std::unordered_map<int, int32_t>       _pendingSpeed;

    struct ProximityPauseState { bool wasInAutoMode; };
    std::unordered_map<int, ProximityPauseState> _proximityPaused;

    // Spectator scanner state — game-thread only
    bool            _factionsResolved{false};
    RE::TESFaction* _spectatorFaction{nullptr};
    RE::TESFaction* _sexlabFaction{nullptr};
    RE::TESFaction* _ostimCountFaction{nullptr};

    // -------------------------------------------------------------------------
    // Queue helpers — must be called under _mutex
    // -------------------------------------------------------------------------

    // Schedule or reschedule a debounced event.
    // Increments the generation (invalidates any existing queue entry for this key)
    // and pushes a new entry with the given debounce duration.
    template <class Duration>
    void RescheduleUnderLock(EventKey key, Duration debounce) {
        uint64_t gen = ++_currentGeneration[key];
        _queue.push({Clock::now() + debounce, key.type, key.threadID, gen});
        _cv.notify_one();
    }

    // Cancel a debounced event by invalidating its generation.
    // The stale queue entry is harmlessly skipped when the background thread pops it.
    void CancelUnderLock(EventKey key) {
        ++_currentGeneration[key];
    }

    // Schedule the next periodic spectator scan.
    void ScheduleNextScanUnderLock() {
        EventKey key{EventType::SpectatorScan, -1};
        uint64_t gen = ++_currentGeneration[key];
        int intervalSec = Config::GetSingleton().SpectatorScanIntervalSeconds();
        _queue.push({Clock::now() + std::chrono::seconds(intervalSec), EventType::SpectatorScan, -1, gen});
        _cv.notify_one();
    }

    void ScheduleNextProximityCheckUnderLock() {
        EventKey key{EventType::ProximityCheck, -1};
        uint64_t gen = ++_currentGeneration[key];
        int intervalSec = Config::GetSingleton().SpectatorScanIntervalSeconds();
        _queue.push({Clock::now() + std::chrono::seconds(intervalSec), EventType::ProximityCheck, -1, gen});
        _cv.notify_one();
    }

    // -------------------------------------------------------------------------
    // Background thread
    // -------------------------------------------------------------------------

    // Payload extracted under the mutex for hand-off to AddTask.
    struct FireData {
        EventType       type;
        int             threadID;
        std::string     sceneID;  // SceneChange only
        ClimaxBatchData climax;   // Climax only
        int32_t         speed{0}; // SpeedChange only
    };

    void ThreadProc() {
        SKSE::log::info("DebounceQueue: background thread started");
        std::unique_lock<std::mutex> lk(_mutex);

        while (_active.load(std::memory_order_relaxed)) {
            // Sleep until the next scheduled entry, or indefinitely if the queue is empty.
            if (_queue.empty()) {
                _cv.wait(lk);
                continue;
            }
            auto wakeAt = _queue.top().fireAt;
            _cv.wait_until(lk, wakeAt);

            if (!_active.load(std::memory_order_relaxed)) break;

            auto now = Clock::now();
            std::vector<FireData> toFire;

            while (!_queue.empty() && _queue.top().fireAt <= now) {
                auto entry = _queue.top();
                _queue.pop();

                // Skip stale entries (generation mismatch means the event was rescheduled or cancelled).
                auto it = _currentGeneration.find({entry.type, entry.threadID});
                if (it == _currentGeneration.end() || it->second != entry.generation)
                    continue;

                // Extract payload data while still holding the lock.
                FireData fd;
                fd.type     = entry.type;
                fd.threadID = entry.threadID;

                if (entry.type == EventType::SceneChange) {
                    auto sit = _pendingSceneID.find(entry.threadID);
                    if (sit != _pendingSceneID.end()) {
                        fd.sceneID = std::move(sit->second);
                        _pendingSceneID.erase(sit);
                    }
                } else if (entry.type == EventType::Climax) {
                    auto cit = _pendingClimax.find(entry.threadID);
                    if (cit != _pendingClimax.end()) {
                        fd.climax.climaxEvents = std::move(cit->second.events);
                        fd.climax.cumApplied   = std::move(cit->second.cumApplied);
                        fd.climax.squirts      = std::move(cit->second.squirts);
                        _pendingClimax.erase(cit);
                    }
                } else if (entry.type == EventType::SpeedChange) {
                    auto sit = _pendingSpeed.find(entry.threadID);
                    if (sit != _pendingSpeed.end()) {
                        fd.speed = sit->second;
                        _pendingSpeed.erase(sit);
                    }
                } else if (entry.type == EventType::SpectatorScan) {
                    // Self-reschedule the periodic scan while still under the lock.
                    ScheduleNextScanUnderLock();
                } else if (entry.type == EventType::ProximityCheck) {
                    ScheduleNextProximityCheckUnderLock();
                }

                toFire.push_back(std::move(fd));
            }

            lk.unlock();

            // Dispatch fired events onto the game thread via AddTask.
            for (auto& fd : toFire) {
                if (auto* taskIF = SKSE::GetTaskInterface()) {
                    taskIF->AddTask([this, fd = std::move(fd)]() mutable {
                        DispatchFireData(std::move(fd));
                    });
                }
            }

            lk.lock();
        }

        SKSE::log::info("DebounceQueue: background thread stopped");
    }

    // Called on the game thread by AddTask.
    void DispatchFireData(FireData fd) {
        switch (fd.type) {
            case EventType::SceneChange:
                SKSE::log::info("DebounceQueue: thread {} scene change debounce resolved", fd.threadID);
                _sceneChangeCb(fd.threadID, fd.sceneID);
                break;
            case EventType::Climax:
                SKSE::log::info("DebounceQueue: thread {} climax batch debounce resolved ({} climax)",
                                fd.threadID, fd.climax.climaxEvents.size());
                _climaxCb(fd.threadID, fd.climax);
                break;
            case EventType::SpeedChange:
                SKSE::log::info("DebounceQueue: thread {} speed change debounce resolved (speed={})",
                                fd.threadID, fd.speed);
                _speedChangeCb(fd.threadID, fd.speed);
                break;
            case EventType::SpectatorScan:
                PerformScan();
                break;
            case EventType::ProximityCheck:
                PerformProximityCheck();
                break;
        }
    }

    // =========================================================================
    // Spectator scanner implementation — game-thread only
    // (ported verbatim from SpectatorScanner.h)
    // =========================================================================

    void ResolveFactions() {
        if (_factionsResolved) return;
        _factionsResolved  = true;
        _spectatorFaction  = RE::TESForm::LookupByEditorID<RE::TESFaction>("TTON_SpectatorFaction");
        _sexlabFaction     = RE::TESForm::LookupByEditorID<RE::TESFaction>("SexLabAnimatingFaction");
        _ostimCountFaction = RE::TESForm::LookupByEditorID<RE::TESFaction>("OStimActorCountFaction");
        SKSE::log::info("DebounceQueue: factions resolved — spectator={} sexlab={} ostim={}",
            _spectatorFaction  ? "ok" : "null",
            _sexlabFaction     ? "ok" : "null",
            _ostimCountFaction ? "ok" : "null");
    }

    bool IsExcludedByFaction(RE::Actor* actor) const {
        if (_spectatorFaction  && actor->IsInFaction(_spectatorFaction))  return true;
        if (_sexlabFaction     && actor->IsInFaction(_sexlabFaction))     return true;
        if (_ostimCountFaction && actor->IsInFaction(_ostimCountFaction)) return true;
        return false;
    }

    static bool HasSpectatorRelationship(RE::Actor* nearby, RE::Actor* threadActor) {
        auto* nearbyBase = nearby      ? nearby->GetActorBase()      : nullptr;
        auto* threadBase = threadActor ? threadActor->GetActorBase() : nullptr;
        if (!nearbyBase || !threadBase || nearbyBase == threadBase) return false;

        auto* rel = RE::BGSRelationship::GetRelationship(nearbyBase, threadBase);
        if (!rel) return false;

        if (rel->level == RE::BGSRelationship::RELATIONSHIP_LEVEL::kLover) return true;

        if (rel->assocType) {
            const char* edid = rel->assocType->GetFormEditorID();
            if (edid && *edid) {
                std::string s(edid);
                std::transform(s.begin(), s.end(), s.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (s.find("spouse")   != std::string::npos) return true;
                if (s.find("courting") != std::string::npos) return true;
            }
        }
        return false;
    }

    void PerformScan() {
        if (!Config::GetSingleton().EnableSpectators()) {
            SKSE::log::info("DebounceQueue: spectator scan skipped — disabled in config");
            return;
        }

        auto activeThreads = _getActiveThreads();
        if (activeThreads.empty()) {
            SKSE::log::info("DebounceQueue: spectator scan skipped — no active threads");
            return;
        }

        // SKSE::log::info("DebounceQueue: spectator scan started — {} active thread(s)", activeThreads.size());

        ResolveFactions();

        std::unordered_set<RE::FormID>          ostimActorIDs;
        std::vector<std::pair<int, RE::Actor*>> threadActors;

        for (const auto& [tid, formIDs] : activeThreads) {
            for (uint32_t fid : formIDs) {
                ostimActorIDs.insert(fid);
                if (auto* a = RE::TESForm::LookupByID<RE::Actor>(fid))
                    threadActors.emplace_back(tid, a);
            }
        }
        if (threadActors.empty()) {
            SKSE::log::info("DebounceQueue: spectator scan skipped — no thread actors found");
            return;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* pl     = RE::ProcessLists::GetSingleton();
        if (!player || !pl) {
            SKSE::log::info("DebounceQueue: spectator scan skipped — player or process lists not found");
            return;
        }

        const RE::NiPoint3 playerPos = player->GetPosition();

        std::unordered_set<RE::FormID> foundThisScan;

        int checkedCount   = 0;
        int skippedOStim   = 0;
        int skippedFaction = 0;
        int skippedRange   = 0;
        int newSpectators  = 0;

        for (auto& handle : pl->highActorHandles) {
            RE::Actor* actor = handle.get().get();
            if (!actor || actor == static_cast<RE::Actor*>(player)) continue;

            if (ostimActorIDs.count(actor->GetFormID())) { ++skippedOStim; continue; }
            if (IsExcludedByFaction(actor))               { ++skippedFaction; continue; }

            const RE::NiPoint3 pos = actor->GetPosition();
            float dx = pos.x - playerPos.x, dy = pos.y - playerPos.y, dz = pos.z - playerPos.z;
            float r = Config::GetSingleton().SpectatorScanRadius();
            if (dx * dx + dy * dy + dz * dz > r * r) { ++skippedRange; continue; }

            ++checkedCount;
            RE::FormID candidateID = actor->GetFormID();
            foundThisScan.insert(candidateID);

            if (ThreadRegistry::GetSingleton().IsSpectatorAlreadyNotified(candidateID)) continue;

            for (const auto& [tid, threadActor] : threadActors) {
                if (!HasSpectatorRelationship(actor, threadActor)) continue;

                RE::FormID targetID = threadActor->GetFormID();
                ThreadRegistry::GetSingleton().AddSpectator(tid, candidateID, targetID);
                ++newSpectators;

                SKSE::log::info("DebounceQueue: {} is spectator for thread {} (target {})",
                    actor->GetDisplayFullName(), tid, threadActor->GetDisplayFullName());

                std::string json;
                json += "{\"spectatorFormID\":";   json += std::to_string(candidateID);
                json += ",\"targetActorFormID\":"; json += std::to_string(targetID);
                json += ",\"threadID\":";          json += std::to_string(tid);
                json += "}";

                FireModEvent("ostimnet_add_spectator", json.c_str(), static_cast<float>(tid), actor);
                break;
            }
        }
    }

    void DispatchStopAutoMode(int threadID) {
        if (auto* taskIF = SKSE::GetTaskInterface()) {
            taskIF->AddTask([threadID]() {
                auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
                if (vm) {
                    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> nullCallback;
                    auto arg = threadID; // Capture correctly
                    vm->DispatchStaticCall("OThread", "StopAutoMode", RE::MakeFunctionArguments(std::move(arg)), nullCallback);
                }
            });
        }
    }

    void DispatchStartAutoMode(int threadID) {
        if (auto* taskIF = SKSE::GetTaskInterface()) {
            taskIF->AddTask([threadID]() {
                auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
                if (vm) {
                    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> nullCallback;
                    auto arg = threadID; // Capture correctly
                    vm->DispatchStaticCall("OThread", "StartAutoMode", RE::MakeFunctionArguments(std::move(arg)), nullCallback);
                }
            });
        }
    }

    void PerformProximityCheck() {
        auto activeThreads = _getActiveThreads();
        if (activeThreads.empty()) return;

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return;

        const RE::NiPoint3 playerPos = player->GetPosition();
        float r = Config::GetSingleton().ProximityPauseRadius();
        float rSq = r * r;

        for (const auto& [tid, formIDs] : activeThreads) {
            if (formIDs.empty()) continue;
            
            bool hasPlayer = false;
            for (uint32_t fid : formIDs) {
                if (fid == player->GetFormID()) {
                    hasPlayer = true;
                    break;
                }
            }
            if (hasPlayer) continue;

            auto* actor0 = RE::TESForm::LookupByID<RE::Actor>(formIDs[0]);
            if (!actor0) continue;

            const RE::NiPoint3 pos = actor0->GetPosition();
            float dx = pos.x - playerPos.x;
            float dy = pos.y - playerPos.y;
            float dz = pos.z - playerPos.z;

            SKSE::log::info("DebounceQueue: proximity check for thread {} (distance={}, radius={})", tid, std::sqrt(dx * dx + dy * dy + dz * dz), r);

            bool isNear = (dx * dx + dy * dy + dz * dz <= rSq);

            bool isPaused = false;
            {
                std::lock_guard<std::mutex> lk(_mutex);
                isPaused = _proximityPaused.contains(tid);
            }

            if (isNear && !isPaused) {
                bool wasAutoMode = false;
                if (g_ostimThreadInterface) {
                    wasAutoMode = g_ostimThreadInterface->IsAutoMode(tid);
                }
                
                {
                    std::lock_guard<std::mutex> lk(_mutex);
                    _proximityPaused[tid] = { wasAutoMode };
                }

                if (wasAutoMode) {
                    DispatchStopAutoMode(tid);
                }
                ScheduledEvalService::GetSingleton().PauseThread(tid);
                SKSE::log::info("DebounceQueue: proximity pause triggered for thread {} (autoMode={})", tid, wasAutoMode);

            } else if (!isNear && isPaused) {
                bool wasAutoMode = false;
                {
                    std::lock_guard<std::mutex> lk(_mutex);
                    wasAutoMode = _proximityPaused[tid].wasInAutoMode;
                    _proximityPaused.erase(tid);
                }

                if (wasAutoMode) {
                    DispatchStartAutoMode(tid);
                }
                ScheduledEvalService::GetSingleton().ResumeThread(tid);
                SKSE::log::info("DebounceQueue: proximity pause ended for thread {}, autoMode restored", tid);
            }
        }
    }
};

}  // namespace OStimNet
