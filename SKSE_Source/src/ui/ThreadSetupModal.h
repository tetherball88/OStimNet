#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace RE {
    class Actor;
}

namespace OStimNet::UI {

    /// Manages Magelight UI modals for configuring external OStim threads and
    /// reassigning mid-scene intent / primary actor roles.
    class ThreadSetupModal {
    public:
        static ThreadSetupModal& GetSingleton();

        /// Applies a new intent and main/secondary actor split to an active thread and emits `ostimnet_intent_changed`.
        /// Passing an empty `mainActors` vector keeps the existing actor role split.
        static void ApplyThreadIntentAndRoles(
            int threadID,
            const std::string& intentLower,
            std::vector<RE::Actor*> mainActors);

        /// Opens the unified Magelight setup modal for a non-OStimNet (external) OStim thread.
        void ShowExternalThreadSetup(int threadID);

        /// Opens the Magelight actor-role checklist modal for an active thread.
        /// Pass `requestedIntent` empty to keep the current intent and only change actor roles.
        void ShowThreadRolesModal(int threadID, std::string_view requestedIntent);

    private:
        ThreadSetupModal() = default;
    };

}  // namespace OStimNet::UI
