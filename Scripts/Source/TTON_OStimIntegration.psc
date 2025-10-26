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
        if(nonSexual)
            if(type == "")
                type = "non-sexual interaction"
            endif
            bool yes = TTON_Utils.ShowConfirmMessage(TTON_Utils.GetActorName(initiator) + " wants to have " + type + " with " + TTON_Utils.GetActorsNamesComaSeparated(actors, initiator) + ". Do you accept?", "confirmStartAffection")
            if(!yes)
                TTON_JData.SetDeniedLastTime(initiator, "startAffection")
                TTON_Utils.RequestSexComment(TTON_Utils.GetActorName(initiator) + " wanted to start "+ type +" with " + TTON_Utils.GetActorName(player) + ", but for some reason "+TTON_Utils.GetActorName(player)+" declined it.", \
                speaker = initiator )
                return -1
            endif
        else
            if(type == "")
                type = "sexual encounter"
            endif
            bool yes = TTON_Utils.ShowConfirmMessage(TTON_Utils.GetActorName(initiator) + " wants to have " + type + " with " + TTON_Utils.GetActorsNamesComaSeparated(actors, initiator) + ". Do you accept?", "confirmStartSex")
            if(!yes)
                TTON_JData.SetDeniedLastTime(initiator, "startSex")
                TTON_Utils.RequestSexComment(TTON_Utils.GetActorName(initiator) + " wanted to have " + type + " with " + TTON_Utils.GetActorName(player) + ", but for some reason "+TTON_Utils.GetActorName(player)+" declined it.", \
                speaker = initiator )
                return -1
            endif
        endif

    endif
    int builderID = OThreadBuilder.create(actors)
    string newScene

    if(!nonSexual)
        ; Check if player furniture selection is enabled
        bool allowPlayerFurnitureSelection = TTON_JData.GetAllowPlayerFurnitureSelection()

        ; Use automatic furniture detection (original behavior)
        ObjectReference furnObject = OFurniture.FindFurnitureOfType(furn, player, 1000)

        if(furnObject)
            OThreadBuilder.SetFurniture(builderId, furnObject)
            newScene = TTON_Utils.getSceneByActions(actors, actions, furn)
        else
            ; Mark scene to not use furniture if none found and player selection is disabled
            if(!allowPlayerFurnitureSelection)
                OThreadBuilder.NoFurniture(builderId)
            endif
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
        bool yes = TTON_Utils.ShowConfirmMessage(TTON_Utils.GetActorName(initiator) + " wishes to end your sexual encounter. Allow them to withdraw?", "confirmStopSex")
        if(!yes)
            TTON_JData.SetDeniedLastTime(initiator, "stopSex")
            TTON_JData.SetThreadForced(ThreadId)
            TTON_Events.RegisterStopSexDeniedEvent(ThreadId, initiator)
            Actor player = TTON_JData.GetPlayer()
            TTON_Utils.RequestSexComment(TTON_Utils.GetActorName(initiator) + " expresses what they think about " + TTON_Utils.GetActorName(player) + "'s decision to ignore their request to stop sexual activity. It is not necessarily negative tone, decide based on previous events and dialogue.", \
            speaker = initiator )
            return
        endif
    endif
    OThread.Stop(ThreadId)
EndFunction

; Changes the animation scene of an ongoing OStim scene
; @param ThreadId The ID of the thread to modify
Function OStimChangeScene(Actor akActor, string type) global
    int ThreadID = OActor.GetSceneID(akActor)
    string furn = OThread.GetFurnitureType(ThreadID)

    if(ThreadID == 0)
        bool yes = TTON_Utils.ShowConfirmMessage("Your partner "+TTON_Utils.GetActorName(akActor)+" suggests changing to " + type + ". Do you wish to proceed?", "confirmChangeScene")
        if(!yes)
            TTON_JData.SetDeniedLastTime(akActor, "sceneChange")
            Actor player = TTON_JData.GetPlayer()
            TTON_Utils.RequestSexComment(TTON_Utils.GetActorName(akActor) + " wanted to change sex position, but "+TTON_Utils.GetActorName(player)+" declined and decided to stay in current position.", \
            speaker = akActor )
            return
        endif
    endif

    Actor[] actors = OThread.GetActors(ThreadID)
    string sceneId = TTON_Utils.getSceneByActions(actors, type, furn)
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
function AddActorsToActiveThread(int ThreadID, actor[] newActors, string type, Actor initiator) global
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
    if(ThreadID == 0 && !shouldSkip)
        string joinInvite
        if(type == "invite")
            joinInvite = "were invited to"
        else
            joinInvite = "seeks to join"
        endif
        bool yes = TTON_Utils.ShowConfirmMessage(TTON_Utils.GetActorsNamesComaSeparated(newActors) + " " + joinInvite + " your sexual activities. Will you allow them?", "confirmAddActors")
        if(!yes)
            TTON_JData.SetDeniedLastTime(initiator, type)
            string msg = TTON_Utils.GetActorName(initiator)
            if(type == "invite")
                msg += " wanted to invite to their sex "+TTON_Utils.GetActorsNamesComaSeparated(newActors)
            else
                msg += " wanted to join ongoing their sex involving "+TTON_Utils.GetActorsNamesComaSeparated(originalActors)
            endif
            Actor player = TTON_JData.GetPlayer()
            msg += ", but " + TTON_Utils.GetActorName(player) + " declined it."
            TTON_Utils.RequestSexComment(msg, speaker = initiator)
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

