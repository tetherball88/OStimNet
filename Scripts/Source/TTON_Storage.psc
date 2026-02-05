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
    SetThreadAffectionOnly(ThreadID, 0)
    StorageUtil.ClearAllPrefix("TTONDec_Thread" + ThreadID)
EndFunction

int[] Function GetActiveThreads() global
    return StorageUtil.IntListToArray(none, "TTONDec_ActiveOStimThreads")
EndFunction

Function ClearOnLoad() global
    StorageUtil.ClearAllPrefix("TTONDec_")

    ; clear possible values from deny actions functionality, which was moved to spell based approach
    StorageUtil.ClearAllPrefix("deny.StartAffectionScene")
    StorageUtil.ClearAllPrefix("deny.StartNewSex")
    StorageUtil.ClearAllPrefix("deny.ChangeSexActivity")
    StorageUtil.ClearAllPrefix("deny.InviteToYourSex")
    StorageUtil.ClearAllPrefix("deny.JoinOngoingSex")
    StorageUtil.ClearAllPrefix("deny.ChangeSexPace")
    StorageUtil.ClearAllPrefix("deny.StopSex")
EndFunction

string Function GetActorsNames(int ThreadID) global
    return PapyrusUtil.StringJoin(StorageUtil.StringListToArray(none, "TTONDec_Thread" + ThreadID + "_ActorsNames"), ", ")
EndFunction

Function SetThreadAffectionOnly(int ThreadID, int value) global
    StorageUtil.SetIntValue(none, "TTONDec_Thread" + ThreadID + "_AffectionOnly", value)
EndFunction

bool Function GetThreadAffectionOnly(int ThreadID) global
    return StorageUtil.GetIntValue(none, "TTONDec_Thread" + ThreadID + "_AffectionOnly", 0) != 0
EndFunction

Function SetThreadContinuationFrom(int OldThreadID, int NewThreadID) global
    StorageUtil.SetIntValue(none, "TTONDec_Thread" + NewThreadID + "_ContinuationFrom", OldThreadID)
EndFunction

int Function GetThreadContinuationFrom(int ThreadID) global
    return StorageUtil.GetIntValue(none, "TTONDec_Thread" + ThreadID + "_ContinuationFrom", -1)
EndFunction

Function SetThreadAddNewActors(int ThreadID, int value) global
    StorageUtil.SetIntValue(none, "TTONDec_Thread" + ThreadID + "_AddNewActors", value)
EndFunction

bool Function GetThreadAddNewActors(int ThreadID) global
    return StorageUtil.GetIntValue(none, "TTONDec_Thread" + ThreadID + "_AddNewActors", 0) != 0
EndFunction
