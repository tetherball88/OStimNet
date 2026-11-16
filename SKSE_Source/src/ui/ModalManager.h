#pragma once

#include "ui/ModalTypes.h"
#include "ui/SessionEpoch.h"

#include <nlohmann/json.hpp>

#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace OStimNet::UI {

    /// Thread-safe FIFO queue and dispatcher for standalone Magelight UI modal dialogs.
    class ModalManager {
    public:
        static ModalManager& GetSingleton();

        ModalManager(const ModalManager&) = delete;
        ModalManager& operator=(const ModalManager&) = delete;
        ModalManager(ModalManager&&) = delete;
        ModalManager& operator=(ModalManager&&) = delete;

        /// Enqueues a structured modal request. Safe from any thread; returns assigned `modalId`.
        std::string ShowModal(ModalRequest request, ModalCallback callback);

        /// Enqueues a pre-built modal spec JSON string. Safe from any thread; returns assigned `modalId`.
        std::string ShowRawJsonModal(const std::string& modalSpecJson, ModalCallback callback);

        /// Handles a `modalResponse` payload received from the UI frontend.
        bool HandleModalResponse(const nlohmann::json& payload);

        [[nodiscard]] bool HasActiveModal() const;
        [[nodiscard]] bool ActiveModalHasTextInput() const;
        [[nodiscard]] std::string GetActiveModalId() const;

        /// Pushes the active modal payload to the UI view if ready (no-op if none active).
        void SendActiveModalToUI(bool forceResend = false);

        /// Promotes the next queued modal on the main game thread and updates UI focus.
        void ProcessQueueOnGameThread();

        /// Cancels all active and queued modals cleanly (with `aborted = true`) on save load/revert.
        void Revert();

    private:
        ModalManager() = default;
        ~ModalManager() = default;

        struct ModalEntry {
            std::string modalId;
            std::string specJson;
            std::vector<std::string> buttonIds{};
            bool hasTextInput{ false };
            ModalCallback callback;
            SessionEpoch expectedEpoch{ 0 };
        };

        std::string EnqueueEntry(std::string modalId,
                                 std::string specJson,
                                 std::vector<std::string> buttonIds,
                                 bool hasTextInput,
                                 ModalCallback callback);
        void ScheduleProcessQueue();

        mutable std::mutex mutex_;
        std::optional<ModalEntry> activeModal_;
        std::deque<ModalEntry> queue_;
        std::string lastSentModalId_;
        std::atomic<std::uint64_t> nextId_{ 1 };
    };

}  // namespace OStimNet::UI
