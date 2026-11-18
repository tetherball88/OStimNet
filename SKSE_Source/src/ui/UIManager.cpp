#include "PCH.h"

#include "ui/UIManager.h"
#include "ThreadRegistry.h"
#include "ui/ModalManager.h"
#include "ui/SessionEpoch.h"

#include <RE/U/UserEventEnabled.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <filesystem>
#include <memory>
#include <thread>
#include <utility>

namespace OStimNet::UI {

    namespace {

        [[nodiscard]] std::string GetModalHtmlPath() {
            wchar_t buf[MAX_PATH]{};
            ::GetModuleFileNameW(nullptr, buf, MAX_PATH);
            const auto path = std::filesystem::path(buf).parent_path() /
                              L"Data" / L"Magelight" / L"OStimNet" / L"views" / L"OStimNet" / L"index.html";
            const auto u8 = path.u8string();
            return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
        }

        [[nodiscard]] bool CapabilitySupported(
            const MAGELIGHT_API::MagelightApi4* api,
            std::uint32_t minVersion,
            const char* capabilityName) noexcept {
            if (!api || api->hostVersionNumber < minVersion) {
                return false;
            }
            if (!capabilityName || !*capabilityName || !api->QueryCapability) {
                return true;
            }
            return api->QueryCapability(capabilityName) == 1;
        }

        constexpr std::uint32_t kCoreGameplayAndMenuMask =
            std::to_underlying(RE::ControlMap::UEFlag::kMovement) |
            std::to_underlying(RE::ControlMap::UEFlag::kLooking) |
            std::to_underlying(RE::ControlMap::UEFlag::kActivate) |
            std::to_underlying(RE::ControlMap::UEFlag::kMenu) |
            std::to_underlying(RE::ControlMap::UEFlag::kPOVSwitch) |
            std::to_underlying(RE::ControlMap::UEFlag::kWheelZoom);

        constexpr std::uint32_t kFullSnapshotMask =
            kCoreGameplayAndMenuMask |
            std::to_underlying(RE::ControlMap::UEFlag::kFighting);

    }  // namespace

    UIManager& UIManager::GetSingleton() {
        static UIManager instance;
        return instance;
    }

    void UIManager::Initialize() {
        DWORD expectedThread = 0;
        mainThreadId_.compare_exchange_strong(
            expectedThread,
            ::GetCurrentThreadId(),
            std::memory_order_acq_rel);

        if (IsInitialized()) {
            return;
        }

        if (!magelight_) {
            magelight_ = MAGELIGHT_API::RequestApi4();
        }
        if (!magelight_) {
            SKSE::log::warn("[UIManager] Magelight.dll v4 API unavailable; UI modals disabled");
            return;
        }

        if (modHandle_ == 0) {
            MAGELIGHT_API::ModDesc modDesc{};
            modDesc.size = sizeof(MAGELIGHT_API::ModDesc);
            modDesc.modId = "OStimNet";
            modDesc.displayName = "OStimNet";
            modDesc.modVersion = MAGELIGHT_API::PackVersion(1, 0, 0);
            modDesc.minHostVersion = MAGELIGHT_API::PackVersion(0, 28, 0);
            modDesc.callbackThread = MAGELIGHT_API::CallbackThread::GameThread;
            modDesc.onEvent = &UIManager::OnMagelightEvent;
            modDesc.onLog = nullptr;
            modDesc.user = this;
            modDesc.sessionName = nullptr;

            const auto modRes = magelight_->RegisterMod(&modDesc, &modHandle_);
            if (modRes != MAGELIGHT_API::Result::Ok || modHandle_ == 0) {
                modHandle_ = 0;
                SKSE::log::error("[UIManager] RegisterMod failed (result={})", static_cast<int>(modRes));
                return;
            }

            if (magelight_->hostVersionNumber >= 3000 && magelight_->SetNetworkPolicy) {
                const auto netRes = magelight_->SetNetworkPolicy(modHandle_, MAGELIGHT_API::NetworkPolicy::FileOnly);
                if (netRes != MAGELIGHT_API::Result::Ok) {
                    SKSE::log::warn("[UIManager] SetNetworkPolicy(FileOnly) failed (result={})", static_cast<int>(netRes));
                }
            }
        }

        if (magelight_->hostVersionNumber >= 901 && magelight_->SetUIModeExitCallback &&
            !exitCallbackRegistered_.exchange(true, std::memory_order_acq_rel)) {
            magelight_->SetUIModeExitCallback(&UIManager::OnUIModeExitCallback);
        }

        isDomReady_.store(false, std::memory_order_release);
        isViewShown_.store(false, std::memory_order_release);

        const std::string pagePath = GetModalHtmlPath();

        MAGELIGHT_API::ViewDesc viewDesc{};
        viewDesc.size = sizeof(MAGELIGHT_API::ViewDesc);
        viewDesc.name = "main";
        viewDesc.htmlPath = pagePath.c_str();
        viewDesc.anchor = MAGELIGHT_API::Anchor::TopLeft;
        viewDesc.x = 0;
        viewDesc.y = 0;
        viewDesc.w = 0;
        viewDesc.h = 0;
        viewDesc.fullscreen = true;
        viewDesc.clickThrough = false;
        viewDesc.startVisible = false;
        viewDesc.layer = MAGELIGHT_API::Layer::Popup;
        viewDesc.uiScale = 1.0f;
        viewDesc.onDomReady = nullptr;

        MAGELIGHT_API::ViewId createdView = 0;
        const auto viewRes = magelight_->CreateViewEx(modHandle_, &viewDesc, &createdView);
        if (viewRes != MAGELIGHT_API::Result::Ok || createdView == 0) {
            const char* errMsg = magelight_->GetLastErrorMessage ? magelight_->GetLastErrorMessage(modHandle_) : "";
            SKSE::log::error(
                "[UIManager] CreateViewEx failed for '{}' (result={}, error='{}')",
                pagePath,
                static_cast<int>(viewRes),
                errMsg ? errMsg : "");
            return;
        }
        view_ = createdView;

        if (CapabilitySupported(magelight_, 2800, "escapecapture") && magelight_->SetEscapeCapture) {
            magelight_->SetEscapeCapture(view_, true);
        }
        if (CapabilitySupported(magelight_, 3111, "prepaint") && magelight_->SetViewPrepaint) {
            magelight_->SetViewPrepaint(view_, true);
        }
        if (CapabilitySupported(magelight_, 10600, "gamepad") && magelight_->SetViewGamepad) {
            MAGELIGHT_API::GamepadConfig gp{};
            gp.size = sizeof(MAGELIGHT_API::GamepadConfig);
            gp.enabled = true;
            gp.style = MAGELIGHT_API::kGamepadStyleKeys;
            gp.allowCursor = true;
            gp.startInCursorMode = true;
            magelight_->SetViewGamepad(view_, &gp);
        }
        if (CapabilitySupported(magelight_, 10900, "focusShare") && magelight_->SetViewFocusSharing) {
            magelight_->SetViewFocusSharing(view_, MAGELIGHT_API::kFocusShareTake);
        }

        RegisterJSListener("ostimnetAction", &UIManager::OnOStimNetAction);
        RegisterJSListener("log", &UIManager::OnLogFromUI);

        SKSE::log::info(
            "[UIManager] Created Magelight view {} for '{}' (host={})",
            view_,
            pagePath,
            magelight_->hostVersion ? magelight_->hostVersion : "unknown");
    }

    void UIManager::StartListeningMenuEvents() {
        if (!menuSinkRegistered_.exchange(true, std::memory_order_acq_rel)) {
            if (auto* ui = RE::UI::GetSingleton()) {
                ui->RemoveEventSink<RE::MenuOpenCloseEvent>(this);
                ui->AddEventSink<RE::MenuOpenCloseEvent>(this);
                SKSE::log::info("[UIManager] Registered MenuOpenCloseEvent sink");
            } else {
                menuSinkRegistered_.store(false, std::memory_order_release);
                SKSE::log::warn("[UIManager] UI singleton unavailable for MenuOpenCloseEvent sink");
            }
        }
    }

    void UIManager::Revert() {
        modalControlBaselineMask_.store(0, std::memory_order_release);
        sceneControlBaselineMask_.store(0, std::memory_order_release);
        midSceneDirectWriteMask_.store(0, std::memory_order_release);
        sceneEndRestorePending_.store(false, std::memory_order_release);
        sceneEndSequence_.fetch_add(1, std::memory_order_acq_rel);

        if (!magelight_ || !magelight_->IsUIModeActive || !magelight_->IsUIModeActive()) {
            focusOwned_.store(false, std::memory_order_release);
            uiModeRequested_.store(false, std::memory_order_release);
            releasePending_.store(false, std::memory_order_release);
            appliedNoTextEntry_.store(true, std::memory_order_release);
        }

        {
            std::lock_guard lock(stateMutex_);
            afterFocusReleasedTasks_.clear();
            pendingJsCalls_.clear();
        }

        if (isDomReady_.load(std::memory_order_acquire)) {
            SendToUI("clearOStimNetModal", "");
        }

        SyncViewVisibilityAndFocus();
    }

    void UIManager::OnPlayerThreadStart() {
        sceneEndRestorePending_.store(false, std::memory_order_release);
        sceneEndSequence_.fetch_add(1, std::memory_order_acq_rel);

        if (!IsOnMainThread()) {
            AddCurrentEpochTask([this]() {
                OnPlayerThreadStart();
            });
            return;
        }

        SnapshotControlBaseline(/*forModal=*/false, /*forScene=*/true);
    }

    void UIManager::OnPlayerThreadEnd() {
        if (sceneControlBaselineMask_.load(std::memory_order_acquire) == 0 &&
            midSceneDirectWriteMask_.load(std::memory_order_acquire) == 0) {
            return;
        }

        const bool alreadyPending = sceneEndRestorePending_.exchange(true, std::memory_order_acq_rel);
        if (IsOnMainThread()) {
            RestoreSceneEndControls("ostim_thread_end", /*finalizeWindow=*/false);
        }
        AddCurrentEpochTask([this]() {
            RestoreSceneEndControls("ostim_thread_end next-frame", /*finalizeWindow=*/false);
        });

        if (!alreadyPending) {
            const std::uint64_t seq = sceneEndSequence_.fetch_add(1, std::memory_order_acq_rel) + 1;
            const SessionEpoch epoch = GetSessionEpoch();

            std::thread([this, seq, epoch]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(750));
                if (!IsSessionEpochCurrent(epoch) ||
                    sceneEndSequence_.load(std::memory_order_acquire) != seq ||
                    !sceneEndRestorePending_.load(std::memory_order_acquire)) {
                    return;
                }
                AddEpochTask(epoch, [this, seq]() {
                    if (sceneEndSequence_.load(std::memory_order_acquire) == seq) {
                        RestoreSceneEndControls("post-end +750ms", /*finalizeWindow=*/false);
                    }
                });

                std::this_thread::sleep_for(std::chrono::milliseconds(1100));
                if (!IsSessionEpochCurrent(epoch) ||
                    sceneEndSequence_.load(std::memory_order_acquire) != seq ||
                    !sceneEndRestorePending_.load(std::memory_order_acquire)) {
                    return;
                }
                AddEpochTask(epoch, [this, seq]() {
                    if (sceneEndSequence_.load(std::memory_order_acquire) == seq) {
                        RestoreSceneEndControls("post-fade +1850ms", /*finalizeWindow=*/true);
                    }
                });
            }).detach();
        }
    }

    void UIManager::SnapshotControlBaseline(bool forModal, bool forScene) {
        if (!forModal && !forScene) {
            return;
        }

        auto* cm = RE::ControlMap::GetSingleton();
        if (!cm) {
            return;
        }

        const std::uint32_t rawEnabled = cm->GetRuntimeData().enabledControls.underlying();
        const bool magelightHoldingControls =
            (magelight_ && magelight_->IsUIModeActive && magelight_->IsUIModeActive()) ||
            (RE::UI::GetSingleton() && RE::UI::GetSingleton()->IsMenuOpen("MagelightFocus"));

        std::uint32_t snapshot = rawEnabled & kFullSnapshotMask;
        if (magelightHoldingControls) {
            snapshot |= kCoreGameplayAndMenuMask;
        }

        if (forModal) {
            modalControlBaselineMask_.fetch_or(snapshot, std::memory_order_acq_rel);
        }
        if (forScene) {
            sceneControlBaselineMask_.fetch_or(snapshot, std::memory_order_acq_rel);
        }

        SKSE::log::debug(
            "[UIManager] Snapshotted control baseline (modal=0x{:X}, scene=0x{:X}, rawEnabled=0x{:X}, magelightHeld={})",
            modalControlBaselineMask_.load(std::memory_order_acquire),
            sceneControlBaselineMask_.load(std::memory_order_acquire),
            rawEnabled,
            magelightHoldingControls);
    }

    void UIManager::ApplyControlMaskRestore(
        std::uint32_t mask,
        std::uint32_t forceNativePulseMask,
        const char* trigger) {
        if (mask == 0 && forceNativePulseMask == 0) {
            return;
        }

        auto* cm = RE::ControlMap::GetSingleton();
        if (!cm) {
            return;
        }

        using UEFlag = RE::ControlMap::UEFlag;
        auto& rd = cm->GetRuntimeData();
        const auto rdOffset =
            reinterpret_cast<std::uintptr_t>(&rd.enabledControls) - reinterpret_cast<std::uintptr_t>(cm);

        const std::uint32_t before = rd.enabledControls.underlying();
        const std::uint32_t missing = mask & ~before;
        const std::uint32_t pulseBits = forceNativePulseMask & (mask | forceNativePulseMask);
        const std::uint32_t toToggle = missing | pulseBits;
        std::uint32_t directFallback = 0;

        if (toToggle != 0) {
            if (pulseBits != 0) {
                rd.enabledControls.reset(static_cast<UEFlag>(pulseBits));
            }

            cm->ToggleControls(static_cast<UEFlag>(toToggle), true, false);

            directFallback = toToggle & ~rd.enabledControls.underlying();
            if (directFallback != 0) {
                // Skyrim's ControlMap::ToggleControls(..., true, false) ignores re-enabling flags
                // while PlayerCharacter is AI-driven (OStim's SetAIDriven(true)). Force the live
                // flags back on now and record them so native ToggleControls(true) is replayed at
                // scene end after SetAIDriven(false).
                midSceneDirectWriteMask_.fetch_or(directFallback, std::memory_order_acq_rel);
                if (ThreadRegistry::GetSingleton().GetPlayerThreadID().has_value()) {
                    sceneControlBaselineMask_.fetch_or(directFallback, std::memory_order_acq_rel);
                }

                const auto prev = rd.enabledControls;
                rd.enabledControls.set(static_cast<UEFlag>(directFallback));
                rd.storedControls.set(static_cast<UEFlag>(directFallback));
                RE::UserEventEnabled ev{ rd.enabledControls, prev };
                cm->SendEvent(std::addressof(ev));
            } else if (pulseBits != 0) {
                midSceneDirectWriteMask_.fetch_and(~pulseBits, std::memory_order_acq_rel);
            }
        }

        int clearedTextEntry = 0;
        if (rd.textEntryCount > 0) {
            auto* ui = RE::UI::GetSingleton();
            const bool textMenuOpen =
                ui && (ui->IsMenuOpen(RE::Console::MENU_NAME) || ui->IsMenuOpen("OStimSearchMenu"));
            if (!textMenuOpen) {
                for (int i = 0; i < 16 && rd.textEntryCount > 0; ++i) {
                    cm->AllowTextInput(false);
                    ++clearedTextEntry;
                }
            }
        }

        if (toToggle != 0 || clearedTextEntry > 0) {
            SKSE::log::info(
                "[UIManager] Control restore ({}) [offset=0x{:X}]: enabled 0x{:X} -> 0x{:X} (missing=0x{:X}, pulse=0x{:X}, directFallback=0x{:X}, textEntryCount={}, clearedTextEntry={})",
                trigger ? trigger : "unknown",
                rdOffset,
                before,
                rd.enabledControls.underlying(),
                missing,
                pulseBits,
                directFallback,
                static_cast<int>(rd.textEntryCount),
                clearedTextEntry);
        }
    }

    void UIManager::RestoreModalControls(bool clearMaskWhenFocusMenuStillOpen) {
        if (modalControlBaselineMask_.load(std::memory_order_acquire) == 0) {
            return;
        }

        if (!IsOnMainThread()) {
            AddCurrentEpochTask([this, clearMaskWhenFocusMenuStillOpen]() {
                RestoreModalControls(clearMaskWhenFocusMenuStillOpen);
            });
            return;
        }

        if (magelight_ && magelight_->IsUIModeActive && magelight_->IsUIModeActive()) {
            return;
        }

        const bool wantUIMode =
            ModalManager::GetSingleton().HasActiveModal() && !IsModalDisplayBlocked();
        if (wantUIMode &&
            (focusOwned_.load(std::memory_order_acquire) ||
             uiModeRequested_.load(std::memory_order_acquire))) {
            return;
        }

        bool clearedStaleOwnership = false;
        if (!wantUIMode) {
            const bool staleOwned = focusOwned_.exchange(false, std::memory_order_acq_rel);
            const bool staleRequested = uiModeRequested_.exchange(false, std::memory_order_acq_rel);
            const bool staleRelease = releasePending_.exchange(false, std::memory_order_acq_rel);
            if (staleOwned || staleRequested || staleRelease) {
                appliedNoTextEntry_.store(true, std::memory_order_release);
                clearedStaleOwnership = true;
            }
        }

        const bool keepMaskUntilFocusMenuCloses =
            !clearMaskWhenFocusMenuStillOpen &&
            RE::UI::GetSingleton() != nullptr &&
            RE::UI::GetSingleton()->IsMenuOpen("MagelightFocus");

        const std::uint32_t mask = keepMaskUntilFocusMenuCloses
            ? modalControlBaselineMask_.load(std::memory_order_acquire)
            : modalControlBaselineMask_.exchange(0, std::memory_order_acq_rel);

        ApplyControlMaskRestore(mask, /*forceNativePulseMask=*/0, "modal UI-mode exit");

        if (clearedStaleOwnership) {
            DrainAfterFocusReleasedTasks();
        }
    }

    void UIManager::RestoreSceneEndControls(const char* trigger, bool finalizeWindow) {
        if (!sceneEndRestorePending_.load(std::memory_order_acquire)) {
            return;
        }

        if (!IsOnMainThread()) {
            AddCurrentEpochTask([this, trigger, finalizeWindow]() {
                RestoreSceneEndControls(trigger, finalizeWindow);
            });
            return;
        }

        // Abort if a new player OStim thread has started since HandleEnd.
        if (ThreadRegistry::GetSingleton().GetPlayerThreadID().has_value()) {
            sceneEndRestorePending_.store(false, std::memory_order_release);
            return;
        }

        const bool uiModeCurrentlyHeld =
            (magelight_ && magelight_->IsUIModeActive && magelight_->IsUIModeActive()) ||
            (ModalManager::GetSingleton().HasActiveModal() && !IsModalDisplayBlocked());
        if (uiModeCurrentlyHeld) {
            if (finalizeWindow) {
                const std::uint32_t carry = sceneControlBaselineMask_.exchange(0, std::memory_order_acq_rel);
                if (carry != 0) {
                    modalControlBaselineMask_.fetch_or(carry, std::memory_order_acq_rel);
                }
                sceneEndRestorePending_.store(false, std::memory_order_release);
            }
            return;
        }

        const std::uint32_t mask = sceneControlBaselineMask_.load(std::memory_order_acquire);
        const std::uint32_t pulse = midSceneDirectWriteMask_.load(std::memory_order_acquire);
        ApplyControlMaskRestore(mask, pulse, trigger);

        if (finalizeWindow) {
            sceneEndRestorePending_.store(false, std::memory_order_release);
            sceneControlBaselineMask_.store(0, std::memory_order_release);
            midSceneDirectWriteMask_.store(0, std::memory_order_release);

            if (auto* cm = RE::ControlMap::GetSingleton()) {
                const auto& rd = cm->GetRuntimeData();
                SKSE::log::debug(
                    "[UIManager] Post-scene control check finalized ({}): enabledControls=0x{:X}, textEntryCount={}",
                    trigger ? trigger : "unknown",
                    rd.enabledControls.underlying(),
                    static_cast<int>(rd.textEntryCount));
            }
        }
    }

    bool UIManager::IsModalDisplayBlocked() const {
        auto* ui = RE::UI::GetSingleton();
        if (!ui) {
            return true;
        }
        return ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME) ||
               ui->IsMenuOpen(RE::MainMenu::MENU_NAME) ||
               ui->IsMenuOpen(RE::RaceSexMenu::MENU_NAME) ||
               ui->IsMenuOpen(RE::Console::MENU_NAME) ||
               ui->IsMenuOpen("Pause Menu") ||
               ui->IsMenuOpen(RE::JournalMenu::MENU_NAME) ||
               ui->IsMenuOpen(RE::TweenMenu::MENU_NAME) ||
               ui->IsMenuOpen("Sleep/Wait Menu") ||
               ui->IsMenuOpen(RE::MapMenu::MENU_NAME);
    }

    bool UIManager::RegisterJSListener(const char* eventName, JSListenerCallback callback) {
        if (!IsInitialized() || !eventName || !*eventName || !callback) {
            return false;
        }
        magelight_->RegisterJSListener(view_, eventName, callback);
        return true;
    }

    void UIManager::PostToGameThread(std::function<void()> task) {
        if (!task) {
            return;
        }

        auto* fn = new std::function<void()>(std::move(task));
        if (magelight_ && magelight_->hostVersionNumber >= 3001 && magelight_->PostGameTask) {
            if (magelight_->PostGameTask(&UIManager::GameTaskTrampoline, fn) == MAGELIGHT_API::Result::Ok) {
                return;
            }
        }

        std::thread([fn]() {
            if (auto* taskIF = SKSE::GetTaskInterface()) {
                taskIF->AddTask([fn]() { UIManager::GameTaskTrampoline(fn); });
            } else {
                delete fn;
            }
        }).detach();
    }

    void UIManager::GameTaskTrampoline(void* userData) {
        std::unique_ptr<std::function<void()>> guard(static_cast<std::function<void()>*>(userData));
        if (!guard || !*guard) {
            return;
        }
        ScopedGameTask scope;
        (*guard)();
    }

    void UIManager::OnUIReady() {
        if (!IsInitialized()) {
            return;
        }

        isDomReady_.store(true, std::memory_order_release);
        FlushPendingAfterDomReady();

        if (ModalManager::GetSingleton().HasActiveModal() && !IsModalDisplayBlocked()) {
            ModalManager::GetSingleton().SendActiveModalToUI(/*forceResend=*/true);
        }
        SyncViewVisibilityAndFocus();
    }

    void UIManager::SyncViewVisibilityAndFocus() {
        if (!IsInitialized()) {
            return;
        }

        const bool hasModal = ModalManager::GetSingleton().HasActiveModal();
        const bool modalBlocked = IsModalDisplayBlocked();
        const bool modalActive = hasModal && !modalBlocked;

        if (!modalActive) {
            ReleaseFocusAndUIMode();
            return;
        }

        const bool wantNoTextEntry = !ModalManager::GetSingleton().ActiveModalHasTextInput();

        if (!isViewShown_.exchange(true, std::memory_order_acq_rel)) {
            magelight_->ShowView(view_, true);
        }
        magelight_->RaiseView(view_);

        if (CapabilitySupported(magelight_, 2800, "escapecapture") && magelight_->SetEscapeCapture) {
            magelight_->SetEscapeCapture(view_, true);
        }

        const bool cancelSupported =
            CapabilitySupported(magelight_, 10100, "uimodecancel") && magelight_->CancelUIModeRequest;

        auto buildFlags = [&](bool includeReclaim) -> std::uint32_t {
            std::uint32_t flags = MAGELIGHT_API::kUIModeFlagPause | MAGELIGHT_API::kUIModeFlagQueue;
            if (wantNoTextEntry) {
                flags |= MAGELIGHT_API::kUIModeFlagNoTextEntry;
            }
            if (includeReclaim && cancelSupported) {
                flags |= MAGELIGHT_API::kUIModeFlagReclaim;
            }
            return flags;
        };

        const bool inPlayerScene = ThreadRegistry::GetSingleton().GetPlayerThreadID().has_value();

        if (focusOwned_.load(std::memory_order_acquire)) {
            const bool needReclaim = releasePending_.load(std::memory_order_acquire);
            const bool needTextEntryUpdate = (appliedNoTextEntry_.load(std::memory_order_acquire) != wantNoTextEntry);
            if (needReclaim || needTextEntryUpdate) {
                SnapshotControlBaseline(/*forModal=*/true, /*forScene=*/inPlayerScene);
                if (magelight_->RequestUIMode(view_, buildFlags(needReclaim)) == MAGELIGHT_API::Result::Ok) {
                    appliedNoTextEntry_.store(wantNoTextEntry, std::memory_order_release);
                    releasePending_.store(false, std::memory_order_release);
                }
            }
            return;
        }

        if (uiModeRequested_.load(std::memory_order_acquire)) {
            if (appliedNoTextEntry_.load(std::memory_order_acquire) != wantNoTextEntry &&
                cancelSupported &&
                magelight_->CancelUIModeRequest(modHandle_) == MAGELIGHT_API::Result::Ok) {
                uiModeRequested_.store(false, std::memory_order_release);
            } else {
                return;
            }
        }

        if (!uiModeRequested_.exchange(true, std::memory_order_acq_rel)) {
            const bool needReclaim = releasePending_.load(std::memory_order_acquire);
            SnapshotControlBaseline(/*forModal=*/true, /*forScene=*/inPlayerScene);
            const auto res = magelight_->RequestUIMode(view_, buildFlags(needReclaim));
            if (res == MAGELIGHT_API::Result::Ok) {
                appliedNoTextEntry_.store(wantNoTextEntry, std::memory_order_release);
                releasePending_.store(false, std::memory_order_release);
            } else {
                uiModeRequested_.store(false, std::memory_order_release);
                SKSE::log::warn("[UIManager] RequestUIMode failed (result={})", static_cast<int>(res));
                RestoreModalControls();
            }
        }
    }

    void UIManager::ReleaseFocusAndUIMode() {
        if (!IsInitialized()) {
            focusOwned_.store(false, std::memory_order_release);
            uiModeRequested_.store(false, std::memory_order_release);
            releasePending_.store(false, std::memory_order_release);
            appliedNoTextEntry_.store(true, std::memory_order_release);
            isViewShown_.store(false, std::memory_order_release);
            RestoreModalControls();
            DrainAfterFocusReleasedTasks();
            return;
        }

        if (uiModeRequested_.load(std::memory_order_acquire) &&
            CapabilitySupported(magelight_, 10100, "uimodecancel") &&
            magelight_->CancelUIModeRequest) {
            if (magelight_->CancelUIModeRequest(modHandle_) == MAGELIGHT_API::Result::Ok) {
                uiModeRequested_.store(false, std::memory_order_release);
            }
        }

        if (isViewShown_.exchange(false, std::memory_order_acq_rel)) {
            magelight_->ShowView(view_, false);
        }

        if (focusOwned_.load(std::memory_order_acquire) || uiModeRequested_.load(std::memory_order_acquire)) {
            if (!releasePending_.exchange(true, std::memory_order_acq_rel)) {
                const auto relRes = magelight_->ReleaseUIMode(modHandle_);
                if (relRes != MAGELIGHT_API::Result::Ok) {
                    releasePending_.store(false, std::memory_order_release);
                    focusOwned_.store(false, std::memory_order_release);
                    uiModeRequested_.store(false, std::memory_order_release);
                    appliedNoTextEntry_.store(true, std::memory_order_release);
                    RestoreModalControls();
                    DrainAfterFocusReleasedTasks();
                }
            } else {
                const bool hostStillActiveOrOwned =
                    (magelight_->IsUIModeActive && magelight_->IsUIModeActive()) ||
                    (magelight_->GetUIModeOwner && magelight_->GetUIModeOwner() == modHandle_);
                if (!hostStillActiveOrOwned) {
                    releasePending_.store(false, std::memory_order_release);
                    focusOwned_.store(false, std::memory_order_release);
                    uiModeRequested_.store(false, std::memory_order_release);
                    appliedNoTextEntry_.store(true, std::memory_order_release);
                    RestoreModalControls();
                    DrainAfterFocusReleasedTasks();
                }
            }
        } else {
            appliedNoTextEntry_.store(true, std::memory_order_release);
            RestoreModalControls();
            DrainAfterFocusReleasedTasks();
        }
    }

    void UIManager::RunAfterFocusReleased(std::function<void()> task) {
        if (!task) {
            return;
        }

        {
            std::lock_guard lock(stateMutex_);
            afterFocusReleasedTasks_.push_back(std::move(task));
        }

        AddCurrentEpochTask([this]() {
            SyncViewVisibilityAndFocus();
        });
    }

    void UIManager::DrainAfterFocusReleasedTasks() {
        if (focusOwned_.load(std::memory_order_acquire) ||
            uiModeRequested_.load(std::memory_order_acquire) ||
            releasePending_.load(std::memory_order_acquire)) {
            return;
        }

        std::vector<std::function<void()>> tasks;
        {
            std::lock_guard lock(stateMutex_);
            if (afterFocusReleasedTasks_.empty()) {
                return;
            }
            tasks.swap(afterFocusReleasedTasks_);
        }

        for (auto& task : tasks) {
            if (task) {
                task();
            }
        }
    }

    void UIManager::SendToUI(const std::string& functionName, const std::string& jsonData) {
        if (!magelight_ || view_ == 0 || !isDomReady_.load(std::memory_order_acquire)) {
            std::lock_guard lock(stateMutex_);
            pendingJsCalls_.emplace_back(functionName, jsonData);
            return;
        }

        if (!IsOnMainThread()) {
            AddCurrentEpochTask([this, functionName, jsonData]() {
                SendToUI(functionName, jsonData);
            });
            return;
        }

        if (!magelight_->IsViewValid || !magelight_->IsViewValid(view_)) {
            return;
        }

        magelight_->InteropCall(view_, functionName.c_str(), jsonData.c_str());
    }

    void UIManager::OnUIModeExitCallback(MAGELIGHT_API::ViewId view) {
        if (view == 0) {
            return;
        }

        ScopedGameTask scope;
        auto& self = UIManager::GetSingleton();
        if (self.view_ != 0 && view == self.view_) {
            self.HandleUIModeExited();
        } else if (self.sceneEndRestorePending_.load(std::memory_order_acquire)) {
            self.RestoreSceneEndControls("external UIModeExitCallback", /*finalizeWindow=*/false);
        }
    }

    void UIManager::HandleUIModeExited() {
        focusOwned_.store(false, std::memory_order_release);
        uiModeRequested_.store(false, std::memory_order_release);
        releasePending_.store(false, std::memory_order_release);
        appliedNoTextEntry_.store(true, std::memory_order_release);

        const bool modalActive =
            ModalManager::GetSingleton().HasActiveModal() && !IsModalDisplayBlocked();
        if (!modalActive) {
            if (isViewShown_.exchange(false, std::memory_order_acq_rel) && IsInitialized()) {
                magelight_->ShowView(view_, false);
            }
        }

        RestoreModalControls();
        if (sceneEndRestorePending_.load(std::memory_order_acquire)) {
            RestoreSceneEndControls("UIModeExited", /*finalizeWindow=*/false);
        }
        DrainAfterFocusReleasedTasks();
    }

    void UIManager::OnMagelightEvent(const MAGELIGHT_API::EventData* ev, void* user) {
        if (!ev) {
            return;
        }

        ScopedGameTask scope;
        auto* self = user ? static_cast<UIManager*>(user) : &UIManager::GetSingleton();

        switch (ev->type) {
            case MAGELIGHT_API::Event::ViewDomReady: {
                if (!self->IsInitialized() || ev->view != self->view_) {
                    return;
                }
                self->RegisterJSListener("ostimnetAction", &UIManager::OnOStimNetAction);
                self->RegisterJSListener("log", &UIManager::OnLogFromUI);

                self->isDomReady_.store(true, std::memory_order_release);
                SKSE::log::info("[UIManager] DOM ready on Magelight view {}; listeners registered", ev->view);

                self->FlushPendingAfterDomReady();

                if (ModalManager::GetSingleton().HasActiveModal() && !self->IsModalDisplayBlocked()) {
                    ModalManager::GetSingleton().SendActiveModalToUI(/*forceResend=*/true);
                }
                self->SyncViewVisibilityAndFocus();
                break;
            }

            case MAGELIGHT_API::Event::ViewReloaded:
                if (ev->view == self->view_) {
                    self->isDomReady_.store(true, std::memory_order_release);
                    SKSE::log::info("[UIManager] Magelight view {} reloaded (DOM ready)", ev->view);
                    if (ModalManager::GetSingleton().HasActiveModal() && !self->IsModalDisplayBlocked()) {
                        ModalManager::GetSingleton().SendActiveModalToUI(/*forceResend=*/true);
                    }
                    self->SyncViewVisibilityAndFocus();
                }
                break;

            case MAGELIGHT_API::Event::ViewLoadFailed:
                SKSE::log::error(
                    "[UIManager] View {} load failed: {}",
                    ev->view,
                    ev->detail ? ev->detail : "");
                break;

            case MAGELIGHT_API::Event::ViewDestroyed:
                if (ev->view == self->view_) {
                    SKSE::log::info("[UIManager] Magelight view {} destroyed", ev->view);
                    self->view_ = 0;
                    self->isDomReady_.store(false, std::memory_order_release);
                    self->isViewShown_.store(false, std::memory_order_release);
                    self->focusOwned_.store(false, std::memory_order_release);
                    self->uiModeRequested_.store(false, std::memory_order_release);
                    self->releasePending_.store(false, std::memory_order_release);
                    self->appliedNoTextEntry_.store(true, std::memory_order_release);
                    self->RestoreModalControls();
                    self->DrainAfterFocusReleasedTasks();
                }
                break;

            case MAGELIGHT_API::Event::UIModeEntered:
                if (ev->view == self->view_) {
                    self->uiModeRequested_.store(false, std::memory_order_release);
                    self->releasePending_.store(false, std::memory_order_release);
                    self->focusOwned_.store(true, std::memory_order_release);
                    self->SyncViewVisibilityAndFocus();
                }
                break;

            case MAGELIGHT_API::Event::UIModeExited: {
                const bool hostReentered =
                    self->magelight_ &&
                    self->magelight_->IsUIModeActive && self->magelight_->IsUIModeActive() &&
                    self->magelight_->GetUIModeOwner && self->magelight_->GetUIModeOwner() == self->modHandle_;
                const bool newModalAwaitingEntry =
                    !self->focusOwned_.load(std::memory_order_acquire) &&
                    !self->releasePending_.load(std::memory_order_acquire) &&
                    self->uiModeRequested_.load(std::memory_order_acquire) &&
                    ModalManager::GetSingleton().HasActiveModal() &&
                    !self->IsModalDisplayBlocked();
                if (!hostReentered && !newModalAwaitingEntry) {
                    self->HandleUIModeExited();
                }
                break;
            }

            case MAGELIGHT_API::Event::UIModeRefused: {
                self->uiModeRequested_.store(false, std::memory_order_release);
                self->releasePending_.store(false, std::memory_order_release);
                self->focusOwned_.store(false, std::memory_order_release);
                self->appliedNoTextEntry_.store(true, std::memory_order_release);
                SKSE::log::warn(
                    "[UIManager] UI mode refused for view {}: {}",
                    ev->view,
                    ev->detail ? ev->detail : "");

                const bool modalActive =
                    ModalManager::GetSingleton().HasActiveModal() && !self->IsModalDisplayBlocked();
                if (!modalActive) {
                    if (self->isViewShown_.exchange(false, std::memory_order_acq_rel) &&
                        self->IsInitialized()) {
                        self->magelight_->ShowView(self->view_, false);
                    }
                }
                self->RestoreModalControls();
                self->DrainAfterFocusReleasedTasks();
                break;
            }

            case MAGELIGHT_API::Event::FocusDenied:
                SKSE::log::debug(
                    "[UIManager] FocusDenied while holding UI mode (requester={})",
                    ev->detail ? ev->detail : "");
                break;

            case MAGELIGHT_API::Event::RenderDead:
                SKSE::log::error("[UIManager] Magelight RenderDead event received; resetting UI state");
                self->isViewShown_.store(false, std::memory_order_release);
                self->focusOwned_.store(false, std::memory_order_release);
                self->uiModeRequested_.store(false, std::memory_order_release);
                self->releasePending_.store(false, std::memory_order_release);
                self->appliedNoTextEntry_.store(true, std::memory_order_release);
                self->isDomReady_.store(false, std::memory_order_release);
                {
                    std::lock_guard lock(self->stateMutex_);
                    self->pendingJsCalls_.clear();
                }
                self->RestoreModalControls();
                self->DrainAfterFocusReleasedTasks();
                break;

            case MAGELIGHT_API::Event::ConsoleMessage:
                if (ev->detail && *ev->detail) {
                    if (ev->x >= 2) {
                        SKSE::log::error("[UI Console] {}", ev->detail);
                    } else if (ev->x == 1) {
                        SKSE::log::warn("[UI Console] {}", ev->detail);
                    } else {
                        SKSE::log::trace("[UI Console] {}", ev->detail);
                    }
                }
                break;

            default:
                break;
        }
    }

    void UIManager::FlushPendingAfterDomReady() {
        std::vector<std::pair<std::string, std::string>> queued;
        {
            std::lock_guard lock(stateMutex_);
            queued.swap(pendingJsCalls_);
        }
        for (const auto& [fn, payload] : queued) {
            SendToUI(fn, payload);
        }
    }

    void UIManager::OnOStimNetAction(const char* argument) {
        std::string raw(argument ? argument : "");
        const auto expectedEpoch = GetSessionEpoch();

        UIManager::GetSingleton().PostToGameThread([expectedEpoch, raw = std::move(raw)]() {
            if (!IsSessionEpochCurrent(expectedEpoch)) {
                return;
            }
            if (raw.empty()) {
                return;
            }

            auto parsed = nlohmann::json::parse(raw, nullptr, false);
            if (parsed.is_discarded() || !parsed.is_object()) {
                SKSE::log::warn("[UIManager] Ignored malformed ostimnetAction payload");
                return;
            }

            std::string action;
            if (auto it = parsed.find("action"); it != parsed.end() && it->is_string()) {
                action = it->get<std::string>();
            }

            nlohmann::json payload = nlohmann::json::object();
            if (auto it = parsed.find("payload"); it != parsed.end() && it->is_object()) {
                payload = *it;
            }

            if (action == "uiReady") {
                UIManager::GetSingleton().OnUIReady();
            } else if (action == "modalResponse") {
                (void)ModalManager::GetSingleton().HandleModalResponse(payload);
            } else {
                SKSE::log::warn("[UIManager] Unknown ostimnetAction '{}'", action);
            }
        });
    }

    void UIManager::OnLogFromUI(const char* argument) {
        if (argument && *argument) {
            SKSE::log::debug("[UI] {}", argument);
        }
    }

    RE::BSEventNotifyControl UIManager::ProcessEvent(
        const RE::MenuOpenCloseEvent* a_event,
        RE::BSTEventSource<RE::MenuOpenCloseEvent>* /*a_eventSource*/) {
        if (!a_event) {
            return RE::BSEventNotifyControl::kContinue;
        }

        if (!a_event->opening && a_event->menuName.c_str()) {
            const std::string_view closedMenu(a_event->menuName.c_str());
            if (closedMenu == "MagelightFocus" &&
                modalControlBaselineMask_.load(std::memory_order_acquire) != 0) {
                AddCurrentEpochTask([this]() {
                    RestoreModalControls(/*clearMaskWhenFocusMenuStillOpen=*/true);
                });
            }
            if (sceneEndRestorePending_.load(std::memory_order_acquire) &&
                (closedMenu == "MagelightFocus" ||
                 closedMenu == "MagelightOverlay" ||
                 closedMenu == "Fader Menu")) {
                AddCurrentEpochTask([this]() {
                    RestoreSceneEndControls("post-end menu close", /*finalizeWindow=*/false);
                });
            }
        }

        if (ModalManager::GetSingleton().HasActiveModal() ||
            a_event->menuName == RE::LoadingMenu::MENU_NAME ||
            a_event->menuName == RE::MainMenu::MENU_NAME ||
            a_event->menuName == RE::Console::MENU_NAME ||
            a_event->menuName == RE::JournalMenu::MENU_NAME ||
            a_event->menuName == RE::RaceSexMenu::MENU_NAME ||
            a_event->menuName == RE::TweenMenu::MENU_NAME ||
            a_event->menuName == RE::MapMenu::MENU_NAME ||
            a_event->menuName == "Pause Menu" ||
            a_event->menuName == "Sleep/Wait Menu") {
            AddCurrentEpochTask([this]() {
                if (ModalManager::GetSingleton().HasActiveModal() && !IsModalDisplayBlocked()) {
                    ModalManager::GetSingleton().SendActiveModalToUI(/*forceResend=*/false);
                }
                SyncViewVisibilityAndFocus();
            });
        }

        return RE::BSEventNotifyControl::kContinue;
    }

}  // namespace OStimNet::UI
