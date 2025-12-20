scriptname TTON_Actions

;==========================================================================
; Main Registration
;==========================================================================

; Registers all available OStimNet actions with the SkyrimNet system.
; This is the main entry point for registering all sex-related actions.
Function RegisterActions() global
    string act = TTON_JData.NextAction()
    while(act != "")
        string fileName = TTON_JData.GetActionScriptFileName(act)
        string tags
        if(TTON_JData.GetActionAddTags(act))
            tags = GetTags()
        endif
        SkyrimNetApi.RegisterAction(TTON_JData.GetActionName(act), \
          TTON_JData.GetActionDescription(act), \
          fileName, TTON_JData.GetActionIsEligible(act), fileName, TTON_JData.GetActionExecute(act), "", "PAPYRUS", 1, \
          TTON_JData.GetActionParameters(act), \
          "", tags)

        act = TTON_JData.NextAction(act)
    endwhile
EndFunction

; -------------------------------------------------
; Body Animation Tag
; -------------------------------------------------

string Function GetTags() global
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
; Start Affection Scene Action
; This action initiates a strictly non-sexual intimacy scene focused on
; gentle interactions like kissing, hugging, or cuddling.
;==========================================================================

; Placeholder eligibility check for the affection scene action.
; Implementation should ensure actors are free from ongoing OStim threads and mutually willing.
Bool Function StartAffectionSceneIsEligible(Actor akActor, string contextJson, string paramsJson) global
    return !TTON_Utils.IsSexLabInCharge() && !TTON_Utils.IsActorBusyWithScenes(akActor) && TTON_Utils.IsOStimEligible(akActor) && TTON_JData.CanUseActionAfterDecline(akActor, "StartAffectionScene")
EndFunction

; Placeholder handler for initiating the affection scene.
; Actual implementation should be added when the underlying scene logic is available.
Function StartAffectionSceneExecute(Actor akActor, string contextJson, string paramsJson) global
    string activity = SkyrimNetApi.GetJsonString(paramsJson, "activity", "kissing")
    Actor participant1 = TTON_Utils.GetEligibleActorFromParam(paramsJson, "participant1")

    if(TTON_Utils.IsSexLabInCharge())
        TTON_Debug.warn("OStimNet is disabled for player scenes while SkyrimNet_SexLab has control.")
        return
    endif

    if(!participant1)
        TTON_Debug.warn(TTON_Utils.GetActorName(akActor) + " wanted to start affection scene but failed to get participant1.")
        return
    endif

    ; check if any of potential participants are already in other ostim threads
    if(TTON_Utils.IsActorBusyWithScenes(akActor) || TTON_Utils.IsActorBusyWithScenes(participant1))
        TTON_Debug.warn("Can't start new OStim scene one or more of potential participants is already a part of ongoing OStim/Sexlab thread.")
        return
    endif

    Actor[] actors = OActorUtil.ToArray(akActor, participant1)
    TTON_OStimIntegration.StartOstim(actors, activity, nonSexual = true, initiator = akActor)
EndFunction

;==========================================================================
; Start New Sex Action
; This action allows NPCs to initiate new sexual encounters with up to 4 participants - total up to 5 participants including speaking npc.
;==========================================================================

; Checks if an actor is eligible to start a new sexual encounter
; @param akActor The actor to check eligibility for
; @param contextJson The context data in JSON format
; @param paramsJson The parameters data in JSON format
; @returns True if the actor is eligible to start a new sexual encounter
Bool Function StartSexActionIsElgigible(Actor akActor, string contextJson, string paramsJson) global
    return !TTON_Utils.IsSexLabInCharge() && !TTON_Utils.IsActorBusyWithScenes(akActor) && TTON_Utils.IsOStimEligible(akActor) && TTON_JData.CanUseActionAfterDecline(akActor, "StartSexAction")
EndFunction

; Starts a new sexual encounter with specified participants
; @param akActor The initiating actor
; @param contextJson The context data in JSON format
; @param paramsJson The parameters containing participant info and encounter type
Function StartSexActionExecute(Actor akActor, string contextJson, string paramsJson) global
    string actions = SkyrimNetApi.GetJsonString(paramsJson, "activity", "vaginalsex")
    string furn = SkyrimNetApi.GetJsonString(paramsJson, "furniture", "none")
    Actor participant1 = TTON_Utils.GetEligibleActorFromParam(paramsJson, "participant1")
    Actor participant2 = TTON_Utils.GetEligibleActorFromParam(paramsJson, "participant2")
    Actor participant3 = TTON_Utils.GetEligibleActorFromParam(paramsJson, "participant3")
    Actor participant4 = TTON_Utils.GetEligibleActorFromParam(paramsJson, "participant4")

    if(TTON_Utils.IsSexLabInCharge())
        TTON_Debug.warn("OStimNet is disabled while SkyrimNet_SexLab has control of player scenes.")
        return
    endif

    ; check if any of potential participants are already in other ostim threads
    if(TTON_Utils.IsActorBusyWithScenes(akActor) || TTON_Utils.IsActorBusyWithScenes(participant1) || TTON_Utils.IsActorBusyWithScenes(participant2) || TTON_Utils.IsActorBusyWithScenes(participant3) || TTON_Utils.IsActorBusyWithScenes(participant4))
        TTON_Debug.warn("Can't start new OStim scene one or more of potential participants is already a part of ongoing OStim/Sexlab thread.")
        return
    endif

    TTON_Debug.warn(TTON_Utils.GetActorName(akActor) + " is starting new OStim scene with activity: " + actions + ", furniture: " + furn + ", participants: " + TTON_Utils.GetActorName(participant1) + ", " + TTON_Utils.GetActorName(participant2) + ", " + TTON_Utils.GetActorName(participant3) + ", " + TTON_Utils.GetActorName(participant4))
    Actor[] actors = OActorUtil.ToArray(akActor, participant1, participant2, participant3, participant4)
    TTON_Debug.warn("Starting OStim scene with actors: " + actors)
    TTON_OStimIntegration.StartOstim(actors, actions, furn, initiator = akActor)
EndFunction

;==========================================================================
; Change Position Action
; This action allows NPCs to change their current sexual activity or position
;==========================================================================

; Checks if an actor is eligible to change their current sexual position
; @param akActor The actor to check eligibility for
; @param contextJson The context data in JSON format
; @param paramsJson The parameters data in JSON format
; @returns True if the actor is currently in an OStim scene
Bool Function ChangeSexActivityIsElgigible(Actor akActor, string contextJson, string paramsJson) global
    int ThreadID = OActor.GetSceneID(akActor)
    return OActor.IsInOStim(akActor) && !TTON_JData.GetThreadAffectionOnly(ThreadID) && TTON_JData.CanUseActionAfterDecline(akActor, "ChangeSexActivity")
EndFunction

; Changes the sexual position or activity in an ongoing encounter
; @param akActor The actor requesting the position change
; @param contextJson The context data in JSON format
; @param paramsJson The parameters containing the new position type
; @returns True if the position was successfully changed
Bool Function ChangeSexActivityActionExecute(Actor akActor, string contextJson, string paramsJson) global
    string activity = SkyrimNetApi.GetJsonString(paramsJson, "activity", "empty")
    TTON_OStimIntegration.OStimChangeScene(akActor, activity)
    SkyrimNetApi.SetActionCooldown("ChangeSexActivity", TTON_JData.GetMcmDenyCooldown())
EndFunction

;==========================================================================
; Invite Others Action
; This action allows NPCs to invite others to join their ongoing sexual encounter
;==========================================================================

; Checks if an actor is eligible to invite others to their sexual encounter
; @param akActor The actor wanting to invite others
; @param contextJson The context data in JSON format
; @param paramsJson The parameters data in JSON format
; @returns True if the actor is currently in an OStim scene
Bool Function InviteToYourSexIsEligible(Actor akActor, string contextJson, string paramsJson) global
    int ThreadID =OActor.GetSceneID(akActor)
    return OActor.IsInOStim(akActor) && !TTON_JData.GetThreadAffectionOnly(ThreadID) && TTON_JData.CanUseActionAfterDecline(akActor, "InviteToYourSex")
EndFunction

; Handles the invitation of new participants to an ongoing sexual encounter
; @param akActor The actor sending the invitation
; @param contextJson The context data in JSON format
; @param paramsJson The parameters containing target actors to invite
Bool Function InviteToYourSexExecute(Actor akActor, string contextJson, string paramsJson) global
    int ThreadID = OActor.GetSceneID(akActor)
    int currentActorsLength = OThread.GetActors(ThreadID).Length
    ; OStim scenes animations at this time can support up to 5 participants total
    Actor participant1 = none
    Actor participant2 = none
    Actor participant3 = none

    ; max participants in current ostim scenes is 5, if it is already 5 skip this action
    if(currentActorsLength == 5)
        return false
    endif

    ; 1 or more free spaces in current thread
    if(currentActorsLength < 5)
        participant1 = TTON_Utils.GetEligibleActorFromParam(paramsJson, "participant1")
        if(StorageUtil.GetIntValue(participant1, "SexInviteConsidering") == 1)
            participant1 = none
        else
            StorageUtil.SetIntValue(participant1, "SexInviteConsidering", 1)
        endif
    endif

    ; 2 or more free spaces in current thread
    if(currentActorsLength < 4)
        participant2 = TTON_Utils.GetEligibleActorFromParam(paramsJson, "participant2")
        if(StorageUtil.GetIntValue(participant2, "SexInviteConsidering") == 1)
            participant2 = none
        else
            StorageUtil.SetIntValue(participant2, "SexInviteConsidering", 1)
        endif
    endif

    ; 3 or more free spaces in current thread
    if(currentActorsLength < 3)
        participant3 = TTON_Utils.GetEligibleActorFromParam(paramsJson, "participant3")
        if(StorageUtil.GetIntValue(participant3, "SexInviteConsidering") == 1)
            participant3 = none
        else
            StorageUtil.SetIntValue(participant3, "SexInviteConsidering", 1)
        endif
    endif
    TTON_OStimIntegration.AddActorsToActiveThread(ThreadID, OActorUtil.ToArray(participant1, participant2, participant3), "InviteToYourSex", akActor)
EndFunction

;==========================================================================
; Join Sex Action
; This action allows NPCs to join an ongoing sexual encounter they are observing
;==========================================================================

; Checks if an actor is eligible to join an ongoing sexual encounter
; @param akActor The actor wanting to join
; @param contextJson The context data in JSON format
; @param paramsJson The parameters data in JSON format
; @returns True if the actor is not in a scene, there are active scenes, and the actor is eligible
Bool Function JoinOngoingSexIsElgigible(Actor akActor, string contextJson, string paramsJson) global
    return TTON_Utils.IsActorEligibleToJoin(akActor) && TTON_JData.CanUseActionAfterDecline(akActor, "JoinOngoingSex")
EndFunction

; Handles joining an ongoing sexual encounter
; @param akActor The actor joining the scene
; @param contextJson The context data in JSON format
; @param paramsJson The parameters containing the target actor to interact with
; @returns True if successfully joined the scene
Bool Function JoinOngoingSexActionExecute(Actor akActor, string contextJson, string paramsJson) global
    Actor participant1 = SkyrimNetApi.GetJsonActor(paramsJson, "participant1", none)
    int ThreadID = OActor.GetSceneID(participant1)
    int currentActorsLength = OThread.GetActors(ThreadID).Length
    ; max participants in current ostim scenes is 5, if it is already 5 skip this action
    if(currentActorsLength == 5)
        return false
    endif
    if(StorageUtil.GetIntValue(participant1, "SexInviteConsidering") == 1)
        return false
    endif
    if(TTON_JData.GetThreadAffectionOnly(ThreadID))
        return false
    endif
    StorageUtil.SetIntValue(participant1, "SexInviteConsidering", 1)
    TTON_OStimIntegration.AddActorsToActiveThread(ThreadID, OActorUtil.ToArray(akActor), "JoinOngoingSex", akActor)
EndFunction

;==========================================================================
; Change Pace Action
; This action allows NPCs to change the speed and intensity of the current sexual activity
;==========================================================================

; Checks if an actor is eligible to change the pace of their current sexual activity
; @param akActor The actor wanting to change pace
; @param contextJson The context data in JSON format
; @param paramsJson The parameters data in JSON format
; @returns True if the actor is in a scene and the scene has available speed changes
Bool Function ChangeSexPaceIsElgigible(Actor akActor, string contextJson, string paramsJson) global
    int ThreadId = OActor.GetSceneID(akActor)
    string SceneId = OThread.GetScene(ThreadId)
    int currentSpeed = OThread.GetSpeed(ThreadId)
    string hasSpeedsToMove = TTON_Utils.GetAvailableSpeedDirections(SceneId, currentSpeed)
    return OActor.IsInOStim(akActor) && hasSpeedsToMove != "none" && !TTON_JData.GetThreadAffectionOnly(ThreadId) && TTON_JData.CanUseActionAfterDecline(akActor, "ChangeSexPace")
EndFunction

; Changes the pace of the current sexual activity
; @param akActor The actor changing the pace
; @param contextJson The context data in JSON format
; @param paramsJson The parameters containing the desired speed change
Function ChangeSexPaceActionExecute(Actor akActor, string contextJson, string paramsJson) global
    int ThreadId = OActor.GetSceneID(akActor)
    string speed = SkyrimNetApi.GetJsonString(paramsJson, "speed", "none")
    if(speed == "none")
        return
    endif

    TTON_OStimIntegration.OStimChangeSpeed(ThreadId, speed)
EndFunction

;==========================================================================
; Stop Sex Action
; This action allows NPCs to end their current sexual encounter
;==========================================================================

; Checks if an actor is eligible to stop their current sexual encounter
; @param akActor The actor wanting to stop
; @param contextJson The context data in JSON format
; @param paramsJson The parameters data in JSON format
; @returns True if the actor is currently in an OStim scene
bool Function StopSexIsElgigible(Actor akActor, string contextJson, string paramsJson) global
    int ThreadID =OActor.GetSceneID(akActor)
    return OActor.IsInOStim(akActor) && !TTON_JData.GetThreadAffectionOnly(ThreadID) && TTON_JData.CanUseActionAfterDecline(akActor, "StopSex")
EndFunction

; Stops the current sexual encounter
; @param akActor The actor stopping the scene
; @param contextJson The context data in JSON format
; @param paramsJson The parameters data in JSON format
Function StopSexActionExecute(Actor akActor, string contextJson, string paramsJson) global
    TTON_OStimIntegration.StopOStim(akActor)
    SkyrimNetApi.SetActionCooldown("StopSex", TTON_JData.GetMcmDenyCooldown())
EndFunction

