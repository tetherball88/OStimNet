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
                                         const std::string& secondaryActorNames) {
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
    return j;
}

// Convenience wrapper: reads intent/isSexual/actor-names from ThreadDataStore.
inline nlohmann::json BuildBaseEventJson(const std::string& type, const std::string& msg,
                                         int threadID, bool skipTrigger = false) {
    auto& store = ThreadDataStore::GetSingleton();
    return BuildBaseEventJson(type, msg, threadID, skipTrigger, "",
                              store.GetIntent(threadID), store.GetSexual(threadID),
                              store.GetFormattedMainActorNames(threadID),
                              store.GetFormattedSecondaryActorNames(threadID));
}

}  // namespace OStimNet

// Central namespace for building tton_event JSON payloads.
// All functions pull thread state from ThreadRegistry; skipTrigger / IsMuted
// are handled internally so callers in OStimEventListener.h do not need to
// pass Config values themselves.
namespace OStimNet::EventPayloadBuilder {

// --- ostimnet_start ---------------------------------------------------------

inline std::string BuildStart(int threadID) {
    std::string msg = GetSceneDescription(static_cast<uint32_t>(threadID));
    return BuildBaseEventJson("sex_start", msg, threadID, Config::GetSingleton().IsMuted()).dump();
}

// Overload used when the caller supplies an explicit description (e.g. undressing phase).
inline std::string BuildStart(int threadID, const std::string& overrideMsg) {
    return BuildBaseEventJson("sex_start", overrideMsg, threadID, Config::GetSingleton().IsMuted()).dump();
}

// --- ostimnet_continue_thread -----------------------------------------------

// oldThreadID: the thread that just ended — provides intent, actors ("main").
// newThreadID: the replacement thread — newly joined actors appear as "secondary".
inline std::string BuildContinueThread(int oldThreadID, int newThreadID) {
    auto& store = ThreadDataStore::GetSingleton();

    const auto& oldPtrs = store.GetActorPtrs(oldThreadID);
    std::unordered_set<uint32_t> oldFormIDs;
    oldFormIDs.reserve(oldPtrs.size());
    for (auto* a : oldPtrs)
        if (a) oldFormIDs.insert(a->GetFormID());

    std::vector<RE::Actor*> joinedActors;
    for (auto* a : store.GetActorPtrs(newThreadID))
        if (a && !oldFormIDs.count(a->GetFormID()))
            joinedActors.push_back(a);

    std::string msg = GetSceneDescription(static_cast<uint32_t>(newThreadID));
    return BuildBaseEventJson("sex_continue_thread", msg, newThreadID,
                              Config::GetSingleton().IsMuted(), "",
                              store.GetIntent(oldThreadID), store.GetSexual(oldThreadID),
                              ThreadDataStore::FormatActorNames(oldPtrs),
                              ThreadDataStore::FormatActorNames(joinedActors)).dump();
}

// --- ostimnet_scene_change --------------------------------------------------

// skipTrigger: caller owns this decision — thread priority filter may set it
// to true for non-priority threads regardless of Config::IsMuted.
inline std::string BuildSceneChange(int threadID, const std::string& sceneID, bool skipTrigger) {
    auto& store = ThreadDataStore::GetSingleton();
    std::string sceneDesc = GetSceneDescription(static_cast<uint32_t>(threadID), sceneID);
    nlohmann::json j = BuildBaseEventJson("sex_change", sceneDesc, threadID, skipTrigger);
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
inline std::optional<std::string> BuildSpeedChange(int threadID, int32_t oldSpeed, int32_t newSpeed, bool skipTrigger) {
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

    nlohmann::json j = BuildBaseEventJson("sex_pace_change", msg, threadID, skipTrigger);
    j["speed"]     = newSpeed;
    j["direction"] = direction;
    return j.dump();
}

// --- ostimnet_climax --------------------------------------------------------

inline std::string BuildClimax(int threadID, const DebounceQueue::ClimaxBatchData& data) {
    ClimaxActorSnapshot snapshot;
    if (g_ostimThreadInterface)
        snapshot.count = g_ostimThreadInterface->GetActors(
            static_cast<uint32_t>(threadID), snapshot.buffer, ClimaxActorSnapshot::kMaxActors);

    auto actorResults = BuildClimaxActorData(data, snapshot);
    std::string humanMsg = FormatClimaxMessage(threadID, actorResults, snapshot);

    nlohmann::json j = BuildBaseEventJson("climax", humanMsg, threadID, Config::GetSingleton().IsMuted());

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

inline std::string BuildStop(int threadID) {
    return BuildBaseEventJson("sex_stop", "", threadID, Config::GetSingleton().IsMuted()).dump();
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
    nlohmann::json j = BuildBaseEventJson("spectator_added", msg, threadID, Config::GetSingleton().IsMuted());
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
    nlohmann::json j = BuildBaseEventJson("spectator_fled", msg, threadID, Config::GetSingleton().IsMuted());
    if (spectator) j["spectatorFormID"] = spectator->GetFormID();
    return j.dump();
}

// --- ostimnet_intent_changed ----------------------------------------------
// oldIntent: the intent string before the change.
// mainActors / secondaryActors: the final actor lists after the reshuffle.

inline std::string BuildIntentChanged(int threadID,
                                       const std::string& oldIntent,
                                       const std::string& newIntent,
                                       bool mainActorsSame) {
    auto& store = ThreadDataStore::GetSingleton();
    const std::string mainNames = store.GetFormattedMainActorNames(threadID);
    const std::string msg       = mainNames + "'s intent changed from " + oldIntent + " to " + newIntent + ".";
    nlohmann::json j = BuildBaseEventJson("intent_changed", msg, threadID, Config::GetSingleton().IsMuted());
    j["oldIntent"]      = oldIntent;
    j["mainActorsSame"] = mainActorsSame;
    return j.dump();
}

}  // namespace OStimNet::EventPayloadBuilder
