#pragma once
#include <string>
#include <windows.h>
#include <functional>

/**
 * SkyrimNet Public API — loaded at runtime via LoadLibraryA + GetProcAddress.
 *
 * Drop this header into your SKSE plugin project. Call FindFunctions() once
 * during initialization (e.g., SKSE kDataLoaded message). If it returns true,
 * the function pointers below are ready to use.
 *
 * ## Initialization timing
 *
 * Call FindFunctions() during kDataLoaded. After it returns true:
 *   - Action registration works immediately.
 *   - Data query functions are safe to call but return empty results until
 *     the database initializes — which happens when a save is loaded (or a
 *     new game starts). Use PublicIsMemorySystemReady() to check.
 *
 * ABI requirement: Both DLLs must use the same MSVC version and CRT linkage
 * (dynamic /MD). All CommonLibSSE-NG SKSE plugins satisfy this.
 *
 * Thread safety: All data query functions (v3+) are thread-safe and return
 * empty results gracefully if called before the database is ready. Action
 * registration should be done during plugin initialization only.
 */
extern "C" {

// =============================================================================
// Core (v2+)
// =============================================================================

/**
 * Returns the runtime API version (currently 12).
 * Version history: 2 = action registration, 3 = data queries + UUID + config,
 *                  4 = diary queries,
 *                  5 = decorator registration + event callbacks + memory creation,
 *                  6 = actor busy state,
 *                  7 = save unique ID + world knowledge CRUD,
 *                  8 = send custom prompt to LLM,
 *                  9 = per-actor world knowledge for prompt enrichment,
 *                  12 = send custom decision to LLM (Jev).
 */
inline int (*PublicGetVersion)() = nullptr;

inline bool (*PublicRegisterCPPAction)(const std::string name, const std::string description, std::function<bool(RE::Actor *)> eligibleCallback,
                                std::function<bool(RE::Actor *, std::string json_params)> executeCallback,
                                const std::string triggeringEventTypesCSV, std::string categoryStr, int priority,
                                std::string parameterSchemaJSON, std::string customCategory, std::string customParentCategory,
                                std::string tagsCSV) = nullptr;

inline bool (*PublicRegisterCPPSubCategory)(const std::string name, const std::string description,
                                     std::function<bool(RE::Actor *)> eligibleCallback, const std::string triggeringEventTypesCSV,
                                     int priority, std::string parameterSchemaJSON, std::string customCategory,
                                     std::string customParentCategory, std::string tagsCSV) = nullptr;

// =============================================================================
// UUID Resolution (v3+)
// =============================================================================

inline uint64_t (*PublicFormIDToUUID)(uint32_t formId) = nullptr;
inline uint32_t (*PublicUUIDToFormID)(uint64_t uuid) = nullptr;
inline std::string (*PublicGetActorNameByUUID)(uint64_t uuid) = nullptr;

// =============================================================================
// Bio Template (v3+)
// =============================================================================

inline std::string (*PublicGetBioTemplateName)(uint32_t formId) = nullptr;

// =============================================================================
// Data Queries (v3+)
// =============================================================================

inline std::string (*PublicGetMemoriesForActor)(uint32_t formId, int maxCount, const char* contextQuery) = nullptr;
inline std::string (*PublicGetRecentEvents)(uint32_t formId, int maxCount, const char* eventTypeFilter) = nullptr;
inline std::string (*PublicGetRecentDialogue)(uint32_t formId, int maxExchanges) = nullptr;
inline std::string (*PublicGetLatestDialogueInfo)() = nullptr;
inline bool (*PublicIsMemorySystemReady)() = nullptr;
inline std::string (*PublicGetActorEngagement)(int maxCount, bool excludePlayer, bool playerEventsOnly, double shortWindowSeconds, double mediumWindowSeconds) = nullptr;
inline std::string (*PublicGetRelatedActors)(uint32_t formId, int maxCount, double shortWindowSeconds, double mediumWindowSeconds) = nullptr;
inline std::string (*PublicGetPlayerContext)(float withinGameHours) = nullptr;
inline std::string (*PublicGetEventPairCounts)(const char* formIdListCSV, int minSharedEvents) = nullptr;

// =============================================================================
// Diary Queries (v4+)
// =============================================================================

inline std::string (*PublicGetDiaryEntries)(uint32_t formId, int maxCount, double startTime, double endTime) = nullptr;

// =============================================================================
// Memory Creation (v5+)
// =============================================================================

inline int (*PublicAddMemory)(uint32_t formId, const char* contentText, float importance,
                       const char* memoryType, const char* emotion, const char* location,
                       const char* tagsJSON, const char* relatedActorsJSON) = nullptr;

// =============================================================================
// Plugin Configuration (v3+)
// =============================================================================

inline std::string (*PublicGetPluginConfig)(const char* pluginName) = nullptr;
inline std::string (*PublicGetPluginConfigValue)(const char* pluginName, const char* path, const char* defaultValue) = nullptr;

// =============================================================================
// Decorator Registration (v5+)
// =============================================================================

inline bool (*PublicRegisterDecorator)(const char* name, const char* description,
                                std::function<std::string(RE::Actor*)> callback) = nullptr;
inline bool (*PublicHasDecorator)(const char* name) = nullptr;

// =============================================================================
// Event Callbacks (v5+)
// =============================================================================

inline uint64_t (*PublicRegisterEventCallback)(const char* eventType,
                                        std::function<void(const char*)> callback) = nullptr;
inline bool (*PublicUnregisterEventCallback)(uint64_t callbackId) = nullptr;

// =============================================================================
// Actor Busy State (v6+)
// =============================================================================

inline bool (*PublicSetActorBusy)(uint32_t formId, const char* reason) = nullptr;
inline bool (*PublicClearActorBusy)(uint32_t formId) = nullptr;
inline bool (*PublicIsActorBusy)(uint32_t formId) = nullptr;

// =============================================================================
// Current save UUID (v7+)
// =============================================================================

inline std::string (*PublicGetSaveUniqueID)() = nullptr;

// =============================================================================
// World Knowledge (v7+)
// =============================================================================

inline int (*PublicAddWorldKnowledge)(const char* content, const char* conditionExpr,
                               bool alwaysInject, float importance,
                               const char* displayName) = nullptr;
inline bool (*PublicRemoveWorldKnowledge)(int memoryId) = nullptr;
inline std::string (*PublicGetWorldKnowledge)(int maxCount) = nullptr;
inline std::string (*PublicGetWorldKnowledgeForActor)(uint32_t formId, int maxResults,
                                               const char* searchQuery) = nullptr;

// =============================================================================
// Custom LLM Prompts (v8+)
// =============================================================================

inline bool (*PublicSendCustomPromptToLLM)(const char* promptName, const char* variant,
                                    const char* contextJson,
                                    std::function<void(const char* response, int success)> callback) = nullptr;

// =============================================================================
// Custom Decisions (v12+)
// =============================================================================

/**
 * Evaluate a decision template through the TypeSafe Decisions endpoint and
 * receive the structured result asynchronously.
 *
 * `templateName` resolves under `prompts/decisions/<templateName>.prompt`.
 *
 * @param templateName      Name under `prompts/decisions/` (with or without `.prompt`).
 * @param contextJson       JSON object of extra Inja variables, or "" / "{}".
 * @param maxProbabilities  Top-K probabilities per choice question (0 = all, <= 255).
 * @param callback          Called on a worker thread with (answer, resultJson, success).
 *                          Do NOT call RE:: functions directly from it.
 * @return true if the task was queued, false if inputs are invalid or engine not ready.
 */
inline bool (*PublicSendCustomDecisionToLLM)(
    const char* templateName,
    const char* contextJson,
    int maxProbabilities,
    std::function<void(const char* answer, const char* resultJson, int success)> callback) = nullptr;

// =============================================================================
// Initialization
// =============================================================================

inline bool FindFunctions() {
    auto hDLL = LoadLibraryA("SkyrimNet");
    if (hDLL != nullptr) {
        PublicGetVersion = (int (*)()) GetProcAddress(hDLL, "PublicGetVersion");
        if (PublicGetVersion != nullptr) {
            int version = PublicGetVersion();

            // v2+ functions
            if (version >= 2) {
                PublicRegisterCPPAction =
                    (bool (*)(const std::string name, const std::string description, std::function<bool(RE::Actor *)> eligibleCallback,
                    std::function<bool(RE::Actor *, std::string json_params)> executeCallback, const std::string triggeringEventTypesCSV,
                              std::string categoryStr, int priority, std::string parameterSchemaJSON, std::string customCategory,
                              std::string customParentCategory, std::string tagsCSV)) GetProcAddress(hDLL, "PublicRegisterCPPAction");
                PublicRegisterCPPSubCategory = (bool (*)(
                    const std::string name, const std::string description, std::function<bool(RE::Actor *)> eligibleCallback,
                    const std::string triggeringEventTypesCSV, int priority, std::string parameterSchemaJSON, std::string customCategory,
                    std::string customParentCategory, std::string tagsCSV)) GetProcAddress(hDLL, "PublicRegisterCPPSubCategory");
            }

            // v3+ functions
            if (version >= 3) {
                PublicFormIDToUUID = reinterpret_cast<uint64_t(*)(uint32_t)>(
                    GetProcAddress(hDLL, "PublicFormIDToUUID"));
                PublicUUIDToFormID = reinterpret_cast<uint32_t(*)(uint64_t)>(
                    GetProcAddress(hDLL, "PublicUUIDToFormID"));
                PublicGetActorNameByUUID = reinterpret_cast<std::string(*)(uint64_t)>(
                    GetProcAddress(hDLL, "PublicGetActorNameByUUID"));

                PublicGetBioTemplateName = reinterpret_cast<std::string(*)(uint32_t)>(
                    GetProcAddress(hDLL, "PublicGetBioTemplateName"));

                PublicGetMemoriesForActor = reinterpret_cast<std::string(*)(uint32_t, int, const char*)>(
                    GetProcAddress(hDLL, "PublicGetMemoriesForActor"));
                PublicGetRecentEvents = reinterpret_cast<std::string(*)(uint32_t, int, const char*)>(
                    GetProcAddress(hDLL, "PublicGetRecentEvents"));
                PublicGetRecentDialogue = reinterpret_cast<std::string(*)(uint32_t, int)>(
                    GetProcAddress(hDLL, "PublicGetRecentDialogue"));
                PublicGetLatestDialogueInfo = reinterpret_cast<std::string(*)()>(
                    GetProcAddress(hDLL, "PublicGetLatestDialogueInfo"));
                PublicIsMemorySystemReady = reinterpret_cast<bool(*)()>(
                    GetProcAddress(hDLL, "PublicIsMemorySystemReady"));
                PublicGetActorEngagement = reinterpret_cast<std::string(*)(int, bool, bool, double, double)>(
                    GetProcAddress(hDLL, "PublicGetActorEngagement"));
                PublicGetRelatedActors = reinterpret_cast<std::string(*)(uint32_t, int, double, double)>(
                    GetProcAddress(hDLL, "PublicGetRelatedActors"));
                PublicGetPlayerContext = reinterpret_cast<std::string(*)(float)>(
                    GetProcAddress(hDLL, "PublicGetPlayerContext"));
                PublicGetEventPairCounts = reinterpret_cast<std::string(*)(const char*, int)>(
                    GetProcAddress(hDLL, "PublicGetEventPairCounts"));

                PublicGetPluginConfig = reinterpret_cast<std::string(*)(const char*)>(
                    GetProcAddress(hDLL, "PublicGetPluginConfig"));
                PublicGetPluginConfigValue = reinterpret_cast<std::string(*)(const char*, const char*, const char*)>(
                    GetProcAddress(hDLL, "PublicGetPluginConfigValue"));
            }

            // v4+ functions
            if (version >= 4) {
                PublicGetDiaryEntries = reinterpret_cast<std::string(*)(uint32_t, int, double, double)>(
                    GetProcAddress(hDLL, "PublicGetDiaryEntries"));
            }

            // v5+ functions
            if (version >= 5) {
                PublicRegisterDecorator = reinterpret_cast<bool(*)(const char*, const char*,
                    std::function<std::string(RE::Actor*)>)>(
                    GetProcAddress(hDLL, "PublicRegisterDecorator"));
                PublicHasDecorator = reinterpret_cast<bool(*)(const char*)>(
                    GetProcAddress(hDLL, "PublicHasDecorator"));

                PublicRegisterEventCallback = reinterpret_cast<uint64_t(*)(const char*,
                    std::function<void(const char*)>)>(
                    GetProcAddress(hDLL, "PublicRegisterEventCallback"));
                PublicUnregisterEventCallback = reinterpret_cast<bool(*)(uint64_t)>(
                    GetProcAddress(hDLL, "PublicUnregisterEventCallback"));

                PublicAddMemory = reinterpret_cast<int(*)(uint32_t, const char*, float,
                    const char*, const char*, const char*, const char*, const char*)>(
                    GetProcAddress(hDLL, "PublicAddMemory"));
            }

            // v6+ functions: Actor busy state
            if (version >= 6) {
                PublicSetActorBusy = reinterpret_cast<bool(*)(uint32_t, const char*)>(
                    GetProcAddress(hDLL, "PublicSetActorBusy"));
                PublicClearActorBusy = reinterpret_cast<bool(*)(uint32_t)>(
                    GetProcAddress(hDLL, "PublicClearActorBusy"));
                PublicIsActorBusy = reinterpret_cast<bool(*)(uint32_t)>(
                    GetProcAddress(hDLL, "PublicIsActorBusy"));
            }

            // v7+ functions
            if (version >= 7) {
                PublicGetSaveUniqueID = reinterpret_cast<std::string(*)()>(
                    GetProcAddress(hDLL, "PublicGetSaveUniqueID"));
                PublicAddWorldKnowledge = reinterpret_cast<int(*)(const char*, const char*,
                    bool, float, const char*)>(
                    GetProcAddress(hDLL, "PublicAddWorldKnowledge"));
                PublicRemoveWorldKnowledge = reinterpret_cast<bool(*)(int)>(
                    GetProcAddress(hDLL, "PublicRemoveWorldKnowledge"));
                PublicGetWorldKnowledge = reinterpret_cast<std::string(*)(int)>(
                    GetProcAddress(hDLL, "PublicGetWorldKnowledge"));
            }

            // v8+ functions
            if (version >= 8) {
                PublicSendCustomPromptToLLM = reinterpret_cast<bool(*)(const char*, const char*,
                    const char*, std::function<void(const char*, int)>)>(
                    GetProcAddress(hDLL, "PublicSendCustomPromptToLLM"));
            }

            // v9+ functions
            if (version >= 9) {
                PublicGetWorldKnowledgeForActor = reinterpret_cast<std::string(*)(uint32_t, int, const char*)>(
                    GetProcAddress(hDLL, "PublicGetWorldKnowledgeForActor"));
            }

            // v12+ functions: Custom Decisions
            if (version >= 12) {
                PublicSendCustomDecisionToLLM = reinterpret_cast<bool(*)(
                    const char*, const char*, int,
                    std::function<void(const char*, const char*, int)>)>(
                    GetProcAddress(hDLL, "PublicSendCustomDecisionToLLM"));
            }
        }
        return true;
    }
    return false;
}
}
