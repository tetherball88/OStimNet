Scriptname TTON_MainController extends Quest

Function Maintenance()
    TTON_JData.ImportStaticData()
    
    RegisterForModEvent("ostim_thread_start", "OStimStart")
    RegisterForModEvent("ostim_thread_scenechanged", "OStimSceneChange")
    RegisterForModEvent("ostim_thread_speedchanged", "OStimSpeedChange")
    RegisterForModEvent("ostim_actor_orgasm", "OStimOrgasm")
    RegisterForModEvent("ostim_thread_end", "OStimEnd")
    
    TTON_Actions.RegisterActions()
    TTON_Events.RegisterEvents()
    TTON_Decorators.RegisterDecorators()
    
EndFunction

Event OStimStart(string eventName, string strArg, float numArg, Form sender)
    
    ; int ThreadID = numArg as int
    ; Actor[] actors = OThread.GetActors(ThreadID)
    
    ; TTON_Events.RegisterSexEvent("Intimate scene started between " + TTON_Utils.GetActorsNames(actors))
EndEvent

Event OStimSceneChange(string eventName, string strArg, float numArg, Form sender)
    
    ; int ThreadID = numArg as int
    ; Actor[] actors = OThread.GetActors(ThreadID)
    ; string sceneId = OThread.GetScene(ThreadID)
    
    ; if(OMetadata.IsTransition(sceneId) || OMetadata.HasAnySceneTagCSV(sceneId, "idle,intro"))
    ;   return
    ; endif
    
    ; TTON_Events.RegisterSexEvent("" + TTON_Utils.GetActorsNames(actors) + " changed their position to: " + TTON_Utils.GenerateOStimSceneDescription(ThreadID))
EndEvent

Event OStimSpeedChange(string eventName, string strArg, float numArg, Form sender)
EndEvent

Event OStimOrgasm(string eventName, string strArg, float numArg, Form sender)
    
    ; Actor OrgasmedActor = sender as Actor
    ; TTON_Events.RegisterSexEvent(TTON_Utils.GetActorName(OrgasmedActor) + " just had orgasm during sex.")
EndEvent

Event OStimEnd(string eventName, string strArg, float numArg, Form sender)
    
    ; int ThreadID = numArg as int
    ; Actor[] actors = OThread.GetActors(ThreadID)
    ; TTON_Events.RegisterSexEvent("Intimate scene finished between " + TTON_Utils.GetActorsNames(actors))
EndEvent

