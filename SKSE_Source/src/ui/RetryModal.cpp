#include "PCH.h"

#include "ui/RetryModal.h"
#include "ui/ModalManager.h"
#include "ui/ModalTypes.h"
#include "ui/SessionEpoch.h"
#include "ui/UIManager.h"

#include <memory>
#include <utility>

namespace OStimNet::UI::RetryModal {

    void Show(
        std::string title,
        const std::string& evaluationType,
        const std::string& description,
        std::function<void()> onRetry,
        std::function<void()> onIgnore) {
        const SessionEpoch expectedEpoch = GetSessionEpoch();

        struct SharedCallbacks {
            std::function<void()> retry;
            std::function<void()> ignore;
        };
        auto cbs = std::make_shared<SharedCallbacks>(SharedCallbacks{ std::move(onRetry), std::move(onIgnore) });

        auto dispatchIgnore = [cbs]() {
            if (cbs->ignore) {
                cbs->ignore();
            }
        };

        if (!AddEpochTask(expectedEpoch, [title = std::move(title), evaluationType, description, expectedEpoch, cbs]() mutable {
                if (!UIManager::GetSingleton().IsInitialized()) {
                    SKSE::log::warn("[RetryModal] Magelight UI unavailable for '{}'; ignoring failure", evaluationType);
                    if (cbs->ignore) {
                        cbs->ignore();
                    }
                    return;
                }

                ModalRequest req;
                req.modalType = "generic";
                req.title = std::move(title);
                req.subtitle = evaluationType;
                req.tone = ModalTone::Danger;
                req.size = ModalSize::Sm;
                req.message = "Choose Retry to try the request again, or Ignore to skip this evaluation.";

                if (!description.empty()) {
                    req.controls.push_back(ModalControl::MakeInfoBox(
                        "error_info",
                        "Error Details",
                        description,
                        ModalTone::Danger));
                }

                req.buttons.push_back(ModalButton::Make("retry", "Retry", ModalButtonVariant::Primary));
                req.buttons.push_back(ModalButton::Make("ignore", "Ignore", ModalButtonVariant::Secondary));

                ModalManager::GetSingleton().ShowModal(
                    std::move(req),
                    [expectedEpoch, cbs](const ModalResponse& resp) {
                        if (!IsSessionEpochCurrent(expectedEpoch)) {
                            return;
                        }
                        if (!resp.aborted && resp.buttonId == "retry") {
                            if (cbs->retry) {
                                cbs->retry();
                            }
                        } else {
                            if (cbs->ignore) {
                                cbs->ignore();
                            }
                        }
                    });
            })) {
            SKSE::log::error("[RetryModal] SKSE TaskInterface unavailable, falling back to ignore");
            dispatchIgnore();
        }
    }

}  // namespace OStimNet::UI::RetryModal
