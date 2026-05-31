#pragma once
#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <nlohmann/json.hpp>
#include "api/OstimNG-API-Thread.h"
#include "api/OStimNavigator_PublicAPI.h"
#include "DebounceQueue.h"
#include "SceneDescriptionUtils.h"
#include "ThreadRegistry.h"

// =============================================================================
// Canonical-position helpers (OStimNavigatorAPI)
// =============================================================================

namespace OStimNavigatorAPI {

// Returns a reference to the lazily-built position-id→display-name map,
// populated once from ONavGetPositionDisplayNames().
// Returns an empty map if OStimNavigator is not loaded.
inline const std::unordered_map<std::string, std::string>& GetPositionDisplayNameMap() {
    static const std::unordered_map<std::string, std::string> s = []() {
        std::unordered_map<std::string, std::string> out;
        if (ONavGetPositionDisplayNames) {
            try {
                auto obj = nlohmann::json::parse(ONavGetPositionDisplayNames());
                for (auto it = obj.begin(); it != obj.end(); ++it)
                    if (it.value().is_string())
                        out.emplace(it.key(), it.value().get<std::string>());
            } catch (...) {}
        }
        return out;
    }();
    return s;
}

// Returns a reference to the lazily-built alias→canonical map,
// populated once from ONavGetPositionAliases().
// Returns an empty map if OStimNavigator is not loaded.
inline const std::unordered_map<std::string, std::string>& GetPositionAliasMap() {
    static const std::unordered_map<std::string, std::string> s = []() {
        std::unordered_map<std::string, std::string> out;
        if (ONavGetPositionAliases) {
            try {
                auto obj = nlohmann::json::parse(ONavGetPositionAliases());
                for (auto it = obj.begin(); it != obj.end(); ++it)
                    if (it.value().is_string())
                        out.emplace(it.key(), it.value().get<std::string>());
            } catch (...) {}
        }
        return out;
    }();
    return s;
}

// Returns a reference to the lazily-built set of canonical position names,
// populated once from ONavGetCanonicalPositions().
// Returns an empty set if OStimNavigator is not loaded.
inline const std::unordered_set<std::string>& GetCanonicalPositionSet() {
    static const std::unordered_set<std::string> s = []() {
        std::unordered_set<std::string> out;
        if (ONavGetCanonicalPositions) {
            try {
                auto arr = nlohmann::json::parse(ONavGetCanonicalPositions());
                for (const auto& item : arr)
                    if (item.is_string())
                        out.emplace(item.get<std::string>());
            } catch (...) {}
        }
        return out;
    }();
    return s;
}

// Returns true when tag (already lowercase) is a canonical position name or a known alias.
// Resolves the canonical name into `out` when returning true.
inline bool ResolvePosition(const std::string& tag, std::string& out) {
    if (GetCanonicalPositionSet().count(tag)) { out = tag; return true; }
    const auto& aliases = GetPositionAliasMap();
    auto it = aliases.find(tag);
    if (it != aliases.end()) { out = it->second; return true; }
    return false;
}

} // namespace OStimNavigatorAPI

namespace OStimNet {

// --- JSON helpers -----------------------------------------------------------

inline std::string EscapeJsonString(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;       break;
        }
    }
    return out;
}

// --- Climax batch JSON builder ---------------------------------------------

// Represents one target that received cum from an orgasmer, with the deduplicated
// set of body areas involved.
struct ClimaxCumTarget {
    RE::FormID               targetFormID;
    std::vector<std::string> areas;   // from OCum (priority) or ONav; sorted & deduplicated
};

// Per-orgasmer result.
// targets is non-empty for schlonged actors with known cum targets.
// An empty targets list means the actor climaxed/squirted without a resolvable target.
// squirtType is "spurt", "flow", or "" for non-squirters.
struct ClimaxActorResult {
    RE::FormID                   actorFormID;
    std::vector<ClimaxCumTarget> targets;
    std::string                  squirtType;
};

// Snapshot of the OStim actor buffer, taken once per climax batch.
// Shared between BuildClimaxActorData and FormatClimaxMessage to avoid calling GetActors twice.
struct ClimaxActorSnapshot {
    static constexpr uint32_t      kMaxActors = 16;
    OstimNG_API::Thread::ActorData buffer[kMaxActors]{};
    uint32_t                       count      = 0;
};

// Normalizes an area string to its canonical form, mapping legacy/variant spellings
// to the actual strings used by OStim's cum stringLists and OCum's area keywords.
//
// Canonical inside areas: vagina, rectum, mouth, throat
// Canonical outside areas: back, belly, butt, chest, face, feet, hands, neck, thighs, vulva
inline std::string NormalizeArea(const std::string& raw) {
    std::string lower = raw;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    // Map variant spellings to canonical names
    if (lower == "vaginal")               return "vagina";
    if (lower == "anal" || lower == "anus") return "rectum";
    return lower;  // already canonical
}

// Finds the 0-based OStim actor buffer position by FormID; returns -1 if not found.
inline int FindActorPosition(const OstimNG_API::Thread::ActorData* buffer,
                             uint32_t count, RE::FormID formID) {
    for (uint32_t i = 0; i < count; ++i)
        if (buffer[i].formID == formID) return static_cast<int>(i);
    return -1;
}

// Merges all climax data from a ClimaxBatchData into a per-orgasmer list.
//
// Priority rules:
//   - OCum (Papyrus-reported) data takes priority per (orgasmer, target) pair:
//     if OCum has any entry for A→B, ONav data for A→B is entirely discarded.
//   - Actors without a schlong appear with an empty targets list ("just climaxed").
//   - Schlonged actors with no resolved targets also appear with empty targets.
//   - Squirt actors are merged into the "just climaxed" bucket (no target).
inline std::vector<ClimaxActorResult> BuildClimaxActorData(
    const DebounceQueue::ClimaxBatchData& data,
    const ClimaxActorSnapshot& snapshot) {

    // 1. Collect orgasmer IDs in insertion order (first-encountered sceneID wins).
    //    A vector+set preserves deterministic output order across runs.
    std::vector<std::pair<RE::FormID, std::string>> orgasmerOrder;
    std::unordered_set<RE::FormID> seenOrgasmers;
    auto addOrgasmer = [&](RE::FormID id, const std::string& sceneID) {
        if (seenOrgasmers.insert(id).second)
            orgasmerOrder.push_back({id, sceneID});
    };
    for (const auto& e : data.climaxEvents) addOrgasmer(e.actorFormID, e.sceneID);
    for (const auto& e : data.cumApplied)   addOrgasmer(e.orgasmerFormID, e.sceneID);
    for (const auto& e : data.squirts)      addOrgasmer(e.actorFormID, e.sceneID);

    // 2. Build squirt type lookup: actorID → "spurt" or "flow".
    std::unordered_map<RE::FormID, std::string> squirtTypes;
    for (const auto& e : data.squirts)
        squirtTypes.try_emplace(e.actorFormID, e.squirtType);

    // 3. Build OCum target map:  orgasmerID → targetID → deduplicated areas.
    std::unordered_map<RE::FormID,
        std::unordered_map<RE::FormID, std::vector<std::string>>> ocumTargets;
    for (const auto& e : data.cumApplied) {
        if (!e.area.empty())
            ocumTargets[e.orgasmerFormID][e.targetFormID].push_back(NormalizeArea(e.area));
    }
    for (auto& [orgID, byTarget] : ocumTargets) {
        for (auto& [tgtID, areas] : byTarget) {
            std::sort(areas.begin(), areas.end());
            areas.erase(std::unique(areas.begin(), areas.end()), areas.end());
        }
    }

    // 4. Build per-actor results in the deterministic order collected above.
    std::vector<ClimaxActorResult> results;
    results.reserve(orgasmerOrder.size());

    for (const auto& [orgasmerID, sceneID] : orgasmerOrder) {
        ClimaxActorResult result;
        result.actorFormID = orgasmerID;

        // Determine schlong status from the live actor buffer when possible.
        int  actorPos   = FindActorPosition(snapshot.buffer, snapshot.count, orgasmerID);
        bool hasSchlong = false;
        if (actorPos >= 0) {
            hasSchlong = snapshot.buffer[actorPos].hasSchlong;
        } else {
            // Thread may have ended before the debounce fired.
            // Fall back: if this actor appears as an orgasmer in OCum they have a schlong.
            hasSchlong = ocumTargets.count(orgasmerID) > 0;
        }

        if (!hasSchlong) {
            // Carry squirt type so FormatClimaxMessage can emit the right sentence.
            if (auto it = squirtTypes.find(orgasmerID); it != squirtTypes.end())
                result.squirtType = it->second;
            results.push_back(std::move(result));
            continue;
        }

        // 4a. OCum targets (highest priority).
        std::unordered_map<RE::FormID, std::vector<std::string>> finalTargets;
        std::unordered_set<RE::FormID> ocumCoveredTargets;
        if (auto it = ocumTargets.find(orgasmerID); it != ocumTargets.end()) {
            for (const auto& [tgtID, areas] : it->second) {
                finalTargets[tgtID] = areas;
                ocumCoveredTargets.insert(tgtID);
            }
        }

        // 4b. ONav targets (fill only pairs not already covered by OCum).
        if (ONavGetCumTargets && !sceneID.empty() && actorPos >= 0) {
            const char* raw = ONavGetCumTargets(sceneID.c_str(), actorPos);
            // raw format: [{"counterpartIndex":N,"areas":["area",...]},...] or "[]"
            if (raw && raw[0] == '[' && raw[1] != ']') {
                auto j = nlohmann::json::parse(raw, nullptr, /*allow_exceptions=*/false);
                if (!j.is_discarded() && j.is_array()) {
                    for (const auto& entry : j) {
                        int counterpartIdx = entry.value("counterpartIndex", -1);
                        if (counterpartIdx < 0 ||
                            static_cast<uint32_t>(counterpartIdx) >= snapshot.count) continue;
                        RE::FormID targetID = snapshot.buffer[counterpartIdx].formID;
                        if (ocumCoveredTargets.count(targetID)) continue;  // OCum wins
                        auto areaArr = entry.value("areas", nlohmann::json::array());
                        for (const auto& a : areaArr) {
                            if (a.is_string())
                                finalTargets[targetID].push_back(NormalizeArea(a.get<std::string>()));
                        }
                    }
                }
            }
        }

        if (finalTargets.empty()) {
            results.push_back(std::move(result));   // schlonged but no targets resolved
            continue;
        }

        for (auto& [tgtID, areas] : finalTargets)
            result.targets.push_back({tgtID, std::move(areas)});
        results.push_back(std::move(result));
    }

    return results;
}

// Generates one full sentence for a single (actor, target, area) combination.
// targetIsMale is used for the chest template.
//
// Canonical areas and their templates:
//   vagina  — "{actor} buried fully into {target} and finished inside {target}'s pussy."
//   rectum  — "{actor} buried fully into {target} and finished inside {target}'s ass."
//   mouth   — "{actor} finished in {target}'s mouth, filling {target}'s mouth completely."
//   throat  — "{actor} finished in {target}'s mouth, filling {target}'s mouth completely."
//   face    — "{actor} finished, leaving {target}'s face and lips covered in cum."
//   chest   — "{actor} covered {target}'s breasts/chest with cum."
//   butt    — "{actor} came over {target}'s butt and lower back, covering both in cum."
//   vulva   — "{actor} came over {target}'s pussy, covering it in cum."
//   hands   — "{actor} came over {target}'s hands, covering them in cum."
//   feet    — "{actor} finished, covering {target}'s feet in cum."
//   belly   — "{actor} came over {target}'s belly, covering it in cum."
//   back    — "{actor} came over {target}'s back, covering it in cum."
//   neck    — "{actor} came on {target}'s neck."
//   thighs  — "{actor} came over {target}'s thighs, covering them in cum."
inline std::string FormatAreaSentence(const std::string& actorName,
                                       const std::string& targetName,
                                       bool targetIsMale,
                                       const std::string& area) {
    if (area == "vagina")
        return actorName + " buried fully into " + targetName + " and finished inside " + targetName + "'s pussy.";
    if (area == "rectum")
        return actorName + " buried fully into " + targetName + " and finished inside " + targetName + "'s ass.";
    if (area == "mouth" || area == "throat")
        return actorName + " finished in " + targetName + "'s mouth, filling " + targetName + "'s mouth completely.";
    if (area == "face")
        return actorName + " finished, leaving " + targetName + "'s face and lips covered in cum.";
    if (area == "chest")
        return actorName + " covered " + targetName + (targetIsMale ? "'s chest" : "'s breasts") + " with cum.";
    if (area == "butt")
        return actorName + " came over " + targetName + "'s butt and lower back, covering both in cum.";
    if (area == "vulva")
        return actorName + " came over " + targetName + "'s pussy, covering it in cum.";
    if (area == "hands")
        return actorName + " came over " + targetName + "'s hands, covering them in cum.";
    if (area == "feet")
        return actorName + " finished, covering " + targetName + "'s feet in cum.";
    if (area == "belly")
        return actorName + " came over " + targetName + "'s belly, covering it in cum.";
    if (area == "back")
        return actorName + " came over " + targetName + "'s back, covering it in cum.";
    if (area == "neck")
        return actorName + " came on " + targetName + "'s neck.";
    if (area == "thighs")
        return actorName + " came over " + targetName + "'s thighs, covering them in cum.";
    // Unknown area — generic fallback
    return actorName + " came on " + targetName + "'s " + area + ".";
}

// Returns a human-readable display label for an area when used inside a joined list.
inline std::string AreaDisplayName(const std::string& area, bool targetIsMale) {
    if (area == "vulva")  return "pussy";
    if (area == "chest")  return targetIsMale ? "chest" : "breasts";
    return area;
}

// Builds one merged sentence for multiple outside areas on the same target by the same
// actor group: "{actors} finished, covering {target}'s {a1}, {a2} and {a3} in cum."
inline std::string FormatMultiOutsideAreaSentence(const std::string& actorList,
                                                    const std::string& targetName,
                                                    bool targetIsMale,
                                                    const std::vector<std::string>& areas) {
    std::vector<std::string> labels;
    labels.reserve(areas.size());
    for (const auto& a : areas)
        labels.push_back(AreaDisplayName(a, targetIsMale));
    return actorList + " finished, covering " + targetName + "'s " +
           FormatActorList(labels) + " in cum.";
}

// Builds a natural-language description from all climax results.
//
// Grouping rules:
//   - Inside areas (vagina, rectum, mouth, throat) each emit their own sentence —
//     the per-area templates are too specific to merge.
//   - Outside areas (face, chest, butt, etc.) that share the exact same actor group
//     and the same target are merged into one sentence.
//   - Multiple actors doing the same (target, area) are joined into one actor list.
//   - Actors with no resolved targets get a plain trailing sentence.
//
// Actor names are resolved from ThreadDataStore (position-indexed, cached at thread start)
// with a fallback to TESForm::LookupByID for actors missing from the live buffer.
inline std::string FormatClimaxMessage(
    int threadID,
    const std::vector<ClimaxActorResult>& actorResults,
    const ClimaxActorSnapshot& snapshot) {

    auto& store          = ThreadDataStore::GetSingleton();
    const auto& posNames = store.GetActorNames(threadID);

    auto GetActorName = [&](RE::FormID formID) -> std::string {
        int pos = FindActorPosition(snapshot.buffer, snapshot.count, formID);
        if (pos >= 0 && static_cast<size_t>(pos) < posNames.size() &&
            !posNames[static_cast<size_t>(pos)].empty())
            return posNames[static_cast<size_t>(pos)];
        if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(formID))
            if (const char* n = actor->GetName(); n && n[0])
                return n;
        return "someone";
    };

    auto IsActorMale = [&](RE::FormID formID) -> bool {
        int pos = FindActorPosition(snapshot.buffer, snapshot.count, formID);
        if (pos >= 0)
            return !snapshot.buffer[pos].isFemale;
        if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(formID))
            if (auto* npc = actor->GetActorBase())
                return npc->GetSex() == RE::SEX::kMale;
        return false;
    };

    static const std::unordered_set<std::string> kInsideAreas{"vagina", "rectum", "mouth", "throat"};

    // Step 1: Build insertion-ordered (targetID, area) → actorNames entries.
    // A single flat vector replaces the dual keyOrder+groups structures.
    struct AreaEntry { RE::FormID targetFormID; std::string area; std::vector<std::string> actorNames; };
    std::vector<AreaEntry> areaEntries;
    std::vector<std::pair<RE::FormID, std::string>> noTargetActors;  // (formID, squirtType)

    auto findOrAddEntry = [&](RE::FormID targetID, const std::string& area) -> AreaEntry& {
        for (auto& e : areaEntries)
            if (e.targetFormID == targetID && e.area == area)
                return e;
        areaEntries.push_back({targetID, area, {}});
        return areaEntries.back();
    };

    for (const auto& result : actorResults) {
        if (result.targets.empty()) {
            noTargetActors.push_back({result.actorFormID, result.squirtType});
            continue;
        }
        std::string actorName = GetActorName(result.actorFormID);
        for (const auto& t : result.targets)
            for (const auto& area : t.areas)
                findOrAddEntry(t.targetFormID, area).actorNames.push_back(actorName);
    }

    // Step 2: Collect unique target IDs in first-encounter order.
    std::vector<RE::FormID>        targetOrder;
    std::unordered_set<RE::FormID> seenTargets;
    for (const auto& e : areaEntries)
        if (seenTargets.insert(e.targetFormID).second)
            targetOrder.push_back(e.targetFormID);

    std::string msg;

    // Step 3: Emit sentences per target.
    for (const auto& targetID : targetOrder) {
        std::string targetName = GetActorName(targetID);
        bool        targetMale = IsActorMale(targetID);

        // Gather areas for this target in encounter order, split by category.
        std::vector<AreaEntry*> insideAreas, outsideAreas;
        for (auto& e : areaEntries) {
            if (e.targetFormID != targetID) continue;
            (kInsideAreas.count(e.area) ? insideAreas : outsideAreas).push_back(&e);
        }

        // Inside areas: one sentence each (templates are too specific to merge).
        for (const auto* e : insideAreas) {
            if (!msg.empty()) msg += ' ';
            msg += FormatAreaSentence(FormatActorList(e->actorNames), targetName, targetMale, e->area);
        }

        // Outside areas: group by actor-list string to merge same-actor-group areas.
        std::vector<std::pair<std::string, std::vector<std::string>>> outsideGroups;
        for (const auto* e : outsideAreas) {
            std::string actorListStr = FormatActorList(e->actorNames);
            auto it = std::find_if(outsideGroups.begin(), outsideGroups.end(),
                                   [&](const auto& g) { return g.first == actorListStr; });
            if (it != outsideGroups.end())
                it->second.push_back(e->area);
            else
                outsideGroups.push_back({actorListStr, {e->area}});
        }

        for (const auto& [actorListStr, areas] : outsideGroups) {
            if (!msg.empty()) msg += ' ';
            if (areas.size() == 1)
                msg += FormatAreaSentence(actorListStr, targetName, targetMale, areas[0]);
            else
                msg += FormatMultiOutsideAreaSentence(actorListStr, targetName, targetMale, areas);
        }
    }

    // Trailing plain sentences for actors with no resolved targets.
    for (const auto& [formID, squirtType] : noTargetActors) {
        if (!msg.empty()) msg += ' ';
        if (squirtType == "spurt")
            msg += GetActorName(formID) + " squirted in bursts.";
        else if (squirtType == "flow")
            msg += GetActorName(formID) + " squirted, soaking everything.";
        else
            msg += GetActorName(formID) + " reached climax.";
    }

    return msg;
}

// Builds the JSON string fired as strArg of the ostimnet_climax mod event.
//
// Structure mirrors the other event helpers (type / msg / skipTrigger / threadID)
// with an extra machine-readable climaxActors array:
// {
//   "tton_type": "sex_climax",
//   "msg": "PlayerName came inside NPC (vaginal). NPC2 climaxed.",
//   "skipTrigger": false,
//   "threadID": N,
//   "climaxActors": [
//     { "actorFormID": N, "targets": [ { "targetFormID": N, "areas": ["vaginal"] } ] },
//     { "actorFormID": N }   <- penisless / squirt / unresolved: no "targets" key
//   ]
// }
// --- position extraction ----------------------------------------------------

// Scans the tags returned by ONavGetSceneTags for the first canonical position name.
// Returns the canonical position string (e.g. "missionary") or "" if none found.
inline std::string ExtractPositionFromSceneTags(const char* sceneId) {
    if (!ONavGetSceneTags || !sceneId || sceneId[0] == '\0') return "";
    const char* raw = ONavGetSceneTags(sceneId);
    if (!raw || raw[0] == '\0' || (raw[0] == '[' && raw[1] == ']')) return "";
    try {
        auto arr = nlohmann::json::parse(raw);
        if (!arr.is_array()) return "";
        std::string resolved;
        for (const auto& item : arr) {
            if (!item.is_string()) continue;
            if (OStimNavigatorAPI::ResolvePosition(item.get_ref<const std::string&>(), resolved))
                return resolved;
        }
    } catch (...) {}
    return "";
}

// --- event dispatch helper --------------------------------------------------
#include "ModEventDispatch.h"

}  // namespace OStimNet
