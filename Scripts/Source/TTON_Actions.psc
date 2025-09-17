scriptname TTON_Actions

;==========================================================================
; Main Registration
;==========================================================================

; Registers all available OStimNet actions with the SkyrimNet system.
; This is the main entry point for registering all sex-related actions.
Function RegisterActions() global
    RegisterActionStartSex()
    RegisterActionChangeSexPosition()
    RegisterActionSexInvite()
    RegisterActionSexJoin()
    RegisterActionSexChangePace()
    RegisterActionSexStop()
EndFunction

;==========================================================================
; Start New Sex Action
; This action allows NPCs to initiate new sexual encounters with up to 4 participants - total up to 5 participants including speaking npc.
; Parameters:
;   participant1: Primary partner to engage in sexual activities (required)
;   participant2: Optional second partner for the sexual encounter
;   participant3: Optional third partner for the sexual encounter
;   participant4: Optional fourth partner for the sexual encounter
;   type: Specific sexual activity type from available actions list
;==========================================================================

; Registers the StartNewSex action with SkyrimNet.
; @returns True if registration was successful, false otherwise
Bool Function RegisterActionStartSex() global
    int res = SkyrimNetApi.RegisterAction("StartNewSex", \
  "{% set speakingNpc = tton_get_speaking_npc_sex_action_info(npc.UUID) %}Initiates an sexual encounter with the {{speakingNpc.name}} and selected partners. Use ONLY for starting new scenes (for joining use JoinOngoingSex action). Consider {{speakingNpc.name}}'s personality and relationship status. Requires at least one partner. Select partners only from: {{speakingNpc.nearbyPotentialPartners}}. Select particular action/position from: {{speakingNpc.actions}}", \
  "TTON_Actions", "StartSexActionIsElgigible", "TTON_Actions", "StartSexAction", "", "PAPYRUS", 1, \
  "{\"participant1\": \"Primary partner to engage in sexual activities with the speaking NPC (required)\", \"participant2\": \"Optional second partner for the sexual encounter\", \"participant3\": \"Optional third partner for the sexual encounter\", \"participant4\": \"Optional fourth partner for the sexual encounter\", \"type\": \"Specific sexual activity type.\"}")

  return res == 1
EndFunction

; Checks if an actor is eligible to start a new sexual encounter
; @param akActor The actor to check eligibility for
; @param contextJson The context data in JSON format
; @param paramsJson The parameters data in JSON format
; @returns True if the actor is eligible to start a new sexual encounter
Bool Function StartSexActionIsElgigible(Actor akActor, string contextJson, string paramsJson) global
    return !OActor.IsInOStim(akActor) && TTON_Utils.IsOStimEligible(akActor)
EndFunction

; Starts a new sexual encounter with specified participants
; @param akActor The initiating actor
; @param contextJson The context data in JSON format
; @param paramsJson The parameters containing participant info and encounter type
Function StartSexAction(Actor akActor, string contextJson, string paramsJson) global
    string type = SkyrimNetApi.GetJsonString(paramsJson, "type", "vaginalsex")
    Actor participant1 = TTON_Utils.GetEligibleActorFromParam(paramsJson, "participant1")
    Actor participant2 = TTON_Utils.GetEligibleActorFromParam(paramsJson, "participant2")
    Actor participant3 = TTON_Utils.GetEligibleActorFromParam(paramsJson, "participant3")
    Actor participant4 = TTON_Utils.GetEligibleActorFromParam(paramsJson, "participant4")

    ; check if any of potential participants are already in other ostim threads
    if(OActor.IsInOStim(akActor) || OActor.IsInOStim(participant1) || OActor.IsInOStim(participant2) || OActor.IsInOStim(participant3) || OActor.IsInOStim(participant4))
        TTON_Debug.warn("Can't start new OStim scene one or more of potential participants is already a part of ongoing OStim thread.")
        return
    endif

    Actor[] actors = OActorUtil.ToArray(akActor, participant1, participant2, participant3, participant4)
    TTON_Utils.StartOstim(actors, type)
EndFunction

;==========================================================================
; Change Position Action
; This action allows NPCs to change their current sexual activity or position
; Parameters:
;   type: The specific sexual activity to change to, selected from available actions list
;==========================================================================

; Registers the ChangeSexPosition action with SkyrimNet
; @returns True if registration was successful, false otherwise
bool Function RegisterActionChangeSexPosition() global
    return SkyrimNetApi.RegisterAction("ChangeSexPosition", \
  "{% set speakingNpc = tton_get_speaking_npc_sex_action_info(npc.UUID) %}Changes the sexual activity during an ongoing encounter. Use when {{speakingNpc.name}} desires a different position or activity. Select particular action/position from: {{speakingNpc.actions}}", \
  "TTON_Actions", "ChangeSexPositionIsElgigible", "TTON_Actions", "ChangeSexPositionAction", "", "PAPYRUS", 1, \
  "{\"type\": \"The specific sexual activity to change to.\"}") == 1
EndFunction

; Checks if an actor is eligible to change their current sexual position
; @param akActor The actor to check eligibility for
; @param contextJson The context data in JSON format
; @param paramsJson The parameters data in JSON format
; @returns True if the actor is currently in an OStim scene
Bool Function ChangeSexPositionIsElgigible(Actor akActor, string contextJson, string paramsJson) global
    return OActor.IsInOStim(akActor)
EndFunction

; Changes the sexual position or activity in an ongoing encounter
; @param akActor The actor requesting the position change
; @param contextJson The context data in JSON format
; @param paramsJson The parameters containing the new position type
; @returns True if the position was successfully changed
Bool Function ChangeSexPositionAction(Actor akActor, string contextJson, string paramsJson) global
    string type = SkyrimNetApi.GetJsonString(paramsJson, "type", "empty")
    int ThreadID = OActor.GetSceneID(akActor)
    string sceneId = TTON_Utils.getSceneByActionsOrTags(OThread.GetActors(ThreadID), type)
    ; try to navigate to new scene
    ; if no scene with requested action type skup this scene change
    if(sceneId)
        OThread.QueueNavigation(ThreadID, sceneId, 5)
    endif
EndFunction

;==========================================================================
; Invite Others Action
; This action allows NPCs to invite others to join their ongoing sexual encounter
; Parameters:
;   target1: First person to invite to the ongoing scene (required)
;   target2: Second person to invite to the ongoing scene (optional)
;   target3: Third person to invite to the ongoing scene (optional)
;==========================================================================

; Registers the InviteToYourSex action with SkyrimNet
; @returns True if registration was successful, false otherwise
bool Function RegisterActionSexInvite() global
    return SkyrimNetApi.RegisterAction("InviteToYourSex", \
  "{% set speakingNpc = tton_get_speaking_npc_sex_action_info(npc.UUID) %}{{speakingNpc.name}} invites others to join ongoing sexual encounter. Use only for adding new participants to an existing {{speakingNpc.name}}'s encounter. Consider relationship dynamics and potential interest of invitees. Select invitees only: : {{speakingNpc.nearbyPotentialPartners}}", \
  "TTON_Actions", "SexInviteIsElgigible", "TTON_Actions", "SexInviteAction", "", "PAPYRUS", 1, \
  "{\"target1\": \"First person to invite to the ongoing sexual scene (required)\", \"target2\": \"Second person to invite to the ongoing sexual scene (optional)\", \"target3\": \"Third person to invite to the ongoing sexual scene (optional)\"}") == 1
EndFunction

; Checks if an actor is eligible to invite others to their sexual encounter
; @param akActor The actor wanting to invite others
; @param contextJson The context data in JSON format
; @param paramsJson The parameters data in JSON format
; @returns True if the actor is currently in an OStim scene
Bool Function SexInviteIsElgigible(Actor akActor, string contextJson, string paramsJson) global
    return OActor.IsInOStim(akActor)
EndFunction

; Handles the invitation of new participants to an ongoing sexual encounter
; @param akActor The actor sending the invitation
; @param contextJson The context data in JSON format
; @param paramsJson The parameters containing target actors to invite
Bool Function SexInviteAction(Actor akActor, string contextJson, string paramsJson) global
    int ThreadID = OActor.GetSceneID(akActor)
    int currentActorsLength = OThread.GetActors(ThreadID).Length
    ; OStim scenes animations at this time can support up to 5 participants total
    Actor target1 = none
    Actor target2 = none
    Actor target3 = none

    ; max participants in current ostim scenes is 5, if it is already 5 skip this action
    if(currentActorsLength == 5)
        return false
    endif

    ; 1 or more free spaces in current thread
    if(currentActorsLength < 5)
        target1 = TTON_Utils.GetEligibleActorFromParam(paramsJson, "target1")
    endif

    ; 2 or more free spaces in current thread
    if(currentActorsLength < 4)
        target2 = TTON_Utils.GetEligibleActorFromParam(paramsJson, "target2")
    endif

    ; 3 or more free spaces in current thread
    if(currentActorsLength < 3)
        target3 = TTON_Utils.GetEligibleActorFromParam(paramsJson, "target3")
    endif
    TTON_Utils.AddActorsToActiveThread(ThreadID, OActorUtil.ToArray(target1, target2, target3))
EndFunction

;==========================================================================
; Join Sex Action
; This action allows NPCs to join an ongoing sexual encounter they are observing
; Parameters:
;   target: Specific participant in the encounter to focus interaction with (required)
;==========================================================================

; Registers the JoinOngoingSex action with SkyrimNet
; @returns True if registration was successful, false otherwise
bool Function RegisterActionSexJoin() global
    return SkyrimNetApi.RegisterAction("JoinOngoingSex", \
  "{% set speakingNpc = tton_get_speaking_npc_sex_join_action_info(npc.UUID) %}{{speakingNpc.name}} joins an ongoing sexual encounter they are currently observing. Consider {{speakingNpc.name}}'s relationship with participants, their current mood, and whether their participation would be welcome. Select target only from: {{speakingNpc.nearbyPotentialPartners}}", \
  "TTON_Actions", "SexJoinIsElgigible", "TTON_Actions", "SexJoinAction", "", "PAPYRUS", 1, \
  "{\"target\": \"Specific participant already in the sexual encounter to focus interaction with - choose based on relationship and current activities (required)\"}") == 1
EndFunction

; Checks if an actor is eligible to join an ongoing sexual encounter
; @param akActor The actor wanting to join
; @param contextJson The context data in JSON format
; @param paramsJson The parameters data in JSON format
; @returns True if the actor is not in a scene, there are active scenes, and the actor is eligible
Bool Function SexJoinIsElgigible(Actor akActor, string contextJson, string paramsJson) global
    return !OActor.IsInOStim(akActor) && OThread.GetThreadCount() > 0 && TTON_Utils.IsOStimEligible(akActor)
EndFunction

; Handles joining an ongoing sexual encounter
; @param akActor The actor joining the scene
; @param contextJson The context data in JSON format
; @param paramsJson The parameters containing the target actor to interact with
; @returns True if successfully joined the scene
Bool Function SexJoinAction(Actor akActor, string contextJson, string paramsJson) global
    Actor akTarget = SkyrimNetApi.GetJsonActor(paramsJson, "target", none)
    int ThreadID = OActor.GetSceneID(akTarget)
    int currentActorsLength = OThread.GetActors(ThreadID).Length
    ; max participants in current ostim scenes is 5, if it is already 5 skip this action
    if(currentActorsLength == 5)
        return false
    endif
    TTON_Utils.AddActorsToActiveThread(ThreadID, OActorUtil.ToArray(akActor))
EndFunction

;==========================================================================
; Change Pace Action
; This action allows NPCs to change the speed and intensity of the current sexual activity
; Parameters:
;   speed: Desired speed dynamic (increase | decrease)
;==========================================================================

; Registers the ChangeSexPace action with SkyrimNet
; @returns True if registration was successful, false otherwise
Bool Function RegisterActionSexChangePace() global
    SkyrimNetApi.RegisterAction("ChangeSexPace", \
  "{% set speakingNpc = tton_get_speaking_npc_sex_action_info(npc.UUID) %}Changes the rhythm and intensity of the current sexual activity. Use when {{speakingNpc.name}} wants to adjust the pace while maintaining the same position or activity. Consider current stamina, emotional intensity, and natural progression of the encounter.", \
  "TTON_Actions", "SexChangePaceIsElgigible", "TTON_Actions", "ChangeSexPaceAction", "", "PAPYRUS", 1, \
  "{\"speed\": \"Desired speed dynamic: increase | decrease\"}") == 1
EndFunction

; Checks if an actor is eligible to change the pace of their current sexual activity
; @param akActor The actor wanting to change pace
; @param contextJson The context data in JSON format
; @param paramsJson The parameters data in JSON format
; @returns True if the actor is in a scene and the scene has available speed changes
Bool Function SexChangePaceIsElgigible(Actor akActor, string contextJson, string paramsJson) global
    int ThreadId = OActor.GetSceneID(akActor)
    string SceneId = OThread.GetScene(ThreadId)
    int currentSpeed = OThread.GetSpeed(ThreadId)
    string hasSpeedsToMove = TTON_Utils.GetAvailableSpeedDirections(SceneId, currentSpeed)
    return OActor.IsInOStim(akActor) && hasSpeedsToMove != "none"
EndFunction

; Changes the pace of the current sexual activity
; @param akActor The actor changing the pace
; @param contextJson The context data in JSON format
; @param paramsJson The parameters containing the desired speed change
Function ChangeSexPaceAction(Actor akActor, string contextJson, string paramsJson) global
    int ThreadId = OActor.GetSceneID(akActor)
    string speed = SkyrimNetApi.GetJsonString(paramsJson, "speed", "none")
    if(speed == "none")
        return
    endif

    TTON_Utils.OStimChangeSpeed(ThreadId, speed)
EndFunction

;==========================================================================
; Stop Sex Action
; This action allows NPCs to end their current sexual encounter
; Parameters:
;   None
;==========================================================================

; Registers the StopSex action with SkyrimNet
; @returns True if registration was successful, false otherwise
bool Function RegisterActionSexStop() global
    return SkyrimNetApi.RegisterAction("StopSex", \
  "{% set speakingNpc = decnpc(npc.UUID) %}{{speakingNpc.name}} decides to end current sexual encounter that {{speakingNpc.name}} is participating in. Use when {{speakingNpc.name}} wishes to stop for any reason - discomfort, interruption, or changing circumstances.", \
  "TTON_Actions", "StopSexIsEligible", "TTON_Actions", "StopSexAction", "", "PAPYRUS", 1, \
  "") == 1
EndFunction

; Checks if an actor is eligible to stop their current sexual encounter
; @param akActor The actor wanting to stop
; @param contextJson The context data in JSON format
; @param paramsJson The parameters data in JSON format
; @returns True if the actor is currently in an OStim scene
bool Function StopSexIsEligible(Actor akActor, string contextJson, string paramsJson) global
    return OActor.IsInOStim(akActor)
EndFunction

; Stops the current sexual encounter
; @param akActor The actor stopping the scene
; @param contextJson The context data in JSON format
; @param paramsJson The parameters data in JSON format
Function StopSexAction(Actor akActor, string contextJson, string paramsJson) global
    int ThreadId = OActor.GetSceneID(akActor)
    OThread.Stop(ThreadId)
EndFunction

