#pragma once

#include <functional>
#include <string>

namespace OStimNet::UI::RetryModal {

    /// Shows a Magelight UI retry/ignore modal on LLM or Jev Decision failure.
    /// Falls back to calling `onIgnore` on the game thread if Magelight UI is unavailable.
    void Show(
        std::string title,
        const std::string& evaluationType,
        const std::string& description,
        std::function<void()> onRetry,
        std::function<void()> onIgnore);

}  // namespace OStimNet::UI::RetryModal
