#pragma once

#include "ui/MagelightUI_API.h"
#include "ui/SessionEpoch.h"

#include <RE/Skyrim.h>
#include <Windows.h>

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace OStimNet::UI {

    /// Manages lifecycle, UI-mode focus, visibility, and JS bridge for OStimNet's Magelight UI modal view.
    class UIManager final : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
    public:
        using JSListenerCallback = MAGELIGHT_API::JsListenerFn;

        static UIManager& GetSingleton();

        UIManager(const UIManager&) = delete;
        UIManager& operator=(const UIManager&) = delete;
        UIManager(UIManager&&) = delete;
        UIManager& operator=(UIManager&&) = delete;

        /// Records the calling thread as Skyrim's main thread (call from SKSEPluginLoad).
        void CaptureMainThread() noexcept {
            mainThreadId_.store(::GetCurrentThreadId(), std::memory_order_release);
        }

        /// Acquires MAGELIGHT_API::MagelightApi4 and creates the persistent hidden modal view (no-op if host absent).
        void Initialize();

        /// Starts listening for menu open/close events so modals defer cleanly behind blocking Skyrim menus.
        void StartListeningMenuEvents();

        [[nodiscard]] bool IsInitialized() const noexcept {
            return magelight_ != nullptr && modHandle_ != 0 && view_ != 0 && magelight_->IsViewValid &&
                   magelight_->IsViewValid(view_);
        }
        [[nodiscard]] bool IsDomReady() const noexcept { return isDomReady_.load(std::memory_order_acquire); }
        [[nodiscard]] bool IsFocusOwned() const noexcept { return focusOwned_.load(std::memory_order_acquire); }
        [[nodiscard]] bool IsOnMainThread() const noexcept {
            if (IsInGameTaskScope()) {
                return true;
            }
            const DWORD mainId = mainThreadId_.load(std::memory_order_acquire);
            return mainId != 0 && ::GetCurrentThreadId() == mainId;
        }
        [[nodiscard]] bool IsModalDisplayBlocked() const;
        [[nodiscard]] MAGELIGHT_API::ViewId GetView() const noexcept { return view_; }

        /// Registers a JavaScript listener on the active UI view.
        bool RegisterJSListener(const char* eventName, JSListenerCallback callback);

        /// Marshals a task from a render-thread UI callback onto the Skyrim game thread safely.
        void PostToGameThread(std::function<void()> task);

        /// Called when React App reports `uiReady` on the game thread.
        void OnUIReady();

        /// Reconciles UI view visibility, UI-mode focus, pause state, and Escape capture.
        void SyncViewVisibilityAndFocus();

        /// Queues a callback to run on the main thread once UI-mode focus has cleanly settled as released.
        void RunAfterFocusReleased(std::function<void()> task);

        /// Invokes a named JS function (`window.<functionName>(<jsonData>)`) on the UI view if ready.
        void SendToUI(const std::string& functionName, const std::string& jsonData);

        /// Reverts transient visibility/focus state on game load / new-game transition (preserves persistent view).
        void Revert();

        /// Snapshots enabled player control flags at OStim player thread start.
        void OnPlayerThreadStart();

        /// Restores and verifies player control flags across OStim player thread teardown and fade-out.
        void OnPlayerThreadEnd();

        RE::BSEventNotifyControl ProcessEvent(
            const RE::MenuOpenCloseEvent* a_event,
            RE::BSTEventSource<RE::MenuOpenCloseEvent>* a_eventSource) override;

    private:
        UIManager() = default;
        ~UIManager() override = default;

        static void OnMagelightEvent(const MAGELIGHT_API::EventData* ev, void* user);
        static void OnUIModeExitCallback(MAGELIGHT_API::ViewId view);
        static void OnOStimNetAction(const char* argument);
        static void OnLogFromUI(const char* argument);
        static void GameTaskTrampoline(void* userData);

        void HandleUIModeExited();
        void SnapshotControlBaseline(bool forModal, bool forScene);
        void ApplyControlMaskRestore(
            std::uint32_t mask,
            std::uint32_t forceNativePulseMask,
            const char* trigger);
        void RestoreModalControls(bool clearMaskWhenFocusMenuStillOpen = false);
        void RestoreSceneEndControls(const char* trigger, bool finalizeWindow);
        void ReleaseFocusAndUIMode();
        void DrainAfterFocusReleasedTasks();
        void FlushPendingAfterDomReady();

        const MAGELIGHT_API::MagelightApi4* magelight_{ nullptr };
        MAGELIGHT_API::ModId modHandle_{ 0 };
        MAGELIGHT_API::ViewId view_{ 0 };
        std::atomic<DWORD> mainThreadId_{ 0 };

        std::atomic<bool> isViewShown_{ false };
        std::atomic<bool> focusOwned_{ false };
        std::atomic<bool> uiModeRequested_{ false };
        std::atomic<bool> releasePending_{ false };
        std::atomic<bool> appliedNoTextEntry_{ true };
        std::atomic<bool> isDomReady_{ false };
        std::atomic<bool> menuSinkRegistered_{ false };
        std::atomic<bool> exitCallbackRegistered_{ false };
        std::atomic<bool> sceneEndRestorePending_{ false };
        std::atomic<std::uint32_t> modalControlBaselineMask_{ 0 };
        std::atomic<std::uint32_t> sceneControlBaselineMask_{ 0 };
        std::atomic<std::uint32_t> midSceneDirectWriteMask_{ 0 };
        std::atomic<std::uint64_t> sceneEndSequence_{ 0 };

        mutable std::mutex stateMutex_;
        std::vector<std::function<void()>> afterFocusReleasedTasks_;
        std::vector<std::pair<std::string, std::string>> pendingJsCalls_;
    };

}  // namespace OStimNet::UI
