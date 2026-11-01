#pragma once
#include <functional>
#include <map>
#include <string>
#include <vector>
#include "RE/Skyrim.h"

namespace OStimNet::SkyrimNetIntegration {

    /**
     * Load SkyrimNet.dll and resolve all exported function pointers.
     * Wraps FindFunctions() — call once from kDataLoaded.
     * @return true if SkyrimNet was found and the API was resolved.
     */
    bool InitSkyrimNetAPI();

    /**
     * Return the connected SkyrimNet API version, or 0 if not connected.
     */
    int GetSkyrimNetAPIVersion();

    /**
     * Read the full config JSON for a plugin from SkyrimNet's plugin config system.
     * Returns an empty string if SkyrimNet is not present.
     */
    std::string GetPluginConfig(const char* plugin);

    /**
     * Read a single value from SkyrimNet's plugin config system.
     * Falls back to defaultValue if SkyrimNet is not present or the key is missing.
     */
    std::string GetPluginConfigValue(const char* plugin, const char* path, const char* defaultValue);

    /**
     * Initialize and register all SkyrimNet decorators.
     *
     * Requires:
     *   - InitSkyrimNetAPI() already called
     *   - ONavFindFunctions() already called (api/OStimNavigator_PublicAPI.h)
     *   - g_ostimThreadInterface already set (via OstimNG_API::Thread::GetAPI)
     *
     * Call once from the kDataLoaded SKSE message handler, after all three
     * APIs have been initialized.
     */
    void Register();

    /**
     * Send the "ostimnet_evaluate_prestart_sexual" prompt to the LLM before an OStim scene
     * has been created. Called from Papyrus as the first step of StartNewSex / StartCareScene.
     *
     * When the LLM responds, fires the Papyrus mod event "ostimnet_sexual_evaluation_finished":
     *   numArg = 1.0  — success; strArg = JSON { intent, sexualPosition, sexualActivities,
     *                                             main, secondary, originalParticipants }
     *   numArg = 2.0  — scene=null (LLM decided insufficient willing participants)
     *   numArg = 0.0  — error (LLM failure or unparseable response)
     *
     * @param participantFormIDs  FormIDs of all intended participants (max 5).
     * @param evalId              Evaluation ID string.
     * @param reason              Optional initiation reason from the triggering action.
     * @param forceChat           When true, bypasses Jev Decisions and forces the GM chat LLM path.
     * @return true if the LLM task was successfully queued.
     */
    bool EvaluatePreStartSexualScene(const std::vector<RE::FormID>& participantFormIDs, const std::string& evalId = "", const std::string& reason = "", bool forceChat = false);

    /**
     * Send the "ostimnet_evaluate_external_sexual_thread" prompt to the LLM for an OStim
     * thread that was started by a third-party mod or manually — not by OStimNet.
     * Called from OStimEventListener::HandleStart for non-OStimNet threads.
     *
     * When the LLM responds, sets intent, actor roles, and ownership directly on
     * ThreadDataStore and calls OStimEventListener::ClaimPendingNonOStimNetThread.
     * No mod event is fired.
     *
     * @param participantFormIDs  FormIDs of the thread's participants (max 5).
     * @param threadID            Active OStim thread ID.
     * @param forceChat           When true, bypasses Jev Decisions and forces the GM chat LLM path.
     * @return true if the LLM task was successfully queued.
     */
    bool EvaluateExternalSexualThread(const std::vector<RE::FormID>& participantFormIDs,
                                      int threadID,
                                      bool forceChat = false);

    /**
     * Send the "ostimnet_evaluate_nonsexual_scene" prompt to the LLM before a non-sexual
     * OStim scene is started. Called from Papyrus.
     *
     * When the LLM responds, fires the Papyrus mod event "ostimnet_nonsexual_evaluation_finished":
     *   numArg = 1.0  — start=true; strArg = JSON { "start": true, "intent": "...", "activity": "...", "main": [...], "secondary": [...] }
     *   numArg = 2.0  — LLM decided start=false (scene should not start)
     *   numArg = 0.0  — error (LLM failure or unparseable response)
     *
     * @param participantFormIDs  FormIDs of all intended participants (max 5).
     * @param activity            Optional activity hint for the LLM (e.g. "spar", "meditate"). Empty = no hint.
     * @param intent              Optional intent hint for the LLM (e.g. "friendly", "hostile"). Empty = no hint.
     * @param initiatorFormID     Optional FormID of the actor who initiated the scene. 0 = no initiator.
     * @param evalId              Evaluation ID string.
     * @param reason              Optional initiation reason from the triggering action.
     * @param forceChat           When true, bypasses Jev Decisions and forces the GM chat LLM path.
     * @return true if the LLM task was successfully queued.
     */
    bool EvaluateNonSexualScene(const std::vector<RE::FormID>& participantFormIDs, const std::string& activity = "", const std::string& intent = "", RE::FormID initiatorFormID = 0, const std::string& evalId = "", const std::string& reason = "", bool forceChat = false);

    /**
     * Send the "ostimnet_evaluate_join_ongoing_sex" prompt to the LLM when an actor outside
     * a running sexual scene wants to join it. Called from Papyrus.
     *
     * When the LLM responds, fires the Papyrus mod event "ostimnet_join_sex_evaluation_finished":
     *   numArg = 1.0  — proceed; strArg = JSON { "threadID": ..., "intent": "...", "main": [...], "secondary": [...] }
     *   numArg = 2.0  — LLM returned scene=null (decided not to proceed)
     *   numArg = 0.0  — error (LLM failure or unparseable response)
     *
     * @param joinerFormID  FormID of the actor requesting to join.
     * @param threadID      Active OStim thread ID.
     * @param evalId        Evaluation ID string.
     * @param reason        Optional initiation reason from the triggering action.
     * @param forceChat     When true, bypasses Jev Decisions and forces the GM chat LLM path.
     * @return true if the LLM task was successfully queued.
     */
    bool EvaluateJoinOngoingSex(RE::FormID joinerFormID,
                                int threadID,
                                const std::string& evalId = "",
                                const std::string& reason = "",
                                bool forceChat = false);

    /**
     * Send the "ostimnet_evaluate_invite_to_sex" prompt to the LLM when an actor in a
     * running sexual scene invites one or more outside actors. Called from Papyrus.
     *
     * When the LLM responds, fires the Papyrus mod event "ostimnet_invite_sex_evaluation_finished":
     *   numArg = 1.0  — proceed; strArg = JSON { "threadID": ..., "intent": "...", "main": [...], "secondary": [...] }
     *   numArg = 2.0  — LLM returned scene=null (decided not to proceed)
     *   numArg = 0.0  — error (LLM failure or unparseable response)
     *
     * @param inviterFormID   FormID of the actor sending the invitation.
     * @param inviteeFormIDs  FormIDs of the actors being invited.
     * @param threadID        Active OStim thread ID.
     * @param evalId          Evaluation ID string.
     * @param reason          Optional initiation reason from the triggering action.
     * @param forceChat       When true, bypasses Jev Decisions and forces the GM chat LLM path.
     * @return true if the LLM task was successfully queued.
     */
    bool EvaluateInviteToSex(RE::FormID inviterFormID,
                             const std::vector<RE::FormID>& inviteeFormIDs,
                             int threadID,
                             const std::string& evalId = "",
                             const std::string& reason = "",
                             bool forceChat = false);

    /**
     * Send the "ostimnet_scan_location" prompt to the LLM.
     * Called from LocationScanService after building the name snapshot.
     * Fires mod event "ostimnet_location_scan_result" on completion.
     *
     * @param contextJson   Pre-built context JSON string.
     * @param nameToFormID  Name→FormID snapshot captured on the game thread.
     * @param scanGen       Scan generation counter.
     * @param forceChat     When true, bypasses Jev Decisions and forces the GM chat LLM path.
     * @return true if the LLM task was successfully queued.
     */
    bool EvaluateLocationScan(const std::string& contextJson,
                              const std::map<std::string, RE::FormID>& nameToFormID,
                              uint64_t scanGen,
                              bool forceChat = false);

    /**
     * Send the "ostimnet_evaluate_scene_advance" prompt to the LLM to choose the next
     * position/activity for a running sexual scene.
     *
     * @param threadID   Active OStim thread ID.
     * @param onDone     Called in all terminal paths (success, null, error after ignore).
     * @param forceChat  When true, bypasses Jev Decisions and forces the GM chat LLM path.
     * @return true if the LLM task was successfully queued.
     */
    bool EvaluateScheduledSceneAdvance(int threadID, std::function<void()> onDone, bool forceChat = false);

    /**
     * Evaluates pullout decision with the LLM using Jev Decisions or GmLlmVariant prompt.
     */
    bool EvaluatePulloutDecision(int threadID, bool forceChat = false);

}  // namespace OStimNet::SkyrimNetIntegration
