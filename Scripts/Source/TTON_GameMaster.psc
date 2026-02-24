scriptname TTON_GameMaster extends Quest
; 0 - sent
; 1 - busy
; 2 - failed
int Function EvaluateNearbyNpcs()
    if(StorageUtil.GetIntValue(none, "TTON_GameMaster_EvaluateNearbyNpcsTaskQueued") == 1)
        TTON_Debug.info("EvaluateNearbyNpcs: Another task is already queued.")
        Return 1
    endif
    int res = SkyrimNetApi.SendCustomPromptToLLM("ostimnet_nearby_npcs_game_master", TTON_JData.GetGmLlmVariant(), "", self, "TTON_GameMaster", "ProcessNearbyNpcs")

    if(res == 1)
        StorageUtil.SetIntValue(none, "TTON_GameMaster_EvaluateNearbyNpcsTaskQueued", 1)
        TTON_Debug.info("EvaluateNearbyNpcs: Task queued successfully.")
        return 0
    elseif(res == -1)
        TTON_Debug.warn("EvaluateNearbyNpcs: Prompt name is empty.")
    elseif(res == -2)
        TTON_Debug.warn("EvaluateNearbyNpcs: Callback parameters are empty or null.")
    elseif(res == -3)
        TTON_Debug.warn("EvaluateNearbyNpcs: LLM configuration failed.")
    endif
    return 2
EndFunction

Function ProcessNearbyNpcs(String response, Int success)
    StorageUtil.SetIntValue(none, "TTON_GameMaster_EvaluateNearbyNpcsTaskQueued", 0)
    If success != 1
        TTON_Debug.warn("ProcessNearbyNpcs: rejected success=" + success )
        Return
    EndIf
    Actor participant1 = SkyrimNetApi.GetJsonActor(response, "participant1", none)
    Actor participant2 = SkyrimNetApi.GetJsonActor(response, "participant2", none)
    Actor participant3 = SkyrimNetApi.GetJsonActor(response, "participant3", none)
    Actor participant4 = SkyrimNetApi.GetJsonActor(response, "participant4", none)
    Actor participant5 = SkyrimNetApi.GetJsonActor(response, "participant5", none)

    Actor[] participants = PapyrusUtil.ActorArray(0)

    string partcipantsString = ""

    if(participant1)
        ; Make sure player isn't first participant, which considered to be initiator.
        if(participant1 == Game.GetPlayer())
            Actor tmp = participant2
            participant2 = participant1
            participant1 = tmp
        endif
        participants = PapyrusUtil.PushActor(participants, participant1)
        partcipantsString += TTON_Utils.GetActorName(participant1) + ", "
    endif

    if(participant2)
        participants = PapyrusUtil.PushActor(participants, participant2)
        partcipantsString += TTON_Utils.GetActorName(participant2) + ", "
    endif

    if(participant3)
        participants = PapyrusUtil.PushActor(participants, participant3)
        partcipantsString += TTON_Utils.GetActorName(participant3) + ", "
    endif

    if(participant4)
        participants = PapyrusUtil.PushActor(participants, participant4)
        partcipantsString += TTON_Utils.GetActorName(participant4) + ", "
    endif

    if(participant5)
        participants = PapyrusUtil.PushActor(participants, participant5)
        partcipantsString += TTON_Utils.GetActorName(participant5)
    endif

    if(participants.Length > 0)
        TTON_OStimIntegration.StartOstim(participants, initiator = participants[0], furn = "bed")
    endif
EndFunction

; 0 - sent
; 1 - busy
; 2 - failed
int Function EvaluatePotentialParticipants(Actor intiator, Actor participant1, Actor participant2, Actor participant3, Actor participant4)
    if(StorageUtil.GetIntValue(none, "TTON_GameMaster_EvaluatePotentialParticipantsTaskQueued") == 1)
        TTON_Debug.info("EvaluatePotentialParticipants: Another task is already queued.")
        Return 1
    endif
    StorageUtil.SetIntValue(none, "TTON_GameMaster_EvaluatePotentialParticipantsTaskQueued", 1)
    StorageUtil.SetFormValue(none, "TTON_GameMaster_EvaluatePotentialParticipants_Initiator", intiator)

    if(participant1)
        StorageUtil.SetFormValue(none, "TTON_GameMaster_EvaluatePotentialParticipants_Participant1", participant1)
    endif
    if(participant2)
        StorageUtil.SetFormValue(none, "TTON_GameMaster_EvaluatePotentialParticipants_Participant2", participant2)
    endif
    if(participant3)
        StorageUtil.SetFormValue(none, "TTON_GameMaster_EvaluatePotentialParticipants_Participant3", participant3)
    endif
    if(participant4)
        StorageUtil.SetFormValue(none, "TTON_GameMaster_EvaluatePotentialParticipants_Participant4", participant4)
    endif
    Utility.Wait(0.5)
    TTON_Debug.debug("EvaluatePotentialParticipants: Sending custom prompt to LLM. Initiator=" + intiator + ", Participant1=" + participant1 + ", Participant2=" + participant2 + ", Participant3=" + participant3 + ", Participant4=" + participant4)
    int res = SkyrimNetApi.SendCustomPromptToLLM("ostimnet_evaluate_potential_partners", TTON_JData.GetGmLlmVariant(), "", self, "TTON_GameMaster", "ProcessPotentialParticipants")

    if(res == 1)
        TTON_Debug.info("EvaluatePotentialParticipants: Task queued successfully.")
        return 0
    elseif(res == -1)
        TTON_Debug.warn("EvaluatePotentialParticipants: Prompt name is empty.")
    elseif(res == -2)
        TTON_Debug.warn("EvaluatePotentialParticipants: Callback parameters are empty or null.")
    elseif(res == -3)
        TTON_Debug.warn("EvaluatePotentialParticipants: LLM configuration failed.")
    endif

    return 2
EndFunction

Function ProcessPotentialParticipants(String response, Int success)
    StorageUtil.SetIntValue(none, "TTON_GameMaster_EvaluatePotentialParticipantsTaskQueued", 0)
    If success != 1
        TTON_Debug.warn("ProcessPotentialParticipants: rejected success=" + success )
        Return
    EndIf

    Actor player = TTON_JData.GetPlayer()
    Actor initiator = StorageUtil.GetFormValue(none, "TTON_GameMaster_EvaluatePotentialParticipants_Initiator") as Actor
    Actor participant1 = StorageUtil.GetFormValue(none, "TTON_GameMaster_EvaluatePotentialParticipants_Participant1") as Actor
    Actor participant2 = StorageUtil.GetFormValue(none, "TTON_GameMaster_EvaluatePotentialParticipants_Participant2") as Actor
    Actor participant3 = StorageUtil.GetFormValue(none, "TTON_GameMaster_EvaluatePotentialParticipants_Participant3") as Actor
    Actor participant4 = StorageUtil.GetFormValue(none, "TTON_GameMaster_EvaluatePotentialParticipants_Participant4") as Actor

    MiscUtil.PrintConsole("ProcessPotentialParticipants: initiator=" + initiator + ", participant1=" + participant1 + ", participant2=" + participant2 + ", participant3=" + participant3 + ", participant4=" + participant4)
    MiscUtil.PrintConsole("ProcessPotentialParticipants:Reason" + SkyrimNetApi.GetJsonString(response, "reason", ""))


    bool participant1Agreed = SkyrimNetApi.GetJsonBool(response, "participant1", true)
    bool participant2Agreed = SkyrimNetApi.GetJsonBool(response, "participant2", true)
    bool participant3Agreed = SkyrimNetApi.GetJsonBool(response, "participant3", true)
    bool participant4Agreed = SkyrimNetApi.GetJsonBool(response, "participant4", true)
    string participantsAgreed = ""
    string participantsRejected = ""

    if(participant1 && participant1 != player)
        if(participant1Agreed)
            participantsAgreed += TTON_Utils.GetActorName(participant1) + ", "
        else
            participantsRejected += TTON_Utils.GetActorName(participant1) + ", "
            participant1 = none
        endif
    endif
    if(participant2 && participant2 != player)
        if(participant2Agreed)
            participantsAgreed += TTON_Utils.GetActorName(participant2) + ", "
        else
            participantsRejected += TTON_Utils.GetActorName(participant2) + ", "
            participant2 = none
        endif
    endif
    if(participant3 && participant3 != player)
        if(participant3Agreed)
            participantsAgreed += TTON_Utils.GetActorName(participant3) + ", "
        else
            participantsRejected += TTON_Utils.GetActorName(participant3) + ", "
            participant3 = none
        endif
    endif
    if(participant4 && participant4 != player)
        if(participant4Agreed)
            participantsAgreed += TTON_Utils.GetActorName(participant4) + ", "
        else
            participantsRejected += TTON_Utils.GetActorName(participant4) + ", "
            participant4 = none
        endif
    endif

    Actor[] actors = OActorUtil.ToArray(initiator, participant1, participant2, participant3, participant4)

    TTON_Debug.debug("FINAL LENGTH:" + actors.Length)

    if(actors.Length > 1)
        SkyrimNetApi.PurgeDialogue(true)
        TTON_OStimIntegration.StartOstim(actors, initiator = initiator, furn = "bed")
        ; Don't request narration and dialogues, it OStimNet start scene should trigger dialogue.
    else
        SkyrimNetApi.DirectNarration("Participants  " + participantsRejected + " rejected to have sex with " + TTON_Utils.GetActorName(initiator) + ".")
    endif

    StorageUtil.ClearAllPrefix("TTON_GameMaster_EvaluatePotentialParticipants_Initiator")
    StorageUtil.ClearAllPrefix("TTON_GameMaster_EvaluatePotentialParticipants_Participant")
EndFunction

Function ClearStorage() global
    StorageUtil.ClearAllPrefix("TTON_GameMaster_")
EndFunction
