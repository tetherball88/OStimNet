scriptname TTON_Storage

Function StartThread(int ThreadID) global
    StorageUtil.IntListAdd(none, "TTONDec_ActiveOStimThreads", ThreadID, false)
    Actor[] actors = OThread.GetActors(ThreadID)
    int i = 0
    while(i < actors.Length)
        Actor current = actors[i]
        StorageUtil.FormListAdd(none, "TTONDec_Thread" + ThreadID + "_Actors", current, false)
        StorageUtil.StringListAdd(none, "TTONDec_Thread" + ThreadID + "_ActorsNames", TTON_Utils.GetActorName(current), false)
        StorageUtil.SetIntValue(current, "TTONDec_ThreadParticipant", ThreadID)

        i += 1
    endwhile
EndFunction

Function SceneChange(int ThreadID) global
    string sceneId = OThread.GetScene(ThreadID)
    bool prevIsSexual = StorageUtil.GetIntValue(none, "TTONDec_Thread" + ThreadID + "_IsSexual", 0) != 0
    if(!prevIsSexual)
        int isSexual = OMetadata.FindActionsSuperloadCSVv2(sceneId, AnyActionTag = "sexual").Length
        if(isSexual > 0)
            StorageUtil.SetIntValue(none, "TTONDec_Thread" + ThreadID + "_IsSexual", 1)
        endif
    endif

    string description = TTON_Utils.GetSceneDescription(sceneId, OThread.GetActors(ThreadID))
    StorageUtil.SetStringValue(none, "TTONDec_Thread" + ThreadID + "_SceneDescription", description)
EndFunction

Function EndThread(int ThreadID) global
    StorageUtil.IntListRemove(none, "TTONDec_ActiveOStimThreads", ThreadID)
    StorageUtil.ClearAllPrefix("TTONDec_Thread" + ThreadID)
EndFunction

Function ClearOnLoad() global
    StorageUtil.ClearAllPrefix("TTONDec_")
EndFunction

Function UpdateNpcSexualData(Actor npc) global
    int solo = TTLL_Store.GetNpcInt(npc, "solosex")
    StorageUtil.SetIntValue(npc, "TTONDec_SexualData_SoloSex", solo)

    int exclusive = TTLL_Store.GetNpcInt(npc, "exclusivesex")
    StorageUtil.SetIntValue(npc, "TTONDec_SexualData_ExclusiveSex", exclusive)

    int groupCount = TTLL_Store.GetNpcInt(npc, "groupsex")
    StorageUtil.SetIntValue(npc, "TTONDec_SexualData_GroupSex", groupCount)

    int sameSexEncounter = TTLL_Store.GetNpcInt(npc, "samesexencounter")
    StorageUtil.SetIntValue(npc, "TTONDec_SexualData_SameSexEncounter", sameSexEncounter)
EndFunction

Function UpdateNpcLoverSexualData(Actor npc, Actor lover) global
    StorageUtil.FormListAdd(npc, "TTONDec_SexualData_Lovers", lover, false)
    string recency = "never"
    int daysAgo = (Utility.GetCurrentGameTime() - TTLL_Store.GetLoverFlt(npc, lover, "lasttime")) as int
    if(daysAgo <= 1)
        recency = "today"
    elseif(daysAgo <= 7)
        recency = daysAgo + " days ago"
    elseif(daysAgo < 30.0)
        recency = ((daysAgo / 7) as int) + " weeks ago"
    elseif(daysAgo <= 365)
        recency = ((daysAgo / 30) as int) + " months ago"
    else
        recency = "over a year ago"
    endif
    StorageUtil.SetStringValue(npc, "TTONDec_Lover_" + lover.GetFormID() + "_recency", recency)

    float score = TTLL_Store.GetLoverScore(npc, lover)
    string bondLevel = "weak"
    if(score > 40)
        bondLevel = "deep bond"
    elseif(score > 20)
        bondLevel = "strong connection"
    elseif(score > 10)
        bondLevel = "established relationship"
    elseif(score > 5)
        bondLevel = "casual connection"
    endif
    StorageUtil.SetFloatValue(npc, "TTONDec_Lover_" + lover.GetFormID() + "_Bond", TTLL_Store.GetLoverScore(npc, lover))

    int exclusive = TTLL_Store.GetLoverInt(npc, lover, "exclusivesex")
    int group = TTLL_Store.GetLoverInt(npc, lover, "groupsex")
    StorageUtil.SetIntValue(npc, "TTONDec_Lover_" + lover.GetFormID() + "_Exclusive", exclusive)
    StorageUtil.SetIntValue(npc, "TTONDec_Lover_" + lover.GetFormID() + "_Group", group)
EndFunction

Form[] Function GetActors(int ThreadID) global
    return StorageUtil.FormListToArray(none, "TTONDec_Thread" + ThreadID + "_Actors")
EndFunction
string Function GetActorsNames(int ThreadID) global
    return PapyrusUtil.StringJoin(StorageUtil.StringListToArray(none, "TTONDec_Thread" + ThreadID + "_ActorsNames"), ", ")
EndFunction
