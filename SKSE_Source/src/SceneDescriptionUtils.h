#pragma once
#include "api/OstimNG-API-Thread.h"
#include "api/OStimNavigator_PublicAPI.h"
#include "Config.h"
#include "ThreadRegistry.h"
#include <string>
#include <vector>
#include <cctype>

namespace OStimNet {


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

// Returns a scene description based on the current config:
//   - useCustomDescriptions == true  → custom (hand-authored) description,
//                                       falling back to auto-generated when none exists.
//   - useCustomDescriptions == false → auto-generated description only.
// Returns an empty string if the thread is invalid or has no active scene.
inline std::string GetSceneDescription(uint32_t threadID, const std::string& sceneID) {
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

    if (!result.empty() && g_ostimThreadInterface && g_ostimThreadInterface->IsThreadValid(threadID)) {
        int32_t maxSpeed = g_ostimThreadInterface->GetMaxSpeed(threadID);
        if (maxSpeed > 1) {
            int32_t currentSpeed = g_ostimThreadInterface->GetCurrentSpeed(threadID);
            if (currentSpeed > 0) {
                int32_t percent = (currentSpeed * 100) / maxSpeed;
                if (percent >= 0 && percent <= 33) {
                    result += " The pace is slow and unhurried.";
                } else if (percent >= 67 && percent <= 100) {
                    result += " The pace is fast and vigorous.";
                }
            }
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

}  // namespace OStimNet
