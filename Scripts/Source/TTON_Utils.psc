scriptname TTON_Utils

;==========================================================================
; Actor Utility Functions
;==========================================================================

; Gets the appropriate display name for an actor
; @param akActor The actor to get the name for
; @returns The actor's name (base name for player, display name for NPCs)
string Function GetActorName(actor akActor) global
    if akActor == TTON_JData.GetPlayer()
        return akActor.GetActorBase().GetName()
    else
        return akActor.GetDisplayName()
    EndIf
EndFunction

Faction Function GetSexLabAnimatingFaction() global
    Faction SexLabAnimatingFaction = none
    if(Game.GetModByName("SexLab.esm") != 255)
        SexLabAnimatingFaction = Game.GetFormFromFile(0x0000E50F, "SexLab.esm") as Faction
    endif

    return SexLabAnimatingFaction
EndFunction

bool Function IsInSexLab(Actor npc) global
    if(!npc)
        return false
    endif

    Faction slFaction = GetSexLabAnimatingFaction()
    if(slFaction == none)
        return false
    endif

    return npc.IsInFaction(slFaction)
EndFunction

bool Function IsActorBusyWithScenes(Actor npc, bool includePending = true) global
    bool isActorPening = false
    if(includePending)
        isActorPening = IsActorPending(npc)
    endif
    return OActor.IsInOStim(npc) || IsInSexLab(npc) || isActorPening
EndFunction

GlobalVariable Function GetSexLabOStimPlayerGlobal() global
    if(Game.GetModByName("SkyrimNet_Sexlab.esp") == 255)
        return none
    endif

    return Game.GetFormFromFile(0x001FC803, "SkyrimNet_Sexlab.esp") as GlobalVariable
EndFunction

int Function GetSexLabOStimPlayerMode() global
    GlobalVariable SexLabOStimPlayer = GetSexLabOStimPlayerGlobal()
    if(SexLabOStimPlayer == none)
        return -1
    endif

    int flag = SexLabOStimPlayer.GetValueInt()
    if(flag < 0)
        return -1
    endif

    return flag
EndFunction

bool Function IsSexLabInCharge() global
    return GetSexLabOStimPlayerMode() == 0
EndFunction



Function MakeParticipantsWalkToFurniture(Actor[] participants, ObjectReference furn) global
    int i = 0
    while i < participants.Length
        Actor current = participants[i]
        if(current && furn && current != TTON_JData.GetPlayer())
            TTON_Debug.debug("Making " + current + " walk to furniture " + furn + " keyword: " + TTON_JData.GetParticipantFurnitureKeyword())
            StorageUtil.SetFormValue(current, "TTON_WalkToFurniture", furn)
            PO3_SKSEFunctions.SetLinkedRef(current, furn, TTON_JData.GetParticipantFurnitureKeyword())
            ActorUtil.AddPackageOverride(current, TTON_JData.GetParticipantFollowPackage(), 60)
            current.EvaluatePackage()
            SetActorPending(current, true)
        endif
        i += 1
    endwhile
EndFunction

Function FreeParticipants(Actor[] participants) global
    int i = 0
    while i < participants.Length
        Actor current = participants[i]
        FreeParticipant(current)
        i += 1
    endwhile
EndFunction

Function FreeParticipant(Actor participant) global
    ObjectReference furn = StorageUtil.GetFormValue(participant, "TTON_WalkToFurniture") as ObjectReference
    if(participant && furn && participant != TTON_JData.GetPlayer())
        TTON_Debug.debug("Freeing " + participant + " from furniture " + furn + " keyword: " + TTON_JData.GetParticipantFurnitureKeyword())
        PO3_SKSEFunctions.SetLinkedRef(participant, furn, none)
        StorageUtil.UnsetFormValue(participant, "TTON_WalkToFurniture")
        ActorUtil.RemovePackageOverride(participant, TTON_JData.GetParticipantFollowPackage())
        participant.EvaluatePackage()
    endif
EndFunction

Function SetActorsPending(Actor[] actors, bool pending) global
    int i = 0
    Actor player = TTON_JData.GetPlayer()
    Faction pendingFaction = TTON_JData.GetOStimPendingFaction()
    while i < actors.Length
        Actor current = actors[i]
        if(current && current != player)
            if(pending)
                TTON_Debug.debug("Setting " + current + " as pending")
                current.addToFaction(pendingFaction)
                StorageUtil.FormListAdd(none, "TTON_PendingActors", current, false)
            else
                TTON_Debug.debug("Freeing " + current + " from pending")
                current.removeFromFaction(pendingFaction)
                StorageUtil.FormListRemove(none, "TTON_PendingActors", current)
            endif
        endif
        i += 1
    endwhile
EndFunction

Function SetActorPending(Actor npc, bool pending) global
    Actor player = TTON_JData.GetPlayer()
    Faction pendingFaction = TTON_JData.GetOStimPendingFaction()
    if(npc && npc != player)
        if(pending)
            TTON_Debug.debug("Setting " + npc + " as pending")
            npc.addToFaction(pendingFaction)
            StorageUtil.FormListAdd(none, "TTON_PendingActors", npc, false)
        else
            TTON_Debug.debug("Freeing " + npc + " from pending")
            npc.removeFromFaction(pendingFaction)
            StorageUtil.FormListRemove(none, "TTON_PendingActors", npc)
        endif
    endif
EndFunction

bool Function IsActorPending(Actor npc) global
    if(!npc)
        return false
    endif
    return npc.IsInFaction(TTON_JData.GetOStimPendingFaction())
EndFunction

Function ClearPendingActorsOnLoad() global
    Form[] pendingActors = StorageUtil.FormListToArray(none, "TTON_PendingActors")
    Faction pendingFaction = TTON_JData.GetOStimPendingFaction()
    TTON_Debug.debug("Clearing pending actors on load: " + pendingActors.Length)
    int i = 0
    while i < pendingActors.Length
        Actor current = pendingActors[i] as Actor
        if(current && current.IsInFaction(pendingFaction))
            TTON_Debug.debug("Freeing " + current + " from pending on load")
            current.removeFromFaction(pendingFaction)
        endif
        i += 1
    endwhile
    StorageUtil.FormListClear(none, "TTON_PendingActors")
EndFunction