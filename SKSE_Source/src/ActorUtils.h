#pragma once

#include <vector>
#include "RE/Skyrim.h"


namespace OStimNet::ActorUtils {

/// Returns true if actor is a member of the faction identified by editorID.
inline bool IsInFactionByEditorID(RE::Actor* actor, const char* editorID) {
    if (!actor) return false;
    auto* faction = RE::TESForm::LookupByEditorID<RE::TESFaction>(editorID);
    return faction && actor->IsInFaction(faction);
}

/// Returns true (and trace-logs) if actor is blocked from starting a new encounter
/// because they are already in SexLabAnimatingFaction, OStimActorCountFaction, or TTON_OStimPending.
inline bool IsBlockedForNewEncounter(RE::Actor* actor, const char* decoratorName) {
    if (!actor) return false;
    const char* actorName = actor->GetName();
    if (IsInFactionByEditorID(actor, "SexLabAnimatingFaction")) {
        SKSE::log::trace("{}: actor '{}' (0x{:08X}) -> unavailable (in SexLabAnimatingFaction)",
            decoratorName, actorName ? actorName : "(unnamed)", actor->GetFormID());
        return true;
    }
    if (IsInFactionByEditorID(actor, "OStimActorCountFaction")) {
        SKSE::log::trace("{}: actor '{}' (0x{:08X}) -> unavailable (in OStimActorCountFaction)",
            decoratorName, actorName ? actorName : "(unnamed)", actor->GetFormID());
        return true;
    }
    if (IsInFactionByEditorID(actor, "TTON_OStimPending")) {
        SKSE::log::trace("{}: actor '{}' (0x{:08X}) -> unavailable (in TTON_OStimPending)",
            decoratorName, actorName ? actorName : "(unnamed)", actor->GetFormID());
        return true;
    }
    return false;
}


/// Controls how GetNearbyActors filters actors based on OStim participation.
enum class OStimCondition {
    /// Only include actors currently in OStimActorCountFaction (i.e. in an OStim scene).
    OnlyInOStim,
    /// Only include actors NOT in OStimActorCountFaction (i.e. not in an OStim scene).
    OnlyNotInOStim,
    /// No OStim filter — include actors regardless of faction membership.
    Any
};

/// Scans the high-actor process list for actors within @p radius game units of
/// @p center and returns them.
///
/// Must be called on the game thread.
///
/// @param center         The actor used as the origin of the distance check.
/// @param radius         Search radius in game units (1 unit ≈ 14.3 mm in Skyrim).
/// @param ostimCondition Controls whether results are restricted to actors that are
///                       in OStim (OnlyInOStim), not in OStim (OnlyNotInOStim),
///                       or unrestricted (Any).
/// @return               Vector of raw Actor* pointers for actors within the radius.
///                       The center actor itself is never included.
///                       The player is included if they are within radius and pass all filters.
inline std::vector<RE::Actor*> GetNearbyActors(
    RE::Actor*      center,
    float           radius,
    OStimCondition  ostimCondition = OStimCondition::Any)
{
    std::vector<RE::Actor*> result;

    if (!center) {
        SKSE::log::warn("GetNearbyActors: center actor is null");
        return result;
    }

    if (!center->GetActorRuntimeData().currentProcess) {
        SKSE::log::warn("GetNearbyActors: center actor has no runtime process");
        return result;
    }

    const auto processLists = RE::ProcessLists::GetSingleton();
    if (!processLists) {
        SKSE::log::warn("GetNearbyActors: failed to get ProcessLists singleton");
        return result;
    }

    // Resolve the OStim faction once up front (null-safe: LookupByEditorID returns nullptr
    // if the faction isn't loaded, in which case all OStim checks will simply be false).
    RE::TESFaction* ostimFaction = nullptr;
    if (ostimCondition != OStimCondition::Any) {
        ostimFaction = RE::TESForm::LookupByEditorID<RE::TESFaction>("OStimActorCountFaction");
    }

    // Build a reusable filter that applies all per-actor checks except the distance test.
    // Returns true if the actor should be considered a candidate.
    auto PassesFilters = [&](RE::Actor* target) -> bool {
        if (!target || target == center) return false;
        if (!target->GetActorRuntimeData().currentProcess) return false;
        if (target->IsDead()) return false;
        if (target->GetCurrentScene()) return false;
        if (target->IsInCombat()) return false;
        if (target->IsChild()) return false;

        // Reject non-humanoid actors (animals, creatures, etc.) by checking
        // for the ActorTypeNPC keyword, which all humanoid races carry.
        //
        // Do NOT call race->HasKeywordString() or HasKeyword() in SkyrimVR:
        // both cause MSVC to emit an AVX2 vpcmpeqq loop over the keyword array
        // that can over-read into an unmapped guard page and crash.
        // Instead, cache the BGSKeyword* once and iterate race->keywords[]
        // through a volatile pointer, forcing individual 8-byte scalar loads.
        const auto* race = target->GetRace();
        if (!race) return false;
        {
            static RE::BGSKeyword* s_kwActorTypeNPC =
                RE::TESForm::LookupByEditorID<RE::BGSKeyword>("ActorTypeNPC");
            if (!s_kwActorTypeNPC) return false;
            bool hasNPCKeyword = false;
            // TESRace stores keywords in BGSKeywordForm::keywords (standard array).
            RE::BGSKeyword* const volatile* kwds = race->keywords;
            const uint32_t count = race->numKeywords;
            if (kwds) {
                for (uint32_t ki = 0; ki < count; ++ki) {
                    if (kwds[ki] == s_kwActorTypeNPC) { hasNPCKeyword = true; break; }
                }
            }
            if (!hasNPCKeyword) return false;
        }
        if (IsInFactionByEditorID(target, "SexLabAnimatingFaction")) return false;
        if (IsInFactionByEditorID(target, "TTON_OStimPending")) return false;
        if (ostimCondition != OStimCondition::Any) {
            const bool inOStim = IsInFactionByEditorID(target, "OStimActorCountFaction");
            SKSE::log::info("GetNearbyActors: actor '{}' (0x{:08X}) inOStim={} ostimCondition={}",
                target->GetName() ? target->GetName() : "(unnamed)", target->GetFormID(), inOStim,
                ostimCondition == OStimCondition::OnlyInOStim ? "OnlyInOStim" :
                ostimCondition == OStimCondition::OnlyNotInOStim ? "OnlyNotInOStim" : "Any");
            if (ostimCondition == OStimCondition::OnlyInOStim && !inOStim) return false;
            if (ostimCondition == OStimCondition::OnlyNotInOStim && inOStim) return false;
        }
        return true;
    };

    for (auto& handle : processLists->highActorHandles) {
        if (!handle) continue;
        auto targetPtr = handle.get();
        if (!targetPtr) continue;
        RE::Actor* target = targetPtr.get();

        if (!PassesFilters(target)) continue;

        const float distance = center->GetPosition().GetDistance(target->GetPosition());
        if (distance >= radius) continue;

        result.push_back(target);
    }

    // The player is not in highActorHandles — check them separately.
    if (auto* player = RE::PlayerCharacter::GetSingleton()) {
        if (PassesFilters(player)) {
            const float distance = center->GetPosition().GetDistance(player->GetPosition());
            if (distance < radius) {
                result.push_back(player);
            }
        }
    }

    return result;
}

}  // namespace OStimNet::ActorUtils
