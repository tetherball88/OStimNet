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

    TTON_Events.RegisterSexStartEvent(ThreadID)
EndEvent

Event OStimSceneChange(string eventName, string strArg, float numArg, Form sender)
    int ThreadID = numArg as int
    string sceneId = OThread.GetScene(ThreadID)

    if(OMetadata.IsTransition(sceneId) || OMetadata.HasAnySceneTagCSV(sceneId, "idle,intro"))
      return
    endif

    TTON_Events.RegisterSexChangeEvent(ThreadID)
EndEvent

Event OStimSpeedChange(string eventName, string strArg, float numArg, Form sender)
    ; int ThreadID = numArg as int
    ; Actor[] actors = OThread.GetActors(ThreadID)
    ; TTON_Events.RegisterSexEvent("" + TTON_Utils.GetActorsNamesComaSeparated(actors) + " changed their sex speed.")
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

    ; int ThreadID = numArg as int
    ; Actor[] actors = OThread.GetActors(ThreadID)
    ; TTON_Events.RegisterSexStopEvent(ThreadID)
EndEvent

Event ThreadFinished(int ThreadID)
    TTON_Events.RegisterSexStopEvent(ThreadID)
EndEvent
