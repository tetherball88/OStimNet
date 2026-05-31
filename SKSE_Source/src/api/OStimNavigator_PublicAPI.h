/*
 * OStimNavigator Public API — loaded at runtime via LoadLibraryA + GetProcAddress.
 *
 * Drop this header into your SKSE plugin project. Call ONavFindFunctions() once
 * during initialization (e.g., SKSE kDataLoaded message). If it returns true,
 * ONavBuildSceneDescription is ready to use.
 *
 * Quick start:
 * @code
 *   void OnDataLoaded() {
 *       if (!ONavFindFunctions()) {
 *           logger::warn("OStimNavigator not found — scene descriptions unavailable");
 *           return;
 *       }
 *       // Later, when a scene is active:
 *       if (ONavBuildSceneDescription) {
 *           const char* desc = ONavBuildSceneDescription("SomeModpack|SomeScene", threadID);
 *           std::string description = desc ? desc : "";  // copy immediately
 *       }
 *   }
 * @endcode
 *
 * ABI safety: Only plain C types (const char*, uint32_t) cross the DLL boundary.
 * The hardcoded tables are header-only and never cross the boundary.
 * Safe to include from any compiler or CRT linkage.
 */

#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <windows.h>

// =============================================================================
// Runtime function pointer
// =============================================================================

/**
 * Build a human-readable description of a scene currently running in an OStim thread.
 *
 * @param sceneId   Scene ID string (e.g. "SomeModpack|SomeScene"). Must not be null.
 * @param threadID  OStim thread ID to resolve live actor data (gender, furniture, etc.).
 *
 * @return A pointer to a null-terminated string inside OStimNavigator.dll's
 *         internal static buffer. COPY IT IMMEDIATELY — it is overwritten on
 *         the next call. Returns "" (never null) if the scene is unknown.
 *
 * @note Not thread-safe. Call only from the SKSE game thread.
 */
#ifndef OSTIMNAVIGATOR_BUILDING
inline const char* (*ONavBuildSceneDescription)(const char* sceneId, uint32_t threadID) = nullptr;
#endif

/**
 * Get the custom animation description for a scene by its ID, as authored in the
 * OStimNet animation description JSON files.
 *
 * @param sceneId  Scene ID string (e.g. "SomeModpack|SomeScene"). Must not be null.
 *                 The lookup is case-insensitive.
 *
 * @return A pointer to a null-terminated string inside OStimNavigator.dll's
 *         internal static buffer. COPY IT IMMEDIATELY — it is overwritten on
 *         the next call. Returns "" (never null) if no description exists for
 *         the given scene ID.
 *
 * @note Not thread-safe. Call only from the SKSE game thread.
 */
#ifndef OSTIMNAVIGATOR_BUILDING
inline const char* (*ONavGetAnimationDescription)(const char* sceneId) = nullptr;
#endif

/**
 * Check whether a scene has an "idle" tag.
 *
 * @param sceneId  Scene ID string (e.g. "SomeModpack|SomeScene"). Must not be null.
 *
 * @return true if the scene carries an "idle" tag; false otherwise
 *         (including when the scene ID is unknown or OStimNavigator is not loaded).
 *
 * @note Not thread-safe. Call only from the SKSE game thread.
 */
#ifndef OSTIMNAVIGATOR_BUILDING
inline bool (*ONavIsIdle)(const char* sceneId) = nullptr;
#endif

/**
 * Check whether a scene has an "intro" tag.
 *
 * @param sceneId  Scene ID string (e.g. "SomeModpack|SomeScene"). Must not be null.
 *
 * @return true if the scene carries an "intro" tag; false otherwise
 *         (including when the scene ID is unknown or OStimNavigator is not loaded).
 *
 * @note Not thread-safe. Call only from the SKSE game thread.
 */
#ifndef OSTIMNAVIGATOR_BUILDING
inline bool (*ONavIsIntro)(const char* sceneId) = nullptr;
#endif

/**
 * Check whether a scene is a transition.
 *
 * @param sceneId  Scene ID string (e.g. "SomeModpack|SomeScene"). Must not be null.
 *
 * @return true if the scene is a transition; false otherwise
 *         (including when the scene ID is unknown or OStimNavigator is not loaded).
 *
 * @note Not thread-safe. Call only from the SKSE game thread.
 */
#ifndef OSTIMNAVIGATOR_BUILDING
inline bool (*ONavIsTransit)(const char* sceneId) = nullptr;
#endif

/**
 * Return all counterparts who can receive cum from the given actor in the given scene,
 * along with the body areas involved, as a JSON array.
 *
 * @param sceneId        Scene ID string (e.g. "SomeModpack|SomeScene"). Must not be null.
 * @param actorPosition  0-based index of the actor with the penis role.
 *
 * @return A pointer to a null-terminated JSON string inside OStimNavigator.dll's
 *         internal static buffer. COPY IT IMMEDIATELY — it is overwritten on
 *         the next call.
 *         Format: [{"counterpartIndex":1,"areas":["vagina"]},{"counterpartIndex":2,"areas":["mouth"]}]
 *         Returns "[]" (never null) if no matching penetration actions are found.
 *
 * @note Not thread-safe. Call only from the SKSE game thread.
 */
#ifndef OSTIMNAVIGATOR_BUILDING
inline const char* (*ONavGetCumTargets)(const char* sceneId, int actorPosition) = nullptr;
#endif

/**
 * Return all position strings that appear in at least one loaded scene, as a JSON array.
 *
 * @return A pointer to a null-terminated JSON string inside OStimNavigator.dll's
 *         internal static buffer. COPY IT IMMEDIATELY — it is overwritten on
 *         the next call.
 *         Format: ["missionary","cowgirl", ...] (sorted, no duplicates)
 *         Returns "[]" (never null) if no scenes are loaded.
 *
 * @note Not thread-safe. Call only from the SKSE game thread.
 */
#ifndef OSTIMNAVIGATOR_BUILDING
inline const char* (*ONavGetAllPositions)() = nullptr;
#endif

/**
 * Return the canonical (hardcoded) list of all known position names, as a JSON array.
 * This is the authoritative list regardless of which scenes are currently loaded.
 *
 * @return A pointer to a null-terminated JSON string inside OStimNavigator.dll's
 *         internal static buffer. COPY IT IMMEDIATELY — it is overwritten on
 *         the next call.
 *         Format: ["missionary","cowgirl", ...] (in definition order)
 *         Returns "[]" (never null).
 *
 * @note Not thread-safe. Call only from the SKSE game thread.
 */
#ifndef OSTIMNAVIGATOR_BUILDING
inline const char* (*ONavGetCanonicalPositions)() = nullptr;
#endif

/**
 * Return the canonical alias → position map as a JSON object.
 * Keys are alias strings (e.g. "doggy"); values are canonical position names (e.g. "doggystyle").
 * All keys are lowercase.
 *
 * @return A pointer to a null-terminated JSON string inside OStimNavigator.dll's
 *         internal static buffer. COPY IT IMMEDIATELY — it is overwritten on
 *         the next call.
 *         Format: {"doggy":"doggystyle","rcg":"reversecowgirl", ...}
 *         Returns "{}" (never null).
 *
 * @note Not thread-safe. Call only from the SKSE game thread.
 */
#ifndef OSTIMNAVIGATOR_BUILDING
inline const char* (*ONavGetPositionAliases)() = nullptr;
#endif

/**
 * Return the position canonical-ID → display-name map as a JSON object.
 * Values are parenthetical description strings (empty string for positions whose name is self-explanatory).
 *
 * @return A pointer to a null-terminated JSON string inside OStimNavigator.dll's
 *         internal static buffer. COPY IT IMMEDIATELY — it is overwritten on
 *         the next call.
 *         Format: {"missionary":"","butterfly":"(missionary, receiver's hips raised)", ...}
 *         Returns "{}" (never null).
 *
 * @note Not thread-safe. Call only from the SKSE game thread.
 */
#ifndef OSTIMNAVIGATOR_BUILDING
inline const char* (*ONavGetPositionDisplayNames)() = nullptr;
#endif

/**
 * Return all action types that appear in at least one loaded scene, as a JSON array.
 * Only actions actually present in scenes are included — registered-but-unused actions
 * are excluded.
 *
 * @param tag  If non-null and non-empty, only return actions that have this tag
 *             (e.g. "sexual"). Pass null or "" to return all actions available in scenes.
 *
 * @return A pointer to a null-terminated JSON string inside OStimNavigator.dll's
 *         internal static buffer. COPY IT IMMEDIATELY — it is overwritten on
 *         the next call.
 *         Format: ["vaginalsex","blowjob", ...] (sorted, no duplicates)
 *         Returns "[]" (never null) if no scenes are loaded.
 *
 * @note Not thread-safe. Call only from the SKSE game thread.
 */
#ifndef OSTIMNAVIGATOR_BUILDING
inline const char* (*ONavGetAllActions)(const char* tag) = nullptr;
#endif

/**
 * Return the action types present in a specific scene, as a JSON array.
 *
 * @param sceneId  Scene ID string (e.g. "SomeModpack|SomeScene"). Must not be null.
 *
 * @return A pointer to a null-terminated JSON string inside OStimNavigator.dll's
 *         internal static buffer. COPY IT IMMEDIATELY — it is overwritten on
 *         the next call.
 *         Format: ["vaginalsex","blowjob", ...] (order of first appearance, no duplicates)
 *         Returns "[]" (never null) if the scene is unknown or has no actions.
 *
 * @note Not thread-safe. Call only from the SKSE game thread.
 */
#ifndef OSTIMNAVIGATOR_BUILDING
inline const char* (*ONavGetSceneActions)(const char* sceneId) = nullptr;
#endif

/**
 * Return the scene-level tags for a scene as a JSON array of strings.
 * Tags include position names (e.g. "missionary", "cowgirl") and any other
 * metadata tags authored in the scene JSON.
 *
 * @param sceneId  Scene ID string (e.g. "SomeModpack|SomeScene"). Must not be null.
 *
 * @return A pointer to a null-terminated JSON string inside OStimNavigator.dll's
 *         internal static buffer. COPY IT IMMEDIATELY — it is overwritten on
 *         the next call.
 *         Format: ["missionary","solo", ...] (order of definition, no duplicates)
 *         Returns "[]" (never null) if the scene is unknown or has no tags.
 *
 * @note Not thread-safe. Call only from the SKSE game thread.
 */
#ifndef OSTIMNAVIGATOR_BUILDING
inline const char* (*ONavGetSceneTags)(const char* sceneId) = nullptr;
#endif

/**
 * Return all furniture type IDs that have at least the given number of sexual scenes
 * associated with them, as a JSON array.
 *
 * @param minSceneCount  Minimum number of sexual scenes required (e.g. 5). Must be >= 1.
 *
 * @return A pointer to a null-terminated JSON string inside OStimNavigator.dll's
 *         internal static buffer. COPY IT IMMEDIATELY — it is overwritten on
 *         the next call.
 *         Format: [{"id":"bed","distance":250.5}, ...] (sorted by id, no duplicates)
 *         The "distance" field is the distance in units from centerRef to the nearest
 *         matching furniture piece, or -1.0 when centerRefID is 0 (no scan performed).
 *         Returns "[]" (never null) if no furniture types meet the threshold.
 *
 * @note Not thread-safe. Call only from the SKSE game thread.
 */
#ifndef OSTIMNAVIGATOR_BUILDING
inline const char* (*ONavGetFurnitureTypesWithSexScenes)(uint32_t minSceneCount, uint32_t centerRefID, float radius) = nullptr;
#endif

/**
 * Check whether any actor position in a scene carries the given tag.
 *
 * @param sceneId  Scene ID string (e.g. "SomeModpack|SomeScene"). Must not be null.
 * @param tag      Actor-position tag to check for (e.g. "climaxing"). Must not be null.
 *
 * @return true if at least one actor position in the scene carries this tag;
 *         false otherwise (including when the scene is unknown or OStimNavigator is not loaded).
 *
 * @note Not thread-safe. Call only from the SKSE game thread.
 */
#ifndef OSTIMNAVIGATOR_BUILDING
inline bool (*ONavSceneHasActorWithTag)(const char* sceneId, const char* tag) = nullptr;
#endif

/**
 * Check whether a scene requires furniture.
 *
 * @param sceneId  Scene ID string (e.g. "SomeModpack|SomeScene"). Must not be null.
 *
 * @return true if the scene's furnitureType is non-empty (i.e. a piece of furniture is needed);
 *         false if the scene has no furniture requirement or the scene is unknown.
 *
 * @note Not thread-safe. Call only from the SKSE game thread.
 */
#ifndef OSTIMNAVIGATOR_BUILDING
inline bool (*ONavSceneHasFurniture)(const char* sceneId) = nullptr;
#endif

/**
 * Return the highest sexual encounter phase rank present in a scene's actions.
 *
 * Phase ranks:
 *   1 = Foreplay  — actions tagged "penilestimulation", "fingering", "toying", or "clitoralstimulation"
 *   2 = Oral      — actions tagged "oral"
 *   3 = Sex       — actions tagged "intercourse"
 *  -1 = No phase-relevant actions found, or scene is unknown
 *
 * @param sceneId  Scene ID string (e.g. "SomeModpack|SomeScene"). Must not be null.
 *
 * @return Highest rank found (1, 2, or 3), or -1.
 *         These values correspond directly to the OStimNet::SexualPhase enum
 *         (Foreplay=1, Oral=2, Sex=3).
 *
 * @note Not thread-safe. Call only from the SKSE game thread.
 */
#ifndef OSTIMNAVIGATOR_BUILDING
inline int (*ONavGetScenePhaseRank)(const char* sceneId) = nullptr;
#endif

/**
 * Load OStimNavigator.dll and resolve all exported function pointers.
 *
 * Call once during plugin initialization (kDataLoaded is recommended so the
 * scene database is already populated when you make your first call).
 *
 * @return true if OStimNavigator.dll was found and ONavBuildSceneDescription resolved.
 *         Returns false (gracefully) when the mod is not installed.
 */
#ifndef OSTIMNAVIGATOR_BUILDING
inline bool ONavFindFunctions() {
    HMODULE hDLL = LoadLibraryA("OStimNavigator");
    if (!hDLL) return false;

    ONavBuildSceneDescription = reinterpret_cast<const char*(*)(const char*, uint32_t)>(
        GetProcAddress(hDLL, "ONavBuildSceneDescription"));

    ONavGetAnimationDescription = reinterpret_cast<const char*(*)(const char*)>(
        GetProcAddress(hDLL, "ONavGetAnimationDescription"));

    ONavIsIdle = reinterpret_cast<bool(*)(const char*)>(
        GetProcAddress(hDLL, "ONavIsIdle"));

    ONavIsIntro = reinterpret_cast<bool(*)(const char*)>(
        GetProcAddress(hDLL, "ONavIsIntro"));

    ONavIsTransit = reinterpret_cast<bool(*)(const char*)>(
        GetProcAddress(hDLL, "ONavIsTransit"));

    ONavGetCumTargets = reinterpret_cast<const char*(*)(const char*, int)>(
        GetProcAddress(hDLL, "ONavGetCumTargets"));

    ONavGetAllPositions = reinterpret_cast<const char*(*)()>(
        GetProcAddress(hDLL, "ONavGetAllPositions"));

    ONavGetCanonicalPositions = reinterpret_cast<const char*(*)()>(
        GetProcAddress(hDLL, "ONavGetCanonicalPositions"));

    ONavGetPositionAliases = reinterpret_cast<const char*(*)()>(
        GetProcAddress(hDLL, "ONavGetPositionAliases"));

    ONavGetPositionDisplayNames = reinterpret_cast<const char*(*)()>(
        GetProcAddress(hDLL, "ONavGetPositionDisplayNames"));

    ONavGetAllActions = reinterpret_cast<const char*(*)(const char*)>(
        GetProcAddress(hDLL, "ONavGetAllActions"));

    ONavGetSceneActions = reinterpret_cast<const char*(*)(const char*)>(
        GetProcAddress(hDLL, "ONavGetSceneActions"));

    ONavGetSceneTags = reinterpret_cast<const char*(*)(const char*)>(
        GetProcAddress(hDLL, "ONavGetSceneTags"));

    ONavGetFurnitureTypesWithSexScenes = reinterpret_cast<const char*(*)(uint32_t, uint32_t, float)>(
        GetProcAddress(hDLL, "ONavGetFurnitureTypesWithSexScenes"));

    ONavSceneHasActorWithTag = reinterpret_cast<bool(*)(const char*, const char*)>(
        GetProcAddress(hDLL, "ONavSceneHasActorWithTag"));

    ONavSceneHasFurniture = reinterpret_cast<bool(*)(const char*)>(
        GetProcAddress(hDLL, "ONavSceneHasFurniture"));

    ONavGetScenePhaseRank = reinterpret_cast<int(*)(const char*)>(
        GetProcAddress(hDLL, "ONavGetScenePhaseRank"));

    return ONavBuildSceneDescription != nullptr;
}
#endif

// =============================================================================
// Hardcoded lookup tables (header-only, no DLL boundary)
// =============================================================================
// These are identical to the tables inside OStimNavigator — kept in sync manually.
// They are compile-time constants in your own translation unit.

namespace OStimNavigatorAPI {






// ─── Intent enum — canonical definition shared between OStimNavigator and OStimNet ──
// OStimNet imports this via `using` aliases in ThreadDataStore.h.

enum class Intent {
    Unset = -1,
    Platonic,
    Romantic,
    Lustful,
    Transactional,
    Dom,
    Aggressive
};

inline Intent IntentFromString(const std::string& s) {
    std::string lower = s + ",";
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });

    size_t start = 0;
    size_t end = 0;
    while ((end = lower.find(',', start)) != std::string::npos) {
        std::string part = lower.substr(start, end - start);
        if (part == "platonic")      return Intent::Platonic;
        if (part == "romantic")      return Intent::Romantic;
        if (part == "lustful")       return Intent::Lustful;
        if (part == "transactional") return Intent::Transactional;
        if (part == "dom")           return Intent::Dom;
        if (part == "aggressive")    return Intent::Aggressive;
        start = end + 1;
    }

    return Intent::Unset;
}

inline const char* IntentToString(Intent intent) {
    switch (intent) {
        case Intent::Platonic:      return "platonic";
        case Intent::Romantic:      return "romantic";
        case Intent::Lustful:       return "lustful";
        case Intent::Transactional: return "transactional";
        case Intent::Dom:           return "dom";
        case Intent::Aggressive:    return "aggressive";
        default:                    return "unset";
    }
}

} // namespace OStimNavigatorAPI
