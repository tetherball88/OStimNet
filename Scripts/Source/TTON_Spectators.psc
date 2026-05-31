scriptname TTON_Spectators

Function TryMakeSpectator(Actor spectator, Actor target) global
    int ThreadID = OActor.GetSceneID(target)
    TTON_Debug.debug("TryMakeSpectator: " + TTON_Utils.GetActorName(spectator) + "->" + TTON_Utils.GetActorName(target) + ": " + ThreadID)
    if(\
        ThreadID != -1 \
        && !spectator.IsInFaction(TTON_JData.GetSpectatorFaction()) \
    )
        LinkSpectatorToOStimParticipant(spectator, target)
        ActorUtil.AddPackageOverride(spectator, TTON_JData.GetSpectatorFollowPackage(), 60)
        spectator.AddToFaction(TTON_JData.GetSpectatorFaction())
        spectator.EvaluatePackage()
        SkyrimNetApi.RegisterEvent("tton_event", OStimNet.BuildSpectatorAddedEventJson(spectator), spectator, target)
    endif
EndFunction

Function LinkSpectatorToOStimParticipant(Actor spectator, Actor target) global
    if(target)
        PO3_SKSEFunctions.SetLinkedRef(spectator, target, TTON_JData.GetSpectatorMarkerKeyword())
    endif
EndFunction

Function MakeSpectatorFlee(Actor spectator) global
    TTON_Debug.debug("MakeSpectatorFlee: " + TTON_Utils.GetActorName(spectator))
    if(!spectator.IsInFaction(TTON_JData.GetSpectatorFleeFaction()))
        SkyrimNetApi.RegisterEvent("tton_event", OStimNet.BuildSpectatorFledEventJson(spectator), spectator, none)
        spectator.AddToFaction(TTON_JData.GetSpectatorFleeFaction())
        ActorUtil.RemovePackageOverride(spectator, TTON_JData.GetSpectatorFollowPackage())
        ActorUtil.AddPackageOverride(spectator, GetFleePackageForSpectator(spectator), 70)
        spectator.EvaluatePackage()
    endif
EndFunction

Function RemoveSpectator(Actor spectator) global
    TTON_Debug.debug("RemoveSpectator: " + TTON_Utils.GetActorName(spectator))
    ActorUtil.RemovePackageOverride(spectator, TTON_JData.GetSpectatorFollowPackage())
    ActorUtil.RemovePackageOverride(spectator, TTON_JData.GetSpectatorFleeEditorLocPackage())
    ActorUtil.RemovePackageOverride(spectator, TTON_JData.GetSpectatorFleeFarPackage())
    spectator.EvaluatePackage()
    spectator.RemoveFromFaction(TTON_JData.GetSpectatorFaction())
    spectator.RemoveFromFaction(TTON_JData.GetSpectatorFleeFaction())
    OStimNet.RemoveSpectator(spectator)
EndFunction

Function CleanAllSpectatorsStorage() global
    StorageUtil.ClearAllPrefix("TTON_AllSpectators")
    StorageUtil.ClearAllPrefix("TTON_Spectator")
    ActorUtil.RemoveAllPackageOverride(TTON_JData.GetSpectatorFollowPackage())
    ActorUtil.RemoveAllPackageOverride(TTON_JData.GetSpectatorFleeEditorLocPackage())
    ActorUtil.RemoveAllPackageOverride(TTON_JData.GetSpectatorFleeFarPackage())
EndFunction

Package Function GetFleePackageForSpectator(Actor spectator) global
    if(!spectator.GetEditorLocation() || spectator.GetCurrentLocation() == spectator.GetEditorLocation())
        ; actor can't flee to editor location, make them flee to Ivarstead map marker(random reference)
        TTON_JData.GetSpectatorFleeFarPackage()
    endif

    return TTON_JData.GetSpectatorFleeEditorLocPackage()
EndFunction
