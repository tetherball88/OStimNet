#pragma once
#include <optional>
#include <string>
#include <vector>
#include "api/OStimNavigator_PublicAPI.h"

namespace OStimNet {

// -------------------------------------------------------------------------
// Sexual encounter phase — tracks gradual progression within a thread.
//
// Undressing  — stripping/undressing animations; advancement mechanism TBD
// Foreplay    — non-penetrative stimulation (handjob, fingering, toying, etc.)
// Oral        — oral sex (blowjob, cunnilingus, rimjob, etc.)
// Sex         — penetrative intercourse (vaginalsex, analsex)
//
// Phases are opt-in via Config::EnableThreadPhases (global toggle).
// Each intent has a hardcoded ordered phase list (see IntentToPhaseList).
// Threads whose intent has an empty list are not phase-tracked.
// Phase advances forward only — never backwards.
// -------------------------------------------------------------------------
enum class SexualPhase { Undressing, Foreplay, Oral, Sex };

inline const char* SexualPhaseToName(SexualPhase p) {
    switch (p) {
        case SexualPhase::Undressing: return "undressing";
        case SexualPhase::Foreplay:   return "foreplay";
        case SexualPhase::Oral:       return "oral";
        case SexualPhase::Sex:        return "sex";
        default:                      return "";
    }
}

// Ordered sequence of phases for each intent.
// Empty list = phases disabled for this intent.
inline const std::vector<SexualPhase>& IntentToPhaseList(OStimNavigatorAPI::Intent intent) {
    static const std::vector<SexualPhase> kRomantic      = {SexualPhase::Undressing, SexualPhase::Foreplay, SexualPhase::Oral, SexualPhase::Sex};
    static const std::vector<SexualPhase> kLustful       = {SexualPhase::Foreplay, SexualPhase::Oral, SexualPhase::Sex};
    static const std::vector<SexualPhase> kTransactional = {SexualPhase::Oral, SexualPhase::Sex};
    static const std::vector<SexualPhase> kSexOnly       = {SexualPhase::Sex};
    static const std::vector<SexualPhase> kDisabled      = {};

    switch (intent) {
        case OStimNavigatorAPI::Intent::Romantic:      return kRomantic;
        case OStimNavigatorAPI::Intent::Lustful:       return kLustful;
        case OStimNavigatorAPI::Intent::Transactional: return kTransactional;
        case OStimNavigatorAPI::Intent::Dom:           return kSexOnly;
        case OStimNavigatorAPI::Intent::Aggressive:    return kSexOnly;
        default:                                       return kDisabled;
    }
}

// Returns the 0-based index of `phase` in the intent's phase list, or -1 if not found.
inline int GetPhaseIndex(OStimNavigatorAPI::Intent intent, SexualPhase phase) {
    const auto& list = IntentToPhaseList(intent);
    for (int i = 0; i < static_cast<int>(list.size()); ++i)
        if (list[i] == phase) return i;
    return -1;
}

// Returns the next phase after `current` in the intent's ordered list,
// or std::nullopt if `current` is the last phase.
inline std::optional<SexualPhase> GetNextPhase(OStimNavigatorAPI::Intent intent, SexualPhase current) {
    const auto& list = IntentToPhaseList(intent);
    for (size_t i = 0; i + 1 < list.size(); ++i)
        if (list[i] == current) return list[i + 1];
    return std::nullopt;
}

// Queries ONavGetScenePhaseRank for a scene and maps the returned rank to a SexualPhase.
//
// Rank values returned by OStim Navigator:
//   3 = Sex      (any action tagged "intercourse")
//   2 = Oral     (any action tagged "oral")
//   1 = Foreplay (any action tagged "penilestimulation", "fingering", "toying", or "clitoralstimulation")
//  -1 = no phase-relevant action found / scene unknown
//
// These correspond directly to SexualPhase enum values (Sex=3, Oral=2, Foreplay=1).
// Tag resolution uses canonical types — aliases are resolved at scene load time inside OStim Navigator.
//
// Must be called from the SKSE game thread.
inline std::optional<SexualPhase> DetectPhaseFromSceneID(const char* sceneID) {
    if (!ONavGetScenePhaseRank || !sceneID || sceneID[0] == '\0') return std::nullopt;
    int rank = ONavGetScenePhaseRank(sceneID);
    if (rank < static_cast<int>(SexualPhase::Foreplay) ||
        rank > static_cast<int>(SexualPhase::Sex)) return std::nullopt;
    return static_cast<SexualPhase>(rank);
}

// Returns a comma-separated string of OStim activity keywords for the given phase.
// Suitable for use as the activity filter in SceneSexSearch.
inline std::string SexualPhaseToActivityKeywordString(SexualPhase p) {
    switch (p) {
        case SexualPhase::Undressing: return "undressing";
        case SexualPhase::Foreplay:   return "foreplay";
        case SexualPhase::Oral:       return "oral";
        case SexualPhase::Sex:        return "vaginalsex,analsex";
        default:                      return "";
    }
}

}  // namespace OStimNet
