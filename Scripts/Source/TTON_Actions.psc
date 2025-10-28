scriptname TTON_Actions

;==========================================================================
; Main Registration
;==========================================================================

; Registers all available OStimNet actions with the SkyrimNet system.
; This is the main entry point for registering all sex-related actions.
Function RegisterActions() global
    RegisterActionStartSex()
    RegisterActionStartAffection()
    RegisterActionChangeSexPosition()
    RegisterActionSexInvite()
    RegisterActionSexJoin()
    RegisterActionSexChangePace()
    RegisterActionSexStop()
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
; Parameters:
;   partner1: Primary companion to share the affectionate moment (required)
;   interaction: Specific gentle interaction to focus on (kiss | hug | cuddle)
;==========================================================================

; Registers the StartAffectionScene action with SkyrimNet.
; @returns True if registration was successful, false otherwise
Bool Function RegisterActionStartAffection() global
    int res = SkyrimNetApi.RegisterAction("StartAffectionScene", \
  TTON_JData.GetActionDescription("StartAffectionScene"), \
  "TTON_Actions", "StartAffectionSceneIsEligible", "TTON_Actions", "StartAffectionSceneAction", "", "PAPYRUS", 1, \
  TTON_JData.GetActionParameters("StartAffectionScene"), \
  "", GetTags())

    return res == 1
EndFunction

; Placeholder eligibility check for the affection scene action.
; Implementation should ensure actors are free from ongoing OStim threads and mutually willing.
Bool Function StartAffectionSceneIsEligible(Actor akActor, string contextJson, string paramsJson) global
    return !OActor.IsInOStim(akActor) && TTON_Utils.IsOStimEligible(akActor) && TTON_JData.IsActionAvailableAfterDeny(akActor, "startAffection") && TTON_JData.GetStartNewSexEnable()
EndFunction

; Placeholder handler for initiating the affection scene.
; Actual implementation should be added when the underlying scene logic is available.
Function StartAffectionSceneAction(Actor akActor, string contextJson, string paramsJson) global
    string actions = SkyrimNetApi.GetJsonString(paramsJson, "type", "kissing")
    Actor participant1 = TTON_Utils.GetEligibleActorFromParam(paramsJson, "participant1")

    if(!participant1)
        TTON_Debug.warn(TTON_Utils.GetActorName(akActor) + " wanted to start affection scene but failed to get participant1.")
        return
    endif

    ; check if any of potential participants are already in other ostim threads
    if(OActor.IsInOStim(akActor) || OActor.IsInOStim(participant1))
        TTON_Debug.warn("Can't start new OStim scene one or more of potential participants is already a part of ongoing OStim thread.")
        return
    endif

    Actor[] actors = OActorUtil.ToArray(akActor, participant1)
    TTON_OStimIntegration.StartOstim(actors, actions, nonSexual = true, initiator = akActor)
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
  TTON_JData.GetActionDescription("StartNewSex"), \
  "TTON_Actions", "StartSexActionIsElgigible", "TTON_Actions", "StartSexAction", "", "PAPYRUS", 1, \
  TTON_JData.GetActionParameters("StartNewSex"), \
  "", GetTags())

  return res == 1
EndFunction

; Checks if an actor is eligible to start a new sexual encounter
; @param akActor The actor to check eligibility for
; @param contextJson The context data in JSON format
; @param paramsJson The parameters data in JSON format
; @returns True if the actor is eligible to start a new sexual encounter
Bool Function StartSexActionIsElgigible(Actor akActor, string contextJson, string paramsJson) global
    return !OActor.IsInOStim(akActor) && TTON_Utils.IsOStimEligible(akActor) && TTON_JData.IsActionAvailableAfterDeny(akActor, "startSex") && TTON_JData.GetStartNewSexEnable()
EndFunction

; Starts a new sexual encounter with specified participants
; @param akActor The initiating actor
; @param contextJson The context data in JSON format
; @param paramsJson The parameters containing participant info and encounter type
Function StartSexAction(Actor akActor, string contextJson, string paramsJson) global
    string actions = SkyrimNetApi.GetJsonString(paramsJson, "type", "vaginalsex")
    string furn = SkyrimNetApi.GetJsonString(paramsJson, "furniture", "none")
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
    TTON_OStimIntegration.StartOstim(actors, actions, furn, initiator = akActor)
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
  TTON_JData.GetActionDescription("ChangeSexPosition"), \
  "TTON_Actions", "ChangeSexPositionIsElgigible", "TTON_Actions", "ChangeSexPositionAction", "", "PAPYRUS", 1, \
  TTON_JData.GetActionParameters("ChangeSexPosition")) == 1
EndFunction

; Checks if an actor is eligible to change their current sexual position
; @param akActor The actor to check eligibility for
; @param contextJson The context data in JSON format
; @param paramsJson The parameters data in JSON format
; @returns True if the actor is currently in an OStim scene
Bool Function ChangeSexPositionIsElgigible(Actor akActor, string contextJson, string paramsJson) global
    int ThreadID =OActor.GetSceneID(akActor)
    return OActor.IsInOStim(akActor) && TTON_JData.IsActionAvailableAfterDeny(akActor, "sceneChange") && !TTON_JData.GetThreadAffectionOnly(ThreadID)
EndFunction

; Changes the sexual position or activity in an ongoing encounter
; @param akActor The actor requesting the position change
; @param contextJson The context data in JSON format
; @param paramsJson The parameters containing the new position type
; @returns True if the position was successfully changed
Bool Function ChangeSexPositionAction(Actor akActor, string contextJson, string paramsJson) global
    string type = SkyrimNetApi.GetJsonString(paramsJson, "type", "empty")
    TTON_OStimIntegration.OStimChangeScene(akActor, type)
    SkyrimNetApi.SetActionCooldown("ChangeSexPosition", TTON_JData.GetMcmDenyCooldown())
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
  TTON_JData.GetActionDescription("InviteToYourSex"), \
  "TTON_Actions", "SexInviteIsElgigible", "TTON_Actions", "SexInviteAction", "", "PAPYRUS", 1, \
  TTON_JData.GetActionParameters("InviteToYourSex")) == 1
EndFunction

; Checks if an actor is eligible to invite others to their sexual encounter
; @param akActor The actor wanting to invite others
; @param contextJson The context data in JSON format
; @param paramsJson The parameters data in JSON format
; @returns True if the actor is currently in an OStim scene
Bool Function SexInviteIsElgigible(Actor akActor, string contextJson, string paramsJson) global
    int ThreadID =OActor.GetSceneID(akActor)
    return OActor.IsInOStim(akActor) && TTON_JData.IsActionAvailableAfterDeny(akActor, "invite") && !TTON_JData.GetThreadAffectionOnly(ThreadID)
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
        if(StorageUtil.GetIntValue(target1, "SexInviteConsidering") == 1)
            target1 = none
        else
            StorageUtil.SetIntValue(target1, "SexInviteConsidering", 1)
        endif
    endif

    ; 2 or more free spaces in current thread
    if(currentActorsLength < 4)
        target2 = TTON_Utils.GetEligibleActorFromParam(paramsJson, "target2")
        if(StorageUtil.GetIntValue(target2, "SexInviteConsidering") == 1)
            target2 = none
        else
            StorageUtil.SetIntValue(target2, "SexInviteConsidering", 1)
        endif
    endif

    ; 3 or more free spaces in current thread
    if(currentActorsLength < 3)
        target3 = TTON_Utils.GetEligibleActorFromParam(paramsJson, "target3")
        if(StorageUtil.GetIntValue(target3, "SexInviteConsidering") == 1)
            target3 = none
        else
            StorageUtil.SetIntValue(target3, "SexInviteConsidering", 1)
        endif
    endif
    TTON_OStimIntegration.AddActorsToActiveThread(ThreadID, OActorUtil.ToArray(target1, target2, target3), "invite", akActor)
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
  TTON_JData.GetActionDescription("JoinOngoingSex"), \
  "TTON_Actions", "SexJoinIsElgigible", "TTON_Actions", "SexJoinAction", "", "PAPYRUS", 1, \
  TTON_JData.GetActionParameters("JoinOngoingSex")) == 1
EndFunction

; Checks if an actor is eligible to join an ongoing sexual encounter
; @param akActor The actor wanting to join
; @param contextJson The context data in JSON format
; @param paramsJson The parameters data in JSON format
; @returns True if the actor is not in a scene, there are active scenes, and the actor is eligible
Bool Function SexJoinIsElgigible(Actor akActor, string contextJson, string paramsJson) global
    return TTON_Utils.IsActorEligibleToJoin(akActor) && TTON_JData.IsActionAvailableAfterDeny(akActor, "selfjoin")
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
    if(StorageUtil.GetIntValue(akTarget, "SexInviteConsidering") == 1)
        return false
    endif
    if(TTON_JData.GetThreadAffectionOnly(ThreadID))
        return false
    endif
    StorageUtil.SetIntValue(akTarget, "SexInviteConsidering", 1)
    TTON_OStimIntegration.AddActorsToActiveThread(ThreadID, OActorUtil.ToArray(akActor), "selfjoin", akActor)
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
  TTON_JData.GetActionDescription("ChangeSexPace"), \
  "TTON_Actions", "SexChangePaceIsElgigible", "TTON_Actions", "ChangeSexPaceAction", "", "PAPYRUS", 1, \
  TTON_JData.GetActionParameters("ChangeSexPace")) == 1
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
    return OActor.IsInOStim(akActor) && hasSpeedsToMove != "none" && !TTON_JData.GetThreadAffectionOnly(ThreadId)
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

    TTON_OStimIntegration.OStimChangeSpeed(ThreadId, speed)
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
  TTON_JData.GetActionDescription("StopSex"), \
  "TTON_Actions", "StopSexIsEligible", "TTON_Actions", "StopSexAction", "", "PAPYRUS", 1, \
  TTON_JData.GetActionParameters("StopSex")) == 1
EndFunction

; Checks if an actor is eligible to stop their current sexual encounter
; @param akActor The actor wanting to stop
; @param contextJson The context data in JSON format
; @param paramsJson The parameters data in JSON format
; @returns True if the actor is currently in an OStim scene
bool Function StopSexIsEligible(Actor akActor, string contextJson, string paramsJson) global
    int ThreadID =OActor.GetSceneID(akActor)
    return OActor.IsInOStim(akActor) &&  TTON_JData.IsActionAvailableAfterDeny(akActor, "stopSex") && !TTON_JData.GetThreadAffectionOnly(ThreadID)
EndFunction

; Stops the current sexual encounter
; @param akActor The actor stopping the scene
; @param contextJson The context data in JSON format
; @param paramsJson The parameters data in JSON format
Function StopSexAction(Actor akActor, string contextJson, string paramsJson) global
    TTON_OStimIntegration.StopOStim(akActor)
    SkyrimNetApi.SetActionCooldown("StopSex", TTON_JData.GetMcmDenyCooldown())
EndFunction

