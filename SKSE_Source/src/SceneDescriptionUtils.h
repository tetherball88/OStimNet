#pragma once
#include "api/OstimNG-API-Thread.h"
#include "api/OStimNavigator_PublicAPI.h"
#include "Config.h"
#include "ThreadRegistry.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// =============================================================================
// Canonical-position helpers (OStimNavigatorAPI)
// =============================================================================

namespace OStimNavigatorAPI {

// Returns a reference to the lazily-built position-id->display-name map,
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

// Returns a reference to the lazily-built alias->canonical map,
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

}  // namespace OStimNavigatorAPI

namespace OStimNet {

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


// Replace all {{scenedata.actors.N}} tokens with the pre-resolved actor names.
// Pure string substitution — no RE:: or API dependencies.
inline std::string ResolveActorNames(std::string description,
                                     const std::vector<std::string>& actorNames) {
    for (uint32_t i = 0; i < static_cast<uint32_t>(actorNames.size()); ++i) {
        std::string token = "{{scenedata.actors." + std::to_string(i) + "}}";
        const std::string& name = actorNames[i];
        std::string::size_type pos = 0;
        while ((pos = description.find(token, pos)) != std::string::npos) {
            description.replace(pos, token.size(), name);
            pos += name.size();
        }
    }
    return description;
}

// Fetch the auto-generated description for the current scene of a thread via
// OStimNavigator, resolve actor display names, substitute {{scenedata.actors.N}}
// tokens, and return the final string.
// Falls back to the node display name when OStimNavigator is unavailable.
// Returns an empty string if the thread is invalid or has no active scene.
inline std::string BuildSceneDescription(uint32_t threadID, const std::string& sceneID) {
    if (sceneID.empty()) {
        SKSE::log::info("BuildSceneDescription [thread {}]: sceneID is empty, returning empty", threadID);
        return "";
    }

    std::string description;
    if (ONavBuildSceneDescription) {
        const char* raw = ONavBuildSceneDescription(sceneID.c_str(), threadID);
        if (raw && *raw) {
            description = raw;
            SKSE::log::info("BuildSceneDescription [thread {}; scene {}]: ONav returned '{}'", threadID, sceneID, description);
        } else {
            SKSE::log::info("BuildSceneDescription [thread {}; scene {}]: ONavBuildSceneDescription returned empty/null", threadID, sceneID);
        }
    } else {
        SKSE::log::info("BuildSceneDescription [thread {}; scene {}]: ONavBuildSceneDescription not available", threadID, sceneID);
    }
    if (description.empty()) {
        if (g_ostimThreadInterface && g_ostimThreadInterface->IsThreadValid(threadID)) {
            const char* nodeName = g_ostimThreadInterface->GetCurrentNodeName(threadID);
            if (nodeName && *nodeName) {
                description = nodeName;
                SKSE::log::info("BuildSceneDescription [thread {}; scene {}]: falling back to node name '{}'", threadID, sceneID, description);
            } else {
                SKSE::log::info("BuildSceneDescription [thread {}; scene {}]: node name also empty/null", threadID, sceneID);
            }
        } else {
            SKSE::log::info("BuildSceneDescription [thread {}; scene {}]: OStim thread interface unavailable or thread invalid", threadID, sceneID);
        }
    }

    // Escape newlines so the result is safe to embed in a JSON string value
    // while still rendering as line breaks on the receiving end.
    std::string escaped;
    escaped.reserve(description.size());
    for (char c : description) {
        if (c == '\r') continue;          // drop CR from CRLF pairs
        if (c == '\n') { escaped += "\\n"; continue; }
        escaped += c;
    }
    description = std::move(escaped);

    return ResolveActorNames(std::move(description),
                             ThreadDataStore::GetSingleton().GetActorNames(static_cast<int>(threadID)));
}

// Returns the custom (hand-authored) description for the current scene of a
// thread from OStimNavigator, with {{scenedata.actors.N}} tokens resolved.
// Returns an empty string when OStimNavigator is unavailable, the thread is
// invalid, or no custom entry exists for this scene.
inline std::string BuildCustomSceneDescription(uint32_t threadID, const std::string& sceneID) {
    if (sceneID.empty()) return "";

    if (!ONavGetAnimationDescription) return "";
    const char* raw = ONavGetAnimationDescription(sceneID.c_str());
    if (!raw || !*raw) return "";

    return ResolveActorNames(raw,
                             ThreadDataStore::GetSingleton().GetActorNames(static_cast<int>(threadID)));
}

// Computes the normalized pace tier ("slow", "moderate", "fast", or "") for an active thread.
inline std::string ComputeScenePaceTier(uint32_t threadID) {
    if (!g_ostimThreadInterface || !g_ostimThreadInterface->IsThreadValid(threadID)) return "";
    int32_t maxSpeed = g_ostimThreadInterface->GetMaxSpeed(threadID);
    if (maxSpeed <= 1) return "";
    int32_t currentSpeed = g_ostimThreadInterface->GetCurrentSpeed(threadID);
    if (currentSpeed <= 0) return "";
    int32_t percent = (currentSpeed * 100) / maxSpeed;
    if (percent >= 0 && percent <= 33) return "slow";
    if (percent >= 67 && percent <= 100) return "fast";
    return "moderate";
}

// Returns the base scene description (custom or auto-generated) WITHOUT appending the
// dynamic animation speed/pace suffix, so pose/activity comparisons are unaffected by speed.
inline std::string GetSceneDescriptionWithoutPace(uint32_t threadID, const std::string& sceneID) {
    std::string result;
    if (OStimNet::Config::GetSingleton().UseCustomDescriptions()) {
        std::string custom = BuildCustomSceneDescription(threadID, sceneID);
        if (!custom.empty()) {
            SKSE::log::info("GetSceneDescription [thread {}; scene {}]: using custom description: {}", threadID, sceneID, custom);
            result = std::move(custom);
        } else {
            SKSE::log::info("GetSceneDescription [thread {}; scene {}]: no custom description found, falling back to auto-generated.", threadID, sceneID);
            result = BuildSceneDescription(threadID, sceneID);
        }
    } else {
        result = BuildSceneDescription(threadID, sceneID);
    }
    return result;
}

inline std::string GetSceneDescriptionWithoutPace(uint32_t threadID) {
    if (!g_ostimThreadInterface || !g_ostimThreadInterface->IsThreadValid(threadID)) return "";
    const char* sceneID = g_ostimThreadInterface->GetCurrentSceneID(threadID);
    return GetSceneDescriptionWithoutPace(threadID, sceneID ? sceneID : "");
}

// Returns a scene description based on the current config:
//   - useCustomDescriptions == true  -> custom (hand-authored) description,
//                                       falling back to auto-generated when none exists.
//   - useCustomDescriptions == false -> auto-generated description only.
// Appends dynamic pace prose when the scene has multiple speeds.
// Returns an empty string if the thread is invalid or has no active scene.
inline std::string GetSceneDescription(uint32_t threadID, const std::string& sceneID) {
    std::string result = GetSceneDescriptionWithoutPace(threadID, sceneID);
    if (!result.empty()) {
        std::string tier = ComputeScenePaceTier(threadID);
        if (tier == "slow") {
            result += " The pace is slow and unhurried.";
        } else if (tier == "fast") {
            result += " The pace is fast and vigorous.";
        }
    }
    return result;
}

// Overload that fetches the current sceneID from the OStim API.
inline std::string GetSceneDescription(uint32_t threadID) {
    if (!g_ostimThreadInterface) {
        SKSE::log::info("GetSceneDescription [thread {}]: OStim thread interface not available", threadID);
        return "";
    }
    if (!g_ostimThreadInterface->IsThreadValid(threadID)) {
        SKSE::log::info("GetSceneDescription [thread {}]: thread is not valid", threadID);
        return "";
    }
    const char* sceneID = g_ostimThreadInterface->GetCurrentSceneID(threadID);
    SKSE::log::info("GetSceneDescription [thread {}]: GetCurrentSceneID returned '{}'", threadID, sceneID ? sceneID : "(null)");
    return GetSceneDescription(threadID, sceneID ? sceneID : "");
}

namespace detail {

inline const char* PhaseRankToLabel(int rank) {
    switch (rank) {
        case 1: return "foreplay";
        case 2: return "oral sex";
        case 3: return "intercourse";
        default: return "";
    }
}

inline std::string HumanizeActionType(const std::string& rawAction) {
    std::string a = rawAction;
    std::transform(a.begin(), a.end(), a.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (a.rfind("cumon", 0) == 0 || a.rfind("ejaculation_on", 0) == 0 || a == "facial") return "";
    if (a == "vaginalsex" || a == "sex") return "vaginal sex";
    if (a == "analsex") return "anal sex";
    if (a == "blowjob" || a == "3pp_kissfellatio1" || a == "3pp_kissfellatio2") return "fellatio";
    if (a == "deepthroating") return "deepthroating";
    if (a == "penilelicking" || a == "3pp_lickingpenis") return "licking penis";
    if (a == "cunnilingus" || a == "lickingvagina") return "cunnilingus";
    if (a == "anallicking" || a == "rimjob") return "anilingus";
    if (a == "handjob") return "handjob";
    if (a == "fingering" || a == "vaginalfingering") return "fingering";
    if (a == "analfingering") return "anal fingering";
    if (a == "rubbingclitoris") return "clitoral rubbing";
    if (a == "boobjob" || a == "3pp_boobjob") return "boobjob";
    if (a == "thighjob") return "thighjob";
    if (a == "footjob") return "footjob";
    if (a == "buttjob" || a == "3pp_buttjob") return "buttjob";
    if (a == "kissing" || a == "frenchkissing") return "kissing";
    if (a == "suckingnipple" || a == "lickingnipple") return "nipple stimulation";
    if (a == "gropingbreast" || a == "3pp_gropingbreast") return "breast groping";
    if (a == "gropingbutt" || a == "3pp_gropingbutt") return "butt groping";
    if (a == "spanking") return "spanking";
    if (a == "malemasturbation" || a == "femalemasturbation") return "masturbation";
    if (a == "cuddling" || a == "holding" || a == "hugging") return a;
    return a;
}

inline std::vector<std::string> ExtractSceneActionLabels(
    const std::string& sceneID,
    const std::vector<std::string>& actorNames) {
    std::vector<std::string> out;
    if (sceneID.empty()) return out;

    std::unordered_set<std::string> seen;
    if (ONavGetSceneActionsDetailed) {
        const char* raw = ONavGetSceneActionsDetailed(sceneID.c_str());
        if (raw && raw[0] != '\0') {
            try {
                auto arr = nlohmann::json::parse(raw, nullptr, /*allow_exceptions=*/false);
                if (!arr.is_discarded() && arr.is_array()) {
                    for (const auto& item : arr) {
                        if (!item.is_object() || !item.contains("type") || !item["type"].is_string()) continue;
                        std::string label = HumanizeActionType(item["type"].get<std::string>());
                        if (label.empty()) continue;

                        int actorSlot  = (item.contains("actor") && item["actor"].is_number_integer())
                            ? item["actor"].get<int>() : -1;
                        int targetSlot = (item.contains("target") && item["target"].is_number_integer())
                            ? item["target"].get<int>() : -1;

                        std::string full = label;
                        if (actorSlot >= 0 && static_cast<size_t>(actorSlot) < actorNames.size()) {
                            const std::string& aName = actorNames[static_cast<size_t>(actorSlot)];
                            if (targetSlot >= 0 && targetSlot != actorSlot &&
                                static_cast<size_t>(targetSlot) < actorNames.size()) {
                                full = label + " (" + aName + " -> " + actorNames[static_cast<size_t>(targetSlot)] + ")";
                            } else if (!aName.empty()) {
                                full = label + " (" + aName + ")";
                            }
                        }
                        if (seen.insert(full).second) out.push_back(std::move(full));
                    }
                }
            } catch (...) {}
        }
    }

    if (out.empty() && ONavGetSceneActions) {
        const char* raw = ONavGetSceneActions(sceneID.c_str());
        if (raw && raw[0] != '\0') {
            try {
                auto arr = nlohmann::json::parse(raw, nullptr, /*allow_exceptions=*/false);
                if (!arr.is_discarded() && arr.is_array()) {
                    for (const auto& item : arr) {
                        if (!item.is_string()) continue;
                        std::string label = HumanizeActionType(item.get<std::string>());
                        if (!label.empty() && seen.insert(label).second)
                            out.push_back(std::move(label));
                    }
                }
            } catch (...) {}
        }
    }
    return out;
}

}  // namespace detail

struct StructuralSceneDiff {
    std::string hintSummary;   // Structured grounding hint for the "meta" diff prompt
    std::string fallbackCue;   // Natural 1-sentence fallback if "meta" LLM is unavailable/fails
    bool        hasTagChange = false;
};

// Computes deterministic structural differences (phase, canonical position, actor-target actions)
// between prevSceneID and currSceneID on the SKSE game thread.
inline StructuralSceneDiff BuildStructuralSceneDiff(
    uint32_t threadID,
    const std::string& prevSceneID,
    const std::string& currSceneID) {
    StructuralSceneDiff out;
    if (currSceneID.empty()) return out;

    const auto actorNames = ThreadDataStore::GetSingleton().GetActorNames(static_cast<int>(threadID));

    const int prevRank = (!prevSceneID.empty() && ONavGetScenePhaseRank) ? ONavGetScenePhaseRank(prevSceneID.c_str()) : -1;
    const int currRank = ONavGetScenePhaseRank ? ONavGetScenePhaseRank(currSceneID.c_str()) : -1;
    const std::string prevPhase = detail::PhaseRankToLabel(prevRank);
    const std::string currPhase = detail::PhaseRankToLabel(currRank);

    const std::string prevPos = prevSceneID.empty() ? "" : ExtractPositionFromSceneTags(prevSceneID.c_str());
    const std::string currPos = ExtractPositionFromSceneTags(currSceneID.c_str());

    const auto prevActions = detail::ExtractSceneActionLabels(prevSceneID, actorNames);
    const auto currActions = detail::ExtractSceneActionLabels(currSceneID, actorNames);

    std::unordered_set<std::string> prevSet(prevActions.begin(), prevActions.end());
    std::unordered_set<std::string> currSet(currActions.begin(), currActions.end());

    std::vector<std::string> startedActions;
    for (const auto& a : currActions) {
        if (!prevSet.count(a)) startedActions.push_back(a);
    }
    std::vector<std::string> stoppedActions;
    for (const auto& a : prevActions) {
        if (!currSet.count(a)) stoppedActions.push_back(a);
    }

    std::vector<std::string> parts;
    if (!prevPhase.empty() && !currPhase.empty() && prevPhase != currPhase) {
        parts.push_back("activity stage shifted from " + prevPhase + " to " + currPhase);
    }
    if (prevPos != currPos) {
        if (!prevPos.empty() && !currPos.empty()) {
            parts.push_back("position changed from " + prevPos + " to " + currPos);
        } else if (!currPos.empty()) {
            parts.push_back("moved into " + currPos + " position");
        }
    }
    if (!startedActions.empty()) {
        parts.push_back("started " + FormatActorList(startedActions));
    }
    if (!stoppedActions.empty() && !prevSceneID.empty()) {
        parts.push_back("ended " + FormatActorList(stoppedActions));
    }

    if (!parts.empty()) {
        out.hasTagChange = true;
        std::string joined;
        for (size_t i = 0; i < parts.size(); ++i) {
            if (i > 0) joined += "; ";
            joined += parts[i];
        }
        if (!joined.empty()) {
            joined[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(joined[0])));
        }
        out.hintSummary = joined + ".";
        out.fallbackCue = out.hintSummary;
    } else {
        out.hasTagChange = false;
        if (!currPos.empty()) {
            out.hintSummary = "Same base position (" + currPos + ") and core actions; compare the two descriptions for subtle posture, limb, grip, or eye-contact changes.";
            out.fallbackCue = "Shifted posture into a new variation of " + currPos + ".";
        } else {
            out.hintSummary = "Same core actions; compare the two descriptions for subtle posture, limb, grip, or angle changes.";
            out.fallbackCue = "Shifted into a new physical posture.";
        }
    }

    return out;
}

}  // namespace OStimNet
