scriptname TTON_Actions extends Quest

;==========================================================================
; Main Registration
;==========================================================================

; Registers all available OStimNet actions with the SkyrimNet system.
; This is the main entry point for registering all sex-related actions.
Function RegisterActions()
; placeholder
EndFunction

; -------------------------------------------------
; Body Animation Tag
; -------------------------------------------------

string Function GetTags()
    Form cuddle = Game.GetFormFromFile(0x800, "SkyrimNet_Cuddle.esp")
    Form sexlab = Game.GetFormFromFile(0x800, "SkyrimNet_SexLab.esp")
    if(cuddle || sexlab)
        ; commenting it for now until wind's sexlab/cuddle implement tag begavior
        ; return "BodyAnimation"
        return ""
    endif
    return ""
EndFunction

;==========================================================================
; Start Care Scene Action
; This action initiates a strictly non-sexual intimacy scene focused on
; gentle interactions like kissing, hugging, or cuddling.
;==========================================================================

; Placeholder handler for initiating the care scene.
; Actual implementation should be added when the underlying scene logic is available.
Function StartCareSceneExecute(Actor akActor, Actor participant1, string activity, string intent, Actor initiator, string reasoning)
    TTON_Debug.debug("StartCareSceneExecute called with actor: " + TTON_Utils.GetActorName(akActor) + ", participant: " + TTON_Utils.GetActorName(participant1) + ", activity: " + activity + ", intent: " + intent + ", initiator: " + TTON_Utils.GetActorName(initiator) + ", reasoning: " + reasoning)
    if(TTON_Utils.IsSexLabInCharge())
        TTON_Debug.warn("OStimNet is disabled for player scenes while SkyrimNet_SexLab has control.")
        return
    endif

    if(!participant1)
        TTON_Debug.warn(TTON_Utils.GetActorName(akActor) + " wanted to start care scene but failed to get participant1.")
        return
    endif

    ; check if any of potential participants are already in other ostim threads
    if(TTON_Utils.IsActorBusyWithScenes(akActor) || TTON_Utils.IsActorBusyWithScenes(participant1))
        TTON_Debug.warn("Can't start new OStim scene one or more of potential participants is already a part of ongoing OStim/Sexlab thread.")
        return
    endif

    Actor main
    Actor secondary
    if(initiator == akActor)
        main = akActor
        secondary = participant1
    else
        main = participant1
        secondary = akActor
    endif
    Actor player = TTON_JData.GetPlayer()
    Actor[] actors = OActorUtil.ToArray(main, secondary)

    TTON_Utils.SetActorsPending(actors, true)

    string json = "{ \"activity\": \"" + activity + "\", \"intent\": \"" + intent + "\", \"isSexual\": false }"
    int result = OStimNet.ShowConfirmationModal("StartCareScene", OActorUtil.ToArray(main), OActorUtil.ToArray(secondary), json)
    bool earlyReturn = result == -1 || result == 1 || result == 2

    if(result == -1)
        TTON_Debug.warn("Failed to show confirmation modal for StartCareScene action.")
    endif

    if(result == 1 || result == 2)
        TTON_Debug.warn("Player declined StartCareScene action.")
    endif

    if(earlyReturn)
        TTON_Utils.SetActorsPending(actors, false)
        return
    endif

    ; When player is secondary it means npc is initiating the care scene,
    ; in that case we can directly start the scene
    ; since player confirmation already happened in the modal.
    if(secondary == player)
        TTON_OStimIntegration.StartOStimCaring(main, secondary, activity, intent)
    else
        Debug.Notification("Finalizing care scene details before starting the scene...")
        OStimNet.EvaluateNonSexualScene(actors, activity, intent, initiator)
    endif
EndFunction

;==========================================================================
; Start New Sex Action
; This action allows NPCs to initiate new sexual encounters with up to 4 participants - total up to 5 participants including speaking npc.
;==========================================================================

; Starts a new sexual encounter with specified participants
; @param akActor The initiating actor
; @param contextJson The context data in JSON format
; @param paramsJson The parameters containing participant info and encounter type
Function StartSexActionExecute(Actor akActor, Actor participant1, Actor participant2, Actor participant3, Actor participant4, string furnitureType, string intent, string reason)
    TTON_Debug.debug("StartSexActionExecute called with actor: " + TTON_Utils.GetActorName(akActor) + ", participants: " + TTON_Utils.GetActorName(participant1) + ", " + TTON_Utils.GetActorName(participant2) + ", " + TTON_Utils.GetActorName(participant3) + ", " + TTON_Utils.GetActorName(participant4) + ", furnitureType: " + furnitureType + ", intent: " + intent + ", reason: " + reason)
    if(TTON_Utils.IsSexLabInCharge())
        TTON_Debug.warn("OStimNet is disabled while SkyrimNet_SexLab has control of player scenes.")
        return
    endif

    ; check if any of potential participants are already in other ostim threads
    if(TTON_Utils.IsActorBusyWithScenes(akActor) || TTON_Utils.IsActorBusyWithScenes(participant1) || TTON_Utils.IsActorBusyWithScenes(participant2) || TTON_Utils.IsActorBusyWithScenes(participant3) || TTON_Utils.IsActorBusyWithScenes(participant4))
        TTON_Debug.warn("Can't start new OStim scene one or more of potential participants is already a part of ongoing OStim/Sexlab thread.")
        return
    endif

    if(akActor == participant1)
        participant1 = none
    elseif(akActor == participant2)
        participant2 = none
    elseif(akActor == participant3)
        participant3 = none
    elseif(akActor == participant4)
        participant4 = none
    endif
    if(participant1)
        if(participant1 == participant2)
            participant2 = none
        elseif(participant1 == participant3)
            participant3 = none
        elseif(participant1 == participant4)
            participant4 = none
        endif
    endif
    if(participant2)
        if(participant2 == participant3)
            participant3 = none
        elseif(participant2 == participant4)
            participant4 = none
        endif
    endif
    if(participant3)
        if(participant3 == participant4)
            participant4 = none
        endif
    endif

    Actor[] actors = OActorUtil.ToArray(akActor, participant1, participant2, participant3, participant4)

    int cooldownResult = OStimNet.CheckAndSetActionCooldown("StartNewSex", OActorUtil.ToArray(akActor))
    if(cooldownResult == 1)
        TTON_Debug.warn("Action StartNewSex is on cooldown for " + TTON_Utils.GetActorName(akActor) + ".")
        return
    endif

    TTON_Utils.SetActorsPending(actors, true)

    string json = "{ \"activity\": \"\", \"intent\": \"" + intent + "\", \"isSexual\": true, \"positions\": \"\", \"furniture\": \"" + furnitureType + "\" }"
    int result = OStimNet.ShowConfirmationModal("StartNewSexPreEval", OActorUtil.ToArray(akActor), PapyrusUtil.RemoveActor(actors, akActor), json)
    bool earlyReturn = result == -1 || result == 1 || result == 2

    if(result == -1)
        TTON_Debug.warn("Failed to show confirmation modal for StartNewSexPreEval action.")
    endif

    if(result == 1 || result == 2)
        TTON_Debug.warn("Player declined StartNewSexPreEval action.")
    endif

    if(earlyReturn)
        TTON_Utils.SetActorsPending(actors, false)
        return
    endif

    ObjectReference finalFurniture = TTON_OStimIntegration.FindSuitableFurniture(akActor, furnitureType)

    if(finalFurniture)
        TTON_Utils.MakeParticipantsWalkToFurniture(actors, finalFurniture)
    endif

    Debug.Notification("Finalizing sexual scene details before starting the scene...")
    OStimNet.EvaluatePreStartSexualScene(actors, intent)
EndFunction

;==========================================================================
; Change Position Action
; This action allows NPCs to change their current sexual activity or position
;==========================================================================


; Changes the sexual position or activity in an ongoing encounter
; @param akActor The actor requesting the position change
; @param contextJson The context data in JSON format
; @param paramsJson The parameters containing the new position type
; @returns True if the position was successfully changed
Function ChangeSexScenePositionExecute(Actor akActor, Actor initiator, string activity, string sexualPosition, string reasoning)
    if(sexualPosition == "" && activity == "")
        TTON_Debug.warn("No parameters provided for ChangeSexScenePosition action.")
        return
    endif

    int ThreadID = OActor.GetSceneID(initiator)

    if(OStimNet.GetThreadPhase(ThreadID) == "undressing")
        TTON_Debug.warn("ChangeSexScenePosition action: cannot change position during undressing.")
        return
    endif

    int cooldownResult = OStimNet.CheckAndSetActionCooldown("ChangeSexScenePosition", OActorUtil.ToArray(initiator))
    if(cooldownResult == 1)
        TTON_Debug.warn("Action ChangeSexScenePosition is on cooldown for " + TTON_Utils.GetActorName(akActor) + ".")
        return
    endif

    Actor[] allActors = OThread.GetActors(OActor.GetSceneID(initiator))
    Actor[] secondaryActors = PapyrusUtil.RemoveActor(allActors, initiator)
    string json = "{ \"activity\": \"" + activity + "\", \"isSexual\": true, \"positions\": \"" + sexualPosition + "\" }"
    int result = OStimNet.ShowConfirmationModal("ChangeSexScenePosition", OActorUtil.ToArray(initiator), secondaryActors, json)

    if(result == -1)
        TTON_Debug.warn("Failed to show confirmation modal for ChangeSexScenePosition action.")
        return
    endif

    if(result == 1 || result == 2)
        TTON_Debug.warn("Player declined ChangeSexScenePosition action.")
        return
    endif

    TTON_OStimIntegration.OStimChangeScene(akActor, activity, sexualPosition, OStimNet.GetThreadIntent(OActor.GetSceneID(akActor)))
EndFunction

;==========================================================================
; Change Intent Action
; This action allows NPCs to change their current sexual intent
;==========================================================================


; Changes the sexual intent in an ongoing encounter
; @param akActor The actor requesting the intent change
; @param contextJson The context data in JSON format
; @param paramsJson The parameters containing the new intent
; @returns True if the intent was successfully changed
Function ChangeSexSceneIntentExecute(Actor akActor, Actor initiator, string intent, string reasoning)
    if(intent == "")
        TTON_Debug.warn("No parameters provided for ChangeSexSceneIntent action.")
        return
    endif


    int ThreadID = OActor.GetSceneID(initiator)

    if(OStimNet.GetThreadPhase(ThreadID) == "undressing")
        TTON_Debug.warn("ChangeSexSceneIntent action: cannot change intent during undressing.")
        return
    endif

    string currentIntent = OStimNet.GetThreadIntent(ThreadID)

    if(intent == currentIntent)
        TTON_Debug.warn("ChangeSexSceneIntent action: intent is already set to " + intent + ".")
        return
    endif

    if(intent == "aggressive" && !SkyrimNetApi.GetConfigBool("Plugin_OStimNet", "tton.settings.enableAggressiveIntent", "false"))
        intent = "dom"
    endif

    int cooldownResult = OStimNet.CheckAndSetActionCooldown("ChangeSexSceneIntent", OActorUtil.ToArray(initiator))
    if(cooldownResult == 1)
        TTON_Debug.warn("Action ChangeSexSceneIntent is on cooldown for " + TTON_Utils.GetActorName(akActor) + ".")
        return
    endif

    Actor[] allActors = OThread.GetActors(OActor.GetSceneID(initiator))
    Actor[] secondaryActors = PapyrusUtil.RemoveActor(allActors, initiator)
    string json = "{ \"intent\": \"" + intent + "\" }"
    int result = OStimNet.ShowConfirmationModal("ChangeSexSceneIntent", OActorUtil.ToArray(initiator), secondaryActors, json)

    if(result == -1)
        TTON_Debug.warn("Failed to show confirmation modal for ChangeSexSceneIntent action.")
        return
    endif

    if(result == 1 || result == 2)
        TTON_Debug.warn("Player declined ChangeSexSceneIntent action.")
        return
    endif
    TTON_OStimIntegration.OStimChangeIntent(akActor, intent, OActorUtil.ToArray(initiator))
EndFunction

;==========================================================================
; Invite Others Action
; This action allows NPCs to invite others to join their ongoing sexual encounter
;==========================================================================

; Handles the invitation of new participants to an ongoing sexual encounter
; @param akActor The actor sending the invitation
; @param contextJson The context data in JSON format
; @param paramsJson The parameters containing target actors to invite
Bool Function InviteToYourSexExecute(Actor akActor, Actor participant1, Actor participant2, Actor participant3, string reason)
    int ThreadID = OActor.GetSceneID(akActor)
    int currentActorsLength = OThread.GetActors(ThreadID).Length
    ; max participants in current ostim scenes is 5, if it is already 5 skip this action
    if(currentActorsLength == 5)
        return false
    endif

    if(OStimNet.GetThreadPhase(ThreadID) == "undressing")
        TTON_Debug.warn("InviteToYourSex action: cannot invite others during undressing.")
        return false
    endif

    ; OStim scenes animations at this time can support up to 5 participants total
    Actor finalParticipant1 = none
    Actor finalParticipant2 = none
    Actor finalParticipant3 = none

    ; 1 or more free spaces in current thread
    if(currentActorsLength < 5)
        finalParticipant1 = participant1
    endif

    ; 2 or more free spaces in current thread
    if(currentActorsLength < 4)
        finalParticipant2 = participant2
    endif

    ; 3 or more free spaces in current thread
    if(currentActorsLength < 3)
        finalParticipant3 = participant3
    endif

    Actor[] newParticipants = OActorUtil.ToArray(finalParticipant1, finalParticipant2, finalParticipant3)

    int cooldownResult = OStimNet.CheckAndSetActionCooldown("InviteToYourSex", OActorUtil.ToArray(akActor))
    if(cooldownResult == 1)
        TTON_Debug.warn("Action InviteToYourSex is on cooldown for " + TTON_Utils.GetActorName(akActor) + ".")
        return false
    endif

    TTON_Utils.SetActorsPending(newParticipants, true)
    Actor player = TTON_JData.GetPlayer()
    bool isPlayerInvitee = PapyrusUtil.CountActor(newParticipants, player) > 0
    string playerStatus = "participant"
    if(isPlayerInvitee)
        playerStatus = "invitee"
    endif
    int result = OStimNet.ShowConfirmationModal("InviteToYourSex", OActorUtil.ToArray(akActor), newParticipants, "{ \"playerStatus\": \"" + playerStatus + "\" }")
    bool earlyReturn = result == -1 || result == 1 || result == 2
    if(result == -1)
        TTON_Debug.warn("Failed to show confirmation modal for InviteToYourSex action.")
        newParticipants = PapyrusUtil.RemoveActor(newParticipants, player)
    endif

    if(result == 1 || result == 2)
        TTON_Debug.warn("Player declined InviteToYourSex action.")
        newParticipants = PapyrusUtil.RemoveActor(newParticipants, player)
    endif

    if(earlyReturn)
        if(isPlayerInvitee)
            Debug.Notification("Player declined invitation to join the scene. Invitation will be sent to other participants if applicable.")
            newParticipants = PapyrusUtil.RemoveActor(newParticipants, player)
        else
            TTON_Utils.SetActorsPending(newParticipants, false)
            return false
        endif
    endif

    if(newParticipants.Length == 0)
        TTON_Debug.warn("No valid participants to invite for InviteToYourSex action after player response.")
        return false
    endif

    OStimNet.EvaluateInviteToSex(akActor, newParticipants, ThreadID)
EndFunction

;==========================================================================
; Join Sex Action
; This action allows NPCs to join an ongoing sexual encounter they are observing
;==========================================================================


; Handles joining an ongoing sexual encounter
; @param akActor The actor joining the scene
; @param contextJson The context data in JSON format
; @param paramsJson The parameters containing the target actor to interact with
; @returns True if successfully joined the scene
Bool Function JoinOngoingSexActionExecute(Actor akActor, Actor participant1, string intent, string reasoning)
    int ThreadID = OActor.GetSceneID(participant1)
    Actor[] actors = OThread.GetActors(ThreadID)
    int currentActorsLength = actors.Length
    ; max participants in current ostim scenes is 5, if it is already 5 skip this action
    if(currentActorsLength == 5)
        return false
    endif

    if(OStimNet.GetThreadPhase(ThreadID) == "undressing")
        TTON_Debug.warn("JoinOngoingSex action: cannot join during undressing.")
        return false
    endif

    int cooldownResult = OStimNet.CheckAndSetActionCooldown("JoinOngoingSex", OActorUtil.ToArray(akActor))
    if(cooldownResult == 1)
        TTON_Debug.warn("Action JoinOngoingSex is on cooldown for " + TTON_Utils.GetActorName(akActor) + ".")
        return false
    endif

    TTON_Utils.SetActorPending(akActor, true)

    int result = OStimNet.ShowConfirmationModal("JoinOngoingSex", OActorUtil.ToArray(akActor), OActorUtil.ToArray(participant1), "{}")
    bool earlyReturn = result == -1 || result == 1 || result == 2
    if(result == -1)
        TTON_Debug.warn("Failed to show confirmation modal for JoinOngoingSex action.")
    endif

    if(result == 1 || result == 2)
        TTON_Debug.warn("Player declined JoinOngoingSex action.")
    endif

    if(earlyReturn)
        TTON_Utils.SetActorPending(akActor, false)
        return false
    endif

    OStimNet.EvaluateJoinOngoingSex(akActor, ThreadID)
EndFunction

;==========================================================================
; Change Pace Action
; This action allows NPCs to change the speed and intensity of the current sexual activity
;==========================================================================

; Changes the pace of the current sexual activity
; @param akActor The actor changing the pace
; @param contextJson The context data in JSON format
; @param paramsJson The parameters containing the desired speed change
Function ChangeSexScenePaceActionExecute(Actor akActor, string speed)
    int ThreadId = OActor.GetSceneID(akActor)
    if(speed != "increase" && speed != "decrease")
        return
    endif

    int cooldownResult = OStimNet.CheckAndSetActionCooldown("ChangeSexPace", OActorUtil.ToArray(akActor))
    if(cooldownResult == 1)
        TTON_Debug.warn("Action ChangeSexPace is on cooldown for " + TTON_Utils.GetActorName(akActor) + ".")
        return
    endif

    TTON_OStimIntegration.OStimChangeSpeed(ThreadId, speed)
EndFunction

;==========================================================================
; Stop Sex Action
; This action allows NPCs to end their current sexual encounter
;==========================================================================


; Stops the current sexual encounter
; @param akActor The actor stopping the scene
; @param contextJson The context data in JSON format
; @param paramsJson The parameters data in JSON format
Function StopSexActionExecute(Actor akActor)
    int cooldownResult = OStimNet.CheckAndSetActionCooldown("StopSex", OActorUtil.ToArray(akActor))
    if(cooldownResult == 1)
        TTON_Debug.warn("Action StopSex is on cooldown for " + TTON_Utils.GetActorName(akActor) + ".")
        return
    endif

    Actor[] allActors = OThread.GetActors(OActor.GetSceneID(akActor))
    Actor[] secondaryActors = PapyrusUtil.RemoveActor(allActors, akActor)

    int result = OStimNet.ShowConfirmationModal("StopSex", OActorUtil.ToArray(akActor), secondaryActors, "")

    if(result == -1)
        TTON_Debug.warn("Failed to show confirmation modal for StopSex action.")
        return
    endif

    if(result == 1 || result == 2)
        TTON_Debug.warn("Player declined StopSex action.")
        return
    endif
    TTON_OStimIntegration.StopOStim(akActor)
EndFunction

Function SpectatorOfSexActionExecute(Actor akActor, Actor target)
    if(!target)
        TTON_Debug.warn("SpectatorOfSexActionExecute: target actor not found in paramsJson")
        return
    endif

    int ThreadID = OActor.GetSceneID(target)

    if(ThreadID == -1)
        TTON_Debug.warn("SpectatorOfSexActionExecute: actor is not in a valid OStim scene")
        return
    endif

    TTON_Debug.debug("SpectatorOfSexActionExecute: " + TTON_Utils.GetActorName(akActor) + "->" + TTON_Utils.GetActorName(target))

    OStimNet.AddSpectator(akActor, ThreadID, target)
EndFunction

Function SpectatorOfSexFleeActionExecute(Actor akActor)
    TTON_Debug.debug("SpectatorOfSexFleeActionExecute: " + TTON_Utils.GetActorName(akActor))
    if(akActor.IsInFaction(TTON_JData.GetSpectatorFaction()))
        TTON_Spectators.MakeSpectatorFlee(akActor)
    endif
EndFunction

