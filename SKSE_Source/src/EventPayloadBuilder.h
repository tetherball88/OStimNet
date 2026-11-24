#pragma once
#include "Config.h"
#include "DebounceQueue.h"
#include "OStimEventHelpers.h"

namespace OStimNet {

// Core builder: all metadata passed directly.
inline nlohmann::json BuildBaseEventJson(const std::string& type, const std::string& msg,
                                         int threadID, bool skipTrigger,
                                         const std::string& declinedAction,
                                         Intent intent, std::optional<bool> isSexual,
                                         const std::string& mainActorNames,
                                         const std::string& secondaryActorNames,
                                         const std::string& speaker = "") {
    nlohmann::json j;
    j["tton_type"]           = type;
    j["declinedAction"]      = declinedAction;
    j["msg"]                 = msg;
    j["skipTrigger"]         = skipTrigger;
    j["threadID"]            = threadID;
    j["intent"]              = IntentToString(intent);
    j["isSexual"]            = isSexual.value_or(false);
    j["mainActorNames"]      = mainActorNames;
    j["secondaryActorNames"] = secondaryActorNames;
    j["speaker"]             = speaker;
    return j;
}

// Convenience wrapper: reads intent/isSexual/actor-names from ThreadDataStore.
inline nlohmann::json BuildBaseEventJson(const std::string& type, const std::string& msg,
                                         int threadID, bool skipTrigger = false,
                                         const std::string& speaker = "") {
    auto& store = ThreadDataStore::GetSingleton();
    return BuildBaseEventJson(type, msg, threadID, skipTrigger, "",
                              store.GetIntent(threadID), store.GetSexual(threadID),
                              store.GetFormattedMainActorNames(threadID),
                              store.GetFormattedSecondaryActorNames(threadID),
                              speaker);
}

}  // namespace OStimNet

// Central namespace for building tton_event JSON payloads.
// All functions pull thread state from ThreadRegistry; skipTrigger / IsMuted
// are handled internally so callers in OStimEventListener.h do not need to
// pass Config values themselves.
namespace OStimNet::EventPayloadBuilder {

// --- ostimnet_start ---------------------------------------------------------

inline std::string BuildStart(int threadID, RE::Actor* speaker = nullptr) {
    std::string msg = GetSceneDescription(static_cast<uint32_t>(threadID));
    std::string speakerName = ThreadDataStore::GetActorDisplayName(speaker, "");
    return BuildBaseEventJson("sex_start", msg, threadID, Config::GetSingleton().IsMuted(), speakerName).dump();
}

// Overload used when the caller supplies an explicit description (e.g. undressing phase).
inline std::string BuildStart(int threadID, const std::string& overrideMsg, RE::Actor* speaker = nullptr) {
    std::string speakerName = ThreadDataStore::GetActorDisplayName(speaker, "");
    return BuildBaseEventJson("sex_start", overrideMsg, threadID, Config::GetSingleton().IsMuted(), speakerName).dump();
}

// --- ostimnet_continue_thread -----------------------------------------------

// oldThreadID: the thread that just ended — provides prior participants ("main").
// newThreadID: the replacement thread — newly joined actors appear as "secondary".
// Emits a concise 1-line milestone (no full scene description dump).
inline std::string BuildContinueThread(int oldThreadID,
                                       int newThreadID,
                                       const std::vector<uint32_t>& preUpdateOldFormIDs = {},
                                       RE::Actor* speaker = nullptr) {
    auto& store = ThreadDataStore::GetSingleton();

    std::unordered_set<uint32_t> oldFormIDs(preUpdateOldFormIDs.begin(), preUpdateOldFormIDs.end());
    std::vector<RE::Actor*> oldActors;
    if (oldFormIDs.empty()) {
        for (auto* a : store.GetActorPtrs(oldThreadID)) {
            if (a) {
                oldFormIDs.insert(a->GetFormID());
                oldActors.push_back(a);
            }
        }
    } else {
        for (uint32_t fid : preUpdateOldFormIDs) {
            if (auto* a = RE::TESForm::LookupByID<RE::Actor>(fid)) {
                oldActors.push_back(a);
            }
        }
    }

    std::vector<RE::Actor*> joinedActors;
    for (auto* a : store.GetActorPtrs(newThreadID))
        if (a && !oldFormIDs.count(a->GetFormID()))
            joinedActors.push_back(a);

    const std::string oldNames    = ThreadDataStore::FormatActorNames(oldActors);
    const std::string joinedNames = ThreadDataStore::FormatActorNames(joinedActors);

    std::string msg;
    if (!joinedNames.empty() && !oldNames.empty()) {
        msg = joinedNames + " joined " + oldNames + " in the ongoing intimate encounter.";
    } else if (!joinedNames.empty()) {
        msg = joinedNames + " joined the ongoing intimate encounter.";
    } else if (!oldNames.empty()) {
        msg = "Participants in " + oldNames + "'s intimate encounter shifted roles.";
    } else {
        msg = "Participants in the ongoing intimate encounter changed.";
    }

    std::string speakerName = ThreadDataStore::GetActorDisplayName(speaker, "");
    // skipTrigger = true because spoken continuation reactions go through TriggerSceneTransitionReaction.
    return BuildBaseEventJson("sex_continue_thread", msg, newThreadID,
                              /*skipTrigger=*/true, "",
                              store.GetIntent(newThreadID), store.GetSexual(newThreadID),
                              oldNames.empty() ? store.GetFormattedMainActorNames(newThreadID) : oldNames,
                              joinedNames.empty() ? store.GetFormattedSecondaryActorNames(newThreadID) : joinedNames,
                              speakerName).dump();
}

// --- ostimnet_scene_change --------------------------------------------------

// skipTrigger: caller owns this decision — thread priority filter may set it
// to true for non-priority threads regardless of Config::IsMuted.
inline std::string BuildSceneChange(int threadID, const std::string& sceneID, bool skipTrigger, RE::Actor* speaker = nullptr) {
    auto& store = ThreadDataStore::GetSingleton();
    std::string sceneDesc = GetSceneDescription(static_cast<uint32_t>(threadID), sceneID);
    std::string speakerName = ThreadDataStore::GetActorDisplayName(speaker, "");
    nlohmann::json j = BuildBaseEventJson("sex_change", sceneDesc, threadID, skipTrigger, speakerName);
    const std::string& position = store.GetCurrentPosition(threadID);
    if (!position.empty()) j["currentPosition"] = position;
    return j.dump();
}

// --- ostimnet_speed_change --------------------------------------------------

// oldSpeed: the speed index before the change (-1 means unknown/first change).
// newSpeed: the new speed index.
// skipTrigger: caller owns this decision — same priority-thread filter as BuildSceneChange.
// direction is derived from the two values; "changed" is the fallback when
// oldSpeed is unknown.
inline std::optional<std::string> BuildSpeedChange(int threadID, int32_t oldSpeed, int32_t newSpeed, bool skipTrigger, RE::Actor* speaker = nullptr) {
    if (newSpeed == oldSpeed)
        return std::nullopt;

    auto& store = ThreadDataStore::GetSingleton();
    const auto& mainNames      = store.GetFormattedMainActorNames(threadID);
    const auto& secondaryNames = store.GetFormattedSecondaryActorNames(threadID);

    std::string allNames;
    if (!mainNames.empty() && !secondaryNames.empty())
        allNames = mainNames + " and " + secondaryNames;
    else if (!mainNames.empty())
        allNames = mainNames;
    else if (!secondaryNames.empty())
        allNames = secondaryNames;

    const std::string direction = (newSpeed > oldSpeed) ? "increased" : "decreased";

    std::string msg;
    if (allNames.empty())
        msg = "The pace " + direction + ".";
    else
        msg = allNames + "'s pace " + direction + ".";

    std::string speakerName = ThreadDataStore::GetActorDisplayName(speaker, "");
    nlohmann::json j = BuildBaseEventJson("sex_pace_change", msg, threadID, skipTrigger, speakerName);
    j["speed"]     = newSpeed;
    j["direction"] = direction;
    return j.dump();
}

// --- ostimnet_climax --------------------------------------------------------

inline std::string BuildClimax(int threadID, const DebounceQueue::ClimaxBatchData& data, RE::Actor* speaker = nullptr) {
    ClimaxActorSnapshot snapshot;
    if (g_ostimThreadInterface)
        snapshot.count = g_ostimThreadInterface->GetActors(
            static_cast<uint32_t>(threadID), snapshot.buffer, ClimaxActorSnapshot::kMaxActors);

    auto actorResults = BuildClimaxActorData(data, snapshot);
    std::string humanMsg = FormatClimaxMessage(threadID, actorResults, snapshot);

    std::string speakerName = ThreadDataStore::GetActorDisplayName(speaker, "");
    nlohmann::json j = BuildBaseEventJson("climax", humanMsg, threadID, Config::GetSingleton().IsMuted(), speakerName);

    auto climaxActors = nlohmann::json::array();
    for (auto& result : actorResults) {
        nlohmann::json actor;
        actor["actorFormID"] = result.actorFormID;
        if (!result.targets.empty()) {
            auto targets = nlohmann::json::array();
            for (auto& t : result.targets) {
                nlohmann::json tgt;
                tgt["targetFormID"] = t.targetFormID;
                tgt["areas"]        = t.areas;
                targets.push_back(std::move(tgt));
            }
            actor["targets"] = std::move(targets);
        }
        climaxActors.push_back(std::move(actor));
    }
    j["climaxActors"] = std::move(climaxActors);
    return j.dump();
}

// --- ostimnet_stop ----------------------------------------------------------

inline std::string BuildStop(int threadID,
                             RE::Actor* speaker = nullptr,
                             const std::string& interruptReason = "",
                             const std::string& summaryMsg = "") {
    auto& store = ThreadDataStore::GetSingleton();
    std::string speakerName = ThreadDataStore::GetActorDisplayName(speaker, "");
    const bool interrupted = !interruptReason.empty();
    std::string msg = summaryMsg;
    if (msg.empty() && interrupted) {
        std::string names = ThreadDataStore::FormatActorNames(store.GetActorPtrs(threadID));
        if (names.empty()) names = "participants";
        if (interruptReason == "arrest") {
            msg = "The intimate encounter involving " + names + " was abruptly cut short by crime and guard intervention.";
        } else {
            msg = "The intimate encounter involving " + names + " was abruptly interrupted by sudden combat and danger.";
        }
    }
    nlohmann::json j = BuildBaseEventJson("sex_stop", msg, threadID, Config::GetSingleton().IsMuted(), speakerName);
    j["interrupted"]     = interrupted;
    j["interruptReason"] = interruptReason;
    return j.dump();
}

// Builds sex_stop payload from a pre-ClearThread EncounterStopSnapshot (safe after async LLM completion).
inline std::string BuildStopFromSnapshot(const EncounterStopSnapshot& snap,
                                         const std::string& summaryMsg) {
    std::string msg = summaryMsg;
    if (msg.empty() && snap.interrupted) {
        std::string names = snap.actorNames.empty() ? "participants" : snap.actorNames;
        if (snap.interruptReason == "arrest") {
            msg = "The intimate encounter involving " + names + " was abruptly cut short by crime and guard intervention.";
        } else {
            msg = "The intimate encounter involving " + names + " was abruptly interrupted by sudden combat and danger.";
        }
    }
    nlohmann::json j = BuildBaseEventJson(
        "sex_stop",
        msg,
        snap.threadID,
        snap.skipTrigger,
        "",
        snap.intent,
        snap.isSexual,
        snap.mainActorNames,
        snap.secondaryActorNames,
        snap.speakerName);
    j["interrupted"]     = snap.interrupted;
    j["interruptReason"] = snap.interruptReason;
    return j.dump();
}

// --- ostimnet_spectator_added -----------------------------------------------
// threadID: already resolved by the caller (e.g. via OStimEventListener).
// spectator: the nearby actor who is watching — provides display name and FormID.
// target:    the specific thread actor the spectator has a relationship with.
//            Falls back to all thread actors when nullptr.

inline std::string BuildSpectatorAdded(int threadID, RE::Actor* spectator, RE::Actor* target) {
    auto& store = ThreadDataStore::GetSingleton();
    const std::string spectatorName = ThreadDataStore::GetActorDisplayName(spectator, "");
    const std::string watchedName   = target
        ? ThreadDataStore::GetActorDisplayName(target, "")
        : ThreadDataStore::FormatActorNames(store.GetActorPtrs(threadID));
    const std::string msg = spectatorName + " is now watching " + watchedName + " engage in an intimate encounter.";
    nlohmann::json j = BuildBaseEventJson("spectator_added", msg, threadID, Config::GetSingleton().IsMuted(), spectatorName);
    if (spectator) j["spectatorFormID"] = spectator->GetFormID();
    return j.dump();
}

// --- ostimnet_spectator_fled ------------------------------------------------
// spectator: the actor who was watching and has now left the area.
// threadID:  -1 if the thread has already ended (fled after stop).

inline std::string BuildSpectatorFled(int threadID, RE::Actor* spectator) {
    auto& store = ThreadDataStore::GetSingleton();
    const std::string spectatorName = ThreadDataStore::GetActorDisplayName(spectator, "someone");
    std::string msg;
    if (threadID == -1) {
        msg = spectatorName + " was watching an intimate encounter but fled the scene.";
    } else {
        const std::string actorNames = ThreadDataStore::FormatActorNames(store.GetActorPtrs(threadID));
        msg = spectatorName + " was watching " + actorNames + " having an intimate encounter but fled the scene.";
    }
    const std::string speakerName = ThreadDataStore::GetActorDisplayName(spectator, "");
    nlohmann::json j = BuildBaseEventJson("spectator_fled", msg, threadID, Config::GetSingleton().IsMuted(), speakerName);
    if (spectator) j["spectatorFormID"] = spectator->GetFormID();
    return j.dump();
}

// --- ostimnet_intent_changed ----------------------------------------------
// oldIntent: the intent string before the change.
// mainActors / secondaryActors: the final actor lists after the reshuffle.

inline std::string BuildIntentChanged(int threadID,
                                       const std::string& oldIntent,
                                       const std::string& newIntent,
                                       bool mainActorsSame,
                                       RE::Actor* speaker = nullptr) {
    auto& store = ThreadDataStore::GetSingleton();
    const std::string mainNames   = store.GetFormattedMainActorNames(threadID);
    const std::string msg         = mainNames + "'s intent changed from " + oldIntent + " to " + newIntent + ".";
    const std::string speakerName = ThreadDataStore::GetActorDisplayName(speaker, "");
    nlohmann::json j = BuildBaseEventJson("intent_changed", msg, threadID, Config::GetSingleton().IsMuted(), speakerName);
    j["oldIntent"]      = oldIntent;
    j["mainActorsSame"] = mainActorsSame;
    return j.dump();
}

// --- ostimnet_partner_near_edge -------------------------------------------
inline std::string BuildPartnerNearEdge(int threadID, RE::Actor* speaker = nullptr) {
    std::string speakerName = ThreadDataStore::GetActorDisplayName(speaker, "");
    std::string msg = "Partner is tensing up and approaching climax.";
    return BuildBaseEventJson("partner_near_edge", msg, threadID, Config::GetSingleton().IsMuted(), speakerName).dump();
}

// --- ostimnet_pullout -------------------------------------------------------
inline std::string BuildPullout(int threadID, RE::Actor* giver, RE::Actor* receiver) {
    std::string giverName = ThreadDataStore::GetActorDisplayName(giver, "");
    std::string receiverName = ThreadDataStore::GetActorDisplayName(receiver, "");
    std::string msg;
    if (!giverName.empty() && !receiverName.empty()) {
        msg = giverName + " pulled out just before climaxing inside " + receiverName + ".";
    } else if (!giverName.empty()) {
        msg = giverName + " pulled out just before climaxing inside.";
    } else {
        msg = "Pulled out just before climaxing inside.";
    }
    return BuildBaseEventJson("pullout", msg, threadID, Config::GetSingleton().IsMuted(), giverName).dump();
}

// --- ostimnet_virginity_lost ------------------------------------------------
namespace detail {
inline const char* GetPossessivePronoun(RE::Actor* actor) {
    if (actor) {
        if (auto* base = actor->GetActorBase()) {
            auto sex = base->GetSex();
            if (sex == RE::SEX::kFemale) return "her";
            if (sex == RE::SEX::kMale) return "his";
        }
    }
    return "their";
}

inline const char* BuildVirginityRoleTag(Intent intent, bool virginIsMain) {
    switch (intent) {
        case Intent::Aggressive:    return virginIsMain ? "aggressor" : "victim";
        case Intent::Dom:           return virginIsMain ? "dominant" : "submissive";
        case Intent::Transactional: return virginIsMain ? "client" : "provider";
        default:                    return "partner";
    }
}
}  // namespace detail

inline std::string BuildVirginityLost(int threadID, RE::Actor* virginActor, RE::Actor* partnerActor, RE::Actor* speaker, Intent intent, bool virginIsMain) {
    std::string virginName = ThreadDataStore::GetActorDisplayName(virginActor, "Someone");
    std::string partnerName = partnerActor ? ThreadDataStore::GetActorDisplayName(partnerActor, "") : "";
    std::string speakerName = ThreadDataStore::GetActorDisplayName(speaker, "");

    bool isFemaleVirgin = false;
    if (virginActor) {
        if (auto* base = virginActor->GetActorBase()) {
            isFemaleVirgin = (base->GetSex() == RE::SEX::kFemale);
        }
    }
    const char* possessive = detail::GetPossessivePronoun(virginActor);

    std::string msg;
    if (intent == Intent::Aggressive) {
        if (virginIsMain) {
            msg = !partnerName.empty()
                ? (virginName + " just lost " + possessive + " virginity while forcing herself on " + partnerName + " in a non-consensual assault.")
                : (virginName + " just lost " + possessive + " virginity as the aggressor in a non-consensual assault.");
        } else {
            msg = !partnerName.empty()
                ? (partnerName + " just forcibly took " + virginName + "'s virginity against " + possessive + " will.")
                : (virginName + " just had " + possessive + " virginity forcibly taken against " + possessive + " will.");
        }
    } else if (intent == Intent::Dom) {
        if (virginIsMain) {
            msg = !partnerName.empty()
                ? (virginName + " just lost " + possessive + " virginity while taking command as the dominant partner over " + partnerName + ".")
                : (virginName + " just lost " + possessive + " virginity as the dominant partner.");
        } else {
            msg = !partnerName.empty()
                ? (virginName + " just willingly surrendered " + possessive + " virginity as " + partnerName + "'s submissive.")
                : (virginName + " just willingly surrendered " + possessive + " virginity as the submissive partner.");
        }
    } else {
        msg = !partnerName.empty()
            ? (virginName + " just lost " + possessive + " virginity to " + partnerName + ".")
            : (virginName + " just lost " + possessive + " virginity.");
    }

    nlohmann::json j = BuildBaseEventJson("virginity_lost", msg, threadID, Config::GetSingleton().IsMuted(), speakerName);
    j["intent"] = IntentToString(intent);
    j["virginActorName"] = virginName;
    j["partnerActorName"] = partnerName;
    j["isFemaleVirgin"] = isFemaleVirgin;
    j["virginIsMain"] = virginIsMain;
    j["virginRole"] = detail::BuildVirginityRoleTag(intent, virginIsMain);
    return j.dump();
}

// --- ostimnet_virginity_lost_diary ------------------------------------------
inline std::string BuildVirginityLostDiary(int threadID, RE::Actor* virginActor, RE::Actor* partnerActor, RE::Actor* diaryActor, Intent intent, bool virginIsMain) {
    std::string virginName = ThreadDataStore::GetActorDisplayName(virginActor, "Someone");
    std::string partnerName = partnerActor ? ThreadDataStore::GetActorDisplayName(partnerActor, "") : "";
    std::string diaryName = ThreadDataStore::GetActorDisplayName(diaryActor, "");

    const char* possessive = detail::GetPossessivePronoun(virginActor);
    bool virginIsOriginator = (virginActor && diaryActor && virginActor->GetFormID() == diaryActor->GetFormID());
    std::string actorLabel = !diaryName.empty() ? diaryName : (!partnerName.empty() ? partnerName : "Someone");

    std::string msg;
    if (intent == Intent::Aggressive) {
        if (virginIsMain) {
            msg = virginIsOriginator
                ? (!partnerName.empty()
                    ? (virginName + " lost " + possessive + " virginity by forcing herself on " + partnerName + " in a non-consensual sexual assault.")
                    : (virginName + " lost " + possessive + " virginity as the aggressor in a non-consensual assault."))
                : (actorLabel + " was sexually assaulted against their will by " + virginName + ", who took " + possessive + " own first time by force.");
        } else {
            msg = virginIsOriginator
                ? (!partnerName.empty()
                    ? (virginName + " had " + possessive + " virginity forcibly taken against " + possessive + " will by " + partnerName + " in a non-consensual sexual assault.")
                    : (virginName + " had " + possessive + " virginity forcibly taken against " + possessive + " will."))
                : (actorLabel + " forcibly took " + virginName + "'s virginity against " + possessive + " will in a non-consensual sexual assault.");
        }
    } else if (intent == Intent::Dom) {
        if (virginIsMain) {
            msg = virginIsOriginator
                ? (!partnerName.empty()
                    ? (virginName + " lost " + possessive + " virginity while dominating " + partnerName + " in a consensual dominant-submissive encounter.")
                    : (virginName + " lost " + possessive + " virginity as the dominant partner."))
                : (actorLabel + " willingly submitted to " + virginName + " as " + possessive + " very first sexual partner while she took the dominant role.");
        } else {
            msg = virginIsOriginator
                ? (!partnerName.empty()
                    ? (virginName + " willingly surrendered " + possessive + " virginity as the submissive partner to " + partnerName + " in a consensual dominant-submissive encounter.")
                    : (virginName + " willingly surrendered " + possessive + " virginity as the submissive partner."))
                : (actorLabel + " took " + virginName + "'s virginity as the dominant partner while she willingly submitted.");
        }
    } else if (virginIsOriginator) {
        const char* suffix = (intent == Intent::Transactional) ? " in a transactional arrangement." : ".";
        msg = !partnerName.empty()
            ? (virginName + " lost " + possessive + " virginity to " + partnerName + suffix)
            : (virginName + " lost " + possessive + " virginity" + suffix);
    } else {
        const char* suffix = (intent == Intent::Transactional) ? "'s very first sexual partner in a transactional arrangement." : "'s very first sexual partner.";
        msg = actorLabel + " was " + virginName + suffix;
    }

    nlohmann::json j = BuildBaseEventJson("virginity_lost_diary", msg, threadID, Config::GetSingleton().IsMuted(), diaryName);
    j["intent"] = IntentToString(intent);
    j["isSexual"] = true;
    j["virginActorName"] = virginName;
    j["partnerActorName"] = partnerName;
    j["virginIsOriginator"] = virginIsOriginator;
    j["virginIsMain"] = virginIsMain;
    j["virginRole"] = detail::BuildVirginityRoleTag(intent, virginIsMain);
    return j.dump();
}

}  // namespace OStimNet::EventPayloadBuilder
