Scriptname TTON_MainController extends Quest

GlobalVariable Property TTON_IsOstimActive  Auto

Event OnInit()
    Maintenance()
EndEvent

Function Maintenance()
    TTON_JData.ImportStaticData()
    TTON_Storage.ClearOnLoad()

    RegisterForModEvent("ostim_thread_start", "OStimStart")
    RegisterForModEvent("ostim_thread_scenechanged", "OStimSceneChange")
    RegisterForModEvent("ostim_thread_speedchanged", "OStimSpeedChange")
    RegisterForModEvent("ostim_actor_orgasm", "OStimOrgasm")
    RegisterForModEvent("ostim_thread_end", "OStimEnd")

    RegisterForModEvent("ttll_thread_data_event", "ThreadFinished")

    TTON_Actions.RegisterActions()
    TTON_Events.RegisterEvents()
    TTON_Decorators.RegisterDecorators()
EndFunction

Event OStimStart(string eventName, string strArg, float numArg, Form sender)
    TTON_IsOstimActive.SetValue(1.0)
    int ThreadID = numArg as int
    StorageUtil.IntListAdd(none, "StartedOStimThreads", ThreadID, false)

    TTON_Storage.StartThread(ThreadID)

    int continuedFromThreadID = TTON_JData.GetThreadContinuationFrom(ThreadID)

    ; if this thread was started as part of adding new actors clean JContainers data related to this and old thread
    if(continuedFromThreadID != -1)
        TTON_JData.SetThreadAddNewActors(continuedFromThreadID, 0)
        TTON_JData.SetThreadContinuationFrom(ThreadID, -1)
    else
        Actor[] actors = OThread.GetActors(ThreadID)
        string sceneId = OThread.GetScene(ThreadID)
        string sceneDesc = TTON_Utils.GetSceneDescription(sceneId, actors)
        TTON_Utils.RequestSexComment(TTON_Utils.GetActorsNamesComaSeparated(actors)+" have begun a new intimate encounter while "+sceneDesc, actors)
        ; TTON_Events.RegisterSexStartEvent(ThreadID)
    endif
EndEvent

Event OStimSceneChange(string eventName, string strArg, float numArg, Form sender)
    int ThreadID = numArg as int
    string sceneId = OThread.GetScene(ThreadID)

    TTON_Storage.SceneChange(ThreadID)

    if(OMetadata.IsTransition(sceneId) || OMetadata.HasAnySceneTagCSV(sceneId, "idle,intro"))
      return
    endif

    ; TTON_Events.RegisterSexChangeEvent(ThreadID)
    Actor[] actors = OThread.GetActors(ThreadID)
    string sceneDesc = TTON_Utils.GetSceneDescription(sceneId, actors)
    TTON_Utils.RequestSexComment(TTON_Utils.GetActorsNamesComaSeparated(actors) + " changed their position to: " + sceneDesc, actors, type = "scene_change", continueNarration = true)
EndEvent

Event OStimOrgasm(string eventName, string strArg, float numArg, Form sender)
    int ThreadID = numArg as int
    TTON_Storage.EndThread(ThreadID)
    Actor orgasmedActor = sender as Actor
    TTON_Events.RegisterSexClimaxEvent(ThreadID, orgasmedActor)
    string climaxLocations = TTON_Utils.GetShclongOrgasmedLocation(orgasmedActor, ThreadID)
    Actor[] actors = OThread.GetActors(ThreadID)
    string msg = ""
    if(climaxLocations != "")
        msg += climaxLocations
    else
        msg += TTON_Utils.GetActorName(orgasmedActor) + " reached orgasm during sexual activity with " + TTON_Utils.GetActorsNamesComaSeparated(actors, orgasmedActor)
    endif

    ; string renderParams = "{\"recent_events\":\"{{msg}}\",\"raw\":\"{{msg}}\",\"compact\":\"{{msg}}\",\"verbose\":\"{{msg}}\"}"
    ; SkyrimNetApi.RegisterEvent(type, "{ \"msg\": \""+msg+"\" }", orgasmedActor, none)

    StorageUtil.IntListAdd(none, "RecentlyOrgasmedThreads", ThreadID)
    StorageUtil.FormListAdd(none, "RecentlyOrgasmedActorsFromThrad"+ThreadID, orgasmedActor)
    StorageUtil.SetStringValue(orgasmedActor, "OrgasmedMsg", msg)

    TTON_Utils.RequestSexComment(msg, speaker = orgasmedActor)
EndEvent

Event OStimEnd(string eventName, string strArg, float numArg, Form sender)
    StorageUtil.IntListRemove(none, "StartedOStimThreads", numArg as int)
    if(OThread.GetThreadCount() == 0)
        TTON_IsOstimActive.SetValue(0.0)
    endif
EndEvent

Event ThreadFinished(int ThreadID)
    if(!TTON_JData.GetThreadAddNewActors(ThreadID))
        Actor[] actors = TTLL_ThreadsCollector.GetActors(ThreadID)
        TTON_Utils.RequestSexComment(TTON_Utils.GetActorsNamesComaSeparated(actors) + " just finished their sexual encounter. Based on the recent context, generate a single in-character, post-sex comment that reflects how the encounter likely went.", actors, continueNarration = true)
        int i = 0
        while(i < actors.Length)
            TTON_Storage.UpdateNpcSexualData(actors[i])

            Actor[] lovers = TTLL_Store.GetAllLovers(actors[i])
            int j = 0
            while(j < lovers.Length)
                TTON_Storage.UpdateNpcLoverSexualData(actors[i], lovers[j])
                j += 1
            endwhile
            i += 1
        endwhile
    endif

    TTON_JData.SetThreadAffectionOnly(ThreadId, 0)
EndEvent
