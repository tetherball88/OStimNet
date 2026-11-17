scriptname TTON_OStimIntegration

; Starts a new OStim scene with the given actors and actions
; @param actors Array of actors to include in the scene
; @param actions Optional scene actions or actions to filter by
; @returns Thread ID of the new scene, or -1 if failed
int function StartOstimSex(Actor[] dom = none, Actor[] sub = none, string actions = "", string sexualPosition = "", string intent = "", Actor[] initiator = none) global
    Actor player = TTON_JData.GetPlayer()
    Actor[] actors = PapyrusUtil.MergeActorArray(dom, sub, true)
    if(!actors || actors.Length == 0)
        TTON_Debug.warn("StartOstim: no valid actors provided. Aborting.")
        return -1
    endif
    actors = OActorUtil.Sort(actors, OActorUtil.EmptyArray())
    bool someActorsBusy = false
    int i = 0
    bool hasPlayer = false
    ObjectReference furn = none
    Cell firstActorCell = actors[0].GetParentCell()
    while(i < actors.Length)
        Actor current = actors[i]
        if(current == player)
            hasPlayer = true
        endif
        if(TTON_Utils.IsActorBusyWithScenes(current, false))
            someActorsBusy = true
        endif

        if(i != 0)
            Cell currenActorCell = current.GetParentCell()
            if (currenActorCell != firstActorCell)
                TTON_Utils.SetActorsPending(actors, false)
                TTON_Utils.FreeParticipants(actors)
                TTON_Debug.warn("StartOstim: actors are not in the same location. Aborting.")
                TTON_Debug.warn("StartOstim: " + TTON_Utils.GetActorName(actors[0]) + " is in " + firstActorCell + ", " + TTON_Utils.GetActorName(current) + " is in " + currenActorCell)
                return -1
            endif
        endif

        Form actorFurniture = StorageUtil.GetFormValue(current, "TTON_FurnitureActor")

        if(actorFurniture)
            furn = actorFurniture as ObjectReference
            StorageUtil.UnsetFormValue(current, "TTON_FurnitureActor")
        endif
        i += 1
    endwhile
    if(someActorsBusy)
        TTON_Utils.SetActorsPending(actors, false)
        TTON_Utils.FreeParticipants(actors)
        TTON_Debug.warn("StartOstim: tried to start new thread with actors who are busy in another thread.")
        return -1
    endif

    if(furn)
        TTON_Debug.debug("StartOstim: waiting for actors to be near furniture before starting scene. Furniture: " + furn)
        int stucktimer = 0
        Actor target = actors[0]
        if(target == player)
            if(actors.Length > 1)
                target = actors[1]
            else
                target = none
            endif
        endif
        int startGen = OStimNet.GetLocationGeneration()
        while (target && target != player && target.GetDistance(furn) > 160 && stucktimer <= 10)
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

    TTON_Debug.info("StartOstim: Starting sexual scene with actors: " + actors + ", actions: " + actions)
    bool isFemdom = dom && dom.Length > 0 && dom[0] && dom[0].GetActorBase().GetSex() == 1
    string newScene = ""
    string phase = OStimNet.GetInitialPhaseByIntent(intent, hasPlayer)
    bool canUseDefaultUndress = phase == "undressing" && actors.length == 2 && actors[0].GetActorBase().GetSex() == 0 && actors[1].GetActorBase().GetSex() == 1 && OStimNavigator.IsSceneLoaded("OStim2PStandingApartMF")

    if(!furn && canUseDefaultUndress)
        TTON_Debug.debug("StartOstim: Skipping scene search for undressing phase with 2 MF actors. Using default undressing scene.")
        newScene = "OStim2PStandingApartMF"
    elseif(furn)
        newScene = TTON_SceneSearch.SceneSexSearch(actors, true, OFurniture.GetFurnitureType(furn), actions, sexualPosition, intent, true, isFemdom, hasPlayer, true)
        if(newScene == "")
            TTON_Debug.warn("StartOstim: No compatible scene found on furniture " + furn + " (type=" + OFurniture.GetFurnitureType(furn) + "). Falling back to floor.")
            StorageUtil.FormListRemove(none, "TTON_FurnitureList", furn)
            furn = none
            if(canUseDefaultUndress)
                newScene = "OStim2PStandingApartMF"
            else
                newScene = TTON_SceneSearch.SceneSexSearch(actors, false, "", actions, sexualPosition, intent, true, isFemdom, hasPlayer, true)
            endif
        endif
    else
        newScene = TTON_SceneSearch.SceneSexSearch(actors, false, "", actions, sexualPosition, intent, true, isFemdom, hasPlayer, true)
    endif

    if(newScene == "")
        if(furn)
            StorageUtil.FormListRemove(none, "TTON_FurnitureList", furn)
        endif
        TTON_Debug.err("StartOstim: No compatible starting animation found for actors: " + actors + " (intent=" + intent + ", actions=" + actions + "). Aborting scene start.")
        Debug.Notification("OStimNet: No compatible animations found for these participants.")
        TTON_Utils.SetActorsPending(actors, false)
        TTON_Utils.FreeParticipants(actors)
        return -1
    endif

    int builderID = OThreadBuilder.create(actors)
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

int Function StartOStimCaring(Actor initiator, Actor participant, string actions = "", string intent = "", Actor mainActor = none, Actor secondaryActor = none) global
    Actor player = TTON_JData.GetPlayer()
    if(!initiator || !participant)
        TTON_Debug.warn("StartOStimCaring: invalid actors provided.")
        if(initiator)
            Actor[] initArr = new Actor[1]
            initArr[0] = initiator
            TTON_Utils.SetActorsPending(initArr, false)
        endif
        if(participant)
            Actor[] partArr = new Actor[1]
            partArr[0] = participant
            TTON_Utils.SetActorsPending(partArr, false)
        endif
        return -1
    endif

    Actor[] actors = OActorUtil.ToArray(initiator, participant)
    if(initiator.GetParentCell() != participant.GetParentCell())
        TTON_Debug.warn("StartOStimCaring: actors are not in the same location. Aborting.")
        TTON_Debug.warn("StartOStimCaring: " + TTON_Utils.GetActorName(initiator) + " is in " + initiator.GetParentCell() + ", " + TTON_Utils.GetActorName(participant) + " is in " + participant.GetParentCell())
        TTON_Utils.SetActorsPending(actors, false)
        return -1
    endif
    bool someActorsBusy = TTON_Utils.IsActorBusyWithScenes(initiator, false) || TTON_Utils.IsActorBusyWithScenes(participant, false)
    if(someActorsBusy)
        TTON_Debug.warn("StartOstim: tried to start new thread with actors who are busy in another thread.")
        TTON_Utils.SetActorsPending(actors, false)
        return -1
    endif

    actors = OActorUtil.Sort(actors, OActorUtil.EmptyArray())

    TTON_Debug.info("StartOstim: Starting non-sexual scene with actors: " + actors + ", actions: " + actions)
    string actionfallback = ""
    string newScene = TTON_SceneSearch.SearchCareScene(actors, intent, actions, initiator, actionfallback)
    if(newScene == "")
        TTON_Debug.err("StartOStimCaring: No compatible care animation found for actors: " + actors + " (intent=" + intent + ", actions=" + actions + "). Aborting scene start.")
        Debug.Notification("OStimNet: No compatible care animations found for these participants.")
        TTON_Utils.SetActorsPending(actors, false)
        return -1
    endif

    int builderID = OThreadBuilder.create(actors)
    OThreadBuilder.NoFurniture(builderID)
    OThreadBuilder.SetDuration(builderID, SkyrimNetApi.GetConfigInt("Plugin_OStimNet", "tton.settings.nonSexualDuration", 20) as float)
    OThreadBuilder.NoAutoMode(builderID)
    OThreadBuilder.NoPlayerControl(builderID)

    OThreadBuilder.SetStartingAnimation(builderID, newScene)
    Actor effectiveMain = initiator
    Actor effectiveSecondary = participant
    if(mainActor && secondaryActor)
        effectiveMain = mainActor
        effectiveSecondary = secondaryActor
    endif
    Actor[] careMain = OActorUtil.ToArray(effectiveMain)
    Actor[] careSecondary = OActorUtil.ToArray(effectiveSecondary)
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
    if(furn == "none" || furn == "floor")
        furn = ""
    endif
    bool forceFurn = true

    Actor[] actors = OThread.GetActors(ThreadID)
    if(activity == "")
        string currentSceneId = OThread.GetScene(ThreadID)
        int[] allActivities = OMetadata.FindAllActionsCSV(currentSceneId, "intercourse")
        if(allActivities.Length == 0)
            allActivities = OMetadata.FindAllActionsCSV(currentSceneId, "fellatio,cunnilingus,anilingus,testicularlicking")
        endif
        if(allActivities.Length == 0)
            allActivities = OMetadata.FindAllActionsCSV(currentSceneId, "sexual")
        endif
        if(allActivities.Length > 0)
            int selectedActivity = Utility.RandomInt(0, allActivities.Length - 1)
            activity = OMetadata.GetActionType(currentSceneId, allActivities[selectedActivity])
        endif
    endif
    string actionBlacklist = ""
    if(activity != "")
        actionBlacklist = OStimNet.GetActionBlacklistForActivity(activity)
    endif
    if(sexualPosition == "" && actionBlacklist == "")
        string[] tags = OMetadata.GetSceneTags(OThread.GetScene(ThreadID))
        sexualPosition = OStimNet.GetSexualPositionFromTags(tags)
    endif
    if(intent == "")
        intent = OStimNet.GetThreadIntent(ThreadID)
    endif
    bool isFemdom = false
    Actor[] mainActors = OStimNet.GetMainActors(ThreadID)
    if(mainActors.Length > 0 && mainActors[0])
        isFemdom = mainActors[0].GetActorBase().GetSex() == 1
    endif
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
    ; clean up deprecated StorageUtil sotrages
    StorageUtil.ClearFormListPrefix("TTON_DoubleBeds")
    StorageUtil.ClearFormListPrefix("TTON_SingleBeds")
    StorageUtil.ClearFormListPrefix("TTON_Bedrolls")
    StorageUtil.ClearFormListPrefix("TTON_OtherBeds")

    ObjectReference doublebed = OFurniture.FindFurnitureOfType("doublebed", centerRef, radius)
    if(doublebed)
        return doublebed
    endif


    ObjectReference singlebed = OFurniture.FindFurnitureOfType("singlebed", centerRef, radius)
    if(singlebed)
        return singlebed
    endif
    ObjectReference bedroll = OFurniture.FindFurnitureOfType("bedroll", centerRef, radius)
    if(bedroll)
        return bedroll
    endif

    return none
EndFunction

ObjectReference Function FindSuitableFurniture(Actor akActor, string furnitureType, float radius = 1200.0) global
    ObjectReference finalFurniture = none

    if(furnitureType && furnitureType == "floor")
        finalFurniture = none
    elseif(furnitureType && furnitureType != "bed")
        finalFurniture = OFurniture.FindFurnitureOfType(furnitureType, akActor, radius)
    else
        finalFurniture = TTON_OStimIntegration.ScanBestBeds(akActor, radius)
    endif

    if(finalFurniture)
        TTON_Debug.debug("Found furniture for OStim scene: " + finalFurniture)
        Storageutil.FormListAdd(none, "TTON_FurnitureList", finalFurniture)
        StorageUtil.SetFormValue(akActor, "TTON_FurnitureActor", finalFurniture)
        return finalFurniture
    endif

    return none
EndFunction
