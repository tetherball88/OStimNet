#pragma once
#include <limits>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "DebounceQueue.h"
#include "Config.h"
#include "ThreadRegistry.h"

namespace OStimNet {

// Returns the threadID that should generate scene-change speech:
//   1. Player thread (threadID 0) if active.
//   2. Otherwise the thread whose closest actor is nearest to the player.
// Returns -1 if activeActors is empty.
inline int SelectPriorityThread(const std::unordered_map<int, std::vector<uint32_t>>& activeActors) {
    if (activeActors.empty()) return -1;
    if (activeActors.size() == 1) return activeActors.begin()->first;

    if (activeActors.count(0)) return 0;

    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return activeActors.begin()->first;

    RE::NiPoint3 playerPos = player->GetPosition();
    int   bestThread = -1;
    float bestDistSq = std::numeric_limits<float>::max();

    for (const auto& [tid, formIDs] : activeActors) {
        const auto& ptrs = ThreadDataStore::GetSingleton().GetActorPtrs(tid);
        for (auto* actor : ptrs) {
            if (!actor) continue;
            RE::NiPoint3 pos = actor->GetPosition();
            float dx = pos.x - playerPos.x;
            float dy = pos.y - playerPos.y;
            float dz = pos.z - playerPos.z;
            float distSq = dx * dx + dy * dy + dz * dz;
            if (distSq < bestDistSq) {
                bestDistSq = distSq;
                bestThread = tid;
            }
        }
    }
    return bestThread;
}

// Picks a speaker actor from the thread according to gender priority.
// genderPriority: 0 = always male, 100 = always female, 50 = equal chance.
// Excludes the player character.
// Returns nullptr if no valid non-player actor is found.
inline RE::Actor* SelectSpeakerActor(int threadID, int genderPriority) {
    const auto& ptrs = ThreadDataStore::GetSingleton().GetActorPtrs(threadID);
    if (ptrs.empty()) return nullptr;

    auto* player = RE::PlayerCharacter::GetSingleton();

    std::vector<RE::Actor*> males, females;
    for (auto* actor : ptrs) {
        if (!actor || actor == static_cast<RE::Actor*>(player)) continue;
        auto* base = actor->GetActorBase();
        if (base && base->GetSex() == RE::SEX::kFemale)
            females.push_back(actor);
        else
            males.push_back(actor);
    }

    static std::mt19937 rng{std::random_device{}()};
    auto pickRandom = [&](std::vector<RE::Actor*>& pool) -> RE::Actor* {
        if (pool.empty()) return nullptr;
        std::uniform_int_distribution<size_t> dist(0, pool.size() - 1);
        return pool[dist(rng)];
    };

    bool preferFemale = std::uniform_int_distribution<int>(0, 99)(rng) < genderPriority;
    if (preferFemale) {
        if (auto* a = pickRandom(females)) return a;
        return pickRandom(males);
    } else {
        if (auto* a = pickRandom(males)) return a;
        return pickRandom(females);
    }
}

// Picks a climax event speaker according to these priority rules:
//   1. Squirting actors (spurt beats flow); player excluded; random among ties.
//   2. Actor targeted by the most distinct orgasmers (from cumApplied); random among ties.
//   3. Gender-weighted fallback via SelectSpeakerActor.
inline RE::Actor* SelectClimaxSpeakerActor(int threadID, const DebounceQueue::ClimaxBatchData& data) {
    auto* player = RE::PlayerCharacter::GetSingleton();
    static std::mt19937 climaxRng{std::random_device{}()};

    auto pickRandom = [&](std::vector<RE::Actor*>& pool) -> RE::Actor* {
        if (pool.empty()) return nullptr;
        std::uniform_int_distribution<size_t> dist(0, pool.size() - 1);
        return pool[dist(climaxRng)];
    };

    // Rule 1: squirters — spurt takes priority over flow, player excluded.
    std::vector<RE::Actor*> spurters, flowters;
    for (const auto& e : data.squirts) {
        auto* actor = RE::TESForm::LookupByID<RE::Actor>(e.actorFormID);
        if (!actor || actor == static_cast<RE::Actor*>(player)) continue;
        if (e.squirtType == "spurt") spurters.push_back(actor);
        else                         flowters.push_back(actor);
    }
    if (!spurters.empty()) return pickRandom(spurters);
    if (!flowters.empty()) return pickRandom(flowters);

    // Rule 2: target receiving the most distinct orgasmers (from cumApplied), player excluded.
    std::unordered_map<RE::FormID, std::unordered_set<RE::FormID>> targetToOrgasmers;
    for (const auto& e : data.cumApplied)
        targetToOrgasmers[e.targetFormID].insert(e.orgasmerFormID);

    if (!targetToOrgasmers.empty()) {
        size_t maxCount = 0;
        for (const auto& [tid, orgasmers] : targetToOrgasmers)
            if (orgasmers.size() > maxCount) maxCount = orgasmers.size();

        std::vector<RE::Actor*> topTargets;
        for (const auto& [tid, orgasmers] : targetToOrgasmers) {
            if (orgasmers.size() != maxCount) continue;
            auto* actor = RE::TESForm::LookupByID<RE::Actor>(tid);
            if (actor && actor != static_cast<RE::Actor*>(player))
                topTargets.push_back(actor);
        }
        if (!topTargets.empty()) return pickRandom(topTargets);
    }

    // Rule 3: gender-weighted fallback.
    return SelectSpeakerActor(threadID, Config::GetSingleton().CommentGenderPriority());
}

}  // namespace OStimNet
