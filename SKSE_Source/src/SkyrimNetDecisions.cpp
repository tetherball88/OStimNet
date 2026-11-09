#include "PCH.h"

#include "SkyrimNetDecisions.h"

#include "ActorUtils.h"
#include "Config.h"
#include "ConfirmationModal.h"
#include "JsonService.h"
#include "LocationScanService.h"
#include "OStimEventHelpers.h"
#include "OStimEventListener.h"
#include "PulloutService.h"
#include "ScheduledEvalService.h"
#include "SceneDescriptionUtils.h"
#include "ThreadRegistry.h"
#include "api/OStimNavigator_PublicAPI.h"
#include "api/OstimNG-API-Thread.h"
#include "api/SkyrimNet_PublicAPI.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace OStimNet::SkyrimNetDecisions {

namespace {

constexpr int kDefaultMaxProbabilities = 10;

// Sanitizes a string for use on a single `[ criteria ]` line (`key: description`).
// Replaces newlines/tabs with spaces, trims leading/trailing whitespace, and returns
// `fallback` if the resulting string is empty.
std::string SanitizeSingleLine(const std::string& raw, const std::string& fallback) {
    std::string out;
    out.reserve(raw.size());
    bool lastWasSpace = true;
    for (unsigned char c : raw) {
        if (c == '\r' || c == '\n' || c == '\t' || c == ' ') {
            if (!lastWasSpace) {
                out.push_back(' ');
                lastWasSpace = true;
            }
        } else {
            out.push_back(static_cast<char>(c));
            lastWasSpace = false;
        }
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out.empty() ? fallback : out;
}

// Sanitizes a criterion key so it contains no colons or whitespace.
std::string SanitizeKey(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    for (unsigned char c : raw) {
        if (c == ':' || c == '\r' || c == '\n' || c == '\t' || c == ' ') continue;
        out.push_back(static_cast<char>(c));
    }
    return out;
}

uint64_t ActorToUUID(RE::Actor* actor) {
    if (!actor || !PublicFormIDToUUID) return 0;
    return PublicFormIDToUUID(actor->GetFormID());
}

uint64_t ActorRefIDToUUID(RE::FormID refFormID) {
    if (!PublicFormIDToUUID || refFormID == 0) return 0;
    auto* actor = RE::TESForm::LookupByID<RE::Actor>(refFormID);
    if (!actor) return 0;
    return ActorToUUID(actor);
}

bool IsPlayerFormID(RE::FormID fid) {
    auto* player = RE::PlayerCharacter::GetSingleton();
    return player && player->GetFormID() == fid;
}

// Parses a JSON array of position IDs (from ONavGetAllPositions / ONavGetThreadPositions /
// ONavGetSetupPositions) into the structured array for Jev `[ criteria ]`:
// [{"id": "missionary", "desc": "missionary position"}, ...]
nlohmann::json ParsePositionsJsonToArray(const char* raw) {
    auto arr = nlohmann::json::array();
    if (!raw || raw[0] == '\0') return arr;

    try {
        auto j = nlohmann::json::parse(raw, nullptr, /*allow_exceptions=*/false);
        if (j.is_discarded() || !j.is_array()) return arr;

        const auto& displayNames = OStimNavigatorAPI::GetPositionDisplayNameMap();
        std::unordered_set<std::string> seen;

        for (const auto& item : j) {
            if (!item.is_string()) continue;
            std::string id = SanitizeKey(item.get<std::string>());
            if (id.empty() || id == "any" || id == "none") continue;
            if (!seen.insert(id).second) continue;

            std::string desc = id + " position";
            auto it = displayNames.find(id);
            if (it != displayNames.end() && !it->second.empty()) {
                std::string rawDesc = it->second;
                if (rawDesc.size() >= 2 && rawDesc.front() == '(' && rawDesc.back() == ')') {
                    rawDesc = rawDesc.substr(1, rawDesc.size() - 2);
                }
                desc = SanitizeSingleLine(id + " — " + rawDesc, id + " position");
            }

            nlohmann::json entry;
            entry["id"]   = id;
            entry["desc"] = desc;
            arr.push_back(std::move(entry));

            // Jev choice criteria limit is 255 (reserve 1 for "any").
            if (arr.size() >= 250) break;
        }
    } catch (...) {}

    return arr;
}

// Builds the structured array of available positions for Jev `[ criteria ]`:
// [{"id": "missionary", "desc": "missionary position"}, ...]
// When threadID >= 0, returns only positions compatible with that active OStim thread.
nlohmann::json BuildPositionsArrayJson(int threadID = -1) {
    const char* raw = nullptr;
    if (threadID >= 0 && ONavGetThreadPositions) {
        raw = ONavGetThreadPositions(static_cast<uint32_t>(threadID));
    } else if (ONavGetAllPositions) {
        raw = ONavGetAllPositions();
    }
    return ParsePositionsJsonToArray(raw);
}

// Builds the structured array of available intercourse positions for a pre-start actor + furniture setup.
nlohmann::json BuildSetupPositionsArrayJson(
    const std::vector<uint32_t>& actorFormIDs,
    const std::string& furnitureType) {
    const char* raw = nullptr;
    if (!actorFormIDs.empty() && ONavGetSetupPositions) {
        raw = ONavGetSetupPositions(
            actorFormIDs.data(),
            static_cast<uint32_t>(actorFormIDs.size()),
            furnitureType.c_str());
    } else if (ONavGetAllPositions) {
        raw = ONavGetAllPositions();
    }
    return ParsePositionsJsonToArray(raw);
}

bool IsExcludedClimaxAction(const std::string& id) {
    for (const char* p : {"cumon", "3pp_", "ejaculation_on", "facial"}) {
        if (id.rfind(p, 0) == 0) return true;
    }
    return false;
}

nlohmann::json ParseActivitiesJsonToArray(const char* raw) {
    auto arr = nlohmann::json::array();
    if (!raw || raw[0] == '\0') return arr;

    try {
        auto j = nlohmann::json::parse(raw, nullptr, /*allow_exceptions=*/false);
        if (j.is_discarded() || !j.is_array()) return arr;

        std::unordered_set<std::string> seen;
        for (const auto& item : j) {
            if (!item.is_string()) continue;
            std::string id = SanitizeKey(item.get<std::string>());
            std::transform(id.begin(), id.end(), id.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            if (id.empty() || id == "generic" || id == "any" || id == "none" || id == "hold") continue;
            if (IsExcludedClimaxAction(id)) continue;
            if (!seen.insert(id).second) continue;

            nlohmann::json entry;
            entry["id"]   = id;
            entry["desc"] = id;
            arr.push_back(std::move(entry));

            if (arr.size() >= 250) break;
        }
    } catch (...) {}

    return arr;
}

// Builds the structured array of available OStim activities for a pre-start actor + furniture setup
// and phase/tag ("foreplay", "oral", "sex", or "sexual"):
// [{"id": "handjob", "desc": "handjob"}, ...]
nlohmann::json BuildSetupActivitiesArrayJson(
    const std::vector<uint32_t>& actorFormIDs,
    const std::string& furnitureType,
    const std::string& phaseOrTag) {
    const char* raw = nullptr;
    if (!actorFormIDs.empty() && ONavGetSetupActions) {
        raw = ONavGetSetupActions(
            actorFormIDs.data(),
            static_cast<uint32_t>(actorFormIDs.size()),
            furnitureType.c_str(),
            phaseOrTag.c_str());
    } else if (ONavGetAllActions) {
        const char* fallbackTag = (phaseOrTag == "oral") ? "oral"
            : (phaseOrTag == "sex" || phaseOrTag == "intercourse") ? "intercourse"
            : "sexual";
        raw = ONavGetAllActions(fallbackTag);
    }
    return ParseActivitiesJsonToArray(raw);
}

// Builds the structured array of available OStim activities for an active OStim thread.
nlohmann::json BuildThreadActivitiesArrayJson(
    int threadID,
    const std::string& phaseOrTag = "sexual") {
    const char* raw = nullptr;
    if (threadID >= 0 && ONavGetThreadActions) {
        raw = ONavGetThreadActions(static_cast<uint32_t>(threadID), phaseOrTag.c_str());
    } else if (ONavGetAllActions) {
        const char* fallbackTag = (phaseOrTag == "oral") ? "oral"
            : (phaseOrTag == "sex" || phaseOrTag == "intercourse") ? "intercourse"
            : "sexual";
        raw = ONavGetAllActions(fallbackTag);
    }
    return ParseActivitiesJsonToArray(raw);
}

// Builds the structured array of nearby furniture types for Jev `[ criteria ]`:
// [{"id": "bed", "desc": "bed (distance 2.5m)"}, ...]
nlohmann::json BuildFurnitureArrayJson(RE::Actor* centerActor) {
    auto arr = nlohmann::json::array();
    if (!ONavGetFurnitureTypesWithSexScenes) return arr;
    uint32_t centerRefID = centerActor ? centerActor->GetFormID() : 0;
    const char* raw = ONavGetFurnitureTypesWithSexScenes(5, centerRefID, 3200.0f);
    if (!raw || raw[0] == '\0') return arr;

    try {
        auto j = nlohmann::json::parse(raw, nullptr, /*allow_exceptions=*/false);
        if (j.is_discarded() || !j.is_array()) return arr;

        std::unordered_set<std::string> seen;
        for (const auto& item : j) {
            if (!item.is_object()) continue;
            std::string id = SanitizeKey(item.value("id", ""));
            if (id.empty() || id == "floor" || id == "none") continue;
            if (!seen.insert(id).second) continue;

            std::string desc = id;
            if (item.contains("distance") && item["distance"].is_number()) {
                float dist = item["distance"].get<float>();
                if (dist >= 0.0f) {
                    float meters = dist * (1.8288f / 128.0f);
                    char buf[32];
                    std::snprintf(buf, sizeof(buf), "%.1f", meters);
                    desc += " (distance " + std::string(buf) + "m)";
                }
            }

            nlohmann::json entry;
            entry["id"]   = id;
            entry["desc"] = SanitizeSingleLine(desc, id);
            arr.push_back(std::move(entry));

            if (arr.size() >= 250) break;
        }
    } catch (...) {}

    return arr;
}

struct DecisionRetryCallback : RE::IMessageBoxCallback {
    std::function<void()> retry;
    std::function<void()> ignore;
    DecisionRetryCallback(std::function<void()> r, std::function<void()> i)
        : retry(std::move(r)), ignore(std::move(i)) {}
    void Run(std::uint8_t a_msg) override {
        if (static_cast<int>(a_msg) == 0) retry();
        else                               ignore();
    }
};

void ShowDecisionRetryModal(
    const std::string& evaluationType,
    const std::string& description,
    std::function<void()> onRetry,
    std::function<void()> onIgnore) {
    auto* taskIF = SKSE::GetTaskInterface();
    if (!taskIF) {
        SKSE::log::error("ShowDecisionRetryModal: no task interface, falling back to ignore");
        onIgnore();
        return;
    }
    taskIF->AddTask([evaluationType, description,
                     onRetry  = std::move(onRetry),
                     onIgnore = std::move(onIgnore)]() mutable {
        auto* factory = RE::MessageDataFactoryManager::GetSingleton();
        auto* ifStr   = RE::InterfaceStrings::GetSingleton();
        auto* creator = factory ? factory->GetCreator<RE::MessageBoxData>(ifStr->messageBoxData) : nullptr;
        if (!creator) {
            onIgnore();
            return;
        }
        auto* msgData = creator->Create();
        if (!msgData) {
            onIgnore();
            return;
        }
        std::string body =
            "OStimNet: Jev Decision request failed (" + evaluationType + ").\n" + description +
            "\n\nChoose Retry to try again, or Ignore to skip this evaluation.";
        msgData->bodyText = body.c_str();
        msgData->buttonText.push_back(RE::BSString("Retry"));
        msgData->buttonText.push_back(RE::BSString("Ignore"));
        msgData->cancelButtonIndex = 1;
        msgData->callback = RE::make_smart<DecisionRetryCallback>(std::move(onRetry), std::move(onIgnore));
        RE::MessageBoxMenu::QueueMessage(msgData);
    });
}

// Inspects a failed decision response (`success == 0`) to determine if we should
// automatically fall back to the regular Game Master LLM chat prompt.
bool ShouldFallbackToChatOnFailure(
    const char* resultJson,
    std::string& outErrorCode,
    std::string& outErrorMessage) {
    outErrorCode    = "unknown";
    outErrorMessage = "Decision request failed.";
    if (!resultJson || resultJson[0] == '\0') return false;

    auto j = nlohmann::json::parse(resultJson, nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded() || !j.is_object()) return false;

    outErrorCode    = j.value("error", "unknown");
    outErrorMessage = j.value("message", outErrorMessage);

    // Only fall back to the chat LLM path if the user has no decisions route configured.
    return outErrorCode == "no_decisions_route";
}

void RunFallbackOnGameThread(std::function<void()> fallbackToChat) {
    if (!fallbackToChat) return;
    if (auto* taskIF = SKSE::GetTaskInterface()) {
        taskIF->AddTask(std::move(fallbackToChat));
    } else {
        fallbackToChat();
    }
}

// Extracts the chosen key for a `choice` question from `result["answers"]`.
std::string GetChoiceAnswer(
    const nlohmann::json& answers,
    const std::string& questionId,
    const std::string& defaultKey = "") {
    if (!answers.is_object() || !answers.contains(questionId)) return defaultKey;
    const auto& q = answers[questionId];
    if (!q.is_object()) return defaultKey;
    return q.value("choice", defaultKey);
}

// Extracts the probability of a specific option key from `answers[questionId]["probabilities"]`.
double GetChoiceProbability(
    const nlohmann::json& answers,
    const std::string& questionId,
    const std::string& optionKey,
    double defaultProb = 0.0) {
    if (!answers.is_object() || !answers.contains(questionId)) return defaultProb;
    const auto& q = answers[questionId];
    if (!q.is_object() || !q.contains("probabilities")) return defaultProb;
    const auto& probs = q["probabilities"];
    if (!probs.is_object() || !probs.contains(optionKey)) return defaultProb;
    const auto& val = probs[optionKey];
    return val.is_number() ? val.get<double>() : defaultProb;
}

// Extracts the boolean answer (p >= 0.5) and raw probability for a `noul` question.
bool GetNoulAnswer(
    const nlohmann::json& answers,
    const std::string& questionId,
    bool defaultVal = false,
    double* outProb = nullptr) {
    if (!answers.is_object() || !answers.contains(questionId)) {
        if (outProb) *outProb = defaultVal ? 1.0 : 0.0;
        return defaultVal;
    }
    const auto& q = answers[questionId];
    if (!q.is_object() || !q.contains("noul") || !q["noul"].is_number()) {
        if (outProb) *outProb = defaultVal ? 1.0 : 0.0;
        return defaultVal;
    }
    double p = q["noul"].get<double>();
    if (outProb) *outProb = p;
    return p >= 0.5;
}

std::string NormalizeActivityChoice(const std::string& choice) {
    std::string key = SanitizeKey(choice);
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (key.empty() || key == "generic" || key == "hold" || key == "none" || key == "any") {
        return "";
    }
    return key;
}

std::string NormalizePositionChoice(const std::string& choice) {
    if (choice.empty() || choice == "any" || choice == "none") return "";
    std::string resolved;
    if (OStimNavigatorAPI::ResolvePosition(choice, resolved)) return resolved;
    return choice;
}

struct ActorRoleScore {
    uint32_t    formID     = 0;
    std::string chosenRole;  // "main" or "secondary"
    double      pMain      = 0.5;
    double      pSecondary = 0.5;
};

// Splits participating actors into `outMain` and `outSecondary` uint32 FormID arrays.
// If all actors chose the same bucket (e.g. all `main` or all `secondary`) and there are
// at least 2 actors, uses Jev's probability distribution to move the best candidate into
// the empty bucket so neither bucket is ever empty.
void BalanceMainAndSecondaryRoles(
    const std::vector<ActorRoleScore>& scores,
    nlohmann::json& outMain,
    nlohmann::json& outSecondary) {
    outMain      = nlohmann::json::array();
    outSecondary = nlohmann::json::array();
    if (scores.empty()) return;

    if (scores.size() == 1) {
        outMain.push_back(scores[0].formID);
        return;
    }

    std::vector<ActorRoleScore> mainBucket;
    std::vector<ActorRoleScore> secBucket;
    for (const auto& s : scores) {
        if (s.chosenRole == "secondary") secBucket.push_back(s);
        else                             mainBucket.push_back(s);
    }

    if (secBucket.empty() && mainBucket.size() > 1) {
        // Pick the actor in mainBucket with the highest relative secondary probability.
        // On a tie, prefer the last actor (matching OStimNet's existing guardrail).
        size_t bestIdx = mainBucket.size() - 1;
        double bestSecRatio = -1.0;
        for (size_t i = 0; i < mainBucket.size(); ++i) {
            double denom = mainBucket[i].pMain + mainBucket[i].pSecondary;
            double ratio = (denom > 1e-6) ? (mainBucket[i].pSecondary / denom) : 0.5;
            if (ratio >= bestSecRatio) {
                bestSecRatio = ratio;
                bestIdx      = i;
            }
        }
        secBucket.push_back(mainBucket[bestIdx]);
        mainBucket.erase(mainBucket.begin() + static_cast<std::ptrdiff_t>(bestIdx));
    } else if (mainBucket.empty() && secBucket.size() > 1) {
        // Pick the actor in secBucket with the highest relative main probability.
        size_t bestIdx = 0;
        double bestMainRatio = -1.0;
        for (size_t i = 0; i < secBucket.size(); ++i) {
            double denom = secBucket[i].pMain + secBucket[i].pSecondary;
            double ratio = (denom > 1e-6) ? (secBucket[i].pMain / denom) : 0.5;
            if (ratio > bestMainRatio) {
                bestMainRatio = ratio;
                bestIdx       = i;
            }
        }
        mainBucket.push_back(secBucket[bestIdx]);
        secBucket.erase(secBucket.begin() + static_cast<std::ptrdiff_t>(bestIdx));
    }

    for (const auto& m : mainBucket) outMain.push_back(m.formID);
    for (const auto& s : secBucket)  outSecondary.push_back(s.formID);
}

struct RoleActorItem {
    uint32_t    formID = 0;
    std::string name;
};

struct RoleCombination {
    std::string           key;
    std::vector<uint32_t> mainFormIDs;
    std::vector<uint32_t> secondaryFormIDs;
    std::string           mainNames;
    std::string           secondaryNames;
};

// Generates all non-empty (main, secondary) partitions for N >= 2 actors,
// ordered by number of main actors (1 main actor first, then 2, ..., up to N-1).
std::vector<RoleCombination> BuildRoleCombinations(
    const std::vector<RoleActorItem>& actors,
    nlohmann::json& outJsonArray) {
    outJsonArray = nlohmann::json::array();
    std::vector<RoleCombination> result;
    const size_t n = actors.size();
    if (n < 2 || n > 6) return result;

    const uint32_t totalMasks = (1u << n);
    size_t comboIdx = 1;

    for (size_t mainCount = 1; mainCount < n; ++mainCount) {
        for (uint32_t mask = 1; mask + 1 < totalMasks; ++mask) {
            size_t bits = 0;
            for (size_t i = 0; i < n; ++i) {
                if (mask & (1u << i)) ++bits;
            }
            if (bits != mainCount) continue;

            RoleCombination combo;
            combo.key = "c" + std::to_string(comboIdx++);

            for (size_t i = 0; i < n; ++i) {
                if (mask & (1u << i)) {
                    combo.mainFormIDs.push_back(actors[i].formID);
                    if (!combo.mainNames.empty()) combo.mainNames += ", ";
                    combo.mainNames += actors[i].name;
                } else {
                    combo.secondaryFormIDs.push_back(actors[i].formID);
                    if (!combo.secondaryNames.empty()) combo.secondaryNames += ", ";
                    combo.secondaryNames += actors[i].name;
                }
            }

            nlohmann::json jObj;
            jObj["key"]            = combo.key;
            jObj["mainNames"]      = combo.mainNames;
            jObj["secondaryNames"] = combo.secondaryNames;
            outJsonArray.push_back(std::move(jObj));
            result.push_back(std::move(combo));
        }
    }
    return result;
}

}  // namespace

// =============================================================================
// Availability check
// =============================================================================

bool IsAvailable() {
    return PublicSendCustomDecisionToLLM != nullptr &&
           OStimNet::Config::GetSingleton().UseDecisions();
}

// =============================================================================
// 1. EvaluatePreStartSexualScene
// =============================================================================

bool EvaluatePreStartSexualScene(
    const std::vector<RE::FormID>& participantFormIDs,
    const std::string& evalId,
    const std::string& reason,
    std::function<void()> fallbackToChat) {
    if (!IsAvailable()) return false;

    struct ParticipantSnap {
        std::string key;
        RE::FormID  formID   = 0;
        uint64_t    uuid     = 0;
        std::string name;
        bool        isPlayer = false;
    };

    std::vector<ParticipantSnap> snaps;
    std::vector<std::string>     actorNameVec;
    nlohmann::json participantsList     = nlohmann::json::array();
    nlohmann::json eventFilterUUIDs     = nlohmann::json::array();
    nlohmann::json originalParticipants = nlohmann::json::array();
    RE::Actor* centerActor              = nullptr;

    for (size_t i = 0; i < participantFormIDs.size(); ++i) {
        RE::FormID fid = participantFormIDs[i];
        originalParticipants.push_back(static_cast<uint32_t>(fid));

        auto* actor = RE::TESForm::LookupByID<RE::Actor>(fid);
        std::string displayName = actor
            ? SanitizeSingleLine(ThreadDataStore::GetActorDisplayName(actor, "Actor"), "Actor")
            : "Actor";
        uint64_t uuid = ActorToUUID(actor);
        if (uuid == 0) continue;

        if (!centerActor) centerActor = actor;
        actorNameVec.push_back(displayName);

        ParticipantSnap snap;
        snap.key      = "p_" + std::to_string(snaps.size() + 1);
        snap.formID   = fid;
        snap.uuid     = uuid;
        snap.name     = displayName;
        snap.isPlayer = IsPlayerFormID(fid);
        snaps.push_back(snap);

        nlohmann::json pObj;
        pObj["key"]      = snap.key;
        pObj["uuid"]     = snap.uuid;
        pObj["name"]     = snap.name;
        pObj["isPlayer"] = snap.isPlayer;
        participantsList.push_back(std::move(pObj));
        eventFilterUUIDs.push_back(snap.uuid);
    }

    if (snaps.empty()) {
        SKSE::log::warn("SkyrimNetDecisions::EvaluatePreStartSexualScene: no valid actor UUIDs resolved");
        return false;
    }

    struct PreStartRoleCombo {
        std::string           key;
        uint32_t              activeMask = 0;
        std::vector<uint32_t> mainFormIDs;
        std::vector<uint32_t> secondaryFormIDs;
        std::string           mainNames;
        std::string           secondaryNames;
        std::string           excludedNames;
    };

    std::vector<PreStartRoleCombo> preStartCombos;
    nlohmann::json roleCombinationsJson = nlohmann::json::array();

    const size_t n            = snaps.size();
    const uint32_t fullMask   = (1u << n) - 1u;
    size_t comboIdx           = 1;

    if (n == 1) {
        PreStartRoleCombo pc;
        pc.key         = "c_1";
        pc.activeMask  = 1u;
        pc.mainFormIDs = {static_cast<uint32_t>(snaps[0].formID)};
        pc.mainNames   = snaps[0].name;

        nlohmann::json jObj;
        jObj["key"]            = pc.key;
        jObj["mainNames"]      = pc.mainNames;
        jObj["secondaryNames"] = "";
        jObj["excludedNames"]  = "";
        roleCombinationsJson.push_back(std::move(jObj));
        preStartCombos.push_back(std::move(pc));
    } else {
        for (size_t activeCount = n; activeCount >= 2; --activeCount) {
            if (n >= 5 && activeCount < n - 1 && activeCount != 2) continue;

            for (uint32_t mask = 1; mask <= fullMask; ++mask) {
                size_t bits = 0;
                bool playerExcluded = false;
                for (size_t i = 0; i < n; ++i) {
                    if (mask & (1u << i)) {
                        ++bits;
                    } else if (snaps[i].isPlayer) {
                        playerExcluded = true;
                    }
                }
                if (bits != activeCount || playerExcluded) continue;

                std::vector<RoleActorItem> activeActors;
                std::string excludedNames;
                for (size_t i = 0; i < n; ++i) {
                    if (mask & (1u << i)) {
                        activeActors.push_back(RoleActorItem{static_cast<uint32_t>(snaps[i].formID), snaps[i].name});
                    } else {
                        if (!excludedNames.empty()) excludedNames += ", ";
                        excludedNames += snaps[i].name;
                    }
                }

                nlohmann::json subJson;
                auto subCombos = BuildRoleCombinations(activeActors, subJson);
                for (auto& sc : subCombos) {
                    PreStartRoleCombo pc;
                    pc.key              = "c_" + std::to_string(comboIdx++);
                    pc.activeMask       = mask;
                    pc.mainFormIDs      = std::move(sc.mainFormIDs);
                    pc.secondaryFormIDs = std::move(sc.secondaryFormIDs);
                    pc.mainNames        = std::move(sc.mainNames);
                    pc.secondaryNames   = std::move(sc.secondaryNames);
                    pc.excludedNames    = excludedNames;

                    nlohmann::json jObj;
                    jObj["key"]            = pc.key;
                    jObj["mainNames"]      = pc.mainNames;
                    jObj["secondaryNames"] = pc.secondaryNames;
                    jObj["excludedNames"]  = pc.excludedNames;
                    roleCombinationsJson.push_back(std::move(jObj));
                    preStartCombos.push_back(std::move(pc));
                }
            }
        }
    }

    std::vector<uint32_t> participantFIDs;
    participantFIDs.reserve(snaps.size());
    bool hasPlayer = false;
    for (const auto& s : snaps) {
        participantFIDs.push_back(static_cast<uint32_t>(s.formID));
        if (s.isPlayer) hasPlayer = true;
    }

    nlohmann::json baseCtx;
    baseCtx["participantsList"] = std::move(participantsList);
    baseCtx["eventFilterUUIDs"] = std::move(eventFilterUUIDs);
    baseCtx["actorNames"]       = OStimNet::FormatActorList(actorNameVec);
    baseCtx["furnitureList"]    = BuildFurnitureArrayJson(centerActor);
    baseCtx["roleCombinations"] = std::move(roleCombinationsJson);
    baseCtx["reason"]           = reason;
    baseCtx["step"]             = 1;

    std::string step1ContextJson = baseCtx.dump();
    std::string templateName     = "ostimnet_evaluate_prestart_sexual";

    SKSE::log::info("SkyrimNetDecisions::EvaluatePreStartSexualScene: queuing Step 1 (intent + furniture), participants={}",
                    snaps.size());

    auto step1CallbackHolder = std::make_shared<std::function<void(const char*, const char*, int)>>();
    *step1CallbackHolder = [step1CallbackHolder, snaps, participantFIDs, hasPlayer, preStartCombos, originalParticipants, evalId,
                            baseCtx, step1ContextJson, templateName, fallbackToChat](
                               const char* answer, const char* resultJson, int success) {
        SKSE::log::info("SkyrimNetDecisions::EvaluatePreStartSexualScene Step 1 callback: success={}, firstAnswer='{}'",
                        success, answer ? answer : "");

        auto retryStep1 = [step1CallbackHolder, templateName, step1ContextJson]() {
            if (PublicSendCustomDecisionToLLM) {
                PublicSendCustomDecisionToLLM(
                    templateName.c_str(), step1ContextJson.c_str(),
                    kDefaultMaxProbabilities, *step1CallbackHolder);
            }
        };
        auto ignoreAction = [originalParticipants, evalId]() {
            nlohmann::json ignoreResult;
            ignoreResult["originalParticipants"] = originalParticipants;
            ignoreResult["evalId"]               = evalId;
            OStimNet::FireModEvent("ostimnet_sexual_evaluation_finished", ignoreResult.dump().c_str(), 0.0f);
        };

        if (!success || !resultJson || resultJson[0] == '\0') {
            std::string errCode, errMsg;
            if (ShouldFallbackToChatOnFailure(resultJson, errCode, errMsg) && fallbackToChat) {
                SKSE::log::warn("SkyrimNetDecisions::EvaluatePreStartSexualScene (Step 1): Jev error '{}' ({}) — falling back to GM chat LLM",
                                errCode, errMsg);
                RunFallbackOnGameThread(fallbackToChat);
                return;
            }
            SKSE::log::warn("SkyrimNetDecisions::EvaluatePreStartSexualScene (Step 1): Jev decision failed ({}: {})",
                            errCode, errMsg);
            ShowDecisionRetryModal("Pre-Start Sexual Scene (Step 1: Intent & Furniture)", errMsg, retryStep1, ignoreAction);
            return;
        }

        auto parsed = nlohmann::json::parse(resultJson, nullptr, /*allow_exceptions=*/false);
        if (parsed.is_discarded() || !parsed.contains("answers") || !parsed["answers"].is_object()) {
            SKSE::log::warn("SkyrimNetDecisions::EvaluatePreStartSexualScene (Step 1): invalid resultJson: {}", resultJson);
            ShowDecisionRetryModal("Pre-Start Sexual Scene (Step 1: Intent & Furniture)", "Unreadable decision response.", retryStep1, ignoreAction);
            return;
        }

        const auto& answers1 = parsed["answers"];
        std::string resolvedIntent    = GetChoiceAnswer(answers1, "intent", "lustful");
        std::string resolvedFurniture = GetChoiceAnswer(answers1, "furniture", "floor");

        OStimNavigatorAPI::Intent intentEnum = OStimNavigatorAPI::IntentFromString(resolvedIntent);
        std::optional<OStimNet::SexualPhase> openingPhase = OStimNet::ResolvePreStartOpeningPhase(
            intentEnum, snaps.size(), hasPlayer, resolvedFurniture);

        nlohmann::json activitiesJson = nlohmann::json::array();
        nlohmann::json positionsJson  = nlohmann::json::array();
        std::string openingPhaseStr;

        if (!openingPhase.has_value()) {
            // Phases disabled: offer all compatible sexual activities and intercourse positions.
            openingPhaseStr = "";
            activitiesJson  = BuildSetupActivitiesArrayJson(participantFIDs, resolvedFurniture, "sexual");
            positionsJson   = BuildSetupPositionsArrayJson(participantFIDs, resolvedFurniture);
        } else if (*openingPhase == OStimNet::SexualPhase::Undressing) {
            // Undressing phase: neither activity nor position is needed.
            openingPhaseStr = "undressing";
        } else {
            // Foreplay, Oral, or Sex phase: walk forward in IntentToPhaseList if a furniture piece
            // has no scenes for the earlier phase.
            const auto& phaseList = OStimNet::IntentToPhaseList(intentEnum);
            OStimNet::SexualPhase effectivePhase = *openingPhase;
            bool foundStart = false;
            for (size_t idx = 0; idx < phaseList.size(); ++idx) {
                if (phaseList[idx] == *openingPhase) foundStart = true;
                if (!foundStart) continue;
                effectivePhase = phaseList[idx];
                activitiesJson = BuildSetupActivitiesArrayJson(
                    participantFIDs, resolvedFurniture, OStimNet::SexualPhaseToName(effectivePhase));
                if (!activitiesJson.empty()) break;
            }
            openingPhaseStr = OStimNet::SexualPhaseToName(effectivePhase);
            if (effectivePhase == OStimNet::SexualPhase::Sex) {
                positionsJson = BuildSetupPositionsArrayJson(participantFIDs, resolvedFurniture);
            }
        }

        SKSE::log::info("SkyrimNetDecisions::EvaluatePreStartSexualScene Step 1 resolved intent='{}', furniture='{}', openingPhase='{}', activities={}, positions={}, queuing Step 2",
                        resolvedIntent, resolvedFurniture, openingPhaseStr, activitiesJson.size(), positionsJson.size());

        nlohmann::json step2Ctx = baseCtx;
        step2Ctx["step"]         = 2;
        step2Ctx["intent"]       = resolvedIntent;
        step2Ctx["furniture"]    = resolvedFurniture;
        step2Ctx["openingPhase"] = openingPhaseStr;
        step2Ctx["activities"]   = std::move(activitiesJson);
        step2Ctx["positions"]    = std::move(positionsJson);
        std::string step2ContextJson = step2Ctx.dump();

        auto step2CallbackHolder = std::make_shared<std::function<void(const char*, const char*, int)>>();
        *step2CallbackHolder = [step2CallbackHolder, snaps, participantFIDs, preStartCombos, originalParticipants,
                                resolvedIntent, resolvedFurniture, openingPhaseStr, step2Ctx, evalId, step2ContextJson, templateName, fallbackToChat](
                                   const char* step2Answer, const char* step2ResultJson, int step2Success) {
            SKSE::log::info("SkyrimNetDecisions::EvaluatePreStartSexualScene Step 2 callback: success={}, firstAnswer='{}'",
                            step2Success, step2Answer ? step2Answer : "");

            auto retryStep2 = [step2CallbackHolder, templateName, step2ContextJson]() {
                if (PublicSendCustomDecisionToLLM) {
                    PublicSendCustomDecisionToLLM(
                        templateName.c_str(), step2ContextJson.c_str(),
                        kDefaultMaxProbabilities, *step2CallbackHolder);
                }
            };
            auto ignoreStep2 = [originalParticipants, evalId]() {
                nlohmann::json ignoreResult;
                ignoreResult["originalParticipants"] = originalParticipants;
                ignoreResult["evalId"]               = evalId;
                OStimNet::FireModEvent("ostimnet_sexual_evaluation_finished", ignoreResult.dump().c_str(), 0.0f);
            };

            if (!step2Success || !step2ResultJson || step2ResultJson[0] == '\0') {
                std::string errCode, errMsg;
                if (ShouldFallbackToChatOnFailure(step2ResultJson, errCode, errMsg) && fallbackToChat) {
                    SKSE::log::warn("SkyrimNetDecisions::EvaluatePreStartSexualScene (Step 2): Jev error '{}' ({}) — falling back to GM chat LLM",
                                    errCode, errMsg);
                    RunFallbackOnGameThread(fallbackToChat);
                    return;
                }
                SKSE::log::warn("SkyrimNetDecisions::EvaluatePreStartSexualScene (Step 2): Jev decision failed ({}: {})",
                                errCode, errMsg);
                ShowDecisionRetryModal("Pre-Start Sexual Scene (Step 2: Roles & Details)", errMsg, retryStep2, ignoreStep2);
                return;
            }

            auto parsed2 = nlohmann::json::parse(step2ResultJson, nullptr, /*allow_exceptions=*/false);
            if (parsed2.is_discarded() || !parsed2.contains("answers") || !parsed2["answers"].is_object()) {
                SKSE::log::warn("SkyrimNetDecisions::EvaluatePreStartSexualScene (Step 2): invalid resultJson: {}", step2ResultJson);
                ShowDecisionRetryModal("Pre-Start Sexual Scene (Step 2: Roles & Details)", "Unreadable decision response.", retryStep2, ignoreStep2);
                return;
            }

            const auto& answers2 = parsed2["answers"];
            uint32_t willingMask = 0;
            size_t   willingCount = 0;
            nlohmann::json excluded = nlohmann::json::array();

            for (size_t i = 0; i < snaps.size(); ++i) {
                bool willing = snaps[i].isPlayer
                    ? true
                    : GetNoulAnswer(answers2, "willing_" + snaps[i].key, true);
                if (willing) {
                    willingMask |= (1u << i);
                    ++willingCount;
                } else {
                    excluded.push_back(static_cast<uint32_t>(snaps[i].formID));
                }
            }

            if ((snaps.size() > 1 && willingCount <= 1) || willingCount == 0) {
                SKSE::log::info("SkyrimNetDecisions::EvaluatePreStartSexualScene: only {} willing actor(s), aborting scene.",
                                willingCount);
                nlohmann::json nullResult;
                nullResult["originalParticipants"] = originalParticipants;
                nullResult["evalId"]               = evalId;
                nullResult["excluded"]             = excluded;
                OStimNet::FireModEvent("ostimnet_sexual_evaluation_finished", nullResult.dump().c_str(), 2.0f);
                return;
            }

            std::string chosenRoleKey = GetChoiceAnswer(answers2, "roles", preStartCombos[0].key);
            const PreStartRoleCombo* selectedCombo = nullptr;
            double bestProb = -1.0;

            for (const auto& c : preStartCombos) {
                if (c.activeMask != willingMask) continue;
                if (c.key == chosenRoleKey) {
                    selectedCombo = &c;
                    break;
                }
                double p = GetChoiceProbability(answers2, "roles", c.key, 0.0);
                if (p > bestProb) {
                    bestProb      = p;
                    selectedCombo = &c;
                }
            }
            if (!selectedCombo) {
                for (const auto& c : preStartCombos) {
                    if (c.key == chosenRoleKey) {
                        selectedCombo = &c;
                        break;
                    }
                }
            }
            if (!selectedCombo) selectedCombo = &preStartCombos[0];

            nlohmann::json mainArr      = nlohmann::json::array();
            nlohmann::json secondaryArr = nlohmann::json::array();
            for (uint32_t fid : selectedCombo->mainFormIDs)      mainArr.push_back(fid);
            for (uint32_t fid : selectedCombo->secondaryFormIDs) secondaryArr.push_back(fid);

            bool askedActivity = step2Ctx.contains("activities") && step2Ctx["activities"].is_array() && !step2Ctx["activities"].empty();
            bool askedPosition = step2Ctx.contains("positions") && step2Ctx["positions"].is_array() && !step2Ctx["positions"].empty();

            std::string rawAct  = askedActivity ? GetChoiceAnswer(answers2, "activity", "any") : "";
            std::string rawPos  = askedPosition ? GetChoiceAnswer(answers2, "position", "any") : "";
            std::string normAct = NormalizeActivityChoice(rawAct);
            std::string normPos = NormalizePositionChoice(rawPos);

            // When phases are disabled, if the AI selected a non-intercourse activity (e.g. foreplay or oral),
            // ignore the selected intercourse position.
            if (openingPhaseStr.empty() && !normAct.empty() && !normPos.empty()) {
                auto sexActs = BuildSetupActivitiesArrayJson(participantFIDs, resolvedFurniture, "sex");
                bool isSexAct = false;
                for (const auto& item : sexActs) {
                    if (item.is_object() && item.value("id", "") == normAct) {
                        isSexAct = true;
                        break;
                    }
                }
                if (!isSexAct) {
                    normPos.clear();
                }
            }

            nlohmann::json result;
            result["originalParticipants"] = originalParticipants;
            result["intent"]               = resolvedIntent;
            result["furniture"]            = (resolvedFurniture == "floor") ? "" : resolvedFurniture;
            result["evalId"]               = evalId;
            result["sexualPosition"]       = normPos;
            result["sexualActivities"]     = normAct;
            result["main"]                 = std::move(mainArr);
            result["secondary"]            = std::move(secondaryArr);
            result["excluded"]             = excluded;

            bool actorsExcluded = (willingCount < snaps.size());
            std::string outStr  = result.dump();
            SKSE::log::info("SkyrimNetDecisions::EvaluatePreStartSexualScene: resolved actors={}, excluded={}, result={}",
                            willingCount, actorsExcluded, outStr);

            float numArg = actorsExcluded ? 3.0f : 1.0f;
            OStimNet::FireModEvent("ostimnet_sexual_evaluation_finished", outStr.c_str(), numArg);
        };

        bool queued2 = PublicSendCustomDecisionToLLM(
            templateName.c_str(), step2ContextJson.c_str(), kDefaultMaxProbabilities, *step2CallbackHolder);
        if (!queued2 && fallbackToChat) {
            RunFallbackOnGameThread(fallbackToChat);
        }
    };

    bool queued = PublicSendCustomDecisionToLLM(
        templateName.c_str(), step1ContextJson.c_str(), kDefaultMaxProbabilities, *step1CallbackHolder);
    if (!queued && fallbackToChat) {
        SKSE::log::warn("SkyrimNetDecisions::EvaluatePreStartSexualScene: PublicSendCustomDecisionToLLM returned false, falling back to chat");
        fallbackToChat();
        return true;
    }
    return queued;
}

// =============================================================================
// 2. EvaluateExternalSexualThread
// =============================================================================

bool EvaluateExternalSexualThread(
    const std::vector<RE::FormID>& participantFormIDs,
    int threadID,
    std::function<void()> fallbackToChat) {
    if (!IsAvailable()) return false;

    std::vector<RoleActorItem> roleActors;
    std::vector<std::string>   actorNameVec;
    nlohmann::json             actorsJsonArray = nlohmann::json::array();

    for (RE::FormID fid : participantFormIDs) {
        auto* actor = RE::TESForm::LookupByID<RE::Actor>(fid);
        std::string displayName = actor
            ? SanitizeSingleLine(ThreadDataStore::GetActorDisplayName(actor, "Actor"), "Actor")
            : "Actor";
        uint64_t uuid = ActorToUUID(actor);
        if (uuid == 0) continue;

        roleActors.push_back(RoleActorItem{static_cast<uint32_t>(fid), displayName});
        actorNameVec.push_back(displayName);
        actorsJsonArray.push_back(uuid);
    }

    if (roleActors.empty()) return false;

    std::string actorNames       = OStimNet::FormatActorList(actorNameVec);
    std::string sceneDescription = OStimNet::GetSceneDescription(static_cast<uint32_t>(threadID));
    std::string templateName     = "ostimnet_evaluate_external_sexual_thread";

    nlohmann::json step1Ctx;
    step1Ctx["step"]             = 1;
    step1Ctx["actors"]           = actorsJsonArray;
    step1Ctx["actorNames"]       = actorNames;
    step1Ctx["sceneDescription"] = sceneDescription;
    step1Ctx["threadID"]         = threadID;

    std::string step1ContextJson = step1Ctx.dump();

    SKSE::log::info("SkyrimNetDecisions::EvaluateExternalSexualThread: queuing Step 1 (intent), threadID={}, participants={}",
                    threadID, roleActors.size());

    auto step1CallbackHolder = std::make_shared<std::function<void(const char*, const char*, int)>>();
    *step1CallbackHolder = [step1CallbackHolder, roleActors, actorsJsonArray, actorNames,
                            sceneDescription, threadID, step1ContextJson, templateName, fallbackToChat](
                               const char* answer, const char* resultJson, int success) {
        SKSE::log::info("SkyrimNetDecisions::EvaluateExternalSexualThread Step 1 callback: success={}, firstAnswer='{}'",
                        success, answer ? answer : "");

        auto retryStep1 = [step1CallbackHolder, templateName, step1ContextJson]() {
            if (PublicSendCustomDecisionToLLM) {
                PublicSendCustomDecisionToLLM(
                    templateName.c_str(), step1ContextJson.c_str(),
                    kDefaultMaxProbabilities, *step1CallbackHolder);
            }
        };
        auto ignoreAction = []() {
            SKSE::log::info("SkyrimNetDecisions::EvaluateExternalSexualThread: player chose to ignore failed evaluation.");
        };

        if (!success || !resultJson || resultJson[0] == '\0') {
            std::string errCode, errMsg;
            if (ShouldFallbackToChatOnFailure(resultJson, errCode, errMsg) && fallbackToChat) {
                SKSE::log::warn("SkyrimNetDecisions::EvaluateExternalSexualThread Step 1: Jev error '{}' ({}) — falling back to GM chat LLM",
                                errCode, errMsg);
                RunFallbackOnGameThread(fallbackToChat);
                return;
            }
            ShowDecisionRetryModal("External Sexual Thread (Step 1: Intent)", errMsg, retryStep1, ignoreAction);
            return;
        }

        auto parsed1 = nlohmann::json::parse(resultJson, nullptr, /*allow_exceptions=*/false);
        if (parsed1.is_discarded() || !parsed1.contains("answers") || !parsed1["answers"].is_object()) {
            ShowDecisionRetryModal("External Sexual Thread (Step 1: Intent)", "Unreadable decision response.", retryStep1, ignoreAction);
            return;
        }

        std::string effectiveIntent = GetChoiceAnswer(parsed1["answers"], "intent", "lustful");
        SKSE::log::info("SkyrimNetDecisions::EvaluateExternalSexualThread Step 1: resolved intent='{}' for thread {}",
                        effectiveIntent, threadID);

        auto claimOnGameThread = [threadID, effectiveIntent](
                                     std::vector<uint32_t> mainFIDs,
                                     std::vector<uint32_t> secFIDs) {
            if (auto* taskIF = SKSE::GetTaskInterface()) {
                taskIF->AddTask([threadID, effectiveIntent, mainFIDs = std::move(mainFIDs), secFIDs = std::move(secFIDs)]() {
                    auto& store = OStimNet::ThreadDataStore::GetSingleton();
                    if (!store.IsRegisteredAndPending(threadID)) {
                        SKSE::log::info("SkyrimNetDecisions::EvaluateExternalSexualThread: thread {} no longer pending, discarding.", threadID);
                        return;
                    }
                    if (g_ostimThreadInterface && !g_ostimThreadInterface->IsThreadValid(static_cast<uint32_t>(threadID))) {
                        SKSE::log::warn("SkyrimNetDecisions::EvaluateExternalSexualThread: thread {} is no longer valid, discarding.", threadID);
                        store.ClearThread(threadID);
                        return;
                    }

                    store.SetOStimNet(threadID, true);
                    store.SetIntent(threadID, OStimNet::IntentFromString(effectiveIntent));
                    store.SetSexual(threadID, true);

                    std::vector<RE::Actor*> mainPtrs, secPtrs;
                    for (uint32_t fid : mainFIDs) {
                        if (auto* a = RE::TESForm::LookupByID<RE::Actor>(fid)) mainPtrs.push_back(a);
                    }
                    for (uint32_t fid : secFIDs) {
                        if (auto* a = RE::TESForm::LookupByID<RE::Actor>(fid)) secPtrs.push_back(a);
                    }

                    store.SetMainActors(threadID, std::move(mainPtrs));
                    store.SetSecondaryActors(threadID, std::move(secPtrs));

                    if (auto* listener = OStimNet::OStimEventListener::GetInstance()) {
                        listener->ClaimPendingNonOStimNetThread(threadID);
                    }
                });
            }
        };

        // Solo scene: no Step 2 needed
        if (roleActors.size() < 2) {
            claimOnGameThread({roleActors[0].formID}, {});
            return;
        }

        // Step 2: Build all non-empty (main, secondary) combinations for the participating actors
        nlohmann::json roleCombinationsJson;
        auto combos = BuildRoleCombinations(roleActors, roleCombinationsJson);

        nlohmann::json step2Ctx;
        step2Ctx["step"]             = 2;
        step2Ctx["actors"]           = actorsJsonArray;
        step2Ctx["actorNames"]       = actorNames;
        step2Ctx["sceneDescription"] = sceneDescription;
        step2Ctx["intent"]           = effectiveIntent;
        step2Ctx["roleCombinations"] = std::move(roleCombinationsJson);
        step2Ctx["threadID"]         = threadID;

        std::string step2ContextJson = step2Ctx.dump();

        SKSE::log::info("SkyrimNetDecisions::EvaluateExternalSexualThread: queuing Step 2 (roles), threadID={}, intent='{}', combinations={}",
                        threadID, effectiveIntent, combos.size());

        auto step2CallbackHolder = std::make_shared<std::function<void(const char*, const char*, int)>>();
        *step2CallbackHolder = [step2CallbackHolder, combos, claimOnGameThread,
                                step2ContextJson, templateName, ignoreAction](
                                   const char* answer2, const char* resultJson2, int success2) {
            SKSE::log::info("SkyrimNetDecisions::EvaluateExternalSexualThread Step 2 callback: success={}, firstAnswer='{}'",
                            success2, answer2 ? answer2 : "");

            auto retryStep2 = [step2CallbackHolder, templateName, step2ContextJson]() {
                if (PublicSendCustomDecisionToLLM) {
                    PublicSendCustomDecisionToLLM(
                        templateName.c_str(), step2ContextJson.c_str(),
                        kDefaultMaxProbabilities, *step2CallbackHolder);
                }
            };

            if (!success2 || !resultJson2 || resultJson2[0] == '\0') {
                std::string errCode, errMsg;
                ShouldFallbackToChatOnFailure(resultJson2, errCode, errMsg);
                ShowDecisionRetryModal("External Sexual Thread (Step 2: Roles)", errMsg, retryStep2, ignoreAction);
                return;
            }

            auto parsed2 = nlohmann::json::parse(resultJson2, nullptr, /*allow_exceptions=*/false);
            if (parsed2.is_discarded() || !parsed2.contains("answers") || !parsed2["answers"].is_object()) {
                ShowDecisionRetryModal("External Sexual Thread (Step 2: Roles)", "Unreadable decision response.", retryStep2, ignoreAction);
                return;
            }

            std::string chosenKey = GetChoiceAnswer(parsed2["answers"], "roles", combos[0].key);
            const RoleCombination* selectedCombo = &combos[0];
            for (const auto& c : combos) {
                if (c.key == chosenKey) {
                    selectedCombo = &c;
                    break;
                }
            }

            SKSE::log::info("SkyrimNetDecisions::EvaluateExternalSexualThread Step 2: selected '{}' (main: {}; secondary: {})",
                            selectedCombo->key, selectedCombo->mainNames, selectedCombo->secondaryNames);
            claimOnGameThread(selectedCombo->mainFormIDs, selectedCombo->secondaryFormIDs);
        };

        PublicSendCustomDecisionToLLM(
            templateName.c_str(), step2ContextJson.c_str(), kDefaultMaxProbabilities, *step2CallbackHolder);
    };

    bool queued = PublicSendCustomDecisionToLLM(
        templateName.c_str(), step1ContextJson.c_str(), kDefaultMaxProbabilities, *step1CallbackHolder);
    if (!queued && fallbackToChat) {
        fallbackToChat();
        return true;
    }
    return queued;
}

// =============================================================================
// 3. EvaluateNonSexualScene
// =============================================================================

bool EvaluateNonSexualScene(
    const std::vector<RE::FormID>& participantFormIDs,
    const std::string& activity,
    const std::string& intent,
    RE::FormID initiatorFormID,
    const std::string& evalId,
    const std::string& reason,
    std::function<void()> fallbackToChat) {
    if (!IsAvailable()) return false;

    struct NonSexParty {
        std::string key;
        RE::FormID  formID;
        uint64_t    uuid;
        std::string name;
        bool        isInitiator;
        bool        isPlayer;
    };

    std::vector<NonSexParty> parties;
    nlohmann::json eventFilterUUIDs = nlohmann::json::array();
    nlohmann::json origParticipants = nlohmann::json::array();
    std::vector<std::string> actorNameVec;
    std::string initiatorName;

    for (size_t i = 0; i < participantFormIDs.size(); ++i) {
        RE::FormID fid = participantFormIDs[i];
        origParticipants.push_back(static_cast<uint32_t>(fid));

        auto* actor = RE::TESForm::LookupByID<RE::Actor>(fid);
        std::string displayName = actor
            ? SanitizeSingleLine(ThreadDataStore::GetActorDisplayName(actor, "Actor"), "Actor")
            : "Actor";
        uint64_t uuid = ActorToUUID(actor);
        if (uuid == 0) continue;

        actorNameVec.push_back(displayName);
        if (fid == initiatorFormID) initiatorName = displayName;

        NonSexParty party;
        party.key         = "p_" + std::to_string(parties.size() + 1);
        party.formID      = fid;
        party.uuid        = uuid;
        party.name        = displayName;
        party.isInitiator = (fid == initiatorFormID);
        party.isPlayer    = IsPlayerFormID(fid);
        parties.push_back(std::move(party));
        eventFilterUUIDs.push_back(uuid);
    }

    if (parties.empty()) return false;

    nlohmann::json participantsList = nlohmann::json::array();
    for (size_t i = 0; i < parties.size(); ++i) {
        std::string partnerName = "the other participant";
        if (parties.size() == 2) {
            partnerName = parties[1 - i].name;
        }
        nlohmann::json pObj;
        pObj["key"]         = parties[i].key;
        pObj["uuid"]        = parties[i].uuid;
        pObj["name"]        = parties[i].name;
        pObj["partnerName"] = partnerName;
        pObj["isInitiator"] = parties[i].isInitiator;
        pObj["isPlayer"]    = parties[i].isPlayer;
        participantsList.push_back(std::move(pObj));
    }

    nlohmann::json baseCtx;
    baseCtx["participantsList"] = std::move(participantsList);
    baseCtx["eventFilterUUIDs"] = std::move(eventFilterUUIDs);
    baseCtx["actorNames"]       = OStimNet::FormatActorList(actorNameVec);
    baseCtx["initiatorName"]    = initiatorName;
    baseCtx["activity"]         = activity;
    baseCtx["intent"]           = intent;
    baseCtx["reason"]           = reason;
    baseCtx["step"]             = 1;

    std::string step1ContextJson = baseCtx.dump();
    std::string templateName     = "ostimnet_evaluate_nonsexual_scene";

    SKSE::log::info("SkyrimNetDecisions::EvaluateNonSexualScene: queuing Step 1 (intent), participants={}",
                    participantFormIDs.size());

    auto step1CallbackHolder = std::make_shared<std::function<void(const char*, const char*, int)>>();
    *step1CallbackHolder = [step1CallbackHolder, parties, participantFormIDs, origParticipants, initiatorFormID,
                            intent, activity, evalId, baseCtx, step1ContextJson, templateName, fallbackToChat](
                               const char* answer, const char* resultJson, int success) {
        SKSE::log::info("SkyrimNetDecisions::EvaluateNonSexualScene Step 1 callback: success={}, firstAnswer='{}'",
                        success, answer ? answer : "");

        auto retryStep1 = [step1CallbackHolder, templateName, step1ContextJson]() {
            if (PublicSendCustomDecisionToLLM) {
                PublicSendCustomDecisionToLLM(
                    templateName.c_str(), step1ContextJson.c_str(),
                    kDefaultMaxProbabilities, *step1CallbackHolder);
            }
        };
        auto ignoreAction = [origParticipants, initiatorFormID, intent, activity, evalId]() {
            nlohmann::json ignoreResult;
            ignoreResult["originalParticipants"] = origParticipants;
            ignoreResult["initiator"]            = static_cast<uint32_t>(initiatorFormID);
            ignoreResult["intent"]               = intent;
            ignoreResult["evalId"]               = evalId;
            ignoreResult["activity"]             = activity;
            OStimNet::FireModEvent("ostimnet_nonsexual_evaluation_finished", ignoreResult.dump().c_str(), 0.0f);
        };

        if (!success || !resultJson || resultJson[0] == '\0') {
            std::string errCode, errMsg;
            if (ShouldFallbackToChatOnFailure(resultJson, errCode, errMsg) && fallbackToChat) {
                SKSE::log::warn("SkyrimNetDecisions::EvaluateNonSexualScene (Step 1): Jev error '{}' ({}) — falling back to GM chat LLM",
                                errCode, errMsg);
                RunFallbackOnGameThread(fallbackToChat);
                return;
            }
            ShowDecisionRetryModal("Non-Sexual Scene (Step 1: Intent)", errMsg, retryStep1, ignoreAction);
            return;
        }

        auto parsed = nlohmann::json::parse(resultJson, nullptr, /*allow_exceptions=*/false);
        if (parsed.is_discarded() || !parsed.contains("answers") || !parsed["answers"].is_object()) {
            ShowDecisionRetryModal("Non-Sexual Scene (Step 1: Intent)", "Unreadable decision response.", retryStep1, ignoreAction);
            return;
        }

        std::string resolvedIntent = GetChoiceAnswer(
            parsed["answers"], "intent", intent.empty() ? "romantic" : intent);
        if (resolvedIntent != "romantic" && resolvedIntent != "platonic") {
            resolvedIntent = (intent == "platonic") ? "platonic" : "romantic";
        }

        SKSE::log::info("SkyrimNetDecisions::EvaluateNonSexualScene Step 1 resolved intent='{}', queuing Step 2 (consent)",
                        resolvedIntent);

        nlohmann::json step2Ctx = baseCtx;
        step2Ctx["step"]   = 2;
        step2Ctx["intent"] = resolvedIntent;
        std::string step2ContextJson = step2Ctx.dump();

        auto step2CallbackHolder = std::make_shared<std::function<void(const char*, const char*, int)>>();
        *step2CallbackHolder = [step2CallbackHolder, parties, participantFormIDs, origParticipants, initiatorFormID,
                                resolvedIntent, activity, evalId, step2ContextJson, templateName, fallbackToChat](
                                   const char* step2Answer, const char* step2ResultJson, int step2Success) {
            SKSE::log::info("SkyrimNetDecisions::EvaluateNonSexualScene Step 2 callback: success={}, firstAnswer='{}'",
                            step2Success, step2Answer ? step2Answer : "");

            auto retryStep2 = [step2CallbackHolder, templateName, step2ContextJson]() {
                if (PublicSendCustomDecisionToLLM) {
                    PublicSendCustomDecisionToLLM(
                        templateName.c_str(), step2ContextJson.c_str(),
                        kDefaultMaxProbabilities, *step2CallbackHolder);
                }
            };
            auto ignoreStep2 = [origParticipants, initiatorFormID, resolvedIntent, activity, evalId]() {
                nlohmann::json ignoreResult;
                ignoreResult["originalParticipants"] = origParticipants;
                ignoreResult["initiator"]            = static_cast<uint32_t>(initiatorFormID);
                ignoreResult["intent"]               = resolvedIntent;
                ignoreResult["evalId"]               = evalId;
                ignoreResult["activity"]             = activity;
                OStimNet::FireModEvent("ostimnet_nonsexual_evaluation_finished", ignoreResult.dump().c_str(), 0.0f);
            };

            if (!step2Success || !step2ResultJson || step2ResultJson[0] == '\0') {
                std::string errCode, errMsg;
                if (ShouldFallbackToChatOnFailure(step2ResultJson, errCode, errMsg) && fallbackToChat) {
                    SKSE::log::warn("SkyrimNetDecisions::EvaluateNonSexualScene (Step 2): Jev error '{}' ({}) — falling back to GM chat LLM",
                                    errCode, errMsg);
                    RunFallbackOnGameThread(fallbackToChat);
                    return;
                }
                ShowDecisionRetryModal("Non-Sexual Scene (Step 2: Consent)", errMsg, retryStep2, ignoreStep2);
                return;
            }

            auto parsed2 = nlohmann::json::parse(step2ResultJson, nullptr, /*allow_exceptions=*/false);
            if (parsed2.is_discarded() || !parsed2.contains("answers") || !parsed2["answers"].is_object()) {
                ShowDecisionRetryModal("Non-Sexual Scene (Step 2: Consent)", "Unreadable decision response.", retryStep2, ignoreStep2);
                return;
            }

            const auto& answers2 = parsed2["answers"];
            bool allWilling = true;
            for (const auto& p : parties) {
                if (p.isPlayer) continue;
                std::string qId = "willing_" + p.key;
                double noulProb = 0.0;
                bool w = GetNoulAnswer(answers2, qId, false, &noulProb);
                SKSE::log::info("SkyrimNetDecisions::EvaluateNonSexualScene Step 2: {} ({}) willing={} (noul={:.3f})",
                                p.name, qId, w, noulProb);
                if (!w) {
                    allWilling = false;
                }
            }

            if (!allWilling) {
                nlohmann::json noStartResult;
                noStartResult["start"]                = false;
                noStartResult["intent"]               = resolvedIntent;
                noStartResult["evalId"]               = evalId;
                noStartResult["activity"]             = activity;
                noStartResult["originalParticipants"] = origParticipants;
                noStartResult["initiator"]            = static_cast<uint32_t>(initiatorFormID);
                OStimNet::FireModEvent("ostimnet_nonsexual_evaluation_finished", noStartResult.dump().c_str(), 2.0f);
                return;
            }

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
            result["intent"]               = resolvedIntent;
            result["evalId"]               = evalId;
            result["activity"]             = activity;
            result["main"]                 = std::move(mainArr);
            result["secondary"]            = std::move(secondaryArr);
            result["originalParticipants"] = origParticipants;
            result["initiator"]            = static_cast<uint32_t>(initiatorFormID);

            std::string outStr = result.dump();
            OStimNet::FireModEvent("ostimnet_nonsexual_evaluation_finished", outStr.c_str(), 1.0f);
        };

        bool queued2 = PublicSendCustomDecisionToLLM(
            templateName.c_str(), step2ContextJson.c_str(), kDefaultMaxProbabilities, *step2CallbackHolder);
        if (!queued2 && fallbackToChat) {
            RunFallbackOnGameThread(fallbackToChat);
        }
    };

    bool queued = PublicSendCustomDecisionToLLM(
        templateName.c_str(), step1ContextJson.c_str(), kDefaultMaxProbabilities, *step1CallbackHolder);
    if (!queued && fallbackToChat) {
        fallbackToChat();
        return true;
    }
    return queued;
}

// =============================================================================
// 4. EvaluateJoinOngoingSex
// =============================================================================

bool EvaluateJoinOngoingSex(
    RE::FormID joinerFormID,
    int threadID,
    const std::string& evalId,
    const std::string& reason,
    std::function<void()> fallbackToChat) {
    if (!IsAvailable()) return false;

    auto& store                  = OStimNet::ThreadDataStore::GetSingleton();
    const auto& currentActorPtrs = store.GetActorPtrs(threadID);
    std::string currentIntent    = OStimNet::IntentToString(store.GetIntent(threadID));

    std::vector<RoleActorItem> allRoleActors;
    std::vector<RE::FormID>    currentParticipantFormIDs;
    std::vector<std::string>   currentActorNameVec;
    nlohmann::json             currentActorUUIDs = nlohmann::json::array();

    for (RE::Actor* actor : currentActorPtrs) {
        if (!actor) continue;
        RE::FormID fid  = actor->GetFormID();
        uint64_t   uuid = ActorToUUID(actor);
        if (uuid == 0) continue;

        currentParticipantFormIDs.push_back(fid);
        std::string displayName = SanitizeSingleLine(ThreadDataStore::GetActorDisplayName(actor, "Actor"), "Actor");
        currentActorNameVec.push_back(displayName);
        currentActorUUIDs.push_back(uuid);
        allRoleActors.push_back(RoleActorItem{static_cast<uint32_t>(fid), displayName});
    }

    auto* joinerActor   = RE::TESForm::LookupByID<RE::Actor>(joinerFormID);
    uint64_t joinerUUID = ActorToUUID(joinerActor);
    if (!joinerActor || joinerUUID == 0 || allRoleActors.empty()) return false;

    std::string joinerName = SanitizeSingleLine(ThreadDataStore::GetActorDisplayName(joinerActor, "Joiner"), "Joiner");
    allRoleActors.push_back(RoleActorItem{static_cast<uint32_t>(joinerFormID), joinerName});

    nlohmann::json roleCombinationsJson;
    auto combos = BuildRoleCombinations(allRoleActors, roleCombinationsJson);
    if (combos.empty()) return false;

    nlohmann::json ctx;
    ctx["currentActors"]     = std::move(currentActorUUIDs);
    ctx["currentActorNames"] = OStimNet::FormatActorList(currentActorNameVec);
    ctx["joiner"]            = joinerUUID;
    ctx["joinerName"]        = joinerName;
    ctx["currentIntent"]     = currentIntent;
    ctx["sceneDescription"]  = OStimNet::GetSceneDescription(static_cast<uint32_t>(threadID));
    ctx["roleCombinations"]  = std::move(roleCombinationsJson);
    ctx["threadID"]          = threadID;
    ctx["reason"]            = reason;

    std::string contextJson  = ctx.dump();
    std::string templateName = "ostimnet_evaluate_join_ongoing_sex";

    SKSE::log::info("SkyrimNetDecisions::EvaluateJoinOngoingSex: queuing Jev decision, threadID={}, joiner='{}', combinations={}",
                    threadID, joinerName, combos.size());

    auto callbackHolder = std::make_shared<std::function<void(const char*, const char*, int)>>();
    *callbackHolder = [callbackHolder, combos, threadID, joinerFormID, currentParticipantFormIDs,
                       currentIntent, evalId, contextJson, templateName, fallbackToChat](
                          const char* answer, const char* resultJson, int success) {
        SKSE::log::info("SkyrimNetDecisions::EvaluateJoinOngoingSex callback: success={}, firstAnswer='{}'",
                        success, answer ? answer : "");

        auto retryAction = [callbackHolder, templateName, contextJson]() {
            if (PublicSendCustomDecisionToLLM) {
                PublicSendCustomDecisionToLLM(
                    templateName.c_str(), contextJson.c_str(),
                    kDefaultMaxProbabilities, *callbackHolder);
            }
        };
        auto ignoreAction = [threadID, joinerFormID, currentParticipantFormIDs, evalId]() {
            nlohmann::json ignoreResult;
            ignoreResult["threadID"] = threadID;
            ignoreResult["joiner"]   = static_cast<uint32_t>(joinerFormID);
            ignoreResult["evalId"]   = evalId;
            auto curArr = nlohmann::json::array();
            for (RE::FormID fid : currentParticipantFormIDs)
                curArr.push_back(static_cast<uint32_t>(fid));
            ignoreResult["currentParticipants"] = std::move(curArr);
            OStimNet::FireModEvent("ostimnet_join_sex_evaluation_finished", ignoreResult.dump().c_str(), 0.0f);
        };

        if (!success || !resultJson || resultJson[0] == '\0') {
            std::string errCode, errMsg;
            if (ShouldFallbackToChatOnFailure(resultJson, errCode, errMsg) && fallbackToChat) {
                SKSE::log::warn("SkyrimNetDecisions::EvaluateJoinOngoingSex: Jev error '{}' ({}) — falling back to GM chat LLM",
                                errCode, errMsg);
                RunFallbackOnGameThread(fallbackToChat);
                return;
            }
            ShowDecisionRetryModal("Join Ongoing Sex", errMsg, retryAction, ignoreAction);
            return;
        }

        auto parsed = nlohmann::json::parse(resultJson, nullptr, /*allow_exceptions=*/false);
        if (parsed.is_discarded() || !parsed.contains("answers") || !parsed["answers"].is_object()) {
            ShowDecisionRetryModal("Join Ongoing Sex", "Unreadable decision response.", retryAction, ignoreAction);
            return;
        }

        const auto& answers = parsed["answers"];
        auto joinCurArr = nlohmann::json::array();
        for (RE::FormID fid : currentParticipantFormIDs)
            joinCurArr.push_back(static_cast<uint32_t>(fid));

        double acceptProb = 0.0;
        bool accepted = GetNoulAnswer(answers, "accept", false, &acceptProb);
        SKSE::log::info("SkyrimNetDecisions::EvaluateJoinOngoingSex: accept={} (noul={:.3f})", accepted, acceptProb);

        if (!accepted) {
            nlohmann::json noJoinResult;
            noJoinResult["threadID"]            = threadID;
            noJoinResult["joiner"]              = static_cast<uint32_t>(joinerFormID);
            noJoinResult["evalId"]              = evalId;
            noJoinResult["currentParticipants"] = std::move(joinCurArr);
            OStimNet::FireModEvent("ostimnet_join_sex_evaluation_finished", noJoinResult.dump().c_str(), 2.0f);
            return;
        }

        std::string chosenRoleKey = GetChoiceAnswer(answers, "roles", combos[0].key);
        const RoleCombination* selectedCombo = &combos[0];
        for (const auto& c : combos) {
            if (c.key == chosenRoleKey) {
                selectedCombo = &c;
                break;
            }
        }

        std::string resolvedIntent = GetChoiceAnswer(answers, "intent", currentIntent);

        nlohmann::json mainArr      = nlohmann::json::array();
        nlohmann::json secondaryArr = nlohmann::json::array();
        for (uint32_t fid : selectedCombo->mainFormIDs)      mainArr.push_back(fid);
        for (uint32_t fid : selectedCombo->secondaryFormIDs) secondaryArr.push_back(fid);

        nlohmann::json result;
        result["threadID"]            = threadID;
        result["joiner"]              = static_cast<uint32_t>(joinerFormID);
        result["evalId"]              = evalId;
        result["currentParticipants"] = std::move(joinCurArr);
        result["intent"]              = resolvedIntent;
        result["main"]                = std::move(mainArr);
        result["secondary"]           = std::move(secondaryArr);

        std::string outStr = result.dump();
        SKSE::log::info("SkyrimNetDecisions::EvaluateJoinOngoingSex: selected '{}' (main: {}; secondary: {})",
                        selectedCombo->key, selectedCombo->mainNames, selectedCombo->secondaryNames);
        OStimNet::FireModEvent("ostimnet_join_sex_evaluation_finished", outStr.c_str(), 1.0f);
    };

    bool queued = PublicSendCustomDecisionToLLM(
        templateName.c_str(), contextJson.c_str(), kDefaultMaxProbabilities, *callbackHolder);
    if (!queued && fallbackToChat) {
        fallbackToChat();
        return true;
    }
    return queued;
}

// =============================================================================
// 5. EvaluateInviteToSex
// =============================================================================

bool EvaluateInviteToSex(
    RE::FormID inviterFormID,
    const std::vector<RE::FormID>& inviteeFormIDs,
    int threadID,
    const std::string& evalId,
    const std::string& reason,
    std::function<void()> fallbackToChat) {
    if (!IsAvailable()) return false;

    auto& store                  = OStimNet::ThreadDataStore::GetSingleton();
    const auto& currentActorPtrs = store.GetActorPtrs(threadID);
    std::string currentIntent    = OStimNet::IntentToString(store.GetIntent(threadID));

    struct InviteeItem {
        std::string key;
        uint32_t    formID   = 0;
        uint64_t    uuid     = 0;
        std::string name;
        bool        isPlayer = false;
    };

    std::vector<RoleActorItem> currentRoleActors;
    std::vector<InviteeItem>   inviteeItems;
    std::vector<RE::FormID>    currentParticipantFormIDs;
    std::vector<std::string>   currentActorNameVec;
    std::vector<std::string>   otherCurrentNameVec;
    std::vector<std::string>   inviteeNameVec;
    nlohmann::json             currentActorUUIDs = nlohmann::json::array();
    nlohmann::json             inviteeUUIDs      = nlohmann::json::array();
    nlohmann::json             inviteesList      = nlohmann::json::array();

    std::string inviterName = "Inviter";

    for (RE::Actor* actor : currentActorPtrs) {
        if (!actor) continue;
        RE::FormID fid  = actor->GetFormID();
        uint64_t   uuid = ActorToUUID(actor);
        if (uuid == 0) continue;

        currentParticipantFormIDs.push_back(fid);
        std::string displayName = SanitizeSingleLine(ThreadDataStore::GetActorDisplayName(actor, "Actor"), "Actor");
        bool isInviter = (fid == inviterFormID);
        bool isPlayer  = IsPlayerFormID(fid);
        if (isInviter) inviterName = displayName;
        if (!isInviter && !isPlayer) otherCurrentNameVec.push_back(displayName);

        currentRoleActors.push_back(RoleActorItem{static_cast<uint32_t>(fid), displayName});
        currentActorNameVec.push_back(displayName);
        currentActorUUIDs.push_back(uuid);
    }

    for (RE::FormID fid : inviteeFormIDs) {
        auto* actor   = RE::TESForm::LookupByID<RE::Actor>(fid);
        uint64_t uuid = ActorToUUID(actor);
        if (!actor || uuid == 0) continue;

        std::string displayName = SanitizeSingleLine(ThreadDataStore::GetActorDisplayName(actor, "Invitee"), "Invitee");
        InviteeItem item;
        item.key      = "i" + std::to_string(inviteeItems.size() + 1);
        item.formID   = static_cast<uint32_t>(fid);
        item.uuid     = uuid;
        item.name     = displayName;
        item.isPlayer = IsPlayerFormID(fid);
        inviteeItems.push_back(item);

        inviteeNameVec.push_back(displayName);
        inviteeUUIDs.push_back(uuid);

        nlohmann::json iObj;
        iObj["key"]      = item.key;
        iObj["uuid"]     = item.uuid;
        iObj["name"]     = item.name;
        iObj["isPlayer"] = item.isPlayer;
        inviteesList.push_back(std::move(iObj));
    }

    if (currentRoleActors.empty() || inviteeItems.empty()) return false;

    bool checkCurrentConsent = !otherCurrentNameVec.empty();

    // Build all valid (main, secondary, excluded) combinations:
    // - All current actors always participate (main or secondary).
    // - At least 1 invitee must participate; player invitee (if any) always participates.
    struct InviteRoleCombo {
        std::string           key;
        uint32_t              invMask = 0;
        std::vector<uint32_t> mainFormIDs;
        std::vector<uint32_t> secondaryFormIDs;
        std::vector<uint32_t> acceptedInviteeFormIDs;
        std::string           mainNames;
        std::string           secondaryNames;
        std::string           excludedNames;
    };

    std::vector<InviteRoleCombo> inviteCombos;
    nlohmann::json roleCombinationsJson = nlohmann::json::array();

    const size_t invCount      = inviteeItems.size();
    const uint32_t fullInvMask = (1u << invCount) - 1u;
    size_t comboIdx            = 1;

    // Order from most invitees accepted down to 1 invitee accepted
    for (size_t acceptedCount = invCount; acceptedCount >= 1; --acceptedCount) {
        if (invCount >= 3 && acceptedCount != invCount && acceptedCount != 1) continue;

        for (uint32_t invMask = 1; invMask <= fullInvMask; ++invMask) {
            size_t bits = 0;
            bool playerExcluded = false;
            for (size_t i = 0; i < invCount; ++i) {
                if (invMask & (1u << i)) {
                    ++bits;
                } else if (inviteeItems[i].isPlayer) {
                    playerExcluded = true;
                }
            }
            if (bits != acceptedCount || playerExcluded) continue;

            std::vector<RoleActorItem> activeActors = currentRoleActors;
            std::vector<uint32_t>      acceptedInvFIDs;
            std::string                excludedNames;

            for (size_t i = 0; i < invCount; ++i) {
                if (invMask & (1u << i)) {
                    activeActors.push_back(RoleActorItem{inviteeItems[i].formID, inviteeItems[i].name});
                    acceptedInvFIDs.push_back(inviteeItems[i].formID);
                } else {
                    if (!excludedNames.empty()) excludedNames += ", ";
                    excludedNames += inviteeItems[i].name;
                }
            }

            nlohmann::json subJson;
            auto subCombos = BuildRoleCombinations(activeActors, subJson);
            for (auto& sc : subCombos) {
                InviteRoleCombo ic;
                ic.key                    = "c" + std::to_string(comboIdx++);
                ic.invMask                = invMask;
                ic.mainFormIDs            = std::move(sc.mainFormIDs);
                ic.secondaryFormIDs       = std::move(sc.secondaryFormIDs);
                ic.acceptedInviteeFormIDs = acceptedInvFIDs;
                ic.mainNames              = std::move(sc.mainNames);
                ic.secondaryNames         = std::move(sc.secondaryNames);
                ic.excludedNames          = excludedNames;

                nlohmann::json jObj;
                jObj["key"]            = ic.key;
                jObj["mainNames"]      = ic.mainNames;
                jObj["secondaryNames"] = ic.secondaryNames;
                jObj["excludedNames"]  = ic.excludedNames;
                roleCombinationsJson.push_back(std::move(jObj));
                inviteCombos.push_back(std::move(ic));
            }
        }
    }

    if (inviteCombos.empty()) return false;

    nlohmann::json ctx;
    ctx["currentActors"]       = std::move(currentActorUUIDs);
    ctx["currentActorNames"]   = OStimNet::FormatActorList(currentActorNameVec);
    ctx["otherCurrentNames"]   = OStimNet::FormatActorList(otherCurrentNameVec);
    ctx["checkCurrentConsent"] = checkCurrentConsent;
    ctx["invitees"]            = std::move(inviteeUUIDs);
    ctx["inviteesList"]        = std::move(inviteesList);
    ctx["inviteeNames"]        = OStimNet::FormatActorList(inviteeNameVec);
    ctx["inviterName"]         = inviterName;
    ctx["currentIntent"]       = currentIntent;
    ctx["sceneDescription"]    = OStimNet::GetSceneDescription(static_cast<uint32_t>(threadID));
    ctx["roleCombinations"]    = std::move(roleCombinationsJson);
    ctx["threadID"]            = threadID;
    ctx["reason"]              = reason;

    std::string contextJson  = ctx.dump();
    std::string templateName = "ostimnet_evaluate_invite_to_sex";

    SKSE::log::info("SkyrimNetDecisions::EvaluateInviteToSex: queuing Jev decision, threadID={}, inviter='{}', invitees={}, combinations={}",
                    threadID, inviterName, inviteeItems.size(), inviteCombos.size());

    auto callbackHolder = std::make_shared<std::function<void(const char*, const char*, int)>>();
    *callbackHolder = [callbackHolder, inviteCombos, inviteeItems, checkCurrentConsent,
                       threadID, inviterFormID, inviteeFormIDs, currentParticipantFormIDs,
                       currentIntent, evalId, contextJson, templateName, fallbackToChat](
                          const char* answer, const char* resultJson, int success) {
        SKSE::log::info("SkyrimNetDecisions::EvaluateInviteToSex callback: success={}, firstAnswer='{}'",
                        success, answer ? answer : "");

        auto retryAction = [callbackHolder, templateName, contextJson]() {
            if (PublicSendCustomDecisionToLLM) {
                PublicSendCustomDecisionToLLM(
                    templateName.c_str(), contextJson.c_str(),
                    kDefaultMaxProbabilities, *callbackHolder);
            }
        };
        auto ignoreAction = [threadID, inviterFormID, inviteeFormIDs, currentParticipantFormIDs, evalId]() {
            nlohmann::json ignoreResult;
            ignoreResult["threadID"] = threadID;
            ignoreResult["inviter"]  = static_cast<uint32_t>(inviterFormID);
            ignoreResult["evalId"]   = evalId;
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

        if (!success || !resultJson || resultJson[0] == '\0') {
            std::string errCode, errMsg;
            if (ShouldFallbackToChatOnFailure(resultJson, errCode, errMsg) && fallbackToChat) {
                SKSE::log::warn("SkyrimNetDecisions::EvaluateInviteToSex: Jev error '{}' ({}) — falling back to GM chat LLM",
                                errCode, errMsg);
                RunFallbackOnGameThread(fallbackToChat);
                return;
            }
            ShowDecisionRetryModal("Invite to Sex", errMsg, retryAction, ignoreAction);
            return;
        }

        auto parsed = nlohmann::json::parse(resultJson, nullptr, /*allow_exceptions=*/false);
        if (parsed.is_discarded() || !parsed.contains("answers") || !parsed["answers"].is_object()) {
            ShowDecisionRetryModal("Invite to Sex", "Unreadable decision response.", retryAction, ignoreAction);
            return;
        }

        const auto& answers = parsed["answers"];
        auto invCurArr = nlohmann::json::array();
        for (RE::FormID fid : currentParticipantFormIDs)
            invCurArr.push_back(static_cast<uint32_t>(fid));

        auto inviteesArrAll = nlohmann::json::array();
        for (RE::FormID fid : inviteeFormIDs)
            inviteesArrAll.push_back(static_cast<uint32_t>(fid));

        bool currentAccepted = true;
        if (checkCurrentConsent) {
            currentAccepted = GetNoulAnswer(answers, "current_accept", true);
        }

        uint32_t willingMask = 0;
        for (size_t i = 0; i < inviteeItems.size(); ++i) {
            bool willing = inviteeItems[i].isPlayer
                ? true
                : GetNoulAnswer(answers, "willing_" + inviteeItems[i].key, false);
            if (willing) willingMask |= (1u << i);
        }

        if (!currentAccepted || willingMask == 0) {
            SKSE::log::info("SkyrimNetDecisions::EvaluateInviteToSex: invitation declined (currentAccepted={}, willingMask=0x{:X})",
                            currentAccepted, willingMask);
            nlohmann::json noInviteResult;
            noInviteResult["threadID"]            = threadID;
            noInviteResult["inviter"]             = static_cast<uint32_t>(inviterFormID);
            noInviteResult["evalId"]              = evalId;
            noInviteResult["invitees"]            = std::move(inviteesArrAll);
            noInviteResult["currentParticipants"] = std::move(invCurArr);
            OStimNet::FireModEvent("ostimnet_invite_sex_evaluation_finished", noInviteResult.dump().c_str(), 2.0f);
            return;
        }

        std::string chosenRoleKey = GetChoiceAnswer(answers, "roles", inviteCombos[0].key);
        const InviteRoleCombo* selectedCombo = nullptr;
        double bestProb = -1.0;

        for (const auto& c : inviteCombos) {
            if (c.invMask != willingMask) continue;
            if (c.key == chosenRoleKey) {
                selectedCombo = &c;
                break;
            }
            double p = GetChoiceProbability(answers, "roles", c.key, 0.0);
            if (p > bestProb) {
                bestProb      = p;
                selectedCombo = &c;
            }
        }
        if (!selectedCombo) {
            for (const auto& c : inviteCombos) {
                if (c.key == chosenRoleKey) {
                    selectedCombo = &c;
                    break;
                }
            }
        }
        if (!selectedCombo) selectedCombo = &inviteCombos[0];

        std::string resolvedIntent = GetChoiceAnswer(answers, "intent", currentIntent);

        nlohmann::json mainArr      = nlohmann::json::array();
        nlohmann::json secondaryArr = nlohmann::json::array();
        for (uint32_t fid : selectedCombo->mainFormIDs)      mainArr.push_back(fid);
        for (uint32_t fid : selectedCombo->secondaryFormIDs) secondaryArr.push_back(fid);

        nlohmann::json result;
        result["threadID"]            = threadID;
        result["inviter"]             = static_cast<uint32_t>(inviterFormID);
        result["evalId"]              = evalId;
        result["currentParticipants"] = std::move(invCurArr);
        result["invitees"]            = std::move(inviteesArrAll);
        result["intent"]              = resolvedIntent;
        result["main"]                = std::move(mainArr);
        result["secondary"]           = std::move(secondaryArr);

        std::string outStr = result.dump();
        SKSE::log::info("SkyrimNetDecisions::EvaluateInviteToSex: selected '{}' (main: {}; secondary: {}; excluded: {})",
                        selectedCombo->key, selectedCombo->mainNames, selectedCombo->secondaryNames, selectedCombo->excludedNames);
        OStimNet::FireModEvent("ostimnet_invite_sex_evaluation_finished", outStr.c_str(), 1.0f);
    };

    bool queued = PublicSendCustomDecisionToLLM(
        templateName.c_str(), contextJson.c_str(), kDefaultMaxProbabilities, *callbackHolder);
    if (!queued && fallbackToChat) {
        fallbackToChat();
        return true;
    }
    return queued;
}

// =============================================================================
// 6. EvaluateLocationScan
// =============================================================================

bool EvaluateLocationScan(
    const std::string& baseContextJson,
    const std::map<std::string, RE::FormID>& nameToFormID,
    uint64_t scanGen,
    std::function<void()> fallbackToChat) {
    if (!IsAvailable()) return false;

    struct CandidateSnap {
        std::string key;
        RE::FormID  formID   = 0;
        uint64_t    uuid     = 0;
        std::string name;
        bool        isPlayer = false;
    };

    std::string playerSelectionChance = OStimNet::Config::GetSingleton().LocationScanPlayerSelectionChance();
    auto* player                      = RE::PlayerCharacter::GetSingleton();
    RE::FormID playerFID              = player ? player->GetFormID() : 0;

    std::vector<CandidateSnap> snaps;
    nlohmann::json candidatesList   = nlohmann::json::array();
    nlohmann::json initiatorsList   = nlohmann::json::array();
    nlohmann::json eventFilterUUIDs = nlohmann::json::array();

    for (const auto& [name, fid] : nameToFormID) {
        bool isPlayer = (fid == playerFID);
        if (isPlayer && playerSelectionChance == "never") continue;

        uint64_t uuid = ActorRefIDToUUID(fid);
        if (uuid == 0) continue;

        CandidateSnap snap;
        snap.key      = "c_" + std::to_string(snaps.size() + 1);
        snap.formID   = fid;
        snap.uuid     = uuid;
        snap.name     = SanitizeSingleLine(name, "Actor");
        snap.isPlayer = isPlayer;
        snaps.push_back(snap);

        nlohmann::json cObj;
        cObj["key"]         = snap.key;
        cObj["uuid"]        = snap.uuid;
        cObj["name"]        = snap.name;
        cObj["isPlayer"]    = snap.isPlayer;
        cObj["isInitiator"] = false;
        cObj["isPartner1"]  = false;
        if (!snap.isPlayer) {
            initiatorsList.push_back(cObj);
        }
        candidatesList.push_back(std::move(cObj));
        eventFilterUUIDs.push_back(snap.uuid);

        if (snaps.size() >= 50) break;
    }

    if (initiatorsList.empty()) {
        SKSE::log::info("SkyrimNetDecisions::EvaluateLocationScan: no eligible NPC initiators found, skipping LLM call");
        OStimNet::FireModEvent("ostimnet_location_scan_result", "", 2.0f);
        return true;
    }

    nlohmann::json baseCtx = nlohmann::json::parse(baseContextJson, nullptr, /*allow_exceptions=*/false);
    if (baseCtx.is_discarded() || !baseCtx.is_object()) baseCtx = nlohmann::json::object();

    if (!baseCtx.contains("location")) baseCtx["location"] = "Unknown";
    baseCtx["enableAggressiveIntent"] = OStimNet::Config::GetSingleton().EnableAggressiveIntent();
    baseCtx["playerSelectionChance"]  = playerSelectionChance;
    baseCtx["candidatesList"]         = std::move(candidatesList);
    baseCtx["initiatorsList"]         = std::move(initiatorsList);
    baseCtx["eventFilterUUIDs"]       = std::move(eventFilterUUIDs);
    baseCtx["step"]                   = 1;

    std::string step1ContextJson = baseCtx.dump();
    std::string templateName     = "ostimnet_scan_location";

    SKSE::log::info("SkyrimNetDecisions::EvaluateLocationScan: queuing Step 1 (initiator), candidates={}",
                    snaps.size());

    auto makeIgnoreAction = [nameToFormID]() {
        return [nameToFormID]() {
            nlohmann::json ignoreResult;
            auto origArr = nlohmann::json::array();
            for (const auto& [name, fid] : nameToFormID)
                origArr.push_back(static_cast<uint32_t>(fid));
            ignoreResult["originalParticipants"] = std::move(origArr);
            OStimNet::FireModEvent("ostimnet_location_scan_result", ignoreResult.dump().c_str(), 0.0f);
        };
    };

    // Helper that runs Step 4 (final consent, role combinations, activity, position)
    // once the 1–3 scene participants, intent, and furniture are selected.
    auto runStep4 = [baseCtx, templateName, scanGen, fallbackToChat, makeIgnoreAction](
                        const std::vector<CandidateSnap>& sceneSnaps,
                        const std::string& resolvedIntent,
                        const std::string& resolvedFurniture) {
        if (OStimNet::LocationScanService::GetSingleton().GetGeneration() != scanGen) {
            SKSE::log::info("SkyrimNetDecisions::EvaluateLocationScan (Step 4): scan gen={} superseded, discarding", scanGen);
            return;
        }

        struct ScanRoleCombo {
            std::string           key;
            uint32_t              activeMask = 0;
            std::vector<uint32_t> mainFormIDs;
            std::vector<uint32_t> secondaryFormIDs;
            std::string           mainNames;
            std::string           secondaryNames;
            std::string           excludedNames;
        };

        std::vector<ScanRoleCombo> scanCombos;
        nlohmann::json roleCombinationsJson = nlohmann::json::array();
        nlohmann::json sceneParticipants    = nlohmann::json::array();
        std::vector<std::string> actorNameVec;

        for (size_t i = 0; i < sceneSnaps.size(); ++i) {
            actorNameVec.push_back(sceneSnaps[i].name);
            nlohmann::json pObj;
            pObj["key"]         = sceneSnaps[i].key;
            pObj["uuid"]        = sceneSnaps[i].uuid;
            pObj["name"]        = sceneSnaps[i].name;
            pObj["isInitiator"] = (i == 0);
            pObj["isPlayer"]    = sceneSnaps[i].isPlayer;
            sceneParticipants.push_back(std::move(pObj));
        }

        const size_t n          = sceneSnaps.size();
        const uint32_t fullMask = (1u << n) - 1u;
        size_t comboIdx         = 1;

        if (n == 1) {
            ScanRoleCombo sc;
            sc.key         = "c_1";
            sc.activeMask  = 1u;
            sc.mainFormIDs = {static_cast<uint32_t>(sceneSnaps[0].formID)};
            sc.mainNames   = sceneSnaps[0].name;
            scanCombos.push_back(std::move(sc));
        } else {
            for (size_t activeCount = n; activeCount >= 2; --activeCount) {
                for (uint32_t mask = 1; mask <= fullMask; ++mask) {
                    if (!(mask & 1u)) continue;  // Initiator (bit 0) is always included
                    size_t bits = 0;
                    for (size_t i = 0; i < n; ++i) {
                        if (mask & (1u << i)) ++bits;
                    }
                    if (bits != activeCount) continue;

                    std::vector<RoleActorItem> activeActors;
                    std::string excludedNames;
                    for (size_t i = 0; i < n; ++i) {
                        if (mask & (1u << i)) {
                            activeActors.push_back(RoleActorItem{
                                static_cast<uint32_t>(sceneSnaps[i].formID), sceneSnaps[i].name});
                        } else {
                            if (!excludedNames.empty()) excludedNames += ", ";
                            excludedNames += sceneSnaps[i].name;
                        }
                    }

                    nlohmann::json subJson;
                    auto subCombos = BuildRoleCombinations(activeActors, subJson);
                    for (auto& rc : subCombos) {
                        ScanRoleCombo sc;
                        sc.key              = "c_" + std::to_string(comboIdx++);
                        sc.activeMask       = mask;
                        sc.mainFormIDs      = std::move(rc.mainFormIDs);
                        sc.secondaryFormIDs = std::move(rc.secondaryFormIDs);
                        sc.mainNames        = std::move(rc.mainNames);
                        sc.secondaryNames   = std::move(rc.secondaryNames);
                        sc.excludedNames    = excludedNames;

                        nlohmann::json jObj;
                        jObj["key"]            = sc.key;
                        jObj["mainNames"]      = sc.mainNames;
                        jObj["secondaryNames"] = sc.secondaryNames;
                        jObj["excludedNames"]  = sc.excludedNames;
                        roleCombinationsJson.push_back(std::move(jObj));
                        scanCombos.push_back(std::move(sc));
                    }
                }
            }
        }

        std::vector<uint32_t> participantFIDs;
        participantFIDs.reserve(sceneSnaps.size());
        bool hasPlayer = false;
        for (const auto& s : sceneSnaps) {
            participantFIDs.push_back(static_cast<uint32_t>(s.formID));
            if (s.isPlayer) hasPlayer = true;
        }

        OStimNavigatorAPI::Intent intentEnum = OStimNavigatorAPI::IntentFromString(resolvedIntent);
        std::optional<OStimNet::SexualPhase> openingPhase = OStimNet::ResolvePreStartOpeningPhase(
            intentEnum, sceneSnaps.size(), hasPlayer, resolvedFurniture);

        nlohmann::json activitiesJson = nlohmann::json::array();
        nlohmann::json positionsJson  = nlohmann::json::array();
        std::string openingPhaseStr;

        if (!openingPhase.has_value()) {
            openingPhaseStr = "";
            activitiesJson  = BuildSetupActivitiesArrayJson(participantFIDs, resolvedFurniture, "sexual");
            positionsJson   = BuildSetupPositionsArrayJson(participantFIDs, resolvedFurniture);
        } else if (*openingPhase == OStimNet::SexualPhase::Undressing) {
            openingPhaseStr = "undressing";
        } else {
            const auto& phaseList = OStimNet::IntentToPhaseList(intentEnum);
            OStimNet::SexualPhase effectivePhase = *openingPhase;
            bool foundStart = false;
            for (size_t idx = 0; idx < phaseList.size(); ++idx) {
                if (phaseList[idx] == *openingPhase) foundStart = true;
                if (!foundStart) continue;
                effectivePhase = phaseList[idx];
                activitiesJson = BuildSetupActivitiesArrayJson(
                    participantFIDs, resolvedFurniture, OStimNet::SexualPhaseToName(effectivePhase));
                if (!activitiesJson.empty()) break;
            }
            openingPhaseStr = OStimNet::SexualPhaseToName(effectivePhase);
            if (effectivePhase == OStimNet::SexualPhase::Sex) {
                positionsJson = BuildSetupPositionsArrayJson(participantFIDs, resolvedFurniture);
            }
        }

        nlohmann::json step4Ctx      = baseCtx;
        step4Ctx["step"]             = 4;
        step4Ctx["intent"]           = resolvedIntent;
        step4Ctx["furniture"]        = resolvedFurniture;
        step4Ctx["openingPhase"]     = openingPhaseStr;
        step4Ctx["activities"]       = std::move(activitiesJson);
        step4Ctx["positions"]        = std::move(positionsJson);
        step4Ctx["initiatorName"]    = sceneSnaps[0].name;
        step4Ctx["actorNames"]       = OStimNet::FormatActorList(actorNameVec);
        step4Ctx["sceneParticipants"] = std::move(sceneParticipants);
        step4Ctx["roleCombinations"] = std::move(roleCombinationsJson);
        std::string step4ContextJson = step4Ctx.dump();

        auto step4CallbackHolder = std::make_shared<std::function<void(const char*, const char*, int)>>();
        *step4CallbackHolder = [step4CallbackHolder, sceneSnaps, participantFIDs, scanCombos, resolvedIntent, resolvedFurniture,
                                openingPhaseStr, step4Ctx, step4ContextJson, templateName, scanGen, fallbackToChat, makeIgnoreAction](
                                   const char*, const char* step4ResultJson, int step4Success) {
            if (OStimNet::LocationScanService::GetSingleton().GetGeneration() != scanGen) {
                SKSE::log::info("SkyrimNetDecisions::EvaluateLocationScan Step 4 callback: scan gen={} superseded, discarding", scanGen);
                return;
            }

            auto retryStep4 = [step4CallbackHolder, templateName, step4ContextJson]() {
                if (PublicSendCustomDecisionToLLM) {
                    PublicSendCustomDecisionToLLM(
                        templateName.c_str(), step4ContextJson.c_str(),
                        kDefaultMaxProbabilities, *step4CallbackHolder);
                }
            };

            if (!step4Success || !step4ResultJson || step4ResultJson[0] == '\0') {
                std::string errCode, errMsg;
                if (ShouldFallbackToChatOnFailure(step4ResultJson, errCode, errMsg) && fallbackToChat) {
                    RunFallbackOnGameThread(fallbackToChat);
                    return;
                }
                ShowDecisionRetryModal("Location Scan (Step 4: Roles & Consent)", errMsg, retryStep4, makeIgnoreAction());
                return;
            }

            auto parsed4 = nlohmann::json::parse(step4ResultJson, nullptr, /*allow_exceptions=*/false);
            if (parsed4.is_discarded() || !parsed4.contains("answers") || !parsed4["answers"].is_object()) {
                ShowDecisionRetryModal("Location Scan (Step 4: Roles & Consent)", "Unreadable decision response.", retryStep4, makeIgnoreAction());
                return;
            }

            const auto& answers4 = parsed4["answers"];
            uint32_t willingMask = 1u;  // Initiator (index 0) is always willing
            size_t   willingCount = 1;

            for (size_t i = 1; i < sceneSnaps.size(); ++i) {
                bool willing = sceneSnaps[i].isPlayer
                    ? true
                    : GetNoulAnswer(answers4, "willing_" + sceneSnaps[i].key, true);
                if (willing) {
                    willingMask |= (1u << i);
                    ++willingCount;
                }
            }

            if (sceneSnaps.size() > 1 && willingCount < 2) {
                SKSE::log::info("SkyrimNetDecisions::EvaluateLocationScan Step 4: partner(s) declined consent, aborting scene.");
                OStimNet::FireModEvent("ostimnet_location_scan_result", "", 2.0f);
                return;
            }

            const ScanRoleCombo* selectedCombo = &scanCombos[0];
            if (sceneSnaps.size() >= 2) {
                std::string chosenRoleKey = GetChoiceAnswer(answers4, "roles", scanCombos[0].key);
                selectedCombo = nullptr;
                double bestProb = -1.0;

                for (const auto& c : scanCombos) {
                    if (c.activeMask != willingMask) continue;
                    if (c.key == chosenRoleKey) {
                        selectedCombo = &c;
                        break;
                    }
                    double p = GetChoiceProbability(answers4, "roles", c.key, 0.0);
                    if (p > bestProb) {
                        bestProb      = p;
                        selectedCombo = &c;
                    }
                }
                if (!selectedCombo) {
                    for (const auto& c : scanCombos) {
                        if (c.key == chosenRoleKey) {
                            selectedCombo = &c;
                            break;
                        }
                    }
                }
                if (!selectedCombo) selectedCombo = &scanCombos[0];
            }

            nlohmann::json mainArr      = nlohmann::json::array();
            nlohmann::json secondaryArr = nlohmann::json::array();
            for (uint32_t fid : selectedCombo->mainFormIDs)      mainArr.push_back(fid);
            for (uint32_t fid : selectedCombo->secondaryFormIDs) secondaryArr.push_back(fid);

            bool askedActivity = step4Ctx.contains("activities") && step4Ctx["activities"].is_array() && !step4Ctx["activities"].empty();
            bool askedPosition = step4Ctx.contains("positions") && step4Ctx["positions"].is_array() && !step4Ctx["positions"].empty();

            std::string rawAct  = askedActivity ? GetChoiceAnswer(answers4, "activity", "any") : "";
            std::string rawPos  = askedPosition ? GetChoiceAnswer(answers4, "position", "any") : "";
            std::string normAct = NormalizeActivityChoice(rawAct);
            std::string normPos = NormalizePositionChoice(rawPos);
            std::string rawFurn = (resolvedFurniture == "floor" || resolvedFurniture == "none") ? "" : resolvedFurniture;

            if (openingPhaseStr.empty() && !normAct.empty() && !normPos.empty()) {
                auto sexActs = BuildSetupActivitiesArrayJson(participantFIDs, resolvedFurniture, "sex");
                bool isSexAct = false;
                for (const auto& item : sexActs) {
                    if (item.is_object() && item.value("id", "") == normAct) {
                        isSexAct = true;
                        break;
                    }
                }
                if (!isSexAct) {
                    normPos.clear();
                }
            }

            nlohmann::json result;
            result["intent"]           = resolvedIntent;
            result["sexualPosition"]   = normPos;
            result["sexualActivities"] = normAct;
            result["main"]             = std::move(mainArr);
            result["secondary"]        = std::move(secondaryArr);
            result["furniture"]        = rawFurn;

            float numArg = (willingCount < sceneSnaps.size()) ? 3.0f : 1.0f;
            std::string jsonStr = result.dump();
            SKSE::log::info("SkyrimNetDecisions::EvaluateLocationScan Step 4 complete (numArg={:.1f}): {}",
                            numArg, jsonStr);
            OStimNet::FireModEvent("ostimnet_location_scan_result", jsonStr.c_str(), numArg);
        };

        bool queued4 = PublicSendCustomDecisionToLLM(
            templateName.c_str(), step4ContextJson.c_str(), kDefaultMaxProbabilities, *step4CallbackHolder);
        if (!queued4 && fallbackToChat) {
            RunFallbackOnGameThread(fallbackToChat);
        }
    };

    auto step1CallbackHolder = std::make_shared<std::function<void(const char*, const char*, int)>>();
    *step1CallbackHolder = [step1CallbackHolder, snaps, baseCtx, step1ContextJson, templateName,
                            scanGen, fallbackToChat, makeIgnoreAction, runStep4](
                               const char* answer, const char* resultJson, int success) {
        if (OStimNet::LocationScanService::GetSingleton().GetGeneration() != scanGen) {
            SKSE::log::info("SkyrimNetDecisions::EvaluateLocationScan Step 1 callback: scan gen={} superseded, discarding",
                            scanGen);
            return;
        }

        SKSE::log::info("SkyrimNetDecisions::EvaluateLocationScan Step 1 callback: success={}, firstAnswer='{}'",
                        success, answer ? answer : "");

        auto retryStep1 = [step1CallbackHolder, templateName, step1ContextJson]() {
            if (PublicSendCustomDecisionToLLM) {
                PublicSendCustomDecisionToLLM(
                    templateName.c_str(), step1ContextJson.c_str(),
                    kDefaultMaxProbabilities, *step1CallbackHolder);
            }
        };

        if (!success || !resultJson || resultJson[0] == '\0') {
            std::string errCode, errMsg;
            if (ShouldFallbackToChatOnFailure(resultJson, errCode, errMsg) && fallbackToChat) {
                SKSE::log::warn("SkyrimNetDecisions::EvaluateLocationScan (Step 1): Jev error '{}' ({}) — falling back to GM chat LLM",
                                errCode, errMsg);
                RunFallbackOnGameThread(fallbackToChat);
                return;
            }
            ShowDecisionRetryModal("Location Scan (Step 1: Initiator)", errMsg, retryStep1, makeIgnoreAction());
            return;
        }

        auto parsed1 = nlohmann::json::parse(resultJson, nullptr, /*allow_exceptions=*/false);
        if (parsed1.is_discarded() || !parsed1.contains("answers") || !parsed1["answers"].is_object()) {
            ShowDecisionRetryModal("Location Scan (Step 1: Initiator)", "Unreadable decision response.", retryStep1, makeIgnoreAction());
            return;
        }

        std::string initiatorKey = GetChoiceAnswer(parsed1["answers"], "initiator", "none");
        const CandidateSnap* initiatorPtr = nullptr;
        for (const auto& s : snaps) {
            if (!s.isPlayer && s.key == initiatorKey) {
                initiatorPtr = &s;
                break;
            }
        }

        if (!initiatorPtr || initiatorKey == "none") {
            SKSE::log::info("SkyrimNetDecisions::EvaluateLocationScan Step 1: initiator='{}', no scene started",
                            initiatorKey);
            OStimNet::FireModEvent("ostimnet_location_scan_result", "", 2.0f);
            return;
        }

        CandidateSnap initiatorSnap = *initiatorPtr;
        auto* initiatorActor        = RE::TESForm::LookupByID<RE::Actor>(initiatorSnap.formID);

        nlohmann::json partner1List = nlohmann::json::array();
        nlohmann::json step2Candidates = baseCtx["candidatesList"];
        for (auto& cObj : step2Candidates) {
            bool isInit = (cObj.value("key", "") == initiatorSnap.key);
            cObj["isInitiator"] = isInit;
            if (!isInit) {
                partner1List.push_back(cObj);
            }
        }

        nlohmann::json step2Ctx   = baseCtx;
        step2Ctx["step"]          = 2;
        step2Ctx["initiatorName"] = initiatorSnap.name;
        step2Ctx["candidatesList"] = std::move(step2Candidates);
        step2Ctx["partner1List"]  = std::move(partner1List);
        step2Ctx["furnitureList"] = BuildFurnitureArrayJson(initiatorActor);
        std::string step2ContextJson = step2Ctx.dump();

        SKSE::log::info("SkyrimNetDecisions::EvaluateLocationScan Step 1 selected initiator='{}', queuing Step 2 (partner_1, intent, furniture)",
                        initiatorSnap.name);

        auto step2CallbackHolder = std::make_shared<std::function<void(const char*, const char*, int)>>();
        *step2CallbackHolder = [step2CallbackHolder, snaps, initiatorSnap, step2Ctx, step2ContextJson,
                                templateName, scanGen, fallbackToChat, makeIgnoreAction, runStep4](
                                   const char*, const char* step2ResultJson, int step2Success) {
            if (OStimNet::LocationScanService::GetSingleton().GetGeneration() != scanGen) {
                SKSE::log::info("SkyrimNetDecisions::EvaluateLocationScan Step 2 callback: scan gen={} superseded, discarding",
                                scanGen);
                return;
            }

            auto retryStep2 = [step2CallbackHolder, templateName, step2ContextJson]() {
                if (PublicSendCustomDecisionToLLM) {
                    PublicSendCustomDecisionToLLM(
                        templateName.c_str(), step2ContextJson.c_str(),
                        kDefaultMaxProbabilities, *step2CallbackHolder);
                }
            };

            if (!step2Success || !step2ResultJson || step2ResultJson[0] == '\0') {
                std::string errCode, errMsg;
                if (ShouldFallbackToChatOnFailure(step2ResultJson, errCode, errMsg) && fallbackToChat) {
                    RunFallbackOnGameThread(fallbackToChat);
                    return;
                }
                ShowDecisionRetryModal("Location Scan (Step 2: Partner & Intent)", errMsg, retryStep2, makeIgnoreAction());
                return;
            }

            auto parsed2 = nlohmann::json::parse(step2ResultJson, nullptr, /*allow_exceptions=*/false);
            if (parsed2.is_discarded() || !parsed2.contains("answers") || !parsed2["answers"].is_object()) {
                ShowDecisionRetryModal("Location Scan (Step 2: Partner & Intent)", "Unreadable decision response.", retryStep2, makeIgnoreAction());
                return;
            }

            const auto& answers2          = parsed2["answers"];
            std::string partner1Key       = GetChoiceAnswer(answers2, "partner_1", "none");
            std::string resolvedIntent    = GetChoiceAnswer(answers2, "intent", "lustful");
            std::string resolvedFurniture = GetChoiceAnswer(answers2, "furniture", "floor");

            if (partner1Key == "none") {
                SKSE::log::info("SkyrimNetDecisions::EvaluateLocationScan Step 2: partner_1='none', no scene started");
                OStimNet::FireModEvent("ostimnet_location_scan_result", "", 2.0f);
                return;
            }

            if (partner1Key == "solo") {
                SKSE::log::info("SkyrimNetDecisions::EvaluateLocationScan Step 2: solo scene selected for '{}', advancing to Step 4",
                                initiatorSnap.name);
                runStep4({initiatorSnap}, resolvedIntent, resolvedFurniture);
                return;
            }

            const CandidateSnap* partner1Ptr = nullptr;
            for (const auto& s : snaps) {
                if (s.key == partner1Key && s.key != initiatorSnap.key) {
                    partner1Ptr = &s;
                    break;
                }
            }
            if (!partner1Ptr) {
                SKSE::log::info("SkyrimNetDecisions::EvaluateLocationScan Step 2: partner_1='{}' not found, aborting",
                                partner1Key);
                OStimNet::FireModEvent("ostimnet_location_scan_result", "", 2.0f);
                return;
            }

            CandidateSnap partner1Snap = *partner1Ptr;
            nlohmann::json partner2List = nlohmann::json::array();
            nlohmann::json step3Candidates = step2Ctx["candidatesList"];
            for (auto& cObj : step3Candidates) {
                std::string k = cObj.value("key", "");
                bool isP1 = (k == partner1Snap.key);
                cObj["isPartner1"] = isP1;
                if (k != initiatorSnap.key && !isP1) {
                    partner2List.push_back(cObj);
                }
            }

            if (partner2List.empty()) {
                SKSE::log::info("SkyrimNetDecisions::EvaluateLocationScan Step 2: no other candidates for partner_2, advancing directly to Step 4");
                runStep4({initiatorSnap, partner1Snap}, resolvedIntent, resolvedFurniture);
                return;
            }

            nlohmann::json step3Ctx    = step2Ctx;
            step3Ctx["step"]           = 3;
            step3Ctx["partner1Name"]   = partner1Snap.name;
            step3Ctx["intent"]         = resolvedIntent;
            step3Ctx["furniture"]      = resolvedFurniture;
            step3Ctx["candidatesList"] = std::move(step3Candidates);
            step3Ctx["partner2List"]   = std::move(partner2List);
            std::string step3ContextJson = step3Ctx.dump();

            SKSE::log::info("SkyrimNetDecisions::EvaluateLocationScan Step 2 selected partner_1='{}' (intent='{}', furniture='{}'), queuing Step 3 (optional 3rd participant)",
                            partner1Snap.name, resolvedIntent, resolvedFurniture);

            auto step3CallbackHolder = std::make_shared<std::function<void(const char*, const char*, int)>>();
            *step3CallbackHolder = [step3CallbackHolder, snaps, initiatorSnap, partner1Snap,
                                    resolvedIntent, resolvedFurniture, step3ContextJson,
                                    templateName, scanGen, fallbackToChat, makeIgnoreAction, runStep4](
                                       const char*, const char* step3ResultJson, int step3Success) {
                if (OStimNet::LocationScanService::GetSingleton().GetGeneration() != scanGen) {
                    SKSE::log::info("SkyrimNetDecisions::EvaluateLocationScan Step 3 callback: scan gen={} superseded, discarding",
                                    scanGen);
                    return;
                }

                auto retryStep3 = [step3CallbackHolder, templateName, step3ContextJson]() {
                    if (PublicSendCustomDecisionToLLM) {
                        PublicSendCustomDecisionToLLM(
                            templateName.c_str(), step3ContextJson.c_str(),
                            kDefaultMaxProbabilities, *step3CallbackHolder);
                    }
                };

                if (!step3Success || !step3ResultJson || step3ResultJson[0] == '\0') {
                    std::string errCode, errMsg;
                    if (ShouldFallbackToChatOnFailure(step3ResultJson, errCode, errMsg) && fallbackToChat) {
                        RunFallbackOnGameThread(fallbackToChat);
                        return;
                    }
                    ShowDecisionRetryModal("Location Scan (Step 3: 3rd Participant)", errMsg, retryStep3, makeIgnoreAction());
                    return;
                }

                auto parsed3 = nlohmann::json::parse(step3ResultJson, nullptr, /*allow_exceptions=*/false);
                if (parsed3.is_discarded() || !parsed3.contains("answers") || !parsed3["answers"].is_object()) {
                    ShowDecisionRetryModal("Location Scan (Step 3: 3rd Participant)", "Unreadable decision response.", retryStep3, makeIgnoreAction());
                    return;
                }

                std::string partner2Key = GetChoiceAnswer(parsed3["answers"], "partner_2", "none");
                std::vector<CandidateSnap> sceneSnaps = {initiatorSnap, partner1Snap};
                if (partner2Key != "none") {
                    for (const auto& s : snaps) {
                        if (s.key == partner2Key && s.key != initiatorSnap.key && s.key != partner1Snap.key) {
                            sceneSnaps.push_back(s);
                            break;
                        }
                    }
                }

                SKSE::log::info("SkyrimNetDecisions::EvaluateLocationScan Step 3 complete (participants={}), advancing to Step 4",
                                sceneSnaps.size());
                runStep4(sceneSnaps, resolvedIntent, resolvedFurniture);
            };

            bool queued3 = PublicSendCustomDecisionToLLM(
                templateName.c_str(), step3ContextJson.c_str(), kDefaultMaxProbabilities, *step3CallbackHolder);
            if (!queued3 && fallbackToChat) {
                RunFallbackOnGameThread(fallbackToChat);
            }
        };

        bool queued2 = PublicSendCustomDecisionToLLM(
            templateName.c_str(), step2ContextJson.c_str(), kDefaultMaxProbabilities, *step2CallbackHolder);
        if (!queued2 && fallbackToChat) {
            RunFallbackOnGameThread(fallbackToChat);
        }
    };

    bool queued = PublicSendCustomDecisionToLLM(
        templateName.c_str(), step1ContextJson.c_str(), kDefaultMaxProbabilities, *step1CallbackHolder);
    if (!queued && fallbackToChat) {
        fallbackToChat();
        return true;
    }
    return queued;
}

// =============================================================================
// 7. EvaluateScheduledSceneAdvance
// =============================================================================

bool EvaluateScheduledSceneAdvance(
    int threadID,
    std::function<void()> onDone,
    std::function<void()> fallbackToChat) {
    if (!IsAvailable()) return false;

    if (!g_ostimThreadInterface || !g_ostimThreadInterface->IsThreadValid(static_cast<uint32_t>(threadID)) ||
        !ThreadDataStore::GetSingleton().IsOStimNet(threadID)) {
        SKSE::log::info("SkyrimNetDecisions::EvaluateScheduledSceneAdvance: thread {} is no longer valid, skipping", threadID);
        onDone();
        return false;
    }

    auto& store       = ThreadDataStore::GetSingleton();
    const auto& actors = store.GetActorPtrs(threadID);
    if (actors.empty()) {
        onDone();
        return false;
    }

    const auto& mainPtrs = store.GetMainActors(threadID);
    const auto& secPtrs  = store.GetSecondaryActors(threadID);

    std::vector<std::string> mainNameVec, secNameVec;
    for (auto* a : (mainPtrs.empty() ? actors : mainPtrs)) {
        if (a) mainNameVec.push_back(SanitizeSingleLine(ThreadDataStore::GetActorDisplayName(a, "Actor"), "Actor"));
    }
    for (auto* a : secPtrs) {
        if (a) secNameVec.push_back(SanitizeSingleLine(ThreadDataStore::GetActorDisplayName(a, "Actor"), "Actor"));
    }

    nlohmann::json participantsList = nlohmann::json::array();
    nlohmann::json eventFilterUUIDs = nlohmann::json::array();
    nlohmann::json actorUUIDs       = nlohmann::json::array();
    std::vector<std::string> allNameVec;

    for (size_t i = 0; i < actors.size(); ++i) {
        RE::Actor* a = actors[i];
        if (!a) continue;
        uint64_t uuid = ActorToUUID(a);
        if (uuid == 0) continue;

        bool isMain = std::find(mainPtrs.begin(), mainPtrs.end(), a) != mainPtrs.end();
        std::string displayName = SanitizeSingleLine(ThreadDataStore::GetActorDisplayName(a, "Actor"), "Actor");
        allNameVec.push_back(displayName);
        nlohmann::json pObj;
        pObj["key"]  = "p_" + std::to_string(i + 1);
        pObj["uuid"] = uuid;
        pObj["name"] = displayName;
        pObj["role"] = isMain ? "main" : "secondary";
        participantsList.push_back(std::move(pObj));
        eventFilterUUIDs.push_back(uuid);
        actorUUIDs.push_back(uuid);
    }

    if (participantsList.empty()) {
        onDone();
        return false;
    }

    bool initialAdvance = false;
    const char* curSceneId = g_ostimThreadInterface->GetCurrentSceneID(static_cast<uint32_t>(threadID));
    if (curSceneId && curSceneId[0] != '\0') {
        if ((ONavIsIdle && ONavIsIdle(curSceneId)) || (ONavIsIntro && ONavIsIntro(curSceneId))) {
            initialAdvance = true;
        }
    }

    nlohmann::json ctx;
    ctx["actors"]              = std::move(actorUUIDs);
    ctx["actorNames"]          = OStimNet::FormatActorList(allNameVec);
    ctx["participantsList"]    = std::move(participantsList);
    ctx["eventFilterUUIDs"]    = std::move(eventFilterUUIDs);
    ctx["mainActorNames"]      = OStimNet::FormatActorList(mainNameVec, "Participants");
    ctx["secondaryActorNames"] = OStimNet::FormatActorList(secNameVec, "");
    ctx["sceneDescription"]    = OStimNet::GetSceneDescription(static_cast<uint32_t>(threadID));
    ctx["intent"]              = OStimNet::IntentToString(store.GetIntent(threadID));
    ctx["initialAdvance"]      = initialAdvance;
    ctx["activities"]          = BuildThreadActivitiesArrayJson(threadID, "sexual");
    ctx["positions"]           = BuildPositionsArrayJson(threadID);
    ctx["threadID"]            = threadID;

    std::string contextJson  = ctx.dump();
    std::string templateName = "ostimnet_evaluate_scene_advance";

    SKSE::log::info("SkyrimNetDecisions::EvaluateScheduledSceneAdvance: queuing Jev decision for thread {} (initialAdvance={})",
                    threadID, initialAdvance);

    auto callbackHolder = std::make_shared<std::function<void(const char*, const char*, int)>>();
    *callbackHolder = [callbackHolder, threadID, initialAdvance, onDone,
                       contextJson, templateName, fallbackToChat](
                          const char* /*answer*/, const char* resultJson, int success) {
        if (!g_ostimThreadInterface || !g_ostimThreadInterface->IsThreadValid(static_cast<uint32_t>(threadID)) ||
            !ThreadDataStore::GetSingleton().IsOStimNet(threadID)) {
            onDone();
            return;
        }
        auto sexual = ThreadDataStore::GetSingleton().GetSexual(threadID);
        if (!sexual.value_or(false)) {
            onDone();
            return;
        }

        auto retryFunc = [callbackHolder, templateName, contextJson]() {
            if (PublicSendCustomDecisionToLLM) {
                PublicSendCustomDecisionToLLM(
                    templateName.c_str(), contextJson.c_str(),
                    kDefaultMaxProbabilities, *callbackHolder);
            }
        };

        if (!success || !resultJson || resultJson[0] == '\0') {
            std::string errCode, errMsg;
            if (ShouldFallbackToChatOnFailure(resultJson, errCode, errMsg) && fallbackToChat) {
                SKSE::log::warn("SkyrimNetDecisions::EvaluateScheduledSceneAdvance: Jev error '{}' ({}) — falling back to GM chat LLM",
                                errCode, errMsg);
                RunFallbackOnGameThread(fallbackToChat);
                return;
            }
            ShowDecisionRetryModal("Game Master Scene Advance", errMsg, retryFunc, onDone);
            return;
        }

        auto parsed = nlohmann::json::parse(resultJson, nullptr, /*allow_exceptions=*/false);
        if (parsed.is_discarded() || !parsed.contains("answers") || !parsed["answers"].is_object()) {
            ShowDecisionRetryModal("Game Master Scene Advance", "Unreadable decision response.", retryFunc, onDone);
            return;
        }

        const auto& answers = parsed["answers"];
        std::string rawAct  = GetChoiceAnswer(answers, "activity", initialAdvance ? "any" : "hold");
        std::string rawPos  = GetChoiceAnswer(answers, "position", "any");

        std::string normPos = NormalizePositionChoice(rawPos);
        std::string normAct = NormalizeActivityChoice(rawAct);

        if (!normAct.empty() && !normPos.empty()) {
            auto sexActs = BuildThreadActivitiesArrayJson(threadID, "sex");
            bool isSexAct = false;
            for (const auto& item : sexActs) {
                if (item.is_object() && item.value("id", "") == normAct) {
                    isSexAct = true;
                    break;
                }
            }
            if (!isSexAct) {
                normPos.clear();
            }
        }

        if (!initialAdvance && normPos.empty() && normAct.empty()) {
            SKSE::log::info("SkyrimNetDecisions::EvaluateScheduledSceneAdvance: both position and activity hold/generic/any for thread {}, holding",
                            threadID);
            onDone();
            return;
        }

        nlohmann::json result;
        result["threadID"]         = threadID;
        result["sexualPosition"]   = normPos;
        result["sexualActivities"] = normAct;

        std::string jsonStr = result.dump();
        SKSE::log::info("SkyrimNetDecisions::EvaluateScheduledSceneAdvance: success -> {}", jsonStr);
        OStimNet::FireModEvent("ostimnet_scene_change_schedule_finished", jsonStr.c_str(), 1.0f);
        onDone();
    };

    bool queued = PublicSendCustomDecisionToLLM(
        templateName.c_str(), contextJson.c_str(), kDefaultMaxProbabilities, *callbackHolder);
    if (!queued) {
        if (fallbackToChat) {
            fallbackToChat();
            return true;
        }
        onDone();
    }
    return queued;
}

// =============================================================================
// 8. EvaluatePulloutDecision
// =============================================================================

bool EvaluatePulloutDecision(
    int threadID,
    std::function<void()> fallbackToChat) {
    if (!IsAvailable()) return false;

    if (!g_ostimThreadInterface || !g_ostimThreadInterface->IsThreadValid(static_cast<uint32_t>(threadID)) ||
        !ThreadDataStore::GetSingleton().IsOStimNet(threadID)) {
        PulloutService::GetSingleton().OnLLMFailure(threadID);
        return false;
    }

    auto allActors = ThreadDataStore::GetSingleton().GetActorPtrs(threadID);
    if (allActors.size() < 2) {
        PulloutService::GetSingleton().OnLLMFailure(threadID);
        return false;
    }

    RE::Actor* maleActor   = nullptr;
    RE::Actor* femaleActor = nullptr;

    const char* curScene = g_ostimThreadInterface->GetCurrentSceneID(static_cast<uint32_t>(threadID));
    auto pairs           = PulloutService::GetVaginalPairs(curScene);

    constexpr uint32_t kMaxActors = 8;
    OstimNG_API::Thread::ActorData buffer[kMaxActors];
    uint32_t count = g_ostimThreadInterface->GetActors(static_cast<uint32_t>(threadID), buffer, kMaxActors);

    int peakingGiverSlot     = -1;
    int matchingReceiverSlot = -1;
    float maxEx              = -1.0f;
    for (const auto& pair : pairs) {
        if (pair.giverSlot >= 0 && static_cast<uint32_t>(pair.giverSlot) < count) {
            float ex = buffer[pair.giverSlot].excitement;
            if (ex > maxEx) {
                maxEx                = ex;
                peakingGiverSlot     = pair.giverSlot;
                matchingReceiverSlot = pair.receiverSlot;
            }
        }
    }

    if (peakingGiverSlot >= 0 && static_cast<uint32_t>(peakingGiverSlot) < count)
        maleActor = RE::TESForm::LookupByID<RE::Actor>(buffer[peakingGiverSlot].formID);
    if (matchingReceiverSlot >= 0 && static_cast<uint32_t>(matchingReceiverSlot) < count)
        femaleActor = RE::TESForm::LookupByID<RE::Actor>(buffer[matchingReceiverSlot].formID);

    if (!maleActor)   maleActor   = allActors[0];
    if (!femaleActor) femaleActor = (allActors.size() > 1 && allActors[1] != maleActor) ? allActors[1] : allActors[0];

    uint64_t maleUUID   = ActorToUUID(maleActor);
    uint64_t femaleUUID = ActorToUUID(femaleActor);
    if (maleUUID == 0 || femaleUUID == 0) {
        PulloutService::GetSingleton().OnLLMFailure(threadID);
        return false;
    }

    std::string maleName   = SanitizeSingleLine(ThreadDataStore::GetActorDisplayName(maleActor, "Partner"), "Partner");
    std::string femaleName = SanitizeSingleLine(ThreadDataStore::GetActorDisplayName(femaleActor, "Receiver"), "Receiver");

    nlohmann::json eventFilterUUIDs = nlohmann::json::array({maleUUID, femaleUUID});

    nlohmann::json ctx;
    ctx["maleActor"]        = maleUUID;
    ctx["femaleActor"]      = femaleUUID;
    ctx["maleName"]         = maleName;
    ctx["femaleName"]       = femaleName;
    ctx["eventFilterUUIDs"] = std::move(eventFilterUUIDs);
    ctx["intent"]           = OStimNet::IntentToString(ThreadDataStore::GetSingleton().GetIntent(threadID));
    ctx["sceneDescription"] = OStimNet::GetSceneDescription(static_cast<uint32_t>(threadID));
    ctx["threadID"]         = threadID;

    std::string contextJson  = ctx.dump();
    std::string templateName = "ostimnet_evaluate_pullout_decision";

    SKSE::log::info("SkyrimNetDecisions::EvaluatePulloutDecision: queuing Jev decision for thread {} (male='{}', female='{}')",
                    threadID, maleName, femaleName);

    auto callbackHolder = std::make_shared<std::function<void(const char*, const char*, int)>>();
    *callbackHolder = [callbackHolder, threadID, contextJson, templateName, fallbackToChat](
                          const char* answer, const char* resultJson, int success) {
        if (!g_ostimThreadInterface || !g_ostimThreadInterface->IsThreadValid(static_cast<uint32_t>(threadID)) ||
            !ThreadDataStore::GetSingleton().IsOStimNet(threadID)) {
            PulloutService::GetSingleton().OnLLMFailure(threadID);
            return;
        }

        auto retryAction = [callbackHolder, templateName, contextJson]() {
            if (PublicSendCustomDecisionToLLM) {
                PublicSendCustomDecisionToLLM(
                    templateName.c_str(), contextJson.c_str(),
                    kDefaultMaxProbabilities, *callbackHolder);
            }
        };
        auto ignoreAction = [threadID]() {
            PulloutService::GetSingleton().OnLLMFailure(threadID);
        };

        if (!success || !resultJson || resultJson[0] == '\0') {
            std::string errCode, errMsg;
            if (ShouldFallbackToChatOnFailure(resultJson, errCode, errMsg) && fallbackToChat) {
                SKSE::log::warn("SkyrimNetDecisions::EvaluatePulloutDecision: Jev error '{}' ({}) — falling back to GM chat LLM",
                                errCode, errMsg);
                RunFallbackOnGameThread(fallbackToChat);
                return;
            }
            ShowDecisionRetryModal("Pullout Decision", errMsg, retryAction, ignoreAction);
            return;
        }

        std::string decision = "finish_inside";
        if (answer && std::string(answer) == "pullout") {
            decision = "pullout";
        } else {
            auto parsed = nlohmann::json::parse(resultJson, nullptr, /*allow_exceptions=*/false);
            if (!parsed.is_discarded() && parsed.contains("answers")) {
                std::string ch = GetChoiceAnswer(parsed["answers"], "decision", "finish_inside");
                if (ch == "pullout") decision = "pullout";
            }
        }

        SKSE::log::info("SkyrimNetDecisions::EvaluatePulloutDecision: Jev returned decision='{}' for thread {}",
                        decision, threadID);
        PulloutService::GetSingleton().OnLLMDecision(threadID, decision);
    };

    bool queued = PublicSendCustomDecisionToLLM(
        templateName.c_str(), contextJson.c_str(), kDefaultMaxProbabilities, *callbackHolder);
    if (!queued) {
        if (fallbackToChat) {
            fallbackToChat();
            return true;
        }
        PulloutService::GetSingleton().OnLLMFailure(threadID);
        return false;
    }
    return true;
}

}  // namespace OStimNet::SkyrimNetDecisions

