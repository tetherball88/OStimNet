scriptname TTON_Undress

; ;head
; int headHelmet = 0x00000001
; int hair = 0x00000002
; int circlet = 0x00001000
; int faceMouth = 0x00004000
; int faceGlassesJewelry = 0x02000000

; ; top
; int bodyCuiras = 0x00000004
; int chestPrimary = 0x00010000
; int chestSecondary = 0x04000000
; int shoulder = 0x08000000
; int backpack = 0x00020000


; ; bottom
; int pelvisPrimarySlot = 0x00080000
; int pelvisSecondarySlot = 0x00400000
; int legArmor = 0x00800000
; int legSecondary = 0x01000000

Function SetupData() global
    StorageUtil.ClearAllPrefix("TTONUndressing_")
    SetupSlotsArrays()
    SetupTimings()
EndFunction

Function GoToScene(int ThreadID, string SceneID, float await = 0.0, bool isWarp = true) global
    if(isWarp)
        OThread.WarpTo(ThreadID, SceneID)
    else
        OThread.NavigateTo(ThreadID, SceneID)
    endif
    if(await != 0)
        Utility.Wait(await)
    endif
EndFunction

Function SmartUndress(int ThreadID, Actor[] actors) global
    if(actors.length != 2)
        TTON_Debug.debug("SmartUndress: Invalid number of actors. Expected 2, got " + actors.length)
        return
    endif

    Actor main = actors[0]
    Actor sub = actors[1]

    bool isAutomode = OThread.IsInAutoMode(ThreadID)
    OThread.StopAutoMode(ThreadID)

    SmartUndressingSub(ThreadID, sub)

    Utility.Wait(0.5) ; Wait a bit before resuming auto mode

    SmartUndressingMain(ThreadID, main)

    string threadIntent = OStimNet.GetThreadIntent(ThreadID)
    string nextScene = TTON_SceneSearch.SceneSexSearch(OActorUtil.Sort(actors, OActorUtil.EmptyArray()), false, "", "", "", threadIntent, false, false, ThreadID == 0, true)
    GoToScene(ThreadID, nextScene, 0.0, true)

    if(isAutomode)
        OThread.StartAutoMode(ThreadID)
    endif
EndFunction

Function SmartUndressingMain(int ThreadID, Actor target) global
    string approach = GetApproach(ThreadID)
    if(approach == "OStim" || approach == "OSAFirstFast")
        GoToScene(ThreadID, "OStim2PUndressBottom0MF", 7.17, true)
    elseif(approach == "OSA")
        SmartUndressing(ThreadID, target, approach, true)
    elseif(approach == "OARE")
        GoToScene(ThreadID, "OARE_FemaleSquattingUndressMaleBottom", 7.17, true)
    endif
EndFunction

Function SmartUndressingSub(int ThreadID, Actor target) global
    string approach = GetApproach(ThreadID)
    SmartUndressing(ThreadID, target, approach, false)
EndFunction

Function SmartUndressing(int ThreadID, Actor target, string approach, bool isMain = false) global
    string[] undressStages = StorageUtil.StringListToArray(none, "TTONUndressing_" + approach + "Stages")
    if(isMain && approach == "OSA")
        undressStages = StorageUtil.StringListToArray(none, "TTONUndressing_" + approach + "StagesMain")
    endif
    Armor[] armors = OUndress.GetWornItems(target)

    int i = 0
    while(i < undressStages.length)
        if(StorageUtil.IntListHas(none, "TTONUndressing_InterruptedThreads", ThreadID))
            StorageUtil.IntListRemove(none, "TTONUndressing_ActiveThreads", ThreadID)
            StorageUtil.IntListRemove(none, "TTONUndressing_InterruptedThreads", ThreadID)
            TTON_Debug.debug("SmartUndressing: Thread " + ThreadID + " has been interrupted. Stopping undressing.")
            return
        endif
        string stage = undressStages[i]
        string undressScene = StorageUtil.StringListGet(none, "TTONUndressing_" + approach + "Scenes", i)
        float duration = StorageUtil.FloatListGet(none, "TTONUndressing_" + approach + "Durations", i)
        if(isMain && approach == "OSA")
            undressScene = StorageUtil.StringListGet(none, "TTONUndressing_" + approach + "ScenesMain", i)
            duration = StorageUtil.FloatListGet(none, "TTONUndressing_" + approach + "DurationsMain", i)
        endif
        if(CheckCanUndress(armors, approach, stage, isMain))
            TTON_Debug.debug("Undressing: Going to undress " + stage + " scene")
            GoToScene(ThreadID, undressScene, duration, true)
        endif
        i += 1
    endwhile
EndFunction

bool Function CheckCanUndress(Armor[] items,string approach, string stage, bool isMain) global
    int i = 0
    while(i < items.length)
        TTON_Debug.debug("CheckCanUndress: Checking item " + items[i].GetName() + " for approach " + approach + " and stage " + stage)
        Armor item = items[i]
        bool canUndress = OUndress.CanUndress(item)
        TTON_Debug.debug("CheckCanUndress: Can undress: " + canUndress)
        if(canUndress)
            int itemSlotMask = item.GetSlotMask()
            bool slotMatch = StorageUtil.IntListHas(none, "TTONUndressing_" + approach + stage + "Slots", itemSlotMask)
            if(isMain && approach == "OSA")
                slotMatch = StorageUtil.IntListHas(none, "TTONUndressing_" + approach + stage + "SlotsMain", itemSlotMask)
            endif
            TTON_Debug.debug("CheckCanUndress: Slot match for item " + item.GetName() + ": " + slotMatch + " (item slot mask: " + itemSlotMask + "); all slots for approach " + approach + " and stage " + stage + ": " + StorageUtil.IntListToArray(none, "TTONUndressing_" + approach + stage + "Slots"))
            if(slotMatch)
                return true
            endif
        endif
        i += 1
    endwhile
    return false
EndFunction

bool Function CanUndress(Armor item, int slotMask) global
    return item.GetSlotMask() == slotMask && OUndress.CanUndress(item)
EndFunction

bool Function IsOSAAttireLoaded() global
    bool res = OStimNavigator.IsSceneLoaded("attire_Sy6!Sy9_ApU_St9Dally+01boots")
    TTON_Debug.debug("IsOSAAttireLoaded: " + res)
    return res
EndFunction

bool Function IsOARELoaded() global
    return OStimNavigator.IsSceneLoaded("OARE_SittingMaleApproachUndressFemale_Pt2")
EndFunction

string Function GetApproach(int ThreadID) global
    string approach
    if(ThreadID == 0)
        approach = SkyrimNetApi.GetConfigString("Plugin_OStimNet", "tton.undress.undressingApproachPlayer", "OStim")
    else
        approach = SkyrimNetApi.GetConfigString("Plugin_OStimNet", "tton.undress.undressingApproachNpc", "OStim")
    endif
    TTON_Debug.debug("GetApproach: Configured approach is " + approach)
    if(IsOARELoaded() && approach == "OARE")
        return "OARE"
    endif

    if(IsOSAAttireLoaded() && (approach == "OSA" || approach == "OSAFirstFast"))
        return approach
    endif

    return "OStim"
EndFunction

Function SetupSlotsArrays() global
    ; OStim head slots
    StorageUtil.IntListAdd(none, "TTONUndressing_OStimHeadSlots", 0x00000001)
    StorageUtil.IntListAdd(none, "TTONUndressing_OStimHeadSlots", 0x00000002)
    StorageUtil.IntListAdd(none, "TTONUndressing_OStimHeadSlots", 0x00001000)
    StorageUtil.IntListAdd(none, "TTONUndressing_OStimHeadSlots", 0x00004000)
    StorageUtil.IntListAdd(none, "TTONUndressing_OStimHeadSlots", 0x02000000)

    ; OStim top slots
    StorageUtil.IntListAdd(none, "TTONUndressing_OStimTopSlots", 0x00000004)
    StorageUtil.IntListAdd(none, "TTONUndressing_OStimTopSlots", 0x00010000)
    StorageUtil.IntListAdd(none, "TTONUndressing_OStimTopSlots", 0x04000000)

    ; OStim bottom slots
    StorageUtil.IntListAdd(none, "TTONUndressing_OStimBottomSlots", 0x00080000)
    StorageUtil.IntListAdd(none, "TTONUndressing_OStimBottomSlots", 0x00400000)
    StorageUtil.IntListAdd(none, "TTONUndressing_OStimBottomSlots", 0x00800000)

    ; OStim hands slots
    StorageUtil.IntListAdd(none, "TTONUndressing_OStimHandsSlots", 0x00000008)
    ; OStim feet slots
    StorageUtil.IntListAdd(none, "TTONUndressing_OStimFeetSlots", 0x00000080)

    ; OSA head slots
    StorageUtil.IntListAdd(none, "TTONUndressing_OSAHeadSlots", 0x00000001)
    StorageUtil.IntListAdd(none, "TTONUndressing_OSAHeadSlots", 0x00000002)
    StorageUtil.IntListAdd(none, "TTONUndressing_OSAHeadSlots", 0x00001000)
    StorageUtil.IntListAdd(none, "TTONUndressing_OSAHeadSlots", 0x00004000)
    StorageUtil.IntListAdd(none, "TTONUndressing_OSAHeadSlots", 0x02000000)

    ; OSA top slots
    StorageUtil.IntListAdd(none, "TTONUndressing_OSATopSlots", 0x00000004)
    StorageUtil.IntListAdd(none, "TTONUndressing_OSATopSlots", 0x00010000)
    StorageUtil.IntListAdd(none, "TTONUndressing_OSATopSlots", 0x04000000)
    StorageUtil.IntListAdd(none, "TTONUndressing_OSATopSlots", 0x08000000)
    StorageUtil.IntListAdd(none, "TTONUndressing_OSATopSlots", 0x00020000)

    ; OSA pants slots
    StorageUtil.IntListAdd(none, "TTONUndressing_OSABottomSlots", 0x00800000)
    StorageUtil.IntListAdd(none, "TTONUndressing_OSABottomSlots", 0x01000000)

    ; OSA pelvis slots
    StorageUtil.IntListAdd(none, "TTONUndressing_OSAPelvisSlots", 0x00080000)
    StorageUtil.IntListAdd(none, "TTONUndressing_OSAPelvisSlots", 0x00400000)

    ; OSA hands slots
    StorageUtil.IntListAdd(none, "TTONUndressing_OSAHandsSlots", 0x00000008)
    ; OSA feet slots
    StorageUtil.IntListAdd(none, "TTONUndressing_OSAFeetSlots", 0x00000080)

    ; OSA main head slots
    StorageUtil.IntListAdd(none, "TTONUndressing_OSAHeadSlotsMain", 0x00000001)
    StorageUtil.IntListAdd(none, "TTONUndressing_OSAHeadSlotsMain", 0x00000002)
    StorageUtil.IntListAdd(none, "TTONUndressing_OSAHeadSlotsMain", 0x00001000)
    StorageUtil.IntListAdd(none, "TTONUndressing_OSAHeadSlotsMain", 0x00004000)
    StorageUtil.IntListAdd(none, "TTONUndressing_OSAHeadSlotsMain", 0x02000000)

    ; OSA main top slots
    StorageUtil.IntListAdd(none, "TTONUndressing_OSATopSlotsMain", 0x00000004)
    StorageUtil.IntListAdd(none, "TTONUndressing_OSATopSlotsMain", 0x00010000)
    StorageUtil.IntListAdd(none, "TTONUndressing_OSATopSlotsMain", 0x04000000)
    StorageUtil.IntListAdd(none, "TTONUndressing_OSATopSlotsMain", 0x08000000)
    StorageUtil.IntListAdd(none, "TTONUndressing_OSATopSlotsMain", 0x00020000)
    StorageUtil.IntListAdd(none, "TTONUndressing_OSATopSlotsMain", 0x00000008)

    ; OSA main pants slots
    StorageUtil.IntListAdd(none, "TTONUndressing_OSABottomSlotsMain", 0x00800000)
    StorageUtil.IntListAdd(none, "TTONUndressing_OSABottomSlotsMain", 0x01000000)
    StorageUtil.IntListAdd(none, "TTONUndressing_OSABottomSlotsMain", 0x00080000)
    StorageUtil.IntListAdd(none, "TTONUndressing_OSABottomSlotsMain", 0x00400000)
    StorageUtil.IntListAdd(none, "TTONUndressing_OSABottomSlotsMain", 0x00000080)

    ; OARE head slots
    StorageUtil.IntListAdd(none, "TTONUndressing_OAREHeadSlots", 0x00000001)
    StorageUtil.IntListAdd(none, "TTONUndressing_OAREHeadSlots", 0x00000002)
    StorageUtil.IntListAdd(none, "TTONUndressing_OAREHeadSlots", 0x00001000)
    StorageUtil.IntListAdd(none, "TTONUndressing_OAREHeadSlots", 0x00004000)
    StorageUtil.IntListAdd(none, "TTONUndressing_OAREHeadSlots", 0x02000000)

    ; OARE top slots
    StorageUtil.IntListAdd(none, "TTONUndressing_OARETopSlots", 0x00000004)
    StorageUtil.IntListAdd(none, "TTONUndressing_OARETopSlots", 0x00010000)
    StorageUtil.IntListAdd(none, "TTONUndressing_OARETopSlots", 0x04000000)
    StorageUtil.IntListAdd(none, "TTONUndressing_OARETopSlots", 0x08000000)
    StorageUtil.IntListAdd(none, "TTONUndressing_OARETopSlots", 0x00020000)

    ; OARE bottom slots
    StorageUtil.IntListAdd(none, "TTONUndressing_OAREBottomSlots", 0x00080000)
    StorageUtil.IntListAdd(none, "TTONUndressing_OAREBottomSlots", 0x00400000)
    StorageUtil.IntListAdd(none, "TTONUndressing_OAREBottomSlots", 0x00800000)
    StorageUtil.IntListAdd(none, "TTONUndressing_OAREBottomSlots", 0x01000000)

    ; OARE hands slots
    StorageUtil.IntListAdd(none, "TTONUndressing_OAREHandsSlots", 0x00000008)
    ; OARE feet slots
    StorageUtil.IntListAdd(none, "TTONUndressing_OAREFeetSlots", 0x00000080)
EndFunction

Function SetupTimings() global
    ; OStim undress timings
    ; OStim hands
    StorageUtil.StringListAdd(none, "TTONUndressing_OStimStages", "Hands")
    StorageUtil.StringListAdd(none, "TTONUndressing_OStimScenes", "OStim2PUndressGloves1MF")
    StorageUtil.FloatListAdd(none, "TTONUndressing_OStimDurations", 2.9)
    ; OStim head
    StorageUtil.StringListAdd(none, "TTONUndressing_OStimStages", "Head")
    StorageUtil.StringListAdd(none, "TTONUndressing_OStimScenes", "OStim2PUndressHead1MF")
    StorageUtil.FloatListAdd(none, "TTONUndressing_OStimDurations", 2.0)
    ; OStim top
    StorageUtil.StringListAdd(none, "TTONUndressing_OStimStages", "Top")
    StorageUtil.StringListAdd(none, "TTONUndressing_OStimScenes", "OStim2PUndressTop1MF")
    StorageUtil.FloatListAdd(none, "TTONUndressing_OStimDurations", 3.433333)
    ; OStim feet
    StorageUtil.StringListAdd(none, "TTONUndressing_OStimStages", "Feet")
    StorageUtil.StringListAdd(none, "TTONUndressing_OStimScenes", "OStim2PUndressBoots1MF")
    StorageUtil.FloatListAdd(none, "TTONUndressing_OStimDurations", 6.0)
    ; OStim bottom
    StorageUtil.StringListAdd(none, "TTONUndressing_OStimStages", "Bottom")
    StorageUtil.StringListAdd(none, "TTONUndressing_OStimScenes", "OStim2PUndressBottom1MF")
    StorageUtil.FloatListAdd(none, "TTONUndressing_OStimDurations", 5.0)

    ; OSA undress timings
    ; OSA hands
    StorageUtil.StringListAdd(none, "TTONUndressing_OSAStages", "Hands")
    StorageUtil.StringListAdd(none, "TTONUndressing_OSAScenes", "attire_Sy6!Sy9_ApU_St9Dally+01gloves")
    StorageUtil.FloatListAdd(none, "TTONUndressing_OSADurations", 14.0)
    ; OSA head
    StorageUtil.StringListAdd(none, "TTONUndressing_OSAStages", "Head")
    StorageUtil.StringListAdd(none, "TTONUndressing_OSAScenes", "attire_Sy6!Sy9_ApU_St9Dally+01helmet")
    StorageUtil.FloatListAdd(none, "TTONUndressing_OSADurations", 10.0)
    StorageUtil.StringListAdd(none, "TTONUndressing_OSAStagesMain", "Head")
    StorageUtil.StringListAdd(none, "TTONUndressing_OSAScenesMain", "attire_Sy6!Sy9_ApU_St9Dally+10helmet")
    StorageUtil.FloatListAdd(none, "TTONUndressing_OSADurationsMain", 4.0)
    ; OSA top
    StorageUtil.StringListAdd(none, "TTONUndressing_OSAStages", "Top")
    StorageUtil.StringListAdd(none, "TTONUndressing_OSAScenes", "attire_Sy6!Sy9_ApU_St9Dally+01cuirass")
    StorageUtil.FloatListAdd(none, "TTONUndressing_OSADurations", 22.0)
    StorageUtil.StringListAdd(none, "TTONUndressing_OSAStagesMain", "Top")
    StorageUtil.StringListAdd(none, "TTONUndressing_OSAScenesMain", "attire_Sy6!Sy9_ApU_St9Dally+10cuirass")
    StorageUtil.FloatListAdd(none, "TTONUndressing_OSADurationsMain", 14.0)
    ; OSA feet
    StorageUtil.StringListAdd(none, "TTONUndressing_OSAStages", "Feet")
    StorageUtil.StringListAdd(none, "TTONUndressing_OSAScenes", "attire_Sy6!Sy9_ApU_St9Dally+01boots")
    StorageUtil.FloatListAdd(none, "TTONUndressing_OSADurations", 10.0)
    ; OSA pelvis
    StorageUtil.StringListAdd(none, "TTONUndressing_OSAStages", "Pelvis")
    StorageUtil.StringListAdd(none, "TTONUndressing_OSAScenes", "attire_Sy6!Sy9_ApU_St9Dally+01miscmid")
    StorageUtil.FloatListAdd(none, "TTONUndressing_OSADurations", 8.0)
    ; OSA bottom
    StorageUtil.StringListAdd(none, "TTONUndressing_OSAStages", "Bottom")
    StorageUtil.StringListAdd(none, "TTONUndressing_OSAScenes", "attire_Sy6!Sy9_ApU_St9Dally+01pants")
    StorageUtil.FloatListAdd(none, "TTONUndressing_OSADurations", 16.0)
    StorageUtil.StringListAdd(none, "TTONUndressing_OSAStagesMain", "Bottom")
    StorageUtil.StringListAdd(none, "TTONUndressing_OSAScenesMain", "attire_Sy6!Sy9_ApU_St9Dally+10pants")
    StorageUtil.FloatListAdd(none, "TTONUndressing_OSADurationsMain", 12.0)

    ; OARE undress timings
    ; OARE hands
    StorageUtil.StringListAdd(none, "TTONUndressing_OAREStages", "Hands")
    StorageUtil.StringListAdd(none, "TTONUndressing_OAREScenes", "OARE_StandingUndressingFemaleGloves")
    StorageUtil.FloatListAdd(none, "TTONUndressing_OAREDurations", 4.0)
    ; OARE head
    StorageUtil.StringListAdd(none, "TTONUndressing_OAREStages", "Head")
    StorageUtil.StringListAdd(none, "TTONUndressing_OAREScenes", "OARE_StandingUndressingFemaleHeadItem")
    StorageUtil.FloatListAdd(none, "TTONUndressing_OAREDurations", 3.0)
    ; OARE top
    StorageUtil.StringListAdd(none, "TTONUndressing_OAREStages", "Top")
    StorageUtil.StringListAdd(none, "TTONUndressing_OAREScenes", "OARE_SHMaleUndressFemaleTorso1")
    StorageUtil.FloatListAdd(none, "TTONUndressing_OAREDurations", 5.0)
    ; OARE feet
    StorageUtil.StringListAdd(none, "TTONUndressing_OAREStages", "Feet")
    StorageUtil.StringListAdd(none, "TTONUndressing_OAREScenes", "OARE_StandingUndressingFemaleShoes")
    StorageUtil.FloatListAdd(none, "TTONUndressing_OAREDurations", 5.0)
    ; OARE bottom
    StorageUtil.StringListAdd(none, "TTONUndressing_OAREStages", "Bottom")
    StorageUtil.StringListAdd(none, "TTONUndressing_OAREScenes", "OARE_StandingUndressingFemaleBottom")
    StorageUtil.FloatListAdd(none, "TTONUndressing_OAREDurations", 6.0)

EndFunction
