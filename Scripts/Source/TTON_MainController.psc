Scriptname TTON_MainController extends Quest

GlobalVariable Property TTON_IsOstimActive  Auto


Event OnInit()
    Maintenance()
EndEvent

Function Maintenance()
    ; clear sexual data from Lover's Ledger which is now its own separate integration
    StorageUtil.ClearAllPrefix("TTONDec_SexualData")
    TTON_Utils.ClearPendingActorsOnLoad()
    TTON_Storage.ClearOnLoad()
    TTON_Events.RegisterEventSchema()
    TTON_GameMaster.ClearStorage()
    TTON_Undress.SetupData()

    RegisterForModEvent("ostim_thread_start", "OStimStart")
    RegisterForModEvent("ostim_thread_scenechanged", "OStimSceneChange")
    RegisterForModEvent("ostim_thread_speedchanged", "OStimSpeedChange")
    RegisterForModEvent("ostim_actor_orgasm", "OStimOrgasm")
    RegisterForModEvent("ostim_thread_end", "OStimEnd")

    RegisterForModEvent("ostimnet_start", "OStimNetStart")
    RegisterForModEvent("ostimnet_continue_thread", "OStimNetContinueThread")
    RegisterForModEvent("ostimnet_stop", "OStimNetStop")
    RegisterForModEvent("ostimnet_scene_change", "OStimNetSceneChange")
    RegisterForModEvent("ostimnet_climax", "OStimNetClimax")
    RegisterForModEvent("ostimnet_decline", "OStimNetDecline")
    RegisterForModEvent("ostimnet_intent_changed", "OStimNetIntentChanged")
    RegisterForModEvent("ostimnet_speed_change", "OStimNetSpeedChange")

    RegisterForModEvent("ocum_squirt_spurt", "OCumSquirt")
    RegisterForModEvent("ocum_squirt_flow", "OCumSquirt")
    RegisterForModEvent("ocum_applied_cum", "OCumApplied")
    RegisterForModEvent("ocum_applied_cum_npc_scene", "OCumAppliedNpcScene")

    RegisterForModEvent("ostimnet_add_spectator", "OnSpectatorAdded")
    RegisterForModEvent("ostimnet_remove_spectator", "OnSpectatorRemoved")

    RegisterForModEvent("ostimnet_sexual_evaluation_finished", "OnSexEvaluationFinished")
    RegisterForModEvent("ostimnet_nonsexual_evaluation_finished", "OnNonSexEvaluationFinished")

    RegisterForModEvent("ostimnet_join_sex_evaluation_finished", "OnJoinSexEvaluationFinished")
    RegisterForModEvent("ostimnet_invite_sex_evaluation_finished", "OnInviteSexEvaluationFinished")

    RegisterForModEvent("ostimnet_location_scan_result", "OnLocationScanResult")
    RegisterForModEvent("ostimnet_scene_change_schedule_finished", "OnGmSceneAdvanceFinished")

    TTON_Events.RegisterEvents()
    TTON_Spectators.CleanAllSpectatorsStorage()
EndFunction

; bridge between ocum and OStimNet skse events.
Event OCumSquirt(string eventName, string strArg, float numArg, Form sender)
    TTON_Debug.debug("Received OCumSquirt event, amount: " + numArg + ", actor: " + sender)
    string type
    if(eventName == "ocum_squirt_spurt")
        type = "spurt"
    else
        type = "flow"
    endif
    OStimNet.OcumSquirt(numArg as int, sender as Actor, strArg, type)
EndEvent

; bridge between ocum and OStimNet skse events.
Event OCumApplied(Form Orgasmer, Form Target, float AmountML, string Area, string SceneID)
    TTON_Debug.debug("Received OCumApplied event, amount: " + AmountML + ", orgasmer: " + Orgasmer + ", target: " + Target + ", area: " + Area + ", scene: " + SceneID)
    OStimNet.OcumApplied(0, Orgasmer as Actor, Target as Actor, AmountML, Area, SceneID)
EndEvent

; bridge between ocum and OStimNet skse events.
Event OCumAppliedNpcScene(Form Orgasmer, Form Target, int ThreadID, float AmountML, string Area, string SceneID)
    TTON_Debug.debug("Received OCumAppliedNpcScene event, amount: " + AmountML + ", orgasmer: " + Orgasmer + ", target: " + Target + ", area: " + Area + ", scene: " + SceneID + ", thread id: " + ThreadID)
    OStimNet.OcumApplied(ThreadID, Orgasmer as Actor, Target as Actor, AmountML, Area, SceneID)
EndEvent

; Custom OStimNet event which already filters out continuation threads and fires only for truly new threads.
Event OStimNetStart(string eventName, string strArg, float numArg, Form akSpeaker)
    TTON_IsOstimActive.SetValue(1.0)
    int ThreadID = numArg as int
    TTON_Debug.debug("Received OStimNet start event, thread id: " + ThreadID + ", data: " + strArg + ", Speaker: " + akSpeaker)
    SkyrimNetApi.RegisterEvent("tton_event", strArg, akSpeaker as Actor, none)

    string phase = OStimNet.GetThreadPhase(ThreadID)
    Actor[] actors = OThread.GetActors(ThreadID)
    ObjectReference furn = OThread.GetFurniture(ThreadID)
    TTON_Debug.debug("OStimNet start event: phase=" + phase + ", actors=" + actors)
    if(phase == "undressing" && actors.length == 2 && !furn)
        StorageUtil.IntListAdd(none, "TTONUndressing_ActiveThreads", ThreadID)
        StorageUtil.IntListRemove(none, "TTONUndressing_InterruptedThreads", ThreadID)
        TTON_Undress.SmartUndress(ThreadID, actors)
    endif
EndEvent

Event OStimNetContinueThread(string eventName, string strArg, float numArg, Form akSpeaker)
    TTON_Debug.debug("Received OStimNet continue thread event, thread id: " + numArg + ", data: " + strArg)
    SkyrimNetApi.RegisterEvent("tton_event", strArg, akSpeaker as Actor, none)
EndEvent

Event OStimNetStop(string eventName, string strArg, float numArg, Form akSpeaker)
    int ThreadID = numArg as int
    TTON_Debug.debug("Received OStimNet stop event, thread id: " + ThreadID)
    SkyrimNetApi.RegisterEvent("tton_event", strArg, akSpeaker as Actor, none)
EndEvent

; Custom OStimNet event for scene changes, fires on scene change already have debounce mechanism.
Event OStimNetSceneChange(string eventName, string strArg, float numArg, Form akSpeaker)
    TTON_Debug.debug("Received OStimNet scene change event with new scene: " + strArg + ", thread id: " + numArg)
    SkyrimNetApi.RegisterEvent("tton_event", strArg, akSpeaker as Actor, none)
EndEvent

; Custom OStimNet event for intent changes, fires on intent change already have debounce mechanism.
Event OStimNetIntentChanged(string eventName, string strArg, float numArg, Form akSpeaker)
    TTON_Debug.debug("Received OStimNet intent change event with new intent: " + strArg + ", thread id: " + numArg)
    SkyrimNetApi.RegisterEvent("tton_event", strArg, akSpeaker as Actor, none)
EndEvent

; Custom OStimNet event for speed changes, fires on speed change already have debounce mechanism.
Event OStimNetSpeedChange(string eventName, string strArg, float numArg, Form akSpeaker)
    TTON_Debug.debug("Received OStimNet speed change event with new speed: " + strArg + ", thread id: " + numArg)
    SkyrimNetApi.RegisterEvent("tton_event", strArg, akSpeaker as Actor, none)
EndEvent

; Custom OStimNet event for climax. It has 2 seconds window to collect all ostim orgams events per thread and fire them in batch.
Event OStimNetClimax(string eventName, string strArg, float numArg, Form akSpeaker)
    TTON_Debug.debug("Received OStimNet climax event with data: " + strArg + ", thread id: " + numArg)
    SkyrimNetApi.RegisterEvent("tton_event", strArg, akSpeaker as Actor, none)
EndEvent

; Bridges the C++ ostimnet_decline mod event to SkyrimNet.
Event OStimNetDecline(string eventName, string strArg, float numArg, Form akSpeaker)
    TTON_Debug.debug("Received OStimNet decline event: " + strArg)
    SkyrimNetApi.RegisterEvent("tton_event", strArg, akSpeaker as Actor, none)
EndEvent

Event OStimStart(string eventName, string strArg, float numArg, Form sender)
    int ThreadID = numArg as int
    TTON_IsOstimActive.SetValue(1.0)

    bool isLlmSceneAdvance = SkyrimNetApi.GetConfigBool("Plugin_OStimNet", "tton.gameMaster.scheduledEvalNpcThreads", true)
    if(ThreadID != 0 && isLlmSceneAdvance && OThread.IsInAutoMode(ThreadID))
        OThread.StopAutoMode(ThreadID)
    endif
EndEvent

Event OStimEnd(string eventName, string json, float numArg, Form sender)
    TTON_Debug.debug("Received OStim end event, thread id: " + numArg + ", data: " + json)
    int ThreadID = numArg as int
    StorageUtil.IntListAdd(none, "TTONUndressing_InterruptedThreads", ThreadID)
    if(OThread.GetThreadCount() == 0)
        TTON_IsOstimActive.SetValue(0.0)
        TTON_Spectators.CleanAllSpectatorsStorage()
    endif
    Actor[] actors = OJSON.GetActors(json)
    Actor player = TTON_JData.GetPlayer()

    Utility.Wait(0.2)
    int i = 0
    while(i < actors.Length)
        Actor current = actors[i]
        if(current != player)
            SkyrimNetApi.ReinforcePackages(current)
        endif
        i += 1
    endwhile
EndEvent

Event OnSpectatorAdded(string eventName, string strArg, float numArg, Form sender)
    Actor spectator = sender as Actor
    Actor partner = OStimNet.GetSpectatorTarget(spectator)
    TTON_Debug.debug("OnSpectatorAdded: " + TTON_Utils.GetActorName(spectator) + "->" + TTON_Utils.GetActorName(partner))
    TTON_Spectators.TryMakeSpectator(spectator, partner)
EndEvent

Event OnSpectatorRemoved(string eventName, string strArg, float numArg, Form sender)
    Actor spectator = sender as Actor
    TTON_Debug.debug("OnSpectatorRemoved: " + TTON_Utils.GetActorName(spectator))
    TTON_Spectators.RemoveSpectator(spectator)
EndEvent

Event OnSexEvaluationFinished(string eventName, string strArg, float numArg, Form sender)
    TTON_Debug.debug("Received sex evaluation finished event with data: " + strArg)
    string evalId = OStimNet.JsonGetString(strArg, "evalId")
    int startGen = StorageUtil.GetIntValue(none, "TTON_EvalGen_" + evalId, -1)
    if (startGen != -1 && startGen != OStimNet.GetLocationGeneration())
        TTON_Debug.info("Player moved to another location before sex evaluation finished. Canceling scene.")
        FreeParticipantsOnCancelOrReject(strArg)
        return
    endif
    if(numArg == 0.0)
        TTON_Debug.warn("Sex evaluation finished with error.")
        Debug.Notification("An error occurred while evaluating the sexual scene. Please check the logs for details.")
        FreeParticipantsOnCancelOrReject(strArg)
        return
    endif
    if(numArg == 2.0)
        TTON_Debug.info("Sex evaluation finished: not enough willing participants.")
        Debug.Notification("After evaluation not enough willing participants for the proposed sexual scene.")
        FreeParticipantsOnCancelOrReject(strArg)
        return
    endif
    string intent = OStimNet.JsonGetString(strArg, "intent")
    string sexualPosition = OStimNet.JsonGetString(strArg, "sexualPosition")
    string sexualActivities = OStimNet.JsonGetString(strArg, "sexualActivities")
    Actor[] main = OStimNet.JsonGetActorArray(strArg, "main")
    Actor[] secondary = OStimNet.JsonGetActorArray(strArg, "secondary")
    Actor[] excluded = OStimNet.JsonGetActorArray(strArg, "excluded")

    if(excluded.Length > 0)
        TTON_Debug.debug("Sex evaluation finished: LLM excluded some participants: " + excluded)
        TTON_Utils.SetActorsPending(excluded, false)
        TTON_Utils.FreeParticipants(excluded)
    endif

    if(numArg == 3.0)
        Debug.Notification("The proposed sexual scene was evaluated positively, but some participants were excluded. Check the logs for details.")

        string json = "{ \"activity\": \"" + sexualActivities + "\", \"intent\": \"" + intent + "\", \"isSexual\": true, \"positions\": \"" + sexualPosition + "\", \"excluded\": " + OStimNet.JsonGetString(strArg, "excluded") + " }"
        int result = OStimNet.ShowConfirmationModal("StartNewSexPostEval", main, secondary, json)
        bool earlyReturn = false

        if(result == -1)
            TTON_Debug.warn("Failed to show confirmation modal for StartNewSexPostEval action.")
        endif

        if(result == 1 || result == 2)
            TTON_Debug.warn("Player declined StartNewSexPostEval action.")
        endif

        if(result == 3)
            TTON_Debug.warn("Action StartNewSexPostEval is on cooldown for " + TTON_Utils.GetActorName(main[0]) + ".")
        endif

        if(earlyReturn)
            FreeParticipantsOnCancelOrReject(strArg)
            return
        endif
    endif

    TTON_OStimIntegration.StartOstimSex(main, secondary, sexualActivities, sexualPosition, intent)
EndEvent

Event OnNonSexEvaluationFinished(string eventName, string strArg, float numArg, Form sender)
    TTON_Debug.debug("Received non-sex evaluation finished event with data: " + strArg)
    string evalId = OStimNet.JsonGetString(strArg, "evalId")
    int startGen = StorageUtil.GetIntValue(none, "TTON_EvalGen_" + evalId, -1)
    if (startGen != -1 && startGen != OStimNet.GetLocationGeneration())
        TTON_Debug.info("Player moved to another location before non-sex evaluation finished. Canceling scene.")
        FreeParticipantsOnCancelOrReject(strArg)
        return
    endif
    if(numArg == 0.0)
        Debug.Notification("An error occurred while evaluating the non-sexual scene. Please check the logs for details.")
        TTON_Debug.warn("Non-sex evaluation finished with error.")
        FreeParticipantsOnCancelOrReject(strArg)
        return
    endif
    if(numArg == 2.0)
        Debug.Notification("LLM decided that one recepient is not willing to participate.")
        TTON_Debug.info("Non-sex evaluation finished: not enough willing participants.")
        FreeParticipantsOnCancelOrReject(strArg)
        return
    endif
    string intent = OStimNet.JsonGetString(strArg, "intent")
    string activity = OStimNet.JsonGetString(strArg, "activity")
    Actor[] main = OActorUtil.EmptyArray()
    Actor[] secondary = OActorUtil.EmptyArray()
    main = OStimNet.JsonGetActorArray(strArg, "main")
    secondary = OStimNet.JsonGetActorArray(strArg, "secondary")

    TTON_OStimIntegration.StartOStimCaring(main[0], secondary[0], activity, intent)
EndEvent

Event OnJoinSexEvaluationFinished(string eventName, string strArg, float numArg, Form sender)
    TTON_Debug.debug("Received join sex evaluation finished event with data: " + strArg)
    string evalId = OStimNet.JsonGetString(strArg, "evalId")
    int startGen = StorageUtil.GetIntValue(none, "TTON_EvalGen_" + evalId, -1)
    if (startGen != -1 && startGen != OStimNet.GetLocationGeneration())
        TTON_Debug.info("Player moved to another location before join sex evaluation finished. Canceling scene.")
        FreeParticipantsOnCancelOrReject(strArg, "joiner")
        return
    endif
    if(numArg == 0.0)
        TTON_Debug.warn("Join sex evaluation finished with error.")
        Debug.Notification("An error occurred while evaluating the joining sexual scene. Please check the logs for details.")
        FreeParticipantsOnCancelOrReject(strArg, "joiner")
        return
    endif
    if(numArg == 2.0)
        TTON_Debug.info("Join sex evaluation finished: not enough willing participants.")
        Debug.Notification("After evaluation not enough willing participants for the proposed joining sexual scene.")
        FreeParticipantsOnCancelOrReject(strArg, "joiner")
        return
    endif
    int threadID = OStimNet.JsonGetInt(strArg, "threadID")
    Actor joiner = OStimNet.JsonGetActor(strArg, "joiner")
    string intent = OStimNet.JsonGetString(strArg, "intent")
    Actor[] main = OStimNet.JsonGetActorArray(strArg, "main")
    Actor[] secondary = OStimNet.JsonGetActorArray(strArg, "secondary")

    TTON_OStimIntegration.AddActorsToActiveThread(threadID, OActorUtil.ToArray(joiner), intent, main, secondary)
EndEvent

Event OnInviteSexEvaluationFinished(string eventName, string strArg, float numArg, Form sender)
    TTON_Debug.debug("Received invite sex evaluation finished event with data: " + strArg)
    string evalId = OStimNet.JsonGetString(strArg, "evalId")
    int startGen = StorageUtil.GetIntValue(none, "TTON_EvalGen_" + evalId, -1)
    if (startGen != -1 && startGen != OStimNet.GetLocationGeneration())
        TTON_Debug.info("Player moved to another location before invite sex evaluation finished. Canceling scene.")
        FreeParticipantsOnCancelOrReject(strArg, "invitees")
        return
    endif
    if(numArg == 0.0)
        TTON_Debug.warn("Invite sex evaluation finished with error.")
        Debug.Notification("An error occurred while evaluating the invite sexual scene. Please check the logs for details.")
        FreeParticipantsOnCancelOrReject(strArg, "invitees")
        return
    endif
    if(numArg == 2.0)
        TTON_Debug.info("Invite sex evaluation finished: not enough willing participants.")
        Debug.Notification("After evaluation not enough willing participants for the proposed invite sexual scene")
        FreeParticipantsOnCancelOrReject(strArg, "invitees")
        return
    endif
    int threadID = OStimNet.JsonGetInt(strArg, "threadID")
    Actor[] invitees = OStimNet.JsonGetActorArray(strArg, "invitees")
    string intent = OStimNet.JsonGetString(strArg, "intent")
    Actor[] main = OStimNet.JsonGetActorArray(strArg, "main")
    Actor[] secondary = OStimNet.JsonGetActorArray(strArg, "secondary")

    TTON_OStimIntegration.AddActorsToActiveThread(threadID, invitees, intent, main, secondary)
EndEvent

Event OnLocationScanResult(string eventName, string strArg, float numArg, Form sender)
    TTON_Debug.debug("Received location scan result event with data: " + strArg)
    if(numArg == 0.0)
        TTON_Debug.warn("Location scan result finished with error.")
        Debug.Notification("An error occurred while evaluating the location scan. Please check the logs for details.")
        return
    endif
    if(numArg == 2.0)
        TTON_Debug.info("Location scan result finished: not enough willing participants.")
        Debug.Notification("After evaluation not enough willing participants for the proposed location scan.")
        return
    endif
    string intent = OStimNet.JsonGetString(strArg, "intent")
    string sexualPosition = OStimNet.JsonGetString(strArg, "sexualPosition")
    string sexualActivities = OStimNet.JsonGetString(strArg, "sexualActivities")
    Actor[] main = OStimNet.JsonGetActorArray(strArg, "main")
    Actor[] secondary = OStimNet.JsonGetActorArray(strArg, "secondary")
    string furn = OStimNet.JsonGetString(strArg, "furniture")

    string json = "{ \"activity\": \"" + sexualActivities + "\", \"intent\": \"" + intent + "\", \"isSexual\": true, \"positions\": \"" + sexualPosition + "\" }"
    int result = OStimNet.ShowConfirmationModal("StartNewSexAfterScan", main, secondary, json)
    bool earlyReturn = false

    if(result == -1)
        TTON_Debug.warn("Failed to show confirmation modal for StartNewSexAfterScan action.")
        return
    endif

    if(result == 1 || result == 2)
        TTON_Debug.warn("Player declined StartNewSexAfterScan action.")
        return
    endif

    if(result == 3)
        TTON_Debug.warn("Action StartNewSexAfterScan is on cooldown for " + TTON_Utils.GetActorName(main[0]) + ".")
        return
    endif

    if(furn)
        ObjectReference finalFurniture = TTON_OStimIntegration.FindSuitableFurniture(main[0], furn)
        if(finalFurniture)
            TTON_Utils.MakeParticipantsWalkToFurniture(PapyrusUtil.MergeActorArray(main, secondary), finalFurniture)
        endif
    endif

    TTON_OStimIntegration.StartOstimSex(main, secondary, sexualActivities, sexualPosition, intent)
EndEvent

Event OnGmSceneAdvanceFinished(string eventName, string strArg, float numArg, Form sender)
    if (numArg == 0.0)
        TTON_Debug.warn("GM scene advance evaluation returned no result, skipping.")
        return
    endif
    int ThreadID = OStimNet.JsonGetInt(strArg, "threadID")
    string sexualPosition = OStimNet.JsonGetString(strArg, "sexualPosition")
    string sexualActivities = OStimNet.JsonGetString(strArg, "sexualActivities")
    string intent = OStimNet.GetThreadIntent(ThreadID)
    Actor[] actors = OThread.GetActors(ThreadID)
    if (actors.Length == 0)
        TTON_Debug.warn("GM scene advance: thread " + ThreadID + " has no actors, skipping.")
        return
    endif
    TTON_Debug.debug("GM scene advance: applying position=" + sexualPosition + ", activity=" + sexualActivities + " to thread " + ThreadID)
    TTON_OStimIntegration.OStimChangeScene(actors[0], sexualActivities, sexualPosition, intent)
EndEvent

Function FreeParticipantsOnCancelOrReject(string json, string jsonKey = "originalParticipants")
    if(jsonKey == "joiner")
        Actor joiner = OStimNet.JsonGetActor(json, jsonKey)
        TTON_Utils.SetActorPending(joiner, false)
        TTON_Utils.FreeParticipant(joiner)
    else
        Actor[] allActors = OStimNet.JsonGetActorArray(json, jsonKey)
        TTON_Utils.SetActorsPending(allActors, false)
        TTON_Utils.FreeParticipants(allActors)
    endif

EndFunction
