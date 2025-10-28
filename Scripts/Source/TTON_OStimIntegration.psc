scriptname TTON_OStimIntegration

; Starts a new OStim scene with the given actors and actions
; @param actors Array of actors to include in the scene
; @param actions Optional scene actions or actions to filter by
; @returns Thread ID of the new scene, or -1 if failed
int function StartOstim(actor[] actors, string actions = "", string furn = "", bool continuation = false, Actor initiator = none, bool nonSexual = false, string type = "") global
    Actor player = TTON_JData.GetPlayer()
    actors = OActorUtil.Sort(actors, OActorUtil.EmptyArray())
    bool someActorsBusy = false
    int i = 0
    bool hasPlayer = false
    while(i < actors.Length)
        if(actors[i] == player)
            hasPlayer = true
        endif
        if(OActor.IsInOStim(actors[i]))
            someActorsBusy = true
        endif
        i += 1
    endwhile
    if(someActorsBusy)
        TTON_Debug.warn("StartOstim: tried to start new thread with actors who are busy in another thread.")
        return -1
    endif
    if(hasPlayer && !continuation)
        string initiatorName = TTON_Utils.GetActorName(initiator)
        if(nonSexual)
            bool yes = TTON_Utils.Ask("StartAffectionScene", initiator, \
                initiatorName + " wants to have non-sexual interaction with you. Do you accept?")
            if(!yes)
                return -1
            endif
        else
            bool yes = TTON_Utils.Ask("StartNewSex", initiator, \
            initiatorName + " wants to have sexual encounter with " + TTON_Utils.GetActorsNamesComaSeparated(actors, initiator) + ". Do you accept?")
            if(!yes)
                return -1
            endif
        endif
    endif
    int builderID = OThreadBuilder.create(actors)
    string newScene

    if(!nonSexual)
        ; Check if player furniture selection is enabled
        bool ostimDefaultStart = TTON_JData.GetUseOStimDefaultStartSelection()

        if(ostimDefaultStart)
            return OThread.QuickStart(actors)
        endif

        ; Use automatic furniture detection (original behavior)
        ObjectReference furnObject = OFurniture.FindFurnitureOfType(furn, player, 1000)

        if(furnObject)
            OThreadBuilder.SetFurniture(builderId, furnObject)
            newScene = TTON_Utils.getSceneByActions(actors, actions, furn)
        else
            ; Mark scene to not use furniture if none found and player selection is disabled
            OThreadBuilder.NoFurniture(builderId)
            newScene = TTON_Utils.getSceneByActions(actors, actions)
        endif
    else
        newScene = TTON_Utils.getSceneByActions(actors, actions, nonSexual = true)
        OThreadBuilder.NoFurniture(builderId)
        OThreadBuilder.SetDuration(builderId, TTON_JData.GetMcmAffectionDuration() as float)
        OThreadBuilder.NoAutoMode(builderId)
        OThreadBuilder.NoPlayerControl(builderId)
    endif


    OThreadBuilder.SetStartingAnimation(builderID, newScene)
    int newThreadID = OThreadBuilder.Start(builderID)

    if(nonSexual)
        TTON_JData.SetThreadAffectionOnly(newThreadID, 1)
    endif

    return newThreadID
endfunction

; Stops ongoing OStim thread
; @param actors Array of actors to include in the scene
; @returns Thread ID of the new scene, or -1 if failed
Function StopOStim(Actor initiator) global
    int ThreadId = OActor.GetSceneID(initiator)
    if(ThreadId == 0 && !TTON_JData.GetThreadAddNewActors(ThreadId))
        string initiatorName = TTON_Utils.GetActorName(initiator)
        bool yes = TTON_Utils.Ask("StopSex", initiator, \
        initiatorName + " wishes to end your sexual encounter. Allow them to withdraw?")
        if(!yes)
            return
        endif
    endif
    OThread.Stop(ThreadId)

    bool hadSex = TTLL_OstimThreadsCollector.GetHadSex(ThreadID)

    Form[] actorsForms = TTLL_OstimThreadsCollector.GetActorsForms(ThreadID)
    Actor[] actors = PapyrusUtil.ActorArray(actorsForms.Length)
    int i = 0
    string climaxedActors = ""
    while(i < actors.Length)
        actors[i] = actorsForms[i] as Actor
        if(TTLL_OstimThreadsCollector.GetOrgasmed(ThreadID, actors[i]))
            climaxedActors += TTON_Utils.GetActorName(actors[i]) + ","
        endif

        i += 1
    endwhile

    climaxedActors += ""

    string msg = ""

    if(hadSex)
        msg += "INTIMATE "
    else
        msg += "SEXUAL "
    endif
    msg += "ACTIVITY CONCLUDED: Participants "+TTON_Utils.GetActorsNamesComaSeparated(actors)
    string LastSexualSceneId = TTLL_OstimThreadsCollector.GetLastSexualSceneId(ThreadID)

    if(hadSex)
        if(climaxedActors != "")
            msg += ", with " + climaxedActors + " having reached orgasm."
        else
            msg += "."
        endif
    else
        if(LastSexualSceneId)
            msg +=  TTON_Utils.GetSceneDescription(LastSexualSceneId, actors)
        endif
        msg += " without sexual activities."
    endif

    ; string last5Scenes = TTON_Utils.Get5LastScenesInThread(ThreadID)
    ; if(last5Scenes != "")
    ;     msg += " During encounter participant engaged in such scenes: \\n- "
    ;     msg += TTON_Utils.Get5LastScenesInThread(ThreadID)
    ; endif

    ; TTON_Utils.RequestSexComment(msg, actors, none, true)
EndFunction

; Changes the animation scene of an ongoing OStim scene
; @param ThreadId The ID of the thread to modify
Function OStimChangeScene(Actor akActor, string activity) global
    int ThreadID = OActor.GetSceneID(akActor)
    string furn = OThread.GetFurnitureType(ThreadID)

    if(ThreadID == 0)
        string initiatorName = TTON_Utils.GetActorName(akActor)
        bool yes = TTON_Utils.Ask("ChangeSexActivity", akActor, \
            initiatorName + " wants to change the sexual activity to " + activity + ". Do you wish to proceed?")
        if(!yes)
            return
        endif
    endif

    Actor[] actors = OThread.GetActors(ThreadID)
    string sceneId = TTON_Utils.getSceneByActions(actors, activity, furn)
    ; try to navigate to new scene
    ; if no scene with requested action type skup this scene change
    if(sceneId)
        OThread.WarpTo(ThreadID, sceneId, 5)
    endif
EndFunction

; Changes the animation speed of an ongoing OStim scene
; @param ThreadId The ID of the thread to modify
; @param speed Direction to change speed ("increase" or "decrease")
Function OStimChangeSpeed(int ThreadId, string speed) global
    int currentSpeed = OThread.GetSpeed(ThreadId)
    string sceneId = OThread.GetScene(ThreadId)
    int maxSpeed = OMetadata.GetMaxSpeed(sceneId)
    int minSpeed = OMetadata.GetDefaultSpeed(sceneId)
    if(speed == "increase" && currentSpeed < maxSpeed)
        currentSpeed += 1
    elseif(speed == "decrease" && currentSpeed > minSpeed)
        currentSpeed -= 1
    endif
    OThread.SetSpeed(ThreadId, currentSpeed)
EndFunction

; Adds new actors to an ongoing OStim scene
; @param ThreadID The ID of the thread to modify
; @param newActors Array of actors to add to the scene
function AddActorsToActiveThread(int ThreadID, actor[] newActors, string actionName, Actor initiator) global
    if(!OThread.IsRunning(ThreadID))
        return
    endif
    actor[] originalActors = OThread.GetActors(ThreadID)
    actor[] allActors = TTON_Utils.GetAllActorsAfterJoin(ThreadID, newActors)

    bool shouldSkip = false

    if(allActors.Length == originalActors.Length)
        TTON_Debug.warn("AddActorsToActiveThread: Amount of actors is same as before there is no one to add.")
        shouldSkip = true
    endif

    if(!TTON_Utils.UserHasScenesForActors(OActorUtil.Sort(allActors, OActorUtil.EmptyArray())))
        TTON_Debug.warn("AddActorsToActiveThread: Don't stop ongoing thread. User doesn't have suitable scene for new set of actors.")
        shouldSkip = true
    endif
    bool youInvited = PapyrusUtil.CountActor(newActors, TTON_JData.GetPlayer()) > 0
    if((ThreadID == 0 || youInvited) && !shouldSkip)
        string joinInvite
        string initiatorName = TTON_Utils.GetActorName(initiator)
        if(actionName == "InviteToYourSex")
            if(youInvited)
                question = initiatorName + " invites you to join their ongoing sexual activities. Do you accept?"
            else
                question = initiatorName + " invites " + TTON_Utils.GetActorsNamesComaSeparated(newActors) + " to join their ongoing sexual activities with you. Will you allow them?"
            endif
        else
            question = initiatorName + " seeks to join your sexual activities. Will you allow them?"
        endif
        string question

        bool yes = TTON_Utils.Ask(actionName, initiator, question, youInvited)
        if(!yes)
            shouldSkip = true
        endif
    endif
    if(!shouldSkip)
        TTON_JData.SetThreadAddNewActors(ThreadID)
        OThread.Stop(ThreadID)
        while(OThread.isRunning(ThreadID))
            Utility.Wait(0.2)
        endwhile
        int NewThreadID = StartOstim(allActors, "", OThread.GetFurnitureType(ThreadID), continuation = true)
        TTON_JData.SetThreadContinuationFrom(ThreadID, NewThreadID)
    endif

    ClearConsideringActors(newActors)
endfunction

Function ClearConsideringActors(Actor[] actors) global
    int i = 0
    while(i < actors.Length)
        if(StorageUtil.GetIntValue(actors[i], "SexInviteConsidering") == 1)
            StorageUtil.UnsetIntValue(actors[i], "SexInviteConsidering")
        endif
    endwhile
EndFunction

