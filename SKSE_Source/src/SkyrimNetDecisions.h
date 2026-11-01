#pragma once

#include <RE/Skyrim.h>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace OStimNet::SkyrimNetDecisions {

// Returns true when Jev decisions are enabled in config (tton.gm.useDecisions)
// and SkyrimNet v12+ PublicSendCustomDecisionToLLM is resolved.
bool IsAvailable();

// 1. Pre-start sexual scene evaluation (StartNewSex)
//    Template: prompts/decisions/ostimnet_evaluate_prestart_sexual.prompt
bool EvaluatePreStartSexualScene(
    const std::vector<RE::FormID>& participantFormIDs,
    const std::string& evalId,
    const std::string& reason,
    std::function<void()> fallbackToChat);

// 2. External (non-OStimNet) sexual thread intent & role evaluation
//    Template: prompts/decisions/ostimnet_evaluate_external_sexual_thread.prompt
bool EvaluateExternalSexualThread(
    const std::vector<RE::FormID>& participantFormIDs,
    int threadID,
    std::function<void()> fallbackToChat);

// 3. Non-sexual care scene evaluation (StartCareScene)
//    Template: prompts/decisions/ostimnet_evaluate_nonsexual_scene.prompt
bool EvaluateNonSexualScene(
    const std::vector<RE::FormID>& participantFormIDs,
    const std::string& activity,
    const std::string& intent,
    RE::FormID initiatorFormID,
    const std::string& evalId,
    const std::string& reason,
    std::function<void()> fallbackToChat);

// 4. Join ongoing sex evaluation (JoinOngoingSex)
//    Template: prompts/decisions/ostimnet_evaluate_join_ongoing_sex.prompt
bool EvaluateJoinOngoingSex(
    RE::FormID joinerFormID,
    int threadID,
    const std::string& evalId,
    const std::string& reason,
    std::function<void()> fallbackToChat);

// 5. Invite to ongoing sex evaluation (InviteToYourSex)
//    Template: prompts/decisions/ostimnet_evaluate_invite_to_sex.prompt
bool EvaluateInviteToSex(
    RE::FormID inviterFormID,
    const std::vector<RE::FormID>& inviteeFormIDs,
    int threadID,
    const std::string& evalId,
    const std::string& reason,
    std::function<void()> fallbackToChat);

// 6. Location scan evaluation (spontaneous scene initiation)
//    Template: prompts/decisions/ostimnet_scan_location.prompt
bool EvaluateLocationScan(
    const std::string& contextJson,
    const std::map<std::string, RE::FormID>& nameToFormID,
    uint64_t scanGen,
    std::function<void()> fallbackToChat);

// 7. Scheduled / manual scene advance evaluation
//    Template: prompts/decisions/ostimnet_evaluate_scene_advance.prompt
bool EvaluateScheduledSceneAdvance(
    int threadID,
    std::function<void()> onDone,
    std::function<void()> fallbackToChat);

// 8. Pullout decision evaluation (near-climax vaginal sex)
//    Template: prompts/decisions/ostimnet_evaluate_pullout_decision.prompt
bool EvaluatePulloutDecision(
    int threadID,
    std::function<void()> fallbackToChat);

}  // namespace OStimNet::SkyrimNetDecisions

