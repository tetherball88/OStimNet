#include "PCH.h"
#include "ScheduledEvalService.h"
#include "Config.h"
#include "ThreadRegistry.h"
#include "SkyrimNetIntegration.h"

namespace OStimNet {

ScheduledEvalService::~ScheduledEvalService() {
    StopLoop();
}

void ScheduledEvalService::OnThreadStart(int threadID) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_lastSceneChange[threadID] = std::chrono::steady_clock::now();
    SKSE::log::info("ScheduledEvalService: tracking thread {} ({} active)", threadID, m_lastSceneChange.size());

    if (m_lastSceneChange.size() == 1) {
        SKSE::log::info("ScheduledEvalService: first thread — starting background loop");
        StartLoop();
    }
}

void ScheduledEvalService::OnSceneChanged(int threadID) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_lastSceneChange.contains(threadID)) {
        m_lastSceneChange[threadID] = std::chrono::steady_clock::now();
        SKSE::log::info("ScheduledEvalService: timer reset for thread {} (scene changed)", threadID);
    }
}

void ScheduledEvalService::OnThreadEnd(int threadID) {
    bool shouldStop = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastSceneChange.erase(threadID);
        m_evaluationInFlight.erase(threadID);
        m_pausedThreads.erase(threadID);
        shouldStop = m_lastSceneChange.empty();
        SKSE::log::info("ScheduledEvalService: thread {} ended ({} remaining)", threadID, m_lastSceneChange.size());
    }

    if (shouldStop) {
        SKSE::log::info("ScheduledEvalService: last thread ended — stopping background loop");
        StopLoop();
    }
}

void ScheduledEvalService::ClearInFlight(int threadID) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_evaluationInFlight.erase(threadID);
    // Reset timer here so the full interval always counts from eval completion,
    // not queue time. Handles slow LLMs (latency > interval) correctly.
    if (m_lastSceneChange.contains(threadID)) {
        m_lastSceneChange[threadID] = std::chrono::steady_clock::now();
    }
    SKSE::log::info("ScheduledEvalService: cleared in-flight for thread {}, timer reset", threadID);
}

void ScheduledEvalService::PauseThread(int threadID) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pausedThreads.insert(threadID);
    SKSE::log::info("ScheduledEvalService: thread {} paused", threadID);
}

void ScheduledEvalService::ResumeThread(int threadID) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pausedThreads.erase(threadID);
    if (m_lastSceneChange.contains(threadID)) {
        m_lastSceneChange[threadID] = std::chrono::steady_clock::now();
    }
    SKSE::log::info("ScheduledEvalService: thread {} resumed, timer reset", threadID);
}

void ScheduledEvalService::StartLoop() {
    if (m_running) return;

    m_running = true;
    m_thread = std::thread([this]() {
        RunLoop();
    });
}

void ScheduledEvalService::StopLoop() {
    if (!m_running) return;

    m_running = false;
    m_cv.notify_all();

    if (m_thread.joinable()) {
        m_thread.join();
    }
    SKSE::log::info("ScheduledEvalService: background loop stopped");
}

void ScheduledEvalService::RunLoop() {
    SKSE::log::info("ScheduledEvalService: background loop started");
    auto lastCheckTime = std::chrono::steady_clock::now();

    while (m_running) {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            if (m_cv.wait_for(lock, std::chrono::seconds(5), [this] { return !m_running; })) {
                break; // m_running became false
            }
        }

        std::vector<int> activeThreads;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (const auto& [threadID, _] : m_lastSceneChange) {
                activeThreads.push_back(threadID);
            }
        }

        auto now = std::chrono::steady_clock::now();
        int intervalSecs = Config::GetSingleton().ScheduledEvalIntervalSeconds();

        for (int threadID : activeThreads) {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (m_evaluationInFlight.contains(threadID)) {
                    SKSE::log::debug("ScheduledEvalService: thread {} — evaluation in-flight, skipping", threadID);
                    continue;
                }

                if (m_pausedThreads.contains(threadID)) {
                    SKSE::log::debug("ScheduledEvalService: thread {} — paused, skipping", threadID);
                    continue;
                }

                if (!m_lastSceneChange.contains(threadID)) {
                    SKSE::log::debug("ScheduledEvalService: thread {} — not in lastSceneChange, skipping", threadID);
                    continue; // thread ended since snapshot
                }

                // Don't count time spent while the game is paused (menu open, etc.).
                // Slide the timer forward on every paused tick so the interval only
                // measures active gameplay time.
                auto* mainSingleton = RE::Main::GetSingleton();
                if (mainSingleton && !mainSingleton->GetRuntimeData().gameActive) {
                    auto overlapStart = std::max(m_lastSceneChange[threadID], lastCheckTime);
                    if (now > overlapStart) {
                        m_lastSceneChange[threadID] += (now - overlapStart);
                    }
                    SKSE::log::debug("ScheduledEvalService: thread {} — game paused, sliding timer forward", threadID);
                    continue;
                }

                auto lastChange = m_lastSceneChange[threadID];
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastChange).count();
                if (elapsed < intervalSecs) {
                    SKSE::log::debug("ScheduledEvalService: thread {} — {}s elapsed, interval {}s, not yet due", threadID, elapsed, intervalSecs);
                    continue;
                }
            }

            auto& store = ThreadDataStore::GetSingleton();
            if (!store.IsOStimNet(threadID)) {
                SKSE::log::debug("ScheduledEvalService: thread {} — not OStimNet, skipping", threadID);
                continue;
            }
            auto sexual = store.GetSexual(threadID);
            if (!sexual.value_or(false)) {
                SKSE::log::debug("ScheduledEvalService: thread {} — not sexual, skipping", threadID);
                continue;
            }

            auto actors = store.GetActorPtrs(threadID);
            bool hasPlayer = false;
            auto* player = RE::PlayerCharacter::GetSingleton();
            for (auto* actor : actors) {
                if (actor == player) {
                    hasPlayer = true;
                    break;
                }
            }

            bool playerEval = Config::GetSingleton().ScheduledEvalPlayerThread();
            bool npcEval    = Config::GetSingleton().ScheduledEvalNpcThreads();

            if (!playerEval && !npcEval) {
                SKSE::log::debug("ScheduledEvalService: thread {} — both bools off, skipping", threadID);
                continue;
            }
            if (hasPlayer && !playerEval) {
                SKSE::log::debug("ScheduledEvalService: thread {} — player thread but scheduledEvalPlayerThread=false, skipping", threadID);
                continue;
            }
            if (!hasPlayer && !npcEval) {
                SKSE::log::debug("ScheduledEvalService: thread {} — NPC thread but scheduledEvalNpcThreads=false, skipping", threadID);
                continue;
            }

            SKSE::log::info("ScheduledEvalService: thread {} — interval elapsed, queuing GM scene advance (hasPlayer={})", threadID, hasPlayer);

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_evaluationInFlight.insert(threadID);
            }

            SkyrimNetIntegration::EvaluateScheduledSceneAdvance(threadID, [this, threadID]() {
                ClearInFlight(threadID);
            });
        }

        lastCheckTime = now;
    }
    SKSE::log::info("ScheduledEvalService: background loop exited");
}

} // namespace OStimNet
