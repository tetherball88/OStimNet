scriptname TTON_Spectators

Function ScanPotentialSpectatorsPeriodically() global
    StorageUtil.SetIntValue(none, "TTON_SpectatorScanLoopRunning", 1)
    TTON_Debug.Trace("Starting periodic scan for potential spectators:"+CanAddSpectator()+":"+TTON_JData.GetSpectatorsEnabled())
    while(CanAddSpectator() && TTON_JData.GetSpectatorsEnabled() && IsScanLoopRunning())
        ScanPotentialSpectators()
        Utility.Wait(TTON_JData.GetMcmSpectatorScanInterval() as float)
    endwhile
    StorageUtil.SetIntValue(none, "TTON_SpectatorScanLoopRunning", 0)
EndFunction

Function StopScanLoop() global
    StorageUtil.SetIntValue(none, "TTON_SpectatorScanLoopRunning", 0)
    TTON_Debug.Trace("Spectator scan loop stopped manually.")
EndFunction

bool Function IsScanLoopRunning() global
    return StorageUtil.GetIntValue(none, "TTON_SpectatorScanLoopRunning", 0) == 1
EndFunction

Function ScanPotentialSpectators() global
    if(!TTON_JData.GetSpectatorsEnabled())
        return
    endif
    TTON_Debug.Trace("Scanning potential spectators...")
    Actor player = Game.GetPlayer()
    Actor[] potentialSpectators = OActorUtil.GetActorsInRangeV2(Game.GetPlayer(), 800, false, false, true)

    int i = 0
    while(i < potentialSpectators.Length)
        Actor current = potentialSpectators[i]
        if(!OActor.IsInOStim(current) && !current.IsInFaction(TTON_JData.GetSpectatorFaction()))
            AnalyzePotentialSpectator(current)
        endif
        i += 1
    endwhile
EndFunction

Function AnalyzePotentialSpectator(Actor potentialSpectator) global
    Actor spouse = TTRF_RelationsFinder.GetSpouse(potentialSpectator)
    bool triedToAdd = false
    if(spouse && OActor.IsInOStim(spouse))
        if(!spouse.IsInFaction(TTON_JData.GetPlayerMarriedFaction()))
            TryMakeSpectator(potentialSpectator, spouse)
            StorageUtil.SetStringValue(potentialSpectator, "TTON_SpectatorTargetType", "spouse")
            triedToAdd = true
        else
            TTON_Debug.Trace("Target " + spouse + " for potential spectator " + potentialSpectator + " is married to player.")
        endif

    endif

    Actor courting = TTRF_RelationsFinder.GetCourting(potentialSpectator)
    if(courting && OActor.IsInOStim(courting) && !triedToAdd)
        if(!courting.IsInFaction(TTON_JData.GetPlayerMarriedFaction()))
            TryMakeSpectator(potentialSpectator, courting)
            StorageUtil.SetStringValue(potentialSpectator, "TTON_SpectatorTargetType", "courting")
            triedToAdd = true
        else
            TTON_Debug.Trace("Target " + courting + " for potential spectator " + potentialSpectator + " is married to player.")
        endif
    endif
    Actor[] lovers = TTRF_RelationsFinder.GetAllLovers(potentialSpectator)
    int j = 0
    while(j < lovers.Length && !triedToAdd)
        Actor lover = lovers[j]
        if(OActor.IsInOStim(lover))
            if(!lover.IsInFaction(TTON_JData.GetPlayerMarriedFaction()))
                StorageUtil.SetStringValue(potentialSpectator, "TTON_SpectatorTargetType", "lover")
                TryMakeSpectator(potentialSpectator, lover)
                triedToAdd = true
            else
                TTON_Debug.Trace("Target " + lover + " for potential spectator " + potentialSpectator + " is married to player.")
            endif
        endif
        j += 1
    endwhile
EndFunction

Function TryMakeSpectator(Actor spectator, Actor target) global
    if(!TTON_JData.GetSpectatorsEnabled())
        return
    endif
    int ThreadID = OActor.GetSceneID(target)
    if(\
        ThreadID != -1 \
        && !spectator.IsInFaction(TTON_JData.GetSpectatorFaction()) \
        && StorageUtil.GetIntValue(spectator, "TTON_SpectatorFailedToGetClose", 0) == 0 \
        && CanAddSpectatorsToThread(ThreadID) \
    )
        StorageUtil.SetFormValue(spectator, "TTON_SpectatorTarget", target)
        StorageUtil.FormListAdd(none, "TTON_AllSpectatorsForAllThreads", spectator, false)
        StorageUtil.FormListAdd(none, "TTON_AllSpectatorsForThread_" + ThreadID, spectator, false)
        LinkSpectatorToOStimParticipant(spectator, target)
        ActorUtil.AddPackageOverride(spectator, TTON_JData.GetSpectatorFollowPackage(), 60)
        spectator.AddToFaction(TTON_JData.GetSpectatorFaction())
        spectator.EvaluatePackage()

        bool closeDistance = spectator.GetDistance(target) < 310
        int maxTries = 12
        int tries = 0
        while(!closeDistance && tries < maxTries)
            if(!closeDistance && spectator.GetDistance(target) < 310)
                closeDistance = true
            endif
            tries += 1
            Utility.Wait(5)
        endwhile

        if(tries >= maxTries)
            StorageUtil.SetIntValue(spectator, "TTON_SpectatorFailedToGetClose", 1)
            RemoveSpectator(spectator)
        endif

        if(closeDistance)
            TTON_Events.RegisterSpectatorAddedEvent(spectator, ThreadID)
            TTON_Debug.trace("Added spectator: " + spectator + " targeting: " + target)
        endif
    endif
EndFunction

Function LinkSpectatorToOStimParticipant(Actor spectator, Actor target) global
    if(target)
        PO3_SKSEFunctions.SetLinkedRef(spectator, target, TTON_JData.GetSpectatorMarkerKeyword())
    endif
EndFunction

Function MakeSpectatorFlee(Actor spectator) global
    if(!spectator.IsInFaction(TTON_JData.GetSpectatorFleeFaction()))
        TTON_Events.RegisterSpectatorFledEvent(spectator, OActor.GetSceneID(StorageUtil.GetFormValue(spectator, "TTON_SpectatorTarget") as Actor))
        spectator.AddToFaction(TTON_JData.GetSpectatorFleeFaction())
        ActorUtil.RemovePackageOverride(spectator, TTON_JData.GetSpectatorFollowPackage())
        ActorUtil.AddPackageOverride(spectator, GetFleePackageForSpectator(spectator), 70)
        spectator.EvaluatePackage()

        TTON_Debug.trace("Spectator " + spectator + " is now fleeing the scene.")
    endif
EndFunction

Function RemoveSpectator(Actor spectator) global
    Actor target = StorageUtil.GetFormValue(spectator, "TTON_SpectatorTarget") as Actor
    if(target)
        int ThreadID = OActor.GetSceneID(target)
        StorageUtil.FormListRemove(none, "TTON_AllSpectatorsForThread_" + ThreadID, spectator)
    endif
    StorageUtil.FormListRemove(none, "TTON_AllSpectatorsForAllThreads", spectator)
    StorageUtil.ClearAllObjPrefix(spectator, "TTON_Spectator")
    ActorUtil.RemovePackageOverride(spectator, TTON_JData.GetSpectatorFollowPackage())
    ActorUtil.RemovePackageOverride(spectator, TTON_JData.GetSpectatorFleeEditorLocPackage())
    ActorUtil.RemovePackageOverride(spectator, TTON_JData.GetSpectatorFleeFarPackage())
    spectator.EvaluatePackage()
    spectator.RemoveFromFaction(TTON_JData.GetSpectatorFaction())
    spectator.RemoveFromFaction(TTON_JData.GetSpectatorFleeFaction())

    TTON_Debug.trace("Removed spectator: " + spectator)
EndFunction

Function ClearSpectatorsForThread(int ThreadID) global
    Form[] spectators = StorageUtil.FormListToArray(none, "TTON_AllSpectatorsForThread_" + ThreadID)
    if(spectators)
        int i = 0
        while(i < spectators.Length)
            RemoveSpectator(spectators[i] as Actor)
            i += 1
        endwhile
    endif
    StorageUtil.ClearAllPrefix("TTON_AllSpectatorsForThread_" + ThreadID)

    TTON_Debug.trace("Cleared spectators for thread: " + ThreadID)
EndFunction

Function ClearAllSpectators() global
    Form[] spectators = StorageUtil.FormListToArray(none, "TTON_AllSpectatorsForAllThreads")
    if(spectators)
        int i = 0
        while(i < spectators.Length)
            RemoveSpectator(spectators[i] as Actor)
            i += 1
        endwhile
    endif
    CleanAllSpectatorsStorage()

    TTON_Debug.trace("Cleared all spectators.")
EndFunction

Function CleanAllSpectatorsStorage() global
    StorageUtil.ClearAllPrefix("TTON_AllSpectators")
    StorageUtil.ClearAllPrefix("TTON_Spectator")
    ActorUtil.RemoveAllPackageOverride(TTON_JData.GetSpectatorFollowPackage())
    ActorUtil.RemoveAllPackageOverride(TTON_JData.GetSpectatorFleeEditorLocPackage())
    ActorUtil.RemoveAllPackageOverride(TTON_JData.GetSpectatorFleeFarPackage())
    TTON_Debug.trace("Cleared all spectators storage.")
EndFunction

Package Function GetFleePackageForSpectator(Actor spectator) global
    if(!spectator.GetEditorLocation() || spectator.GetCurrentLocation() == spectator.GetEditorLocation())
        ; actor can't flee to editor location, make them flee to Ivarstead map marker(random reference)
        TTON_JData.GetSpectatorFleeFarPackage()
    endif

    return TTON_JData.GetSpectatorFleeEditorLocPackage()
EndFunction

bool Function CanAddSpectator() global
    return TTON_JData.GetSpectatorsEnabled() && TTON_Storage.GetActiveThreads().Length > 0 && StorageUtil.GetIntValue(none, "TTON_AllSpectatorsForAllThreads", 0) < TTON_JData.GetMcmMaxSpectatorsOverall()
EndFunction


bool Function CanAddSpectatorsToThread(int ThreadID) global
    return StorageUtil.GetIntValue(none, "TTON_AllSpectatorsForThread_" + ThreadID, 0) < TTON_JData.GetMcmMaxSpectatorsPerThread()
EndFunction
