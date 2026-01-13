scriptname TTON_OStimIntegration

; Starts a new OStim scene with the given actors and actions
; @param actors Array of actors to include in the scene
; @param actions Optional scene actions or actions to filter by
; @returns Thread ID of the new scene, or -1 if failed
int function StartOstim(actor[] actors, string actions = "", string furn = "", bool continuation = false, Actor initiator = none, bool nonSexual = false, string type = "") global
    if(TTON_Utils.IsSexLabInCharge())
        TTON_Debug.warn("StartOstim aborted because SkyrimNet_SexLab is handling player scenes.")
        return -1
    endif
    Actor player = TTON_JData.GetPlayer()
    actors = OActorUtil.Sort(actors, OActorUtil.EmptyArray())
    bool someActorsBusy = false
    int i = 0
    bool hasPlayer = false
    while(i < actors.Length)
        if(actors[i] == player)
            hasPlayer = true
        endif
        if(TTON_Utils.IsActorBusyWithScenes(actors[i]))
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
        TTON_Debug.warn("StartOstim: Starting sexual scene with actors: " + actors + ", actions: " + actions + ", furniture: " + furn)
        ; Check if player furniture selection is enabled
        bool ostimDefaultStart = TTON_JData.GetUseOStimDefaultStartSelection()

        if(ostimDefaultStart)
            TTON_Debug.warn("StartOstim: Using OStim default start selection for furniture." + actors)
            return OThread.QuickStart(actors)
        endif

        ; Use automatic furniture detection (original behavior)
        ObjectReference furnObject = OFurniture.FindFurnitureOfType(furn, player, 1000)

        if(furnObject)
            OThreadBuilder.SetFurniture(builderId, furnObject)
            newScene = TTON_Utils.getSceneByActions(actors, actions, furn)
            TTON_Debug.warn("StartOstim: Found furniture of type " + furn + " for actors: " + actors + ". Using scene: " + newScene)
        else
            ; Mark scene to not use furniture if none found and player selection is disabled
            OThreadBuilder.NoFurniture(builderId)
            newScene = TTON_Utils.getSceneByActions(actors, actions)
            TTON_Debug.warn("StartOstim: No furniture found of type " + furn + " for actors: " + actors + ". Using scene: " + newScene)
        endif
    else
        TTON_Debug.warn("StartOstim: Starting non-sexual scene with actors: " + actors + ", actions: " + actions)
        newScene = TTON_Utils.getSceneByActions(actors, actions, nonSexual = true)
        OThreadBuilder.NoFurniture(builderId)
        OThreadBuilder.SetDuration(builderId, TTON_JData.GetMcmAffectionDuration() as float)
        OThreadBuilder.NoAutoMode(builderId)
        OThreadBuilder.NoPlayerControl(builderId)
    endif

    OThreadBuilder.SetStartingAnimation(builderID, newScene)
    int newThreadID = OThreadBuilder.Start(builderID)

    if(nonSexual)
        TTON_Storage.SetThreadAffectionOnly(newThreadID, 1)
    else
        TTON_Storage.SetThreadAffectionOnly(newThreadID, 0)
    endif

    return newThreadID
endfunction

; Stops ongoing OStim thread
; @param actors Array of actors to include in the scene
; @returns Thread ID of the new scene, or -1 if failed
Function StopOStim(Actor initiator) global
    int ThreadId = OActor.GetSceneID(initiator)
    if(ThreadId == 0 && !TTON_Storage.GetThreadAddNewActors(ThreadId))
        string initiatorName = TTON_Utils.GetActorName(initiator)
        bool yes = TTON_Utils.Ask("StopSex", initiator, \
        initiatorName + " wishes to end your sexual encounter. Allow them to withdraw?")
        if(!yes)
            return
        endif
    endif
    OThread.Stop(ThreadId)
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
        TTON_Storage.SetThreadAddNewActors(ThreadID, 1)
        OThread.Stop(ThreadID)
        while(OThread.isRunning(ThreadID))
            Utility.Wait(0.2)
        endwhile
        int NewThreadID = StartOstim(allActors, "", OThread.GetFurnitureType(ThreadID), continuation = true)
        TTON_Storage.SetThreadContinuationFrom(ThreadID, NewThreadID)
    endif

    ClearConsideringActors(newActors)
endfunction

Function ClearConsideringActors(Actor[] actors) global
    int i = 0
    while(i < actors.Length)
        if(StorageUtil.GetIntValue(actors[i], "SexInviteConsidering") == 1)
            StorageUtil.UnsetIntValue(actors[i], "SexInviteConsidering")
        endif
        i += 1
    endwhile
EndFunction

