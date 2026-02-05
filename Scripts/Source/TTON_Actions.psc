scriptname TTON_Actions extends Quest

;==========================================================================
; Main Registration
;==========================================================================

; Registers all available OStimNet actions with the SkyrimNet system.
; This is the main entry point for registering all sex-related actions.
Function RegisterActions() global
; placeholder
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

; Placeholder handler for initiating the affection scene.
; Actual implementation should be added when the underlying scene logic is available.
Function StartAffectionSceneExecute(Actor akActor, Actor participant1, string activity)
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

; Starts a new sexual encounter with specified participants
; @param akActor The initiating actor
; @param contextJson The context data in JSON format
; @param paramsJson The parameters containing participant info and encounter type
Function StartSexActionExecute(Actor akActor, Actor participant1, Actor participant2, Actor participant3, Actor participant4, string activity, string furn) global
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

    TTON_Debug.warn("Starting OStim scene with actors: " + actors)
    TTON_OStimIntegration.StartOstim(actors, activity, furn, initiator = akActor)
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
Bool Function ChangeSexActivityActionExecute(Actor akActor, string activity) global
    TTON_OStimIntegration.OStimChangeScene(akActor, activity)
    SkyrimNetApi.SetActionCooldown("ChangeSexActivity", TTON_JData.GetMcmDenyCooldown())
EndFunction

;==========================================================================
; Invite Others Action
; This action allows NPCs to invite others to join their ongoing sexual encounter
;==========================================================================

; Handles the invitation of new participants to an ongoing sexual encounter
; @param akActor The actor sending the invitation
; @param contextJson The context data in JSON format
; @param paramsJson The parameters containing target actors to invite
Bool Function InviteToYourSexExecute(Actor akActor, Actor participant1, Actor participant2, Actor participant3) global
    int ThreadID = OActor.GetSceneID(akActor)
    int currentActorsLength = OThread.GetActors(ThreadID).Length
    ; max participants in current ostim scenes is 5, if it is already 5 skip this action
    if(currentActorsLength == 5)
        return false
    endif

    ; OStim scenes animations at this time can support up to 5 participants total
    Actor finalParticipant1 = none
    Actor finalParticipant2 = none
    Actor finalParticipant3 = none

    ; 1 or more free spaces in current thread
    if(currentActorsLength < 5)
        finalParticipant1 = participant1
        if(StorageUtil.GetIntValue(participant1, "SexInviteConsidering") == 1)
            finalParticipant1 = none
        else
            StorageUtil.SetIntValue(finalParticipant1, "SexInviteConsidering", 1)
        endif
    endif

    ; 2 or more free spaces in current thread
    if(currentActorsLength < 4)
        finalParticipant2 = participant2
        if(StorageUtil.GetIntValue(participant2, "SexInviteConsidering") == 1)
            finalParticipant2 = none
        else
            StorageUtil.SetIntValue(finalParticipant2, "SexInviteConsidering", 1)
        endif
    endif

    ; 3 or more free spaces in current thread
    if(currentActorsLength < 3)
        finalParticipant3 = participant3
        if(StorageUtil.GetIntValue(participant3, "SexInviteConsidering") == 1)
            finalParticipant3 = none
        else
            StorageUtil.SetIntValue(finalParticipant3, "SexInviteConsidering", 1)
        endif
    endif
    TTON_OStimIntegration.AddActorsToActiveThread(ThreadID, OActorUtil.ToArray(participant1, participant2, participant3), "InviteToYourSex", akActor)
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
Bool Function JoinOngoingSexActionExecute(Actor akActor, Actor participant1) global
    int ThreadID = OActor.GetSceneID(participant1)
    int currentActorsLength = OThread.GetActors(ThreadID).Length
    ; max participants in current ostim scenes is 5, if it is already 5 skip this action
    if(currentActorsLength == 5)
        return false
    endif
    if(StorageUtil.GetIntValue(participant1, "SexInviteConsidering") == 1)
        return false
    endif
    if(TTON_Storage.GetThreadAffectionOnly(ThreadID))
        return false
    endif
    StorageUtil.SetIntValue(participant1, "SexInviteConsidering", 1)
    TTON_OStimIntegration.AddActorsToActiveThread(ThreadID, OActorUtil.ToArray(akActor), "JoinOngoingSex", akActor)
EndFunction

;==========================================================================
; Change Pace Action
; This action allows NPCs to change the speed and intensity of the current sexual activity
;==========================================================================

; Changes the pace of the current sexual activity
; @param akActor The actor changing the pace
; @param contextJson The context data in JSON format
; @param paramsJson The parameters containing the desired speed change
Function ChangeSexPaceActionExecute(Actor akActor, string speed) global
    int ThreadId = OActor.GetSceneID(akActor)
    if(speed != "increase" && speed != "decrease")
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
Function StopSexActionExecute(Actor akActor) global
    TTON_OStimIntegration.StopOStim(akActor)
    SkyrimNetApi.SetActionCooldown("StopSex", TTON_JData.GetMcmDenyCooldown())
EndFunction

Function SpectatorOfSexActionExecute(Actor akActor, string contextJson, string paramsJson) global
    Actor target = SkyrimNetApi.GetJsonActor(paramsJson, "target", none)
    if(!target)
        TTON_Debug.warn("SpectatorOfSexActionExecute: target actor not found in paramsJson")
        return
    endif

    int ThreadID = OActor.GetSceneID(akActor)

    if(ThreadID == -1)
        TTON_Debug.warn("SpectatorOfSexActionExecute: actor is not in a valid OStim scene")
        return
    endif

    if(!TTON_Spectators.CanAddSpectator())
        TTON_Debug.warn("SpectatorOfSexActionExecute: cannot add spectator")
        return
    endif

    if(!TTON_Spectators.CanAddSpectatorsToThread(ThreadID))
        TTON_Debug.warn("SpectatorOfSexActionExecute: cannot add more spectators to thread " + ThreadID)
        return
    endif

    TTON_Spectators.TryMakeSpectator(akActor, target)
EndFunction

Function SpectatorOfSexFleeActionExecute(Actor akActor, string contextJson, string paramsJson) global
    if(akActor.IsInFaction(TTON_JData.GetSpectatorFaction()))
    endif
    TTON_Spectators.MakeSpectatorFlee(akActor)
EndFunction

Function PrintEligibilityConditions(Actor npc, string functionName, string keyName, bool value) global
    ; string npcName = TTON_Utils.GetActorName(npc)
    ; TTON_Debug.Trace(functionName + ":" + npcName + ":" + keyName + " is " + value)
EndFunction
