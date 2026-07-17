#include "PCH.h"

#include "SkyrimNetIntegration.h"
#include "api/OstimNG-API-Thread.h"
#include "api/OStimNavigator_PublicAPI.h"
#include "api/SkyrimNet_PublicAPI.h"
#include "ConfirmationModal.h"
#include "JsonService.h"
#include "OCumOverlayReader.h"
#include "OStimEventHelpers.h"
#include "OStimEventListener.h"
#include "ActorUtils.h"

#include <algorithm>
#include <map>

// Definition for the extern declared in api/OstimNG-API-Thread.h.
// All other files that include api/OstimNG-API-Thread.h reference this.
OstimNG_API::Thread::IThreadInterface* g_ostimThreadInterface = nullptr;

namespace OStimNet::SkyrimNetIntegration {

bool InitSkyrimNetAPI() {
    return FindFunctions();
}

int GetSkyrimNetAPIVersion() {
    return PublicGetVersion ? PublicGetVersion() : 0;
}

std::string GetPluginConfig(const char* plugin) {
    if (!PublicGetPluginConfig) return {};
    return PublicGetPluginConfig(plugin);
}

std::string GetPluginConfigValue(const char* plugin, const char* path, const char* defaultValue) {
    if (!PublicGetPluginConfigValue) return defaultValue;
    return PublicGetPluginConfigValue(plugin, path, defaultValue);
}

namespace {
// Wraps PublicRegisterDecorator + success/failure logging in one call.
void RegisterDecorator(const char* name, const char* description,
                       std::function<std::string(RE::Actor*)> callback) {
    bool ok = PublicRegisterDecorator(name, description, std::move(callback));
    if (ok)
        SKSE::log::info("SkyrimNetIntegration::Register: decorator '{}' registered successfully", name);
    else
        SKSE::log::error("SkyrimNetIntegration::Register: failed to register decorator '{}' (name conflict or null callback)", name);
}

// Returns true (and trace-logs) if actor is NOT in OStimActorCountFaction.
// Use as a pre-check for decorators that require an active ongoing encounter.
bool IsNotInOngoingEncounter(RE::Actor* actor, const char* decoratorName) {
    if (!ActorUtils::IsInFactionByEditorID(actor, "OStimActorCountFaction")) {
        SKSE::log::trace("{}: actor '{}' (0x{:08X}) -> unavailable (not in OStimActorCountFaction)",
            decoratorName, actor->GetName() ? actor->GetName() : "(unnamed)", actor->GetFormID());
        return true;
    }
    return false;
}

// Returns the SkyrimNet UUID for an actor pointer,
// using the actor's base FormID (which SkyrimNet tracks by base, not ref).
// Returns 0 if actor is null or PublicFormIDToUUID is null.
uint64_t ActorToUUID(RE::Actor* actor) {
    if (!actor || !PublicFormIDToUUID) {
        SKSE::log::info("ActorToUUID: null actor or PublicFormIDToUUID");
        return 0;
    }
    uint64_t uuid = PublicFormIDToUUID(actor->GetFormID());
    return uuid;
}

// Returns the SkyrimNet UUID for an actor identified by their reference FormID,
// using the actor's base FormID (which SkyrimNet tracks by base, not ref).
// Returns 0 if the actor cannot be found or PublicFormIDToUUID is null.
uint64_t ActorRefIDToUUID(RE::FormID refFormID) {
    if (!PublicFormIDToUUID) {
        SKSE::log::info("ActorRefIDToUUID: PublicFormIDToUUID is null, returning 0 for refFormID=0x{:08X}", refFormID);
        return 0;
    }
    auto* actor = RE::TESForm::LookupByID<RE::Actor>(refFormID);
    if (!actor) {
        SKSE::log::info("ActorRefIDToUUID: actor lookup failed for refFormID=0x{:08X}", refFormID);
        return 0;
    }
    return ActorToUUID(actor);
}



// Builds the JSON object for a single active scene (shared by ostimnet_active_scenes
// and ostimnet_actor_scene).
nlohmann::json BuildSceneJson(const OStimNet::ThreadDataStore::ActiveSceneInfo& scene) {
    nlohmann::json s;
    s["threadID"]    = scene.threadID;
    s["description"] = OStimNet::GetSceneDescription(static_cast<uint32_t>(scene.threadID));
    s["intent"]      = OStimNet::IntentToString(OStimNet::ThreadDataStore::GetSingleton().GetIntent(scene.threadID));
    auto sexual      = OStimNet::ThreadDataStore::GetSingleton().GetSexual(scene.threadID);
    s["isSexual"]    = sexual.value_or(false);

    auto actors = nlohmann::json::array();
    for (auto formID : scene.actorFormIDs) {
        uint64_t uuid = ActorRefIDToUUID(formID);
        if (uuid != 0) actors.push_back(uuid);
    }
    s["actors"] = std::move(actors);

    auto spectators = nlohmann::json::array();
    for (auto formID : OStimNet::ThreadDataStore::GetSingleton().GetSpectatorFormIDs(scene.threadID)) {
        uint64_t uuid = ActorRefIDToUUID(formID);
        if (uuid != 0) spectators.push_back(uuid);
    }
    s["spectators"] = std::move(spectators);

    auto mainActors = nlohmann::json::array();
    std::vector<std::string> mainActorNames;
    if (PublicFormIDToUUID) {
        auto& store = OStimNet::ThreadDataStore::GetSingleton();
        const auto& assignedMain = store.GetMainActors(scene.threadID);
        // Fall back to all actors when roles haven't been assigned yet
        // (e.g. LLM evaluation still in flight, or non-OStimNet thread).
        const auto& sourcePtrs = assignedMain.empty()
            ? store.GetActorPtrs(scene.threadID)
            : assignedMain;
        for (auto* actor : sourcePtrs) {
            if (!actor) continue;
            uint64_t uuid = ActorToUUID(actor);
            if (uuid != 0) {
                mainActors.push_back(uuid);
                mainActorNames.push_back(OStimNet::ThreadDataStore::GetActorDisplayName(actor, ""));
            }
        }
    }
    s["mainActors"]     = std::move(mainActors);
    s["mainActorNames"] = OStimNet::FormatActorList(mainActorNames);

    auto secondaryActors = nlohmann::json::array();
    std::vector<std::string> secondaryActorNames;
    if (PublicFormIDToUUID) {
        for (auto* actor : OStimNet::ThreadDataStore::GetSingleton().GetSecondaryActors(scene.threadID)) {
            if (!actor) continue;
            uint64_t uuid = ActorToUUID(actor);
            if (uuid != 0) {
                secondaryActors.push_back(uuid);
                secondaryActorNames.push_back(OStimNet::ThreadDataStore::GetActorDisplayName(actor, ""));
            }
        }
    }
    s["secondaryActors"]     = std::move(secondaryActors);
    s["secondaryActorNames"] = OStimNet::FormatActorList(secondaryActorNames);

    return s;
}

struct LLMRetryCallback : RE::IMessageBoxCallback {
    std::function<void()> retry;
    std::function<void()> ignore;
    LLMRetryCallback(std::function<void()> r, std::function<void()> i)
        : retry(std::move(r)), ignore(std::move(i)) {}
    void Run(Message a_msg) override {
        if (static_cast<int>(a_msg) == 0) retry();
        else                               ignore();
    }
};

void ShowLLMRetryModal(const std::string& evaluationType,
                       const std::string& description,
                       std::function<void()> onRetry,
                       std::function<void()> onIgnore)
{
    auto* taskIF = SKSE::GetTaskInterface();
    if (!taskIF) {
        SKSE::log::error("ShowLLMRetryModal: no task interface, falling back to ignore");
        onIgnore();
        return;
    }
    taskIF->AddTask([evaluationType,
                     description,
                     onRetry  = std::move(onRetry),
                     onIgnore = std::move(onIgnore)]() mutable {
        auto* factory = RE::MessageDataFactoryManager::GetSingleton();
        auto* ifStr   = RE::InterfaceStrings::GetSingleton();
        auto* creator = factory
            ? factory->GetCreator<RE::MessageBoxData>(ifStr->messageBoxData)
            : nullptr;
        if (!creator) {
            SKSE::log::error("ShowLLMRetryModal: null creator, ignoring");
            onIgnore(); return;
        }
        auto* msgData = creator->Create();
        if (!msgData) {
            SKSE::log::error("ShowLLMRetryModal: null msgData, ignoring");
            onIgnore(); return;
        }
        std::string body =
            "OStimNet: LLM request failed (" + evaluationType + ").\n" + description +
            "\n\nChoose Retry to try again, or Ignore to skip this evaluation.";
        msgData->bodyText       = body.c_str();
        msgData->buttonText.push_back(RE::BSString("Retry"));
        msgData->buttonText.push_back(RE::BSString("Ignore"));
        msgData->cancelOptionIndex = 1;
        msgData->callback =
            RE::make_smart<LLMRetryCallback>(std::move(onRetry), std::move(onIgnore));
        RE::MessageBoxMenu::QueueMessage(msgData);
    });
}
}  // namespace

void Register() {
    if (!PublicRegisterDecorator) {
        SKSE::log::warn("SkyrimNetIntegration::Register: PublicRegisterDecorator is null (SkyrimNet v5+ required), skipping decorator registration");
        return;
    }

    RegisterDecorator(
        "ocum_description",
        "One or more sentences describing all active OCum overlays on this actor across "
        "every body area (face, breasts, butt, anal, vagina, legs, hands, feet). "
        "Sentences are space-separated. Empty when no cum is applied.",
        [](RE::Actor* actor) -> std::string {
            return OStimNet::GetOCumDescription(actor);
        });

    RegisterDecorator(
        "ostimnet_active_scenes",
        "JSON object listing all currently active OStim scenes. "
        "Each entry contains a human-readable description, the SkyrimNet UUIDs of participating actors, "
        "spectating actors, main actors (primary participants), and secondary actors (supporting participants). "
        "Empty scenes array when no OStim thread is running.",
        [](RE::Actor*) -> std::string {
            nlohmann::json j;
            auto arr = nlohmann::json::array();
            for (const auto& scene : OStimNet::ThreadDataStore::GetSingleton().GetActiveScenes())
                arr.push_back(BuildSceneJson(scene));
            j["scenes"] = std::move(arr);
            return j.dump();
        });

    RegisterDecorator(
        "ostimnet_actor_thread_id",
        "Returns the OStim thread ID that this actor is currently participating in, or -1 if the actor is not in any active OStim scene.",
        [](RE::Actor* actor) -> std::string {
            if (!actor) return "-1";
            int threadID = OStimNet::ThreadDataStore::GetSingleton().GetActorThreadID(actor->GetFormID());
            SKSE::log::debug("ostimnet_actor_thread_id: actor '{}' (0x{:08X}) -> threadID={}",
                actor->GetName() ? actor->GetName() : "(unnamed)", actor->GetFormID(), threadID);
            return std::to_string(threadID);
        });

    RegisterDecorator(
        "ostimnet_spectator_thread_id",
        "Returns the OStim thread ID that this actor is currently spectating, or -1 if the actor is not a spectator of any active OStim scene.",
        [](RE::Actor* actor) -> std::string {
            if (!actor) return "-1";
            return std::to_string(OStimNet::ThreadDataStore::GetSingleton().GetSpectatorThreadID(actor->GetFormID()));
        });

    RegisterDecorator(
        "ostimnet_actor_thread_intent",
        "Returns the intent of the OStim scene that this actor is currently participating in "
        "(e.g. 'romantic', 'transactional', 'dom', 'aggressive'), or an empty string if the actor is not in any active OStim scene.",
        [](RE::Actor* actor) -> std::string {
            if (!actor) return "";
            int threadID = OStimNet::ThreadDataStore::GetSingleton().GetActorThreadID(actor->GetFormID());
            if (threadID == -1) return "";
            return OStimNet::IntentToString(OStimNet::ThreadDataStore::GetSingleton().GetIntent(threadID));
        });

    RegisterDecorator(
        "ostimnet_actor_is_sexual",
        "Returns 'true' if the OStim scene that this actor is currently participating in is sexual, "
        "'false' if it is not sexual, or an empty string if the actor is not in any active OStim scene.",
        [](RE::Actor* actor) -> std::string {
            if (!actor) return "";
            int threadID = OStimNet::ThreadDataStore::GetSingleton().GetActorThreadID(actor->GetFormID());
            if (threadID == -1) {
                SKSE::log::debug("ostimnet_actor_is_sexual: actor '{}' (0x{:08X}) is not in any active thread",
                    actor->GetName() ? actor->GetName() : "(unnamed)", actor->GetFormID());
                return "";
            }
            auto sexual = OStimNet::ThreadDataStore::GetSingleton().GetSexual(threadID);
            bool result = sexual.value_or(false);
            SKSE::log::debug("ostimnet_actor_is_sexual: actor '{}' (0x{:08X}) threadID={} isSexual={}",
                actor->GetName() ? actor->GetName() : "(unnamed)", actor->GetFormID(), threadID, result);
            return result ? "true" : "false";
        });

    RegisterDecorator(
        "ostimnet_actor_scene",
        "JSON object describing the single OStim scene that this actor is currently participating in. "
        "Contains the same fields as entries in ostimnet_active_scenes (threadID, description, intent, isSexual, "
        "actors, spectators, mainActors, mainActorNames, secondaryActors, secondaryActorNames). "
        "Returns '{\"scene\":null}' if the actor is not in any active OStim scene.",
        [](RE::Actor* actor) -> std::string {
            if (!actor) {
                SKSE::log::warn("ostimnet_actor_scene: null actor");
                return "{\"scene\":null}";
            }
            const char* actorName = actor->GetName();
            int threadID = OStimNet::ThreadDataStore::GetSingleton().GetActorThreadID(actor->GetFormID());
            if (threadID == -1) {
                SKSE::log::debug("ostimnet_actor_scene: actor '{}' (0x{:08X}) is not in any active thread",
                    actorName ? actorName : "(unnamed)", actor->GetFormID());
                return "{\"scene\":null}";
            }

            const auto scenes = OStimNet::ThreadDataStore::GetSingleton().GetActiveScenes();
            auto it = std::find_if(scenes.begin(), scenes.end(),
                                   [threadID](const auto& sc) { return sc.threadID == threadID; });
            if (it == scenes.end()) {
                SKSE::log::warn("ostimnet_actor_scene: actor '{}' (0x{:08X}) maps to threadID={} but no matching scene found in {} active scene(s)",
                    actorName ? actorName : "(unnamed)", actor->GetFormID(), threadID, scenes.size());
                return "{\"scene\":null}";
            }

            SKSE::log::debug("ostimnet_actor_scene: building scene JSON for actor '{}' (0x{:08X}) in threadID={}",
                actorName ? actorName : "(unnamed)", actor->GetFormID(), threadID);
            nlohmann::json j;
            j["scene"] = BuildSceneJson(*it);
            return j.dump();
        });

    RegisterDecorator(
        "get_nearby_actors_in_ostim",
        "JSON object listing all nearby actors (within the configured nearbyActors.radius) that are currently "
        "participating in an OStim scene (in OStimActorCountFaction). "
        "Fields: actorIds (array of SkyrimNet UUIDs), actorsNameString (formatted name list like 'A, B and C'). "
        "actorIds is empty when no qualifying actors are found.",
        [](RE::Actor* actor) -> std::string {
            nlohmann::json j;
            j["actorIds"]        = nlohmann::json::array();
            j["actorsNameString"] = "";
            if (!actor || !PublicFormIDToUUID) return j.dump();

            const float radius = OStimNet::Config::GetSingleton().NearbyActorsRadius();
            const auto nearby = OStimNet::ActorUtils::GetNearbyActors(
                actor, radius, OStimNet::ActorUtils::OStimCondition::OnlyInOStim);

            std::vector<std::string> names;
            for (RE::Actor* a : nearby) {
                uint64_t uuid = ActorToUUID(a);
                if (uuid != 0) {
                    j["actorIds"].push_back(uuid);
                    if (const char* n = a->GetName(); n && n[0])
                        names.push_back(n);
                }
            }
            j["actorsNameString"] = OStimNet::FormatActorList(names);
            return j.dump();
        });

    RegisterDecorator(
        "get_nearby_actors_not_in_ostim",
        "JSON object listing all nearby actors (within the configured nearbyActors.radius) that are NOT currently "
        "participating in an OStim scene (not in OStimActorCountFaction). "
        "Fields: actorIds (array of SkyrimNet UUIDs), actorsNameString (formatted name list like 'A, B and C'). "
        "actorIds is empty when no qualifying actors are found.",
        [](RE::Actor* actor) -> std::string {
            nlohmann::json j;
            j["actorIds"]        = nlohmann::json::array();
            j["actorsNameString"] = "";
            if (!actor || !PublicFormIDToUUID) return j.dump();

            const float radius = OStimNet::Config::GetSingleton().NearbyActorsRadius();
            const auto nearby = OStimNet::ActorUtils::GetNearbyActors(
                actor, radius, OStimNet::ActorUtils::OStimCondition::OnlyNotInOStim);

            std::vector<std::string> names;
            for (RE::Actor* a : nearby) {
                uint64_t uuid = ActorToUUID(a);
                if (uuid != 0) {
                    j["actorIds"].push_back(uuid);
                    if (const char* n = a->GetName(); n && n[0])
                        names.push_back(n);
                }
            }
            j["actorsNameString"] = OStimNet::FormatActorList(names);
            return j.dump();
        });

    RegisterDecorator(
        "ostimnet_available_positions",
        "Newline-separated list of all sex positions that exist in at least one loaded OStim scene, "
        "with human-readable descriptions. Each entry is prefixed with '- '. "
        "Requires OStimNavigator. Empty when OStimNavigator is not installed.",
        [](RE::Actor*) -> std::string {
            if (!ONavGetAllPositions) return "";
            const char* raw = ONavGetAllPositions();
            if (!raw || raw[0] == '\0') return "";
            try {
                auto j = nlohmann::json::parse(raw);
                if (!j.is_array()) return "";
                std::string result;
                for (const auto& item : j) {
                    if (!item.is_string()) continue;
                    const std::string& id = item.get_ref<const std::string&>();
                    const auto& displayNames = OStimNavigatorAPI::GetPositionDisplayNameMap();
                    auto it = displayNames.find(id);
                    std::string label = id;
                    if (it != displayNames.end() && !it->second.empty())
                        label += it->second;
                    if (!result.empty()) result += ',';
                    result += label;
                }
                return result;
            } catch (...) {
                return "";
            }
        });

    RegisterDecorator(
        "ostimnet_available_sexual_actions",
        "Comma-separated list of all sexual act type keywords that exist in at least one loaded OStim scene. "
        "Requires OStimNavigator. Empty when OStimNavigator is not installed.",
        [](RE::Actor*) -> std::string {
            if (!ONavGetAllActions) return "";
            const char* raw = ONavGetAllActions("sexual");
            if (!raw || raw[0] == '\0') return "";
            try {
                auto j = nlohmann::json::parse(raw);
                if (!j.is_array()) return "";
                auto startsWithAny = [](const std::string& s, std::initializer_list<const char*> prefixes) {
                    for (const char* p : prefixes)
                        if (s.rfind(p, 0) == 0) return true;
                    return false;
                };
                std::string result;
                for (const auto& item : j) {
                    if (!item.is_string()) continue;
                    const std::string& id = item.get_ref<const std::string&>();
                    if (startsWithAny(id, {"cumon", "3pp_", "ejaculation_on", "facial"})) continue;
                    if (!result.empty()) result += ',';
                    result += id;
                }
                return result;
            } catch (...) {
                return "";
            }
        });

    RegisterDecorator(
        "is_start_care_scene_available",
        "Returns 'available' if all of the following are true: the OStim/SkyrimNet sex integration global "
        "(skyrimnet_sexlab_ostim_player) is set to 1, the actor is not a child, the actor is not in combat, "
        "and the StartCareScene action is not on cooldown for this actor. Returns 'unavailable' otherwise.",
        [](RE::Actor* actor) -> std::string {
            SKSE::log::debug("is_start_care_scene_available: checking availability.");
            if (!actor) return "unavailable";
            const char* actorName = actor->GetName();
            // Faction checks.
            if (ActorUtils::IsBlockedForNewEncounter(actor, "is_start_care_scene_available")) return "unavailable";
            // Check global flag.
            auto* global = RE::TESForm::LookupByEditorID<RE::TESGlobal>("skyrimnet_sexlab_ostim_player");
            if (global && static_cast<int>(global->value) != 1) {
                SKSE::log::debug("is_start_care_scene_available: actor '{}' (0x{:08X}) -> unavailable (global flag not set, value={})",
                    actorName ? actorName : "(unnamed)", actor->GetFormID(), static_cast<int>(global->value));
                return "unavailable";
            }
            // Child check.
            if (actor->IsChild()) {
                SKSE::log::debug("is_start_care_scene_available: actor '{}' (0x{:08X}) -> unavailable (is child)",
                    actorName ? actorName : "(unnamed)", actor->GetFormID());
                return "unavailable";
            }
            // Combat check.
            if (actor->IsInCombat()) {
                SKSE::log::debug("is_start_care_scene_available: actor '{}' (0x{:08X}) -> unavailable (in combat)",
                    actorName ? actorName : "(unnamed)", actor->GetFormID());
                return "unavailable";
            }
            // Cooldown check.
            bool onCooldown = OStimNet::ConfirmationModal::GetSingleton().IsOnCooldown("StartCareScene", actor);
            SKSE::log::debug("is_start_care_scene_available: actor '{}' (0x{:08X}) -> {} (cooldown={})",
                actorName ? actorName : "(unnamed)", actor->GetFormID(), onCooldown ? "unavailable" : "available", onCooldown);
            return onCooldown ? "unavailable" : "available";
        });

    RegisterDecorator(
        "is_start_new_sex_available",
        "Returns 'available' if all of the following are true: the OStim/SkyrimNet sex integration global "
        "(skyrimnet_sexlab_ostim_player) is set to 1, the actor is not in SexLabAnimatingFaction, "
        "OStimActorCountFaction, or TTON_OStimPending, the actor is not a child, the actor is not in combat, "
        "and the StartNewSex action is not on cooldown for this actor. Returns 'unavailable' otherwise.",
        [](RE::Actor* actor) -> std::string {
            SKSE::log::debug("is_start_new_sex_available: checking availability.");
            if (!actor) return "unavailable";
            const char* actorName = actor->GetName();
            // Faction checks.
            if (ActorUtils::IsBlockedForNewEncounter(actor, "is_start_new_sex_available")) return "unavailable";
            // Check global flag.
            auto* global = RE::TESForm::LookupByEditorID<RE::TESGlobal>("skyrimnet_sexlab_ostim_player");
            if (global && static_cast<int>(global->value) != 1) {
                SKSE::log::debug("is_start_new_sex_available: actor '{}' (0x{:08X}) -> unavailable (global flag not set, value={})",
                    actorName ? actorName : "(unnamed)", actor->GetFormID(), static_cast<int>(global->value));
                return "unavailable";
            }
            // Child check.
            if (actor->IsChild()) {
                SKSE::log::debug("is_start_new_sex_available: actor '{}' (0x{:08X}) -> unavailable (is child)",
                    actorName ? actorName : "(unnamed)", actor->GetFormID());
                return "unavailable";
            }
            // Combat check.
            if (actor->IsInCombat()) {
                SKSE::log::debug("is_start_new_sex_available: actor '{}' (0x{:08X}) -> unavailable (in combat)",
                    actorName ? actorName : "(unnamed)", actor->GetFormID());
                return "unavailable";
            }
            // Cooldown check.
            bool onCooldown = OStimNet::ConfirmationModal::GetSingleton().IsOnCooldown("StartNewSex", actor);
            SKSE::log::debug("is_start_new_sex_available: actor '{}' (0x{:08X}) -> {} (cooldown={})",
                actorName ? actorName : "(unnamed)", actor->GetFormID(), onCooldown ? "unavailable" : "available", onCooldown);
            return onCooldown ? "unavailable" : "available";
        });

    RegisterDecorator(
        "is_spectate_sex_available",
        "Returns 'available' if all of the following are true: the TTON_IsOstimActive global is set to 1, "
        "the actor is not in OStimActorCountFaction, the actor is not in TTON_SpectatorFaction, "
        "the actor is not a child, the actor is not in combat, "
        "and the SpectatorOfSex action is not on cooldown for this actor. Returns 'unavailable' otherwise.",
        [](RE::Actor* actor) -> std::string {
            SKSE::log::debug("is_spectate_sex_available: checking availability.");
            if (!actor) return "unavailable";
            const char* actorName = actor->GetName();

            // Faction checks.
            if (ActorUtils::IsInFactionByEditorID(actor, "OStimActorCountFaction")) {
                SKSE::log::debug("is_spectate_sex_available: actor '{}' (0x{:08X}) -> unavailable (in OStimActorCountFaction)",
                    actorName ? actorName : "(unnamed)", actor->GetFormID());
                return "unavailable";
            }
            if (ActorUtils::IsInFactionByEditorID(actor, "TTON_SpectatorFaction")) {
                SKSE::log::debug("is_spectate_sex_available: actor '{}' (0x{:08X}) -> unavailable (in TTON_SpectatorFaction)",
                    actorName ? actorName : "(unnamed)", actor->GetFormID());
                return "unavailable";
            }

            // Active thread check.
            if (OStimNet::ThreadDataStore::GetSingleton().GetActiveScenes().empty()) {
                SKSE::log::debug("is_spectate_sex_available: actor '{}' (0x{:08X}) -> unavailable (no active scenes)",
                    actorName ? actorName : "(unnamed)", actor->GetFormID());
                return "unavailable";
            }

            // Child check.
            if (actor->IsChild()) {
                SKSE::log::debug("is_spectate_sex_available: actor '{}' (0x{:08X}) -> unavailable (is child)",
                    actorName ? actorName : "(unnamed)", actor->GetFormID());
                return "unavailable";
            }

            // Combat check.
            if (actor->IsInCombat()) {
                SKSE::log::debug("is_spectate_sex_available: actor '{}' (0x{:08X}) -> unavailable (in combat)",
                    actorName ? actorName : "(unnamed)", actor->GetFormID());
                return "unavailable";
            }

            return "available";
        });

    RegisterDecorator(
        "is_spectate_sex_flee_available",
        "Returns 'available' if the actor is in TTON_SpectatorFaction and not in TTON_SpectatorFleeFaction. Returns 'unavailable' otherwise.",
        [](RE::Actor* actor) -> std::string {
            SKSE::log::debug("is_spectate_sex_flee_available: checking availability.");
            if (!actor) return "unavailable";
            const char* actorName = actor->GetName();

            if (!ActorUtils::IsInFactionByEditorID(actor, "TTON_SpectatorFaction")) {
                SKSE::log::debug("is_spectate_sex_flee_available: actor '{}' (0x{:08X}) -> unavailable (not in TTON_SpectatorFaction)",
                    actorName ? actorName : "(unnamed)", actor->GetFormID());
                return "unavailable";
            }

            if (ActorUtils::IsInFactionByEditorID(actor, "TTON_SpectatorFleeFaction")) {
                SKSE::log::debug("is_spectate_sex_flee_available: actor '{}' (0x{:08X}) -> unavailable (in TTON_SpectatorFleeFaction)",
                    actorName ? actorName : "(unnamed)", actor->GetFormID());
                return "unavailable";
            }

            return "available";
        });
    RegisterDecorator(
        "is_join_ongoing_sex_available",
        "Returns 'available' if all of the following are true: the OStim/SkyrimNet sex integration global "
        "(skyrimnet_sexlab_ostim_player) is set to 1, the actor is not a child, the actor is not in combat, "
        "at least one OStim thread is currently running, "
        "and the JoinOngoingSex action is not on cooldown for this actor. Returns 'unavailable' otherwise.",
        [](RE::Actor* actor) -> std::string {
            if (!actor) return "unavailable";
            // Faction checks.
            if (ActorUtils::IsBlockedForNewEncounter(actor, "is_join_ongoing_sex_available")) return "unavailable";
            // Check global flag.
            auto* global = RE::TESForm::LookupByEditorID<RE::TESGlobal>("skyrimnet_sexlab_ostim_player");
            if (global && static_cast<int>(global->value) != 1) return "unavailable";
            // Child check.
            if (actor->IsChild()) return "unavailable";
            // Combat check.
            if (actor->IsInCombat()) return "unavailable";
            // Active thread check.
            if (OStimNet::ThreadDataStore::GetSingleton().GetActiveScenes().empty()) return "unavailable";
            // Cooldown check.
            return OStimNet::ConfirmationModal::GetSingleton().IsOnCooldown("JoinOngoingSex", actor)
                ? "unavailable" : "available";
        });

    RegisterDecorator(
        "is_sex_scene_position_change_available",
        "Returns 'available' if this actor is currently in a sexual OStim scene (OStimActorCountFaction) and the ChangeSexScenePosition action is not on cooldown for them. "
        "Returns 'unavailable' otherwise (actor not in OStimActorCountFaction, not in a scene, scene is not sexual, or action is on cooldown).",
        [](RE::Actor* actor) -> std::string {
            SKSE::log::debug("is_sex_scene_position_change_available: checking availability.");
            if (!actor) return "unavailable";
            const char* actorName = actor->GetName();
            if (IsNotInOngoingEncounter(actor, "is_sex_scene_position_change_available")) return "unavailable";
            int threadID = OStimNet::ThreadDataStore::GetSingleton().GetActorThreadID(actor->GetFormID());
            if (threadID == -1) {
                SKSE::log::debug("is_sex_scene_position_change_available: actor '{}' (0x{:08X}) -> unavailable (not in a scene)",
                    actorName ? actorName : "(unnamed)", actor->GetFormID());
                return "unavailable";
            }
            auto sexual = OStimNet::ThreadDataStore::GetSingleton().GetSexual(threadID);
            if (!sexual.value_or(false)) {
                SKSE::log::debug("is_sex_scene_position_change_available: actor '{}' (0x{:08X}) threadID={} -> unavailable (scene is not sexual)",
                    actorName ? actorName : "(unnamed)", actor->GetFormID(), threadID);
                return "unavailable";
            }
            bool onCooldown = OStimNet::ConfirmationModal::GetSingleton().IsOnCooldown("ChangeSexScenePosition", actor);
            SKSE::log::debug("is_sex_scene_position_change_available: actor '{}' (0x{:08X}) threadID={} -> {} (cooldown={})",
                actorName ? actorName : "(unnamed)", actor->GetFormID(), threadID, onCooldown ? "unavailable" : "available", onCooldown);
            return onCooldown ? "unavailable" : "available";
        });

    RegisterDecorator(
        "is_sex_scene_intent_change_available",
        "Returns 'available' if this actor is currently in a sexual OStim scene and the ChangeSexSceneIntent action is not on cooldown for them. "
        "Returns 'unavailable' otherwise (actor not in a scene, scene is not sexual, or action is on cooldown).",
        [](RE::Actor* actor) -> std::string {
            if (!actor) return "unavailable";
            if (IsNotInOngoingEncounter(actor, "is_sex_scene_intent_change_available")) return "unavailable";
            int threadID = OStimNet::ThreadDataStore::GetSingleton().GetActorThreadID(actor->GetFormID());
            if (threadID == -1) return "unavailable";
            auto sexual = OStimNet::ThreadDataStore::GetSingleton().GetSexual(threadID);
            if (!sexual.value_or(false)) return "unavailable";
            return OStimNet::ConfirmationModal::GetSingleton().IsOnCooldown("ChangeSexSceneIntent", actor)
                ? "unavailable" : "available";
        });

    RegisterDecorator(
        "is_sex_scene_pace_change_available",
        "Returns 'available' if this actor is currently in a sexual OStim scene, the scene has multiple animation speeds, "
        "and the ChangeSexPace action is not on cooldown for them. "
        "Returns 'unavailable' otherwise (actor not in a scene, scene is not sexual, only one speed available, or action is on cooldown).",
        [](RE::Actor* actor) -> std::string {
            if (!actor) return "unavailable";
            if (IsNotInOngoingEncounter(actor, "is_sex_scene_pace_change_available")) return "unavailable";
            int threadID = OStimNet::ThreadDataStore::GetSingleton().GetActorThreadID(actor->GetFormID());
            if (threadID == -1) return "unavailable";
            auto sexual = OStimNet::ThreadDataStore::GetSingleton().GetSexual(threadID);
            if (!sexual.value_or(false)) return "unavailable";
            if (!g_ostimThreadInterface) return "unavailable";
            int32_t maxSpeed = g_ostimThreadInterface->GetMaxSpeed(static_cast<uint32_t>(threadID));
            if (maxSpeed <= 0) return "unavailable";
            return OStimNet::ConfirmationModal::GetSingleton().IsOnCooldown("ChangeSexPace", actor)
                ? "unavailable" : "available";
        });

    RegisterDecorator(
        "ostimnet_sex_pace_direction",
        "Returns the available pace-change direction(s) for the OStim scene this actor is currently in. "
        "'increase' when at minimum speed (can only go faster), "
        "'decrease' when at maximum speed (can only go slower), "
        "'both' when somewhere in between. "
        "Returns an empty string if the actor is not in a sexual scene or the scene has only one speed.",
        [](RE::Actor* actor) -> std::string {
            if (!actor) return "";
            int threadID = OStimNet::ThreadDataStore::GetSingleton().GetActorThreadID(actor->GetFormID());
            if (threadID == -1) return "";
            auto sexual = OStimNet::ThreadDataStore::GetSingleton().GetSexual(threadID);
            if (!sexual.value_or(false)) return "";
            if (!g_ostimThreadInterface) return "";
            int32_t maxSpeed     = g_ostimThreadInterface->GetMaxSpeed(static_cast<uint32_t>(threadID));
            if (maxSpeed <= 0) return "";
            int32_t currentSpeed = g_ostimThreadInterface->GetCurrentSpeed(static_cast<uint32_t>(threadID));
            if (currentSpeed <= 0)  return "increase";
            if (currentSpeed >= maxSpeed) return "decrease";
            return "both";
        });

    RegisterDecorator(
        "is_invite_to_your_sex_available",
        "Returns 'available' if all of the following are true: this actor is currently in a sexual OStim scene, "
        "the scene has fewer than 5 participants, and the InviteToYourSex action is not on cooldown for them. "
        "Returns 'unavailable' otherwise (actor not in a scene, scene is not sexual, scene is full, or action is on cooldown).",
        [](RE::Actor* actor) -> std::string {
            if (!actor) return "unavailable";
            if (IsNotInOngoingEncounter(actor, "is_invite_to_your_sex_available")) return "unavailable";
            int threadID = OStimNet::ThreadDataStore::GetSingleton().GetActorThreadID(actor->GetFormID());
            if (threadID == -1) return "unavailable";
            auto sexual = OStimNet::ThreadDataStore::GetSingleton().GetSexual(threadID);
            if (!sexual.value_or(false)) return "unavailable";
            if (OStimNet::ThreadDataStore::GetSingleton().GetActorPtrs(threadID).size() >= 5) return "unavailable";
            return OStimNet::ConfirmationModal::GetSingleton().IsOnCooldown("InviteToYourSex", actor)
                ? "unavailable" : "available";
        });

    RegisterDecorator(
        "is_stop_sex_available",
        "Returns 'available' if this actor is currently in a sexual OStim scene and the StopSex action is not on cooldown for them. "
        "Returns 'unavailable' otherwise (actor not in a scene, scene is not sexual, or action is on cooldown).",
        [](RE::Actor* actor) -> std::string {
            if (!actor) return "unavailable";
            if (IsNotInOngoingEncounter(actor, "is_stop_sex_available")) return "unavailable";
            int threadID = OStimNet::ThreadDataStore::GetSingleton().GetActorThreadID(actor->GetFormID());
            if (threadID == -1) return "unavailable";
            auto sexual = OStimNet::ThreadDataStore::GetSingleton().GetSexual(threadID);
            if (!sexual.value_or(false)) return "unavailable";
            return OStimNet::ConfirmationModal::GetSingleton().IsOnCooldown("StopSex", actor)
                ? "unavailable" : "available";
        });

    RegisterDecorator(
        "ostimnet_settings",
        "JSON object containing current OStimNet settings relevant to scene behaviour. "
        "Fields: enableAggressiveIntent (bool).",
        [](RE::Actor*) -> std::string {
            nlohmann::json j;
            j["enableAggressiveIntent"] = OStimNet::Config::GetSingleton().EnableAggressiveIntent();
            return j.dump();
        });

    RegisterDecorator(
        "ostimnet_available_furniture_types",
        "Comma-separated list of furniture types that have at least 5 associated sexual scenes "
        "nearby (within 3200 units of the actor), with their distance in Skyrim units appended "
        "(e.g. 'bed:250,chair:180'). Requires OStimNavigator. Empty when OStimNavigator is not installed "
        "or no furniture type meets the threshold.",
        [](RE::Actor* actor) -> std::string {
            if (!ONavGetFurnitureTypesWithSexScenes) return "";
            uint32_t centerRefID = actor ? actor->GetFormID() : 0;
            const char* raw = ONavGetFurnitureTypesWithSexScenes(5, centerRefID, 3200.0f);
            if (!raw || raw[0] == '\0') return "";
            try {
                auto j = nlohmann::json::parse(raw);
                if (!j.is_array()) return "";
                std::string result;
                for (const auto& item : j) {
                    if (!item.is_object()) continue;
                    auto idIt = item.find("id");
                    if (idIt == item.end() || !idIt->is_string()) continue;
                    if (!result.empty()) result += ",";
                    result += idIt->get<std::string>();
                    auto distIt = item.find("distance");
                    if (distIt != item.end() && distIt->is_number()) {
                        float dist = distIt->get<float>();
                        if (dist >= 0.0f) {
                            float meters = dist * (1.8288f / 128.0f);
                            char buf[32];
                            std::snprintf(buf, sizeof(buf), "%.1f", meters);
                            result += "(distance " + std::string(buf) + "m)";
                        }
                    }
                }
                return result;
            } catch (...) {
                return "";
            }
        });

    RegisterDecorator(
        "ostimnet_thread_phase",
        "Returns a JSON object describing the current sexual encounter phase for the OStim scene "
        "this actor is participating in. Fields: currentPhase (string), nextPhase (string or null), "
        "currentPhaseActions (comma-separated OStim activity keywords), nextPhaseActions (same, or empty). "
        "Returns an empty string if the actor is not in a scene, phases are disabled globally, "
        "or the scene intent has no phase list (e.g. dom or aggressive).",
        [](RE::Actor* actor) -> std::string {
            if (!actor) return "";
            if (!OStimNet::Config::GetSingleton().EnableThreadPhases()) return "";
            int threadID = OStimNet::ThreadDataStore::GetSingleton().GetActorThreadID(actor->GetFormID());
            if (threadID == -1) return "";
            auto phase = OStimNet::ThreadDataStore::GetSingleton().GetSexualPhase(threadID);
            if (!phase.has_value()) return "";

            OStimNet::Intent intent = OStimNet::ThreadDataStore::GetSingleton().GetIntent(threadID);
            auto nextPhase = OStimNet::GetNextPhase(intent, *phase);

            nlohmann::json j;
            j["currentPhase"]        = OStimNet::SexualPhaseToName(*phase);
            j["nextPhase"]           = nextPhase.has_value()
                                           ? nlohmann::json(OStimNet::SexualPhaseToName(*nextPhase))
                                           : nlohmann::json(nullptr);
            j["currentPhaseActions"] = OStimNet::SexualPhaseToActivityKeywordString(*phase);
            j["nextPhaseActions"]    = nextPhase.has_value()
                                           ? OStimNet::SexualPhaseToActivityKeywordString(*nextPhase)
                                           : "";
            return j.dump();
        });
}

// SanitizeLLMJson is defined in JsonService.h and shared across all translation units.

bool EvaluatePreStartSexualScene(const std::vector<RE::FormID>& participantFormIDs, const std::string& intent) {
    if (!PublicSendCustomPromptToLLM) {
        SKSE::log::warn("EvaluatePreStartSexualScene: PublicSendCustomPromptToLLM unavailable (SkyrimNet v8+ required).");
        return false;
    }

    // Build name→FormID map and actors name list from the participant list.
    // Captured by value into the callback so name resolution works without
    // walking all loaded forms.
    std::map<std::string, RE::FormID> nameToFormID;
    nlohmann::json actorUUIDs = nlohmann::json::array();
    std::vector<std::string> actorNameVec;

    for (RE::FormID fid : participantFormIDs) {
        std::string displayName;
        if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(fid))
            displayName = OStimNet::ThreadDataStore::GetActorDisplayName(actor, "");
        if (!displayName.empty()) {
            nameToFormID[displayName] = fid;
            actorNameVec.push_back(displayName);
            uint64_t uuid = ActorRefIDToUUID(fid);
            if (uuid != 0) actorUUIDs.push_back(uuid);
        }
    }

    // Build context JSON for the prompt template.
    nlohmann::json ctx;
    ctx["actors"]            = actorUUIDs;
    ctx["actorNames"]        = OStimNet::FormatActorList(actorNameVec);
    ctx["participant_count"] = static_cast<int>(participantFormIDs.size());
    ctx["intent"]            = intent;

    // Also inject flat slots in case the prompt template needs them.
    for (int i = 0; i < 5; ++i) {
        std::string keyId   = "participant" + std::to_string(i + 1) + "_formid";
        std::string keyName = "participant" + std::to_string(i + 1) + "_name";
        if (i < static_cast<int>(participantFormIDs.size())) {
            RE::FormID fid = participantFormIDs[i];
            ctx[keyId] = static_cast<uint32_t>(fid);
            auto it = std::find_if(nameToFormID.begin(), nameToFormID.end(),
                                   [fid](const auto& p) { return p.second == fid; });
            ctx[keyName] = (it != nameToFormID.end()) ? it->first : "";
        } else {
            ctx[keyId]   = 0;
            ctx[keyName] = "";
        }
    }

    // Snapshot originalParticipants as uint32 array for the result JSON.
    nlohmann::json originalParticipants = nlohmann::json::array();
    for (RE::FormID fid : participantFormIDs)
        originalParticipants.push_back(static_cast<uint32_t>(fid));

    std::string contextJson = ctx.dump();
    SKSE::log::info("EvaluatePreStartSexualScene: queuing LLM prompt, participants={}",
                    participantFormIDs.size());

    std::string promptName = "ostimnet_evaluations/ostimnet_evaluate_prestart_sexual";
    std::string llmVariant = OStimNet::Config::GetSingleton().GmLlmVariant();

    auto callbackHolder = std::make_shared<std::function<void(const char*, int)>>();

    *callbackHolder = [callbackHolder, nameToFormID, originalParticipants, intent,
                       contextJson, promptName, llmVariant](const char* response, int success) {
        SKSE::log::info("EvaluatePreStartSexualScene callback: success={}", success);

        auto retryAction = [callbackHolder, promptName, llmVariant, contextJson]() {
            SKSE::log::info("EvaluatePreStartSexualScene: retrying LLM request.");
            if (PublicSendCustomPromptToLLM)
                PublicSendCustomPromptToLLM(
                    promptName.c_str(), llmVariant.c_str(),
                    contextJson.c_str(), *callbackHolder);
        };
        auto ignoreAction = [originalParticipants]() {
            SKSE::log::info("EvaluatePreStartSexualScene: player chose to ignore failed evaluation.");
            nlohmann::json ignoreResult;
            ignoreResult["originalParticipants"] = originalParticipants;
            OStimNet::FireModEvent("ostimnet_sexual_evaluation_finished", ignoreResult.dump().c_str(), 0.0f);
        };

        if (!success || !response || response[0] == '\0') {
            SKSE::log::warn("EvaluatePreStartSexualScene: LLM returned failure or empty response.");
            ShowLLMRetryModal(
                "Pre-Start Sexual Scene",
                "The LLM evaluation failed (HTTP or connection error).",
                retryAction, ignoreAction);
            return;
        }

        nlohmann::json resp = nlohmann::json::parse(response, nullptr, /*exceptions=*/false);
        if (resp.is_discarded()) {
            // LLM may have wrapped the JSON in markdown fences or added prose — try to salvage it.
            SKSE::log::warn("EvaluatePreStartSexualScene: raw response was not valid JSON, attempting sanitization.");
            std::string sanitized = JsonService::SanitizeLLMJson(response);
            if (!sanitized.empty())
                resp = nlohmann::json::parse(sanitized, nullptr, /*exceptions=*/false);
        }
        if (resp.is_discarded()) {
            SKSE::log::warn("EvaluatePreStartSexualScene: LLM response was not valid JSON (even after sanitization). Raw: {}",
                            response ? response : "(null)");
            ShowLLMRetryModal(
                "Pre-Start Sexual Scene",
                "The LLM returned an unreadable response.",
                retryAction, ignoreAction);
            return;
        }

        auto resolveNames = [&](const nlohmann::json& src, const char* key) {
            return JsonService::ResolveNamesToFormIDs(src, key, nameToFormID, "EvaluatePreStartSexualScene");
        };

        // Prompt returns {"reason":..., "scene": {"intent":..., "mainActors":[...], "secondaryActors":[...], ...}}
        // "scene" is null when the LLM determines no viable role assignments are possible.
        if (!resp.contains("scene") || resp["scene"].is_null()) {
            SKSE::log::info("EvaluatePreStartSexualScene: LLM returned scene=null (not enough willing participants).");
            nlohmann::json nullResult;
            nullResult["originalParticipants"] = originalParticipants;
            OStimNet::FireModEvent("ostimnet_sexual_evaluation_finished", nullResult.dump().c_str(), 2.0f);
            return;
        }
        const nlohmann::json& scene = resp["scene"];

        // Convert scene.activities array → comma-separated string for Papyrus.
        std::string resolvedActivities;
        if (scene.contains("activities") && scene["activities"].is_array()) {
            for (const auto& a : scene["activities"]) {
                if (!a.is_string()) continue;
                if (!resolvedActivities.empty()) resolvedActivities += ',';
                resolvedActivities += a.get<std::string>();
            }
        }

        nlohmann::json result;
        result["originalParticipants"] = originalParticipants;
        result["intent"]               = intent;
        result["sexualPosition"]       = scene.value("sexualPosition", "");
        result["sexualActivities"]     = scene.value("sexualActivity", "");
        result["main"]                 = resolveNames(scene, "mainActors");
        result["secondary"]            = resolveNames(scene, "secondaryActors");

        // Guardrail: LLMs sometimes place all actors in mainActors and leave secondaryActors empty.
        // If secondary is empty and main has more than one actor, move the last main actor to secondary.
        if (result["secondary"].empty() && result["main"].size() > 1) {
            SKSE::log::info("EvaluatePreStartSexualScene: secondary actors empty with {} main actors — moving last main actor to secondary.",
                            result["main"].size());
            result["secondary"].push_back(result["main"].back());
            result["main"].erase(result["main"].size() - 1);
        }

        // Count total resolved actors and detect if the LLM excluded any.
        // resolveNames only maps names from the original participants, so the final
        // set is always a subset — a count difference is sufficient.
        std::size_t totalActors = result["main"].size() + result["secondary"].size();
        bool actorsExcluded = (totalActors < originalParticipants.size());

        // Build excluded FormID array: originalParticipants minus (main ∪ secondary).
        nlohmann::json excluded = nlohmann::json::array();
        if (actorsExcluded) {
            for (const auto& origFID : originalParticipants) {
                uint32_t fidVal = origFID.get<uint32_t>();
                bool found = false;
                for (const auto& m : result["main"])
                    if (m.get<uint32_t>() == fidVal) { found = true; break; }
                if (!found)
                    for (const auto& s : result["secondary"])
                        if (s.get<uint32_t>() == fidVal) { found = true; break; }
                if (!found) excluded.push_back(fidVal);
            }
        }
        result["excluded"] = excluded;

        std::string resultJson = result.dump();
        SKSE::log::info("EvaluatePreStartSexualScene: result built, totalActors={}, actorsExcluded={}, result={}",
                        totalActors, actorsExcluded, resultJson);

        if (originalParticipants.size() > 1 && totalActors <= 1) {
            SKSE::log::info("EvaluatePreStartSexualScene: only {} actor(s) after evaluation, aborting.", totalActors);
            OStimNet::FireModEvent("ostimnet_sexual_evaluation_finished", resultJson.c_str(), 2.0f);
        } else if (actorsExcluded) {
            SKSE::log::info("EvaluatePreStartSexualScene: LLM excluded actor(s), final={} vs original={}.",
                            totalActors, originalParticipants.size());
            OStimNet::FireModEvent("ostimnet_sexual_evaluation_finished", resultJson.c_str(), 3.0f);
        } else {
            OStimNet::FireModEvent("ostimnet_sexual_evaluation_finished", resultJson.c_str(), 1.0f);
        }
    };

    bool queued = PublicSendCustomPromptToLLM(
        promptName.c_str(), llmVariant.c_str(), contextJson.c_str(), *callbackHolder);

    if (!queued)
        SKSE::log::warn("EvaluatePreStartSexualScene: PublicSendCustomPromptToLLM returned false (immediate failure).");
    return queued;
}

bool EvaluateExternalSexualThread(const std::vector<RE::FormID>& participantFormIDs,
                                   int threadID) {
    if (!PublicSendCustomPromptToLLM) {
        SKSE::log::warn("EvaluateExternalSexualThread: PublicSendCustomPromptToLLM unavailable (SkyrimNet v8+ required).");
        return false;
    }

    // Build name→FormID map and actors name list from the participant list.
    // Captured by value into the callback so name resolution works without
    // walking all loaded forms.
    std::map<std::string, RE::FormID> nameToFormID;
    nlohmann::json actorUUIDs = nlohmann::json::array();
    std::vector<std::string> actorNameVec;

    for (RE::FormID fid : participantFormIDs) {
        std::string displayName;
        if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(fid))
            displayName = OStimNet::ThreadDataStore::GetActorDisplayName(actor, "");
        if (!displayName.empty()) {
            nameToFormID[displayName] = fid;
            actorNameVec.push_back(displayName);
            uint64_t uuid = ActorRefIDToUUID(fid);
            if (uuid != 0) actorUUIDs.push_back(uuid);
        }
    }

    // Build context JSON for the prompt template.
    nlohmann::json ctx;
    ctx["actors"]            = actorUUIDs;
    ctx["actorNames"]        = OStimNet::FormatActorList(actorNameVec);
    ctx["participant_count"] = static_cast<int>(participantFormIDs.size());
    ctx["threadID"]          = threadID;

    // Also inject flat slots in case the prompt template needs them.
    for (int i = 0; i < 5; ++i) {
        std::string keyId   = "participant" + std::to_string(i + 1) + "_formid";
        std::string keyName = "participant" + std::to_string(i + 1) + "_name";
        if (i < static_cast<int>(participantFormIDs.size())) {
            RE::FormID fid = participantFormIDs[i];
            ctx[keyId] = static_cast<uint32_t>(fid);
            auto it = std::find_if(nameToFormID.begin(), nameToFormID.end(),
                                   [fid](const auto& p) { return p.second == fid; });
            ctx[keyName] = (it != nameToFormID.end()) ? it->first : "";
        } else {
            ctx[keyId]   = 0;
            ctx[keyName] = "";
        }
    }

    std::string contextJson = ctx.dump();
    SKSE::log::info("EvaluateExternalSexualThread: queuing LLM prompt, threadID={}, participants={}",
                    threadID, participantFormIDs.size());

    std::string promptName = "ostimnet_evaluations/ostimnet_evaluate_external_sexual_thread";
    std::string llmVariant = OStimNet::Config::GetSingleton().GmLlmVariant();

    auto callbackHolder = std::make_shared<std::function<void(const char*, int)>>();

    *callbackHolder = [callbackHolder, nameToFormID, threadID, contextJson, promptName, llmVariant](const char* response, int success) {
        SKSE::log::info("EvaluateExternalSexualThread callback: success={}", success);

        auto retryAction = [callbackHolder, promptName, llmVariant, contextJson]() {
            SKSE::log::info("EvaluateExternalSexualThread: retrying LLM request.");
            if (PublicSendCustomPromptToLLM)
                PublicSendCustomPromptToLLM(
                    promptName.c_str(), llmVariant.c_str(),
                    contextJson.c_str(), *callbackHolder);
        };
        auto ignoreAction = []() {
            SKSE::log::info("EvaluateExternalSexualThread: player chose to ignore failed evaluation.");
        };

        if (!success || !response || response[0] == '\0') {
            SKSE::log::warn("EvaluateExternalSexualThread: LLM returned failure or empty response.");
            ShowLLMRetryModal(
                "External Sexual Thread",
                "The LLM evaluation failed (HTTP or connection error).",
                retryAction, ignoreAction);
            return;
        }

        nlohmann::json resp = nlohmann::json::parse(response, nullptr, /*exceptions=*/false);
        if (resp.is_discarded()) {
            // LLM may have wrapped the JSON in markdown fences or added prose — try to salvage it.
            SKSE::log::warn("EvaluateExternalSexualThread: raw response was not valid JSON, attempting sanitization.");
            std::string sanitized = JsonService::SanitizeLLMJson(response);
            if (!sanitized.empty())
                resp = nlohmann::json::parse(sanitized, nullptr, /*exceptions=*/false);
        }
        if (resp.is_discarded()) {
            SKSE::log::warn("EvaluateExternalSexualThread: LLM response was not valid JSON (even after sanitization). Raw: {}",
                            response ? response : "(null)");
            ShowLLMRetryModal(
                "External Sexual Thread",
                "The LLM returned an unreadable response.",
                retryAction, ignoreAction);
            return;
        }

        auto resolveNames = [&](const nlohmann::json& src, const char* key) {
            return JsonService::ResolveNamesToFormIDs(src, key, nameToFormID, "EvaluateExternalSexualThread");
        };

        // Prompt returns {"reason":..., "scene": {"intent":..., "mainActors":[...], "secondaryActors":[...], ...}}
        // "scene" is null when the LLM determines no viable role assignments are possible.
        if (!resp.contains("scene") || resp["scene"].is_null()) {
            SKSE::log::info("EvaluateExternalSexualThread: LLM returned scene=null (not enough willing participants).");
            return;
        }
        const nlohmann::json& scene = resp["scene"];
        std::string effectiveIntent = scene.value("intent", "");

        nlohmann::json result;
        result["main"]      = resolveNames(scene, "mainActors");
        result["secondary"] = resolveNames(scene, "secondaryActors");

        // Guardrail: LLMs sometimes place all actors in mainActors and leave secondaryActors empty.
        // If secondary is empty and main has more than one actor, move the last main actor to secondary.
        if (result["secondary"].empty() && result["main"].size() > 1) {
            SKSE::log::info("EvaluateExternalSexualThread: secondary actors empty with {} main actors — moving last main actor to secondary.",
                            result["main"].size());
            result["secondary"].push_back(result["main"].back());
            result["main"].erase(result["main"].size() - 1);
        }

        SKSE::log::info("EvaluateExternalSexualThread: result built, threadID={}", threadID);

        auto& store = OStimNet::ThreadDataStore::GetSingleton();
        if (!store.IsRegisteredAndPending(threadID)) {
            SKSE::log::info("EvaluateExternalSexualThread: thread {} no longer pending (ended before LLM response), discarding.", threadID);
            return;
        }
        if (!g_ostimThreadInterface || !g_ostimThreadInterface->IsThreadValid(static_cast<uint32_t>(threadID))) {
            SKSE::log::warn("EvaluateExternalSexualThread: thread {} is no longer valid, discarding.", threadID);
            store.ClearThread(threadID);
            return;
        }

        SKSE::log::info("EvaluateExternalSexualThread: claiming thread {} as OStimNet", threadID);
        store.SetOStimNet(threadID, true);
        store.SetIntent(threadID, OStimNet::IntentFromString(effectiveIntent));
        store.SetSexual(threadID, true);

        // Resolve role arrays (already stored in `result` as uint32_t FormIDs) back to Actor*.
        auto resolveActors = [&](const char* key) -> std::vector<RE::Actor*> {
            std::vector<RE::Actor*> actors;
            if (!result.contains(key) || !result[key].is_array()) return actors;
            for (const auto& item : result[key]) {
                if (!item.is_number_unsigned()) continue;
                auto* actor = RE::TESForm::LookupByID<RE::Actor>(item.get<uint32_t>());
                if (actor) actors.push_back(actor);
            }
            return actors;
        };

        store.SetMainActors(threadID, resolveActors("main"));
        store.SetSecondaryActors(threadID, resolveActors("secondary"));

        auto* listener = OStimNet::OStimEventListener::GetInstance();
        if (listener) {
            listener->ClaimPendingNonOStimNetThread(threadID);
        } else {
            SKSE::log::warn("EvaluateExternalSexualThread: listener is null, cannot claim thread {}", threadID);
        }
    };

    bool queued = PublicSendCustomPromptToLLM(
        promptName.c_str(), llmVariant.c_str(), contextJson.c_str(), *callbackHolder);

    if (!queued)
        SKSE::log::warn("EvaluateExternalSexualThread: PublicSendCustomPromptToLLM returned false (immediate failure).");
    return queued;
}

bool EvaluateNonSexualScene(const std::vector<RE::FormID>& participantFormIDs, const std::string& activity, const std::string& intent, RE::FormID initiatorFormID) {
    if (!PublicSendCustomPromptToLLM) {
        SKSE::log::warn("EvaluateNonSexualScene: PublicSendCustomPromptToLLM unavailable (SkyrimNet v8+ required).");
        return false;
    }

    // Build name→FormID map and actors name list from the participant list.
    std::map<std::string, RE::FormID> nameToFormID;
    nlohmann::json actorUUIDs = nlohmann::json::array();
    std::vector<std::string> actorNameVec;

    for (RE::FormID fid : participantFormIDs) {
        std::string displayName;
        if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(fid))
            displayName = OStimNet::ThreadDataStore::GetActorDisplayName(actor, "");
        if (!displayName.empty()) {
            nameToFormID[displayName] = fid;
            actorNameVec.push_back(displayName);
            uint64_t uuid = ActorRefIDToUUID(fid);
            if (uuid != 0) actorUUIDs.push_back(uuid);
        }
    }

    // Build context JSON for the prompt template.
    nlohmann::json ctx;
    ctx["actors"]            = actorUUIDs;
    ctx["actorNames"]        = OStimNet::FormatActorList(actorNameVec);
    ctx["participant_count"] = static_cast<int>(participantFormIDs.size());
    ctx["activity"]          = activity;
    ctx["intent"]            = intent;
    if (initiatorFormID != 0) {
        uint64_t uuid = ActorRefIDToUUID(initiatorFormID);
        ctx["initiator"] = uuid != 0 ? uuid : static_cast<uint64_t>(0);
    } else {
        ctx["initiator"] = nullptr;
    }

    // Also inject flat slots in case the prompt template needs them.
    for (int i = 0; i < 5; ++i) {
        std::string keyId   = "participant" + std::to_string(i + 1) + "_formid";
        std::string keyName = "participant" + std::to_string(i + 1) + "_name";
        if (i < static_cast<int>(participantFormIDs.size())) {
            RE::FormID fid = participantFormIDs[i];
            ctx[keyId] = static_cast<uint32_t>(fid);
            auto it = std::find_if(nameToFormID.begin(), nameToFormID.end(),
                                   [fid](const auto& p) { return p.second == fid; });
            ctx[keyName] = (it != nameToFormID.end()) ? it->first : "";
        } else {
            ctx[keyId]   = 0;
            ctx[keyName] = "";
        }
    }

    std::string contextJson = ctx.dump();
    SKSE::log::info("EvaluateNonSexualScene: queuing LLM prompt, participants={}",
                    participantFormIDs.size());

    std::string promptName = "ostimnet_evaluations/ostimnet_evaluate_nonsexual_scene";
    std::string llmVariant = OStimNet::Config::GetSingleton().GmLlmVariant();

    auto callbackHolder = std::make_shared<std::function<void(const char*, int)>>();

    *callbackHolder = [callbackHolder, initiatorFormID, participantFormIDs, intent, activity,
                       contextJson, promptName, llmVariant](const char* response, int success) {
        SKSE::log::info("EvaluateNonSexualScene callback: success={}", success);

        auto retryAction = [callbackHolder, promptName, llmVariant, contextJson]() {
            SKSE::log::info("EvaluateNonSexualScene: retrying LLM request.");
            if (PublicSendCustomPromptToLLM)
                PublicSendCustomPromptToLLM(
                    promptName.c_str(), llmVariant.c_str(),
                    contextJson.c_str(), *callbackHolder);
        };
        auto ignoreAction = [participantFormIDs, initiatorFormID, intent, activity]() {
            SKSE::log::info("EvaluateNonSexualScene: player chose to ignore failed evaluation.");
            nlohmann::json ignoreResult;
            auto origArr = nlohmann::json::array();
            for (RE::FormID fid : participantFormIDs)
                origArr.push_back(static_cast<uint32_t>(fid));
            ignoreResult["originalParticipants"] = std::move(origArr);
            ignoreResult["initiator"]            = static_cast<uint32_t>(initiatorFormID);
            ignoreResult["intent"]               = intent;
            ignoreResult["activity"]             = activity;
            OStimNet::FireModEvent("ostimnet_nonsexual_evaluation_finished", ignoreResult.dump().c_str(), 0.0f);
        };

        if (!success || !response || response[0] == '\0') {
            SKSE::log::warn("EvaluateNonSexualScene: LLM returned failure or empty response.");
            ShowLLMRetryModal(
                "Non-Sexual Scene",
                "The LLM evaluation failed (HTTP or connection error).",
                retryAction, ignoreAction);
            return;
        }

        nlohmann::json resp = nlohmann::json::parse(response, nullptr, /*exceptions=*/false);
        if (resp.is_discarded()) {
            // LLM may have wrapped the JSON in markdown fences or added prose — try to salvage it.
            SKSE::log::warn("EvaluateNonSexualScene: raw response was not valid JSON, attempting sanitization.");
            std::string sanitized = JsonService::SanitizeLLMJson(response);
            if (!sanitized.empty())
                resp = nlohmann::json::parse(sanitized, nullptr, /*exceptions=*/false);
        }
        if (resp.is_discarded()) {
            SKSE::log::warn("EvaluateNonSexualScene: LLM response was not valid JSON (even after sanitization). Raw: {}",
                            response ? response : "(null)");
            ShowLLMRetryModal(
                "Non-Sexual Scene",
                "The LLM returned an unreadable response.",
                retryAction, ignoreAction);
            return;
        }

        // Build originalParticipants array once — reused in all result branches.
        auto origParticipants = nlohmann::json::array();
        for (RE::FormID fid : participantFormIDs)
            origParticipants.push_back(static_cast<uint32_t>(fid));

        bool start = resp.value("start", false);
        if (!start) {
            SKSE::log::info("EvaluateNonSexualScene: LLM returned start=false (scene should not start).");
            nlohmann::json noStartResult;
            noStartResult["start"]                = false;
            noStartResult["intent"]               = intent;
            noStartResult["activity"]             = activity;
            noStartResult["originalParticipants"] = origParticipants;
            noStartResult["initiator"]            = static_cast<uint32_t>(initiatorFormID);
            OStimNet::FireModEvent("ostimnet_nonsexual_evaluation_finished", noStartResult.dump().c_str(), 2.0f);
            return;
        }

        // main = initiator; secondary = all other participants
        auto mainArr = nlohmann::json::array();
        if (initiatorFormID != 0)
            mainArr.push_back(static_cast<uint32_t>(initiatorFormID));

        auto secondaryArr = nlohmann::json::array();
        for (RE::FormID fid : participantFormIDs) {
            if (fid != initiatorFormID)
                secondaryArr.push_back(static_cast<uint32_t>(fid));
        }

        nlohmann::json result;
        result["start"]                = true;
        result["intent"]               = intent;
        result["activity"]             = activity;
        result["main"]                 = std::move(mainArr);
        result["secondary"]            = std::move(secondaryArr);
        result["originalParticipants"] = std::move(origParticipants);
        result["initiator"]            = static_cast<uint32_t>(initiatorFormID);

        std::string resultJson = result.dump();
        SKSE::log::info("EvaluateNonSexualScene: result built, result={}", resultJson);
        OStimNet::FireModEvent("ostimnet_nonsexual_evaluation_finished", resultJson.c_str(), 1.0f);
    };

    bool queued = PublicSendCustomPromptToLLM(
        promptName.c_str(), llmVariant.c_str(), contextJson.c_str(), *callbackHolder);

    if (!queued)
        SKSE::log::warn("EvaluateNonSexualScene: PublicSendCustomPromptToLLM returned false (immediate failure).");
    return queued;
}

bool EvaluateJoinOngoingSex(RE::FormID joinerFormID,
                            int threadID) {
    if (!PublicSendCustomPromptToLLM) {
        SKSE::log::warn("EvaluateJoinOngoingSex: PublicSendCustomPromptToLLM unavailable (SkyrimNet v8+ required).");
        return false;
    }

    // Fetch current participants and intent from ThreadDataStore.
    auto& store = OStimNet::ThreadDataStore::GetSingleton();
    const auto& currentActorPtrs = store.GetActorPtrs(threadID);
    std::string currentIntent = OStimNet::IntentToString(store.GetIntent(threadID));

    std::vector<RE::FormID> currentParticipantFormIDs;
    currentParticipantFormIDs.reserve(currentActorPtrs.size());
    for (RE::Actor* a : currentActorPtrs)
        if (a) currentParticipantFormIDs.push_back(a->GetFormID());

    // Captured by value into the callback so name resolution works without
    // walking all loaded forms.
    std::map<std::string, RE::FormID> nameToFormID;
    nlohmann::json currentActorUUIDs = nlohmann::json::array();
    std::vector<std::string> currentActorNameVec;

    for (RE::FormID fid : currentParticipantFormIDs) {
        std::string displayName;
        if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(fid))
            displayName = OStimNet::ThreadDataStore::GetActorDisplayName(actor, "");
        if (!displayName.empty()) {
            nameToFormID[displayName] = fid;
            currentActorNameVec.push_back(displayName);
            uint64_t uuid = ActorRefIDToUUID(fid);
            if (uuid != 0) currentActorUUIDs.push_back(uuid);
        }
    }

    std::string joinerName;
    uint64_t joinerUUID = 0;
    if (auto* joiner = RE::TESForm::LookupByID<RE::Actor>(joinerFormID)) {
        joinerName = OStimNet::ThreadDataStore::GetActorDisplayName(joiner, "");
        joinerUUID = ActorToUUID(joiner);
    }
    if (!joinerName.empty())
        nameToFormID[joinerName] = joinerFormID;

    // Build context JSON for the prompt template.
    nlohmann::json ctx;
    ctx["currentActors"]     = currentActorUUIDs;
    ctx["currentActorNames"] = OStimNet::FormatActorList(currentActorNameVec);
    ctx["joiner"]            = joinerUUID;
    ctx["joinerName"]        = joinerName;
    ctx["joinerFormID"]      = static_cast<uint32_t>(joinerFormID);
    ctx["currentIntent"]     = currentIntent;
    ctx["threadID"]          = threadID;
    ctx["participant_count"] = static_cast<int>(currentParticipantFormIDs.size()) + 1;

    std::string contextJson = ctx.dump();
    SKSE::log::info("EvaluateJoinOngoingSex: queuing LLM prompt, threadID={}, joiner='{}', currentParticipants={}",
                    threadID, joinerName, currentParticipantFormIDs.size());

    std::string promptName = "ostimnet_evaluations/ostimnet_evaluate_join_ongoing_sex";
    std::string llmVariant = OStimNet::Config::GetSingleton().GmLlmVariant();

    auto callbackHolder = std::make_shared<std::function<void(const char*, int)>>();

    *callbackHolder = [callbackHolder, nameToFormID, threadID, joinerFormID, currentParticipantFormIDs,
                       contextJson, promptName, llmVariant](const char* response, int success) {
        SKSE::log::info("EvaluateJoinOngoingSex callback: success={}", success);

        auto retryAction = [callbackHolder, promptName, llmVariant, contextJson]() {
            SKSE::log::info("EvaluateJoinOngoingSex: retrying LLM request.");
            if (PublicSendCustomPromptToLLM)
                PublicSendCustomPromptToLLM(
                    promptName.c_str(), llmVariant.c_str(),
                    contextJson.c_str(), *callbackHolder);
        };
        auto ignoreAction = [threadID, joinerFormID, currentParticipantFormIDs]() {
            SKSE::log::info("EvaluateJoinOngoingSex: player chose to ignore failed evaluation.");
            nlohmann::json ignoreResult;
            ignoreResult["threadID"] = threadID;
            ignoreResult["joiner"]   = static_cast<uint32_t>(joinerFormID);
            auto curArr = nlohmann::json::array();
            for (RE::FormID fid : currentParticipantFormIDs)
                curArr.push_back(static_cast<uint32_t>(fid));
            ignoreResult["currentParticipants"] = std::move(curArr);
            OStimNet::FireModEvent("ostimnet_join_sex_evaluation_finished", ignoreResult.dump().c_str(), 0.0f);
        };

        if (!success || !response || response[0] == '\0') {
            SKSE::log::warn("EvaluateJoinOngoingSex: LLM returned failure or empty response.");
            ShowLLMRetryModal(
                "Join Ongoing Sex",
                "The LLM evaluation failed (HTTP or connection error).",
                retryAction, ignoreAction);
            return;
        }

        nlohmann::json resp = nlohmann::json::parse(response, nullptr, /*exceptions=*/false);
        if (resp.is_discarded()) {
            SKSE::log::warn("EvaluateJoinOngoingSex: raw response was not valid JSON, attempting sanitization.");
            std::string sanitized = JsonService::SanitizeLLMJson(response);
            if (!sanitized.empty())
                resp = nlohmann::json::parse(sanitized, nullptr, /*exceptions=*/false);
        }
        if (resp.is_discarded()) {
            SKSE::log::warn("EvaluateJoinOngoingSex: LLM response was not valid JSON (even after sanitization). Raw: {}",
                            response ? response : "(null)");
            ShowLLMRetryModal(
                "Join Ongoing Sex",
                "The LLM returned an unreadable response.",
                retryAction, ignoreAction);
            return;
        }

        auto resolveNames = [&](const nlohmann::json& src, const char* key) {
            return JsonService::ResolveNamesToFormIDs(src, key, nameToFormID, "EvaluateJoinOngoingSex");
        };

        auto joinCurArr = nlohmann::json::array();
        for (RE::FormID fid : currentParticipantFormIDs)
            joinCurArr.push_back(static_cast<uint32_t>(fid));

        if (!resp.contains("scene") || resp["scene"].is_null()) {
            SKSE::log::info("EvaluateJoinOngoingSex: LLM returned scene=null (decided not to proceed).");
            nlohmann::json noJoinResult;
            noJoinResult["threadID"]           = threadID;
            noJoinResult["joiner"]             = static_cast<uint32_t>(joinerFormID);
            noJoinResult["currentParticipants"] = joinCurArr;
            OStimNet::FireModEvent("ostimnet_join_sex_evaluation_finished", noJoinResult.dump().c_str(), 2.0f);
            return;
        }
        const nlohmann::json& scene = resp["scene"];

        nlohmann::json result;
        result["threadID"]           = threadID;
        result["joiner"]             = static_cast<uint32_t>(joinerFormID);
        result["currentParticipants"] = std::move(joinCurArr);
        result["intent"]             = scene.value("intent", "");
        result["main"]               = resolveNames(scene, "mainActors");
        result["secondary"]          = resolveNames(scene, "secondaryActors");

        // Guardrail: LLMs sometimes place all actors in mainActors and leave secondaryActors empty.
        // If secondary is empty and main has more than one actor, move the last main actor to secondary.
        if (result["secondary"].empty() && result["main"].size() > 1) {
            SKSE::log::info("EvaluateJoinOngoingSex: secondary actors empty with {} main actors — moving last main actor to secondary.",
                            result["main"].size());
            result["secondary"].push_back(result["main"].back());
            result["main"].erase(result["main"].size() - 1);
        }

        std::string resultJson = result.dump();
        SKSE::log::info("EvaluateJoinOngoingSex: result built, threadID={}, result={}", threadID, resultJson);
        OStimNet::FireModEvent("ostimnet_join_sex_evaluation_finished", resultJson.c_str(), 1.0f);
    };

    bool queued = PublicSendCustomPromptToLLM(
        promptName.c_str(), llmVariant.c_str(), contextJson.c_str(), *callbackHolder);

    if (!queued)
        SKSE::log::warn("EvaluateJoinOngoingSex: PublicSendCustomPromptToLLM returned false (immediate failure).");
    return queued;
}

bool EvaluateInviteToSex(RE::FormID inviterFormID,
                         const std::vector<RE::FormID>& inviteeFormIDs,
                         int threadID) {
    if (!PublicSendCustomPromptToLLM) {
        SKSE::log::warn("EvaluateInviteToSex: PublicSendCustomPromptToLLM unavailable (SkyrimNet v8+ required).");
        return false;
    }

    // Fetch current participants and intent from ThreadDataStore.
    auto& store = OStimNet::ThreadDataStore::GetSingleton();
    const auto& currentActorPtrs = store.GetActorPtrs(threadID);
    std::string currentIntent = OStimNet::IntentToString(store.GetIntent(threadID));

    std::vector<RE::FormID> currentParticipantFormIDs;
    currentParticipantFormIDs.reserve(currentActorPtrs.size());
    for (RE::Actor* a : currentActorPtrs)
        if (a) currentParticipantFormIDs.push_back(a->GetFormID());

    // Build a combined name→FormID map: current participants + invitees.
    // Captured by value into the callback so name resolution works without
    // walking all loaded forms.
    std::map<std::string, RE::FormID> nameToFormID;
    nlohmann::json currentActorUUIDs = nlohmann::json::array();
    std::vector<std::string> currentActorNameVec;

    for (RE::FormID fid : currentParticipantFormIDs) {
        std::string displayName;
        if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(fid))
            displayName = OStimNet::ThreadDataStore::GetActorDisplayName(actor, "");
        if (!displayName.empty()) {
            nameToFormID[displayName] = fid;
            currentActorNameVec.push_back(displayName);
            uint64_t uuid = ActorRefIDToUUID(fid);
            if (uuid != 0) currentActorUUIDs.push_back(uuid);
        }
    }

    nlohmann::json inviteeUUIDs = nlohmann::json::array();
    std::vector<std::string> inviteeNameVec;
    for (RE::FormID fid : inviteeFormIDs) {
        std::string displayName;
        if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(fid))
            displayName = OStimNet::ThreadDataStore::GetActorDisplayName(actor, "");
        if (!displayName.empty()) {
            nameToFormID[displayName] = fid;
            inviteeNameVec.push_back(displayName);
            uint64_t uuid = ActorRefIDToUUID(fid);
            if (uuid != 0) inviteeUUIDs.push_back(uuid);
        }
    }

    std::string inviterName;
    uint64_t inviterUUID = 0;
    if (auto* inviter = RE::TESForm::LookupByID<RE::Actor>(inviterFormID)) {
        inviterName = OStimNet::ThreadDataStore::GetActorDisplayName(inviter, "");
        inviterUUID = ActorToUUID(inviter);
    }

    // Build context JSON for the prompt template.
    nlohmann::json ctx;
    ctx["currentActors"]     = currentActorUUIDs;
    ctx["currentActorNames"] = OStimNet::FormatActorList(currentActorNameVec);
    ctx["inviter"]           = inviterUUID;
    ctx["inviterName"]       = inviterName;
    ctx["inviterFormID"]     = static_cast<uint32_t>(inviterFormID);
    ctx["invitees"]          = inviteeUUIDs;
    ctx["inviteeNames"]      = OStimNet::FormatActorList(inviteeNameVec);
    ctx["currentIntent"]     = currentIntent;
    ctx["threadID"]          = threadID;
    ctx["participant_count"] = static_cast<int>(currentParticipantFormIDs.size()) + static_cast<int>(inviteeFormIDs.size());

    std::string contextJson = ctx.dump();
    SKSE::log::info("EvaluateInviteToSex: queuing LLM prompt, threadID={}, inviter='{}', invitees={}",
                    threadID, inviterName, inviteeFormIDs.size());

    std::string promptName = "ostimnet_evaluations/ostimnet_evaluate_invite_to_sex";
    std::string llmVariant = OStimNet::Config::GetSingleton().GmLlmVariant();

    auto callbackHolder = std::make_shared<std::function<void(const char*, int)>>();

    *callbackHolder = [callbackHolder, nameToFormID, threadID, inviterFormID, inviteeFormIDs, currentParticipantFormIDs,
                       contextJson, promptName, llmVariant](const char* response, int success) {

        auto retryAction = [callbackHolder, promptName, llmVariant, contextJson]() {
            SKSE::log::info("EvaluateInviteToSex: retrying LLM request.");
            if (PublicSendCustomPromptToLLM)
                PublicSendCustomPromptToLLM(
                    promptName.c_str(), llmVariant.c_str(),
                    contextJson.c_str(), *callbackHolder);
        };
        auto ignoreAction = [threadID, inviterFormID, inviteeFormIDs, currentParticipantFormIDs]() {
            SKSE::log::info("EvaluateInviteToSex: player chose to ignore failed evaluation.");
            nlohmann::json ignoreResult;
            ignoreResult["threadID"] = threadID;
            ignoreResult["inviter"]  = static_cast<uint32_t>(inviterFormID);
            nlohmann::json inviteesArr = nlohmann::json::array();
            for (RE::FormID fid : inviteeFormIDs)
                inviteesArr.push_back(static_cast<uint32_t>(fid));
            ignoreResult["invitees"] = std::move(inviteesArr);
            auto curArr = nlohmann::json::array();
            for (RE::FormID fid : currentParticipantFormIDs)
                curArr.push_back(static_cast<uint32_t>(fid));
            ignoreResult["currentParticipants"] = std::move(curArr);
            OStimNet::FireModEvent("ostimnet_invite_sex_evaluation_finished", ignoreResult.dump().c_str(), 0.0f);
        };

        if (!success || !response || response[0] == '\0') {
            SKSE::log::warn("EvaluateInviteToSex: LLM returned failure or empty response.");
            ShowLLMRetryModal(
                "Invite to Sex",
                "The LLM evaluation failed (HTTP or connection error).",
                retryAction, ignoreAction);
            return;
        }

        nlohmann::json resp = nlohmann::json::parse(response, nullptr, /*exceptions=*/false);
        if (resp.is_discarded()) {
            SKSE::log::warn("EvaluateInviteToSex: raw response was not valid JSON, attempting sanitization.");
            std::string sanitized = JsonService::SanitizeLLMJson(response);
            if (!sanitized.empty())
                resp = nlohmann::json::parse(sanitized, nullptr, /*exceptions=*/false);
        }
        if (resp.is_discarded()) {
            SKSE::log::warn("EvaluateInviteToSex: LLM response was not valid JSON (even after sanitization). Raw: {}",
                            response ? response : "(null)");
            ShowLLMRetryModal(
                "Invite to Sex",
                "The LLM returned an unreadable response.",
                retryAction, ignoreAction);
            return;
        }

        auto resolveNames = [&](const nlohmann::json& src, const char* key) {
            return JsonService::ResolveNamesToFormIDs(src, key, nameToFormID, "EvaluateInviteToSex");
        };

        auto invCurArr = nlohmann::json::array();
        for (RE::FormID fid : currentParticipantFormIDs)
            invCurArr.push_back(static_cast<uint32_t>(fid));

        if (!resp.contains("scene") || resp["scene"].is_null()) {
            SKSE::log::info("EvaluateInviteToSex: LLM returned scene=null (decided not to proceed).");
            nlohmann::json noInviteResult;
            noInviteResult["threadID"] = threadID;
            noInviteResult["inviter"]  = static_cast<uint32_t>(inviterFormID);
            nlohmann::json inviteesArrNo = nlohmann::json::array();
            for (RE::FormID fid : inviteeFormIDs)
                inviteesArrNo.push_back(static_cast<uint32_t>(fid));
            noInviteResult["invitees"]            = std::move(inviteesArrNo);
            noInviteResult["currentParticipants"] = invCurArr;
            OStimNet::FireModEvent("ostimnet_invite_sex_evaluation_finished", noInviteResult.dump().c_str(), 2.0f);
            return;
        }
        const nlohmann::json& scene = resp["scene"];

        nlohmann::json result;
        result["threadID"]            = threadID;
        result["inviter"]             = static_cast<uint32_t>(inviterFormID);
        result["currentParticipants"] = std::move(invCurArr);
        nlohmann::json inviteesArr = nlohmann::json::array();
        for (RE::FormID fid : inviteeFormIDs)
            inviteesArr.push_back(static_cast<uint32_t>(fid));
        result["invitees"]  = inviteesArr;
        result["intent"]    = scene.value("intent", "");
        result["main"]      = resolveNames(scene, "mainActors");
        result["secondary"] = resolveNames(scene, "secondaryActors");

        std::string resultJson = result.dump();
        SKSE::log::info("EvaluateInviteToSex: result built, threadID={}, result={}", threadID, resultJson);
        OStimNet::FireModEvent("ostimnet_invite_sex_evaluation_finished", resultJson.c_str(), 1.0f);
    };

    bool queued = PublicSendCustomPromptToLLM(
        promptName.c_str(), llmVariant.c_str(), contextJson.c_str(), *callbackHolder);

    if (!queued)
        SKSE::log::warn("EvaluateInviteToSex: PublicSendCustomPromptToLLM returned false (immediate failure).");
    return queued;
}

bool EvaluateLocationScan(const std::string& contextJson,
                          const std::map<std::string, RE::FormID>& nameToFormID) {
    if (!PublicSendCustomPromptToLLM) {
        SKSE::log::warn("EvaluateLocationScan: PublicSendCustomPromptToLLM is null (SkyrimNet v8+ required), scan skipped");
        return false;
    }

    SKSE::log::info("EvaluateLocationScan: queuing LLM scan (context={})", contextJson);

    std::string promptName = "ostimnet_evaluations/ostimnet_scan_location";
    std::string llmVariant = OStimNet::Config::GetSingleton().GmLlmVariant();

    auto callbackHolder = std::make_shared<std::function<void(const char*, int)>>();

    *callbackHolder = [callbackHolder, nameToFormID, contextJson, promptName, llmVariant](const char* response, int success) {
        SKSE::log::info("EvaluateLocationScan callback: success={}", success);

        auto retryAction = [callbackHolder, promptName, llmVariant, contextJson]() {
            SKSE::log::info("EvaluateLocationScan: retrying LLM request.");
            if (PublicSendCustomPromptToLLM)
                PublicSendCustomPromptToLLM(
                    promptName.c_str(), llmVariant.c_str(),
                    contextJson.c_str(), *callbackHolder);
        };
        auto ignoreAction = [nameToFormID]() {
            SKSE::log::info("EvaluateLocationScan: player chose to ignore failed evaluation.");
            nlohmann::json ignoreResult;
            auto origArr = nlohmann::json::array();
            for (const auto& [name, fid] : nameToFormID)
                origArr.push_back(static_cast<uint32_t>(fid));
            ignoreResult["originalParticipants"] = std::move(origArr);
            FireModEvent("ostimnet_location_scan_result", ignoreResult.dump().c_str(), 0.0f);
        };

        if (!success || !response || response[0] == '\0') {
            SKSE::log::warn("EvaluateLocationScan: LLM returned failure or empty response");
            ShowLLMRetryModal(
                "Location Scan",
                "The LLM evaluation failed (HTTP or connection error).",
                retryAction, ignoreAction);
            return;
        }

        nlohmann::json resp = nlohmann::json::parse(response, nullptr, /*exceptions=*/false);
        if (resp.is_discarded()) {
            SKSE::log::warn("EvaluateLocationScan: raw response was not valid JSON, attempting sanitization");
            std::string sanitized = JsonService::SanitizeLLMJson(response);
            if (!sanitized.empty())
                resp = nlohmann::json::parse(sanitized, nullptr, /*exceptions=*/false);
        }

        if (resp.is_discarded()) {
            SKSE::log::warn("EvaluateLocationScan: LLM response was not valid JSON (even after sanitization). Raw: {}",
                            response ? response : "(null)");
            ShowLLMRetryModal(
                "Location Scan",
                "The LLM returned an unreadable response.",
                retryAction, ignoreAction);
            return;
        }

        // Prompt returns {"reason":..., "scene": {"intent":..., "mainActors":[...], "secondaryActors":[...], ...}}
        // "scene" is null when the LLM determines no viable role assignments are possible.
        if (!resp.contains("scene") || resp["scene"].is_null()) {
            SKSE::log::info("EvaluateLocationScan: LLM returned scene=null (not enough willing participants).");
            OStimNet::FireModEvent("ostimnet_location_scan_result", "", 2.0f);
            return;
        }
        const nlohmann::json& scene = resp["scene"];

        auto resolveNames = [&](const nlohmann::json& src, const char* key) {
            return JsonService::ResolveNamesToFormIDs(src, key, nameToFormID, "EvaluateLocationScan");
        };

        bool hasParticipants = scene.contains("mainActors") &&
                               scene["mainActors"].is_array() &&
                               !scene["mainActors"].empty();

        nlohmann::json result;
        result["intent"]           = scene.value("intent", "");
        result["sexualPosition"]   = scene.value("sexualPosition", "");
        result["sexualActivities"] = scene.value("sexualActivity", "");
        result["main"]             = resolveNames(scene, "mainActors");
        result["secondary"]        = resolveNames(scene, "secondaryActors");
        result["furniture"]        = scene.value("furniture", "bed");

        std::string jsonStr = result.dump();
        float numArg = hasParticipants ? 1.0f : 2.0f;
        SKSE::log::info("EvaluateLocationScan: result — {} | reason: {}",
                        hasParticipants ? "participants found" : "no participants",
                        result.value("reason", "(none)"));

        FireModEvent("ostimnet_location_scan_result", jsonStr.c_str(), numArg);
    };

    bool queued = PublicSendCustomPromptToLLM(
        promptName.c_str(), llmVariant.c_str(), contextJson.c_str(), *callbackHolder);

    if (!queued)
        SKSE::log::warn("EvaluateLocationScan: PublicSendCustomPromptToLLM returned false (immediate failure)");
    return queued;
}

bool EvaluateScheduledSceneAdvance(int threadID, std::function<void()> onDone) {
    if (!PublicSendCustomPromptToLLM) {
        SKSE::log::warn("EvaluateScheduledSceneAdvance: PublicSendCustomPromptToLLM unavailable");
        onDone();
        return false;
    }

    // Validate thread is still alive before we waste an LLM round-trip.
    if (!g_ostimThreadInterface || !g_ostimThreadInterface->IsThreadValid(static_cast<uint32_t>(threadID)) ||
        !ThreadDataStore::GetSingleton().IsOStimNet(threadID)) {
        SKSE::log::info("EvaluateScheduledSceneAdvance: thread {} is no longer valid, skipping", threadID);
        onDone();
        return false;
    }

    auto actors = ThreadDataStore::GetSingleton().GetActorPtrs(threadID);
    if (actors.empty()) {
        SKSE::log::warn("EvaluateScheduledSceneAdvance: thread {} has no actors", threadID);
        onDone();
        return false;
    }

    nlohmann::json actorUUIDs = nlohmann::json::array();
    std::vector<std::string> actorNameVec;
    for (auto* actor : actors) {
        if (!actor) continue;
        uint64_t uuid = ActorToUUID(actor);
        if (uuid != 0) {
            actorUUIDs.push_back(uuid);
            actorNameVec.push_back(ThreadDataStore::GetActorDisplayName(actor, ""));
        }
    }

    nlohmann::json contextJson;
    contextJson["actors"] = std::move(actorUUIDs);
    contextJson["actorNames"] = OStimNet::FormatActorList(actorNameVec);
    contextJson["intent"] = OStimNet::IntentToString(ThreadDataStore::GetSingleton().GetIntent(threadID));
    contextJson["currentSceneDescription"] = OStimNet::GetSceneDescription(static_cast<uint32_t>(threadID));
    contextJson["threadID"] = threadID;

    std::string contextStr = contextJson.dump();
    std::string promptName = "ostimnet_evaluations/ostimnet_evaluate_scene_advance";
    std::string llmVariant = OStimNet::Config::GetSingleton().GmLlmVariant();

    // Use shared_ptr so callback can capture itself for retry loop.
    auto callbackHolder = std::make_shared<std::function<void(const char*, int)>>();
    *callbackHolder = [threadID, onDone, promptName, llmVariant, contextStr, callbackHolder](const char* responseJsonStr, int success) {
        // Re-verify thread is still valid.
        if (!g_ostimThreadInterface || !g_ostimThreadInterface->IsThreadValid(static_cast<uint32_t>(threadID)) ||
            !ThreadDataStore::GetSingleton().IsOStimNet(threadID)) {
            SKSE::log::info("EvaluateScheduledSceneAdvance: thread {} ended or invalid during LLM wait", threadID);
            onDone();
            return;
        }
        auto sexual = ThreadDataStore::GetSingleton().GetSexual(threadID);
        if (!sexual.value_or(false)) {
            SKSE::log::info("EvaluateScheduledSceneAdvance: thread {} is no longer sexual", threadID);
            onDone();
            return;
        }

        auto retryFunc = [promptName, llmVariant, contextStr, callbackHolder]() {
            PublicSendCustomPromptToLLM(promptName.c_str(), llmVariant.c_str(), contextStr.c_str(), *callbackHolder);
        };

        if (!success || !responseJsonStr) {
            SKSE::log::error("EvaluateScheduledSceneAdvance: LLM request failed");
            ShowLLMRetryModal("Game Master Scene Advance", "The LLM request failed or returned empty.", retryFunc, onDone);
            return;
        }

        nlohmann::json resp = nlohmann::json::parse(responseJsonStr, nullptr, /*exceptions=*/false);
        if (resp.is_discarded()) {
            SKSE::log::warn("EvaluateScheduledSceneAdvance: raw response was not valid JSON, attempting sanitization");
            std::string sanitized = JsonService::SanitizeLLMJson(responseJsonStr);
            if (!sanitized.empty())
                resp = nlohmann::json::parse(sanitized, nullptr, /*exceptions=*/false);
        }

        if (resp.is_discarded()) {
            SKSE::log::error("EvaluateScheduledSceneAdvance: failed to parse response JSON: {}", responseJsonStr ? responseJsonStr : "(null)");
            ShowLLMRetryModal("Game Master Scene Advance", "Failed to parse the LLM response.", retryFunc, onDone);
            return;
        }

        if (resp.contains("scene") && resp["scene"].is_null()) {
            SKSE::log::info("EvaluateScheduledSceneAdvance: LLM returned scene=null (no advance appropriate)");
            onDone();
            return;
        }

        if (!resp.contains("scene") || !resp["scene"].is_object()) {
            SKSE::log::error("EvaluateScheduledSceneAdvance: response missing 'scene' object");
            ShowLLMRetryModal("Game Master Scene Advance", "Response missing 'scene' object.", retryFunc, onDone);
            return;
        }

        const auto& scene = resp["scene"];
        nlohmann::json result;
        result["threadID"] = threadID;
        result["sexualPosition"] = scene.value("sexualPosition", "");
        result["sexualActivities"] = scene.value("sexualActivity", "");

        std::string jsonStr = result.dump();
        SKSE::log::info("EvaluateScheduledSceneAdvance: success -> {}", jsonStr);
        FireModEvent("ostimnet_scene_change_schedule_finished", jsonStr.c_str(), 1.0f);
        onDone();
    };

    bool queued = PublicSendCustomPromptToLLM(promptName.c_str(), llmVariant.c_str(), contextStr.c_str(), *callbackHolder);
    if (!queued) {
        SKSE::log::warn("EvaluateScheduledSceneAdvance: PublicSendCustomPromptToLLM returned false");
        onDone();
    }
    return queued;
}

}  // namespace OStimNet::SkyrimNetIntegration
