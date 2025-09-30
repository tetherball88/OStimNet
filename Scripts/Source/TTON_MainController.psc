Scriptname TTON_MainController extends Quest

GlobalVariable Property TTON_IsOstimActive  Auto

Function Maintenance()
    TTON_JData.ImportStaticData()

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

    int continuedFromThreadID = TTON_JData.GetThreadContinuationFrom(ThreadID)

    ; if this thread was started as part of adding new actors clean JContainers data related to this and old thread
    if(continuedFromThreadID != -1)
        TTON_JData.SetThreadAddNewActors(continuedFromThreadID, 0)
        TTON_JData.SetThreadContinuationFrom(ThreadID, -1)
    else
        TTON_Events.RegisterSexStartEvent(ThreadID)
    endif
EndEvent

Event OStimSceneChange(string eventName, string strArg, float numArg, Form sender)
    int ThreadID = numArg as int
    string sceneId = OThread.GetScene(ThreadID)

    if(OMetadata.IsTransition(sceneId) || OMetadata.HasAnySceneTagCSV(sceneId, "idle,intro"))
      return
    endif

    TTON_Events.RegisterSexChangeEvent(ThreadID)
    Actor[] actors = OThread.GetActors(ThreadID)
    TTON_Utils.RequestSexComment(TTON_Utils.GetActorsNamesComaSeparated(actors) + " changed their sex position to : " + TTON_Utils.GetSceneDescription(sceneId, actors), actors)
EndEvent

Event OStimOrgasm(string eventName, string strArg, float numArg, Form sender)
    int ThreadID = numArg as int
    Actor orgasmedActor = sender as Actor
    TTON_Events.RegisterSexClimaxEvent(ThreadID, orgasmedActor)
EndEvent

Event OStimEnd(string eventName, string strArg, float numArg, Form sender)
    if(OThread.GetThreadCount() == 0)
        TTON_IsOstimActive.SetValue(0.0)
    endif
EndEvent

Event ThreadFinished(int ThreadID)
    if(!TTON_JData.GetThreadAddNewActors(ThreadID))
        TTON_Events.RegisterSexStopEvent(ThreadID)
    endif

    TTON_JData.SetThreadForced(ThreadId, 0)
EndEvent
