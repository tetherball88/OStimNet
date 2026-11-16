#pragma once

#include "SKSE/SKSE.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <utility>

namespace OStimNet::UI {

    using SessionEpoch = std::uint64_t;

    namespace detail {
        inline std::atomic<SessionEpoch>& EpochCounter() noexcept {
            static std::atomic<SessionEpoch> counter{ 1 };
            return counter;
        }

        inline std::uint32_t& GameTaskDepth() noexcept {
            thread_local std::uint32_t depth = 0;
            return depth;
        }
    }  // namespace detail

    class ScopedGameTask {
    public:
        ScopedGameTask() noexcept { ++detail::GameTaskDepth(); }
        ~ScopedGameTask() noexcept { --detail::GameTaskDepth(); }
        ScopedGameTask(const ScopedGameTask&) = delete;
        ScopedGameTask& operator=(const ScopedGameTask&) = delete;
    };

    [[nodiscard]] inline bool IsInGameTaskScope() noexcept {
        return detail::GameTaskDepth() > 0;
    }

    [[nodiscard]] inline SessionEpoch GetSessionEpoch() noexcept {
        return detail::EpochCounter().load(std::memory_order_acquire);
    }

    inline SessionEpoch BumpSessionEpoch() noexcept {
        return detail::EpochCounter().fetch_add(1, std::memory_order_acq_rel) + 1;
    }

    [[nodiscard]] inline bool IsSessionEpochCurrent(SessionEpoch expected) noexcept {
        return expected != 0 && GetSessionEpoch() == expected;
    }

    inline bool AddEpochTask(SessionEpoch expectedEpoch, std::function<void()> fn) {
        if (!fn) {
            return false;
        }
        auto* taskIF = SKSE::GetTaskInterface();
        if (!taskIF) {
            return false;
        }
        taskIF->AddTask([expectedEpoch, fn = std::move(fn)]() {
            if (!IsSessionEpochCurrent(expectedEpoch)) {
                return;
            }
            ScopedGameTask scope;
            fn();
        });
        return true;
    }

    inline bool AddCurrentEpochTask(std::function<void()> fn) {
        return AddEpochTask(GetSessionEpoch(), std::move(fn));
    }

}  // namespace OStimNet::UI
