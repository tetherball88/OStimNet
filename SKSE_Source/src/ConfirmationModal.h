#pragma once

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

#include <chrono>
#include <cstdint>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace OStimNet {

// ---------------------------------------------------------------------------
// ConfirmationModal — singleton. Registers as a LATENT Papyrus native so the
// Papyrus stack suspends instead of blocking the main thread.
//
// Return codes (via vm->ReturnLatentResult):
//    0 = player accepted (or confirmation disabled / Magelight UI unavailable)
//    1 = player declined (Cancel or modal aborted on reload)
//    2 = player chose "Decline (Explain)" — TriggerTextInput fired, decline cooldown applied
//    3 = cooldown still active (modal not shown)
//    4 = player not among actors (accept by default)
//    5 = intent is "aggressive" (accept by default)
// ---------------------------------------------------------------------------
class ConfirmationModal {
public:
    static ConfirmationModal& GetSingleton();

    bool IsOnCooldown(const char* actionType, RE::Actor* originator);
    void Reset();

    // Atomic check-and-set for the normal (non-decline) cooldown.
    // Returns 0 if the cooldown was free and has now been set; 1 if already on cooldown (blocked).
    std::int32_t CheckAndSet(const char* actionType, const std::vector<RE::Actor*>& actors);

    // Called from the Papyrus native (which is registered as latent).
    // Returns immediately; result delivered via vm->ReturnLatentResult() on the game thread.
    RE::BSScript::LatentStatus Show(
        RE::BSScript::Internal::VirtualMachine* vm,
        RE::VMStackID stackID,
        const char* actionType,
        const std::vector<RE::Actor*>& mainActors,
        const std::vector<RE::Actor*>& secondaryActors,
        const char* paramsJson);

private:
    ConfirmationModal() = default;

    void SetCooldown(const std::string& key, int seconds);
    void SetCooldownAll(const std::vector<std::string>& keys, int seconds);

    std::unordered_map<std::string, std::chrono::steady_clock::time_point> _cooldowns;
    std::shared_mutex _cooldownMutex;
};

}  // namespace OStimNet
