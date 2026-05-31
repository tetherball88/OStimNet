#pragma once
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <chrono>

namespace OStimNet {

class ScheduledEvalService {
public:
    static ScheduledEvalService& GetSingleton() {
        static ScheduledEvalService instance;
        return instance;
    }

    void OnThreadStart(int threadID);
    void OnSceneChanged(int threadID);
    void OnThreadEnd(int threadID);
    void ClearInFlight(int threadID);
    void PauseThread(int threadID);
    void ResumeThread(int threadID);

private:
    ScheduledEvalService() = default;
    ~ScheduledEvalService();

    std::unordered_map<int, std::chrono::steady_clock::time_point> m_lastSceneChange;
    std::unordered_set<int> m_evaluationInFlight;
    std::unordered_set<int> m_pausedThreads;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::atomic<bool> m_running{false};
    std::thread m_thread;

    void StartLoop();
    void StopLoop();
    void RunLoop();
};

} // namespace OStimNet
