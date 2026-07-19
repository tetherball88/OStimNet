scriptname TTON_OStimIntegration

; Starts a new OStim scene with the given actors and actions
; @param actors Array of actors to include in the scene
; @param actions Optional scene actions or actions to filter by
; @returns Thread ID of the new scene, or -1 if failed
int function StartOstimSex(Actor[] dom = none, Actor[] sub = none, string actions = "", string sexualPosition = "", string intent = "", Actor[] initiator = none) global
    Actor player = TTON_JData.GetPlayer()
    Actor[] actors = PapyrusUtil.MergeActorArray(dom, sub, true)
    actors = OActorUtil.Sort(actors, OActorUtil.EmptyArray())
    bool someActorsBusy = false
    int i = 0
    bool hasPlayer = false
    ObjectReference furn = none
    while(i < actors.Length)
        Actor current = actors[i]
        if(current == player)
            hasPlayer = true
        endif
        if(TTON_Utils.IsActorBusyWithScenes(current, false))
            someActorsBusy = true
        endif

        Form actorFurniture = StorageUtil.GetFormValue(current, "TTON_FurnitureActor")

        if(actorFurniture)
            furn = actorFurniture as ObjectReference
            StorageUtil.UnsetFormValue(current, "TTON_FurnitureActor")
        endif
        i += 1
    endwhile
    if(someActorsBusy)
        TTON_Debug.warn("StartOstim: tried to start new thread with actors who are busy in another thread.")
        return -1
    endif

    if(actions == "foreplay")
        actions = "boobjob,breastsmothering,buttsmothering,footjob,handjob,lickingnipple,rubbingclitoris,suckingnipple,vaginalfingering"
    elseif(actions == "oral")
        actions = "blowjob,deepthroat,lickingpenis,lickingtesticles,cunnilingus,lickingvagina,anilingus"
    endif

    if(furn)
        TTON_Debug.debug("StartOstim: waiting for actors to be near furniture before starting scene. Furniture: " + furn)
        int stucktimer = 0
        Actor target = actors[0]
        if(target == player)
            target = actors[1]
        endif
        int startGen = OStimNet.GetLocationGeneration()
        while (target.GetDistance(furn) > 160 && stucktimer <= 10)
            if (startGen != OStimNet.GetLocationGeneration())
                TTON_Debug.info("Player moved to another location while waiting for actors to reach furniture. Canceling scene.")
                StorageUtil.FormListRemove(none, "TTON_FurnitureList", furn)
                TTON_Utils.SetActorsPending(actors, false)
                TTON_Utils.FreeParticipants(actors)
                return -1
            endif
            TTON_Debug.debug("StartOstim: actor " + target + " is not near furniture yet. Distance: " + target.GetDistance(furn) + ". Waiting...")
            stucktimer += 1
            Utility.Wait(5.0)
        endwhile
        TTON_Debug.debug("Finished waiting for actors to be near furniture. Stucktimer: " + stucktimer)
    endif

    int builderID = OThreadBuilder.create(actors)
    if(dom && dom.Length > 0)
        OThreadBuilder.SetDominantActors(builderID, dom)
    endif

    TTON_Debug.info("StartOstim: Starting sexual scene with actors: " + actors + ", actions: " + actions)
    bool isFemdom = dom[0].GetActorBase().GetSex() == 1
    string newScene = ""
    string phase = OStimNet.GetInitialPhaseByIntent(intent, hasPlayer)
    if(phase == "undressing" && actors.length == 2 && !furn)
        TTON_Debug.debug("StartOstim: Skipping scene search for undressing phase with 2 actors. Using default undressing scene.")
        newScene = "OStim2PStandingApartMF"
    else
        newScene = TTON_SceneSearch.SceneSexSearch(actors, true, OFurniture.GetFurnitureType(furn), actions, sexualPosition, intent, true, isFemdom, hasPlayer, true)
    endif

    if(furn)
        OThreadBuilder.SetFurniture(builderID, furn)
        StorageUtil.FormListRemove(none, "TTON_FurnitureList", furn)
    else
        OThreadBuilder.NoFurniture(builderID)
    endif

    OThreadBuilder.SetStartingAnimation(builderID, newScene)
    bool isLlmSceneAdvance = SkyrimNetApi.GetConfigBool("Plugin_OStimNet", "tton.gameMaster.scheduledEvalNpcThreads", true)
    if(!hasPlayer && isLlmSceneAdvance)
        OThreadBuilder.NoAutoMode(builderID)
    endif
    int claimToken = OStimNet.ClaimThread(intent, true, dom, sub)
    int newThreadID = OThreadBuilder.Start(builderID)
    OStimNet.ConfirmThread(claimToken, newThreadID)
    TTON_Utils.SetActorsPending(actors, false)
    TTON_Utils.FreeParticipants(actors)

    return newThreadID
endfunction

int Function StartOStimCaring(Actor initiator, Actor participant, string actions = "", string intent = "") global
    Actor player = TTON_JData.GetPlayer()
    int i = 0
    if(!initiator || !participant)
        TTON_Debug.warn("StartOStimCaring: invalid actors provided.")
        return -1
    endif
    bool hasPlayer = initiator == player || participant == player
    bool someActorsBusy = TTON_Utils.IsActorBusyWithScenes(initiator, false) || TTON_Utils.IsActorBusyWithScenes(participant, false)
    if(someActorsBusy)
        TTON_Debug.warn("StartOstim: tried to start new thread with actors who are busy in another thread.")
        return -1
    endif

    Actor[] actors =  OActorUtil.ToArray(initiator, participant)
    actors = OActorUtil.Sort(actors, OActorUtil.EmptyArray())

    int builderID = OThreadBuilder.create(actors)

    TTON_Debug.info("StartOstim: Starting non-sexual scene with actors: " + actors + ", actions: " + actions)
    string actionfallback = ""

    string newScene = TTON_SceneSearch.SearchCareScene(actors, intent, actions, initiator, actionfallback)
    OThreadBuilder.NoFurniture(builderID)
    OThreadBuilder.SetDuration(builderID, SkyrimNetApi.GetConfigInt("Plugin_OStimNet", "tton.settings.nonSexualDuration", 20) as float)
    OThreadBuilder.NoAutoMode(builderID)
    OThreadBuilder.NoPlayerControl(builderID)

    OThreadBuilder.SetStartingAnimation(builderID, newScene)
    Actor[] careMain = OActorUtil.ToArray(initiator)
    Actor[] careSecondary = OActorUtil.ToArray(participant)
    int claimToken = OStimNet.ClaimThread(intent, false, careMain, careSecondary)
    int newThreadID = OThreadBuilder.Start(builderID)
    OStimNet.ConfirmThread(claimToken, newThreadID)
    TTON_Utils.SetActorsPending(actors, false)

    return newThreadID
EndFunction

; Stops ongoing OStim thread
; @param actors Array of actors to include in the scene
; @returns Thread ID of the new scene, or -1 if failed
Function StopOStim(Actor initiator) global
    int ThreadId = OActor.GetSceneID(initiator)
    OThread.Stop(ThreadId)
EndFunction

; Changes the intent of an ongoing OStim thread, reshuffles actor roles, and warps
; the scene to one appropriate for the new intent.
; @param akActor       Any actor currently participating in the thread
; @param newIntent     The new intent string (e.g. "dom", "lustful", "romantic")
; @param newMainActors Actors to assign as the primary role for the new intent.
;                      Remaining thread participants become secondary automatically.
Function OStimChangeIntent(Actor akActor, string newIntent, Actor[] newMainActors) global
    int ThreadID = OActor.GetSceneID(akActor)
    if(ThreadID == -1)
        TTON_Debug.warn("OStimChangeIntent: actor " + akActor + " is not in any active thread.")
        return
    endif
    OStimNet.SetThreadIntent(ThreadID, newIntent, newMainActors)
    OStimChangeScene(akActor, "", "", newIntent)
EndFunction

; Changes the animation scene of an ongoing OStim scene
; @param ThreadId The ID of the thread to modify
Function OStimChangeScene(Actor akActor, string activity, string sexualPosition, string intent) global
    int ThreadID = OActor.GetSceneID(akActor)
    string furn = OThread.GetFurnitureType(ThreadID)
    bool forceFurn = furn != ""

    Actor[] actors = OThread.GetActors(ThreadID)
    if(activity == "")
        string currentSceneId = OThread.GetScene(ThreadID)
        int[] allActivities = OMetadata.FindAllActionsCSV(currentSceneId, "sexual")
        int selectedActivity = Utility.RandomInt(0, allActivities.Length - 1)
        activity = OMetadata.GetActionType(currentSceneId, allActivities[selectedActivity])
    endif
    if(sexualPosition == "")
        string[] tags = OMetadata.GetSceneTags(OThread.GetScene(ThreadID))
        sexualPosition = OStimNet.GetSexualPositionFromTags(tags)
    endif
    if(intent == "")
        intent = OStimNet.GetThreadIntent(ThreadID)
    endif
    bool isFemdom = OStimNet.GetMainActors(ThreadID)[0].GetActorBase().GetSex() == 1
    string sceneId = TTON_SceneSearch.SceneSexSearch(actors, forceFurn, furn, activity, sexualPosition, intent, false, isFemdom, ThreadID == 0, false)
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
; @param newIntent The new intent string for the thread
; @param main LLM-evaluated main actors for the new thread (optional, actor-add path)
; @param secondary LLM-evaluated secondary actors for the new thread (optional, actor-add path)
function AddActorsToActiveThread(int ThreadID, actor[] newActors, string newIntent, Actor[] main = none, Actor[] secondary = none) global
    TTON_Utils.SetActorsPending(newActors, false)
    if(!OThread.IsRunning(ThreadID))
        return
    endif
    OStimNet.SetContinuationOverride(ThreadID, newIntent, main, secondary)
    OStimNet.SetThreadContinuation(ThreadID, true)
    int newThreadID = OStimHotSwap.AddActorsToThread(ThreadID, newActors)
endfunction

ObjectReference Function ScanBestBeds(ObjectReference centerRef, float radius = 1200.0) global
    ObjectReference[] beds = OSANative.FindBed(centerRef, radius)
    TTON_Debug.debug("Found " + beds.length + " pieces of furniture near actors for potential use in OStim scene.")

    int i = 0
    while(i < beds.Length)
        string furnType = OFurniture.GetFurnitureType(beds[i])
        bool isBed = OFurniture.IsChildOf("bed", furnType)
        TTON_Debug.debug("Checking furniture " + beds[i] + " of type " + furnType + " for suitability in OStim scene, isBed: " + isBed)
        if(isBed && Storageutil.FormListCountValue(none, "TTON_FurnitureList", beds[i]) == 0)
            if(furnType == "doublebed")
                StorageUtil.FormListAdd(none, "TTON_DoubleBeds", beds[i])
            elseif(furnType == "singlebed")
                StorageUtil.FormListAdd(none, "TTON_SingleBeds", beds[i])
            elseif(furnType == "bedroll")
                StorageUtil.FormListAdd(none, "TTON_Bedrolls", beds[i])
            else
                StorageUtil.FormListAdd(none, "TTON_OtherBeds", beds[i])
            endif
        endif
        i += 1
    endwhile

    if(StorageUtil.FormListCount(none, "TTON_DoubleBeds") > 0)
        return StorageUtil.FormListGet(none, "TTON_DoubleBeds", 0) as ObjectReference
    elseif(StorageUtil.FormListCount(none, "TTON_SingleBeds") > 0)
        return StorageUtil.FormListGet(none, "TTON_SingleBeds", 0) as ObjectReference
    elseif(StorageUtil.FormListCount(none, "TTON_Bedrolls") > 0)
        return StorageUtil.FormListGet(none, "TTON_Bedrolls", 0) as ObjectReference
    elseif(StorageUtil.FormListCount(none, "TTON_OtherBeds") > 0)
        return StorageUtil.FormListGet(none, "TTON_OtherBeds", 0) as ObjectReference
    endif
EndFunction

ObjectReference Function FindSuitableFurniture(Actor akActor, string furnitureType, float radius = 1200.0) global
    ObjectReference finalFurniture = none

    if(furnitureType && furnitureType == "floor")
        finalFurniture = none
    elseif(furnitureType && furnitureType != "bed")
        finalFurniture = OFurniture.FindFurnitureOfType(furnitureType, akActor, radius)
    else
        finalFurniture = TTON_OStimIntegration.ScanBestBeds(akActor)
    endif

    if(finalFurniture)
        TTON_Debug.debug("Found furniture for OStim scene: " + finalFurniture)
        Storageutil.FormListAdd(none, "TTON_FurnitureList", finalFurniture)
        StorageUtil.SetFormValue(akActor, "TTON_FurnitureActor", finalFurniture)
        return finalFurniture
    endif

    return none
EndFunction
