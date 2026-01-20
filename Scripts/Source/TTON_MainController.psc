Scriptname TTON_MainController extends Quest

GlobalVariable Property TTON_IsOstimActive  Auto


Event OnInit()
    Maintenance()
EndEvent

Function Maintenance()
    ; clear sexual data from Lover's Ledger which is now its own separate integration
    StorageUtil.ClearAllPrefix("TTONDec_SexualData")
    TTON_JData.ImportStaticData()
    TTON_Storage.ClearOnLoad()
    TTON_Events.RegisterEventSchema()

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
    TTON_IsOstimActive.SetValue(1.0)
    int ThreadID = numArg as int

    TTON_Storage.StartThread(ThreadID)

    int continuedFromThreadID = TTON_Storage.GetThreadContinuationFrom(ThreadID)

    ; if this thread was started as part of adding new actors clean data related to this and old thread
    if(continuedFromThreadID != -1)
        TTON_Storage.SetThreadAddNewActors(continuedFromThreadID, 0)
        TTON_Storage.SetThreadContinuationFrom(ThreadID, -1)
    else
        TTON_Events.RegisterSexStartEvent(ThreadID)
    endif
EndEvent

Event OStimSceneChange(string eventName, string strArg, float numArg, Form sender)
    int ThreadID = numArg as int
    string sceneId = OThread.GetScene(ThreadID)

    TTON_Storage.SceneChange(ThreadID)

    if(OMetadata.IsTransition(sceneId) || OMetadata.HasAnySceneTagCSV(sceneId, "idle,intro"))
      return
    endif

    StorageUtil.IntListAdd(none, "TTONSceneChange_Threads", ThreadID, false)
    SceneChangeDebounced()
EndEvent

Event OStimOrgasm(string eventName, string strArg, float numArg, Form sender)
    int ThreadID = numArg as int
    Actor orgasmedActor = sender as Actor
    StorageUtil.IntListAdd(none, "TTONClimax_ClimaxedThreads", ThreadID, false)
    StorageUtil.FormListAdd(none, "TTONClimax_Thread" + ThreadID + "_ClimaxedActors", orgasmedActor, false)
    ClimaxDebounced()
EndEvent

Event OStimEnd(string eventName, string json, float numArg, Form sender)
    int ThreadID = numArg as int
    if(OThread.GetThreadCount() == 0)
        TTON_IsOstimActive.SetValue(0.0)
    endif
    Actor[] actors = OJSON.GetActors(json)
    TTON_Events.RegisterSexStopEvent(ThreadID, actors)
    Actor player = TTON_JData.GetPlayer()

    Utility.Wait(0.2)
    int i = 0
    while(i < actors.Length)
        Actor current = actors[i]
        if(current != player)
            SkyrimNetApi.ReinforcePackages(current)
        endif
        i += 1
    endwhile

    TTON_Storage.EndThread(ThreadID)
EndEvent

Float Property SceneChangeDebounceSeconds = 2.0 Auto
Float Property ClimaxDebounceSeconds = 1.0 Auto

Float _climaxLast = 0.0
Bool  _climaxArmed = False

Float _sceneChangeLast = 0.0
Bool  _sceneChangeArmed = False

Function ClimaxDebounced()
    _climaxLast = Utility.GetCurrentRealTime()

    if(!_climaxArmed && !_sceneChangeArmed)
        RegisterForSingleUpdate(1.0)
    endif
    if !_climaxArmed
        _climaxArmed = True
    endif
EndFunction

Function SceneChangeDebounced()
    _sceneChangeLast = Utility.GetCurrentRealTime()

    if(!_climaxArmed && !_sceneChangeArmed)
        RegisterForSingleUpdate(1.0)
    endif
    if !_sceneChangeArmed
        _sceneChangeArmed = True
    endif
EndFunction

Event OnUpdate()
    Float now = Utility.GetCurrentRealTime()
    Float climaxElapsed = now - _climaxLast
    Float sceneChangedElapsed = now - _sceneChangeLast

    if _climaxArmed && climaxElapsed >= ClimaxDebounceSeconds
        _climaxArmed = False
        FireEventForMultipleClimaxes()
    endif

    if _sceneChangeArmed && sceneChangedElapsed >= SceneChangeDebounceSeconds
        _sceneChangeArmed = False
        FireEventForSceneChange()
    endif

    if(_sceneChangeArmed || _climaxArmed)
        RegisterForSingleUpdate(1.0)
    endif
EndEvent

Function FireEventForMultipleClimaxes()
    int i = 0
    int[] climaxedThreads = StorageUtil.IntListToArray(none, "TTONClimax_ClimaxedThreads")
    while(i < climaxedThreads.Length)
        int ThreadID = climaxedThreads[i]
        Form[] climaxedActorsForms = StorageUtil.FormListToArray(none, "TTONClimax_Thread" + ThreadID + "_ClimaxedActors")
        
        ; Convert Form[] to Actor[]
        Actor[] climaxedActors = PapyrusUtil.ActorArray(0)
        int j = 0
        while j < climaxedActorsForms.Length
            Actor orgasmedActor = climaxedActorsForms[j] as Actor
            if orgasmedActor
                climaxedActors = PapyrusUtil.PushActor(climaxedActors, orgasmedActor)
            endif
            j += 1
        endwhile
        
        TTON_Events.RegisterSexClimaxEvent(ThreadID, climaxedActors)
        i += 1
    endwhile

    Utility.Wait(1)
    StorageUtil.ClearAllPrefix("TTONClimax_")
EndFunction

Function FireEventForSceneChange()
    int i = 0
    int[] climaxedThreads = StorageUtil.IntListToArray(none, "TTONSceneChange_Threads")
    while(i < climaxedThreads.Length)
        int ThreadID = climaxedThreads[i]
        TTON_Events.RegisterSexChangeEvent(ThreadID)
        i += 1
    endwhile

    Utility.Wait(1)
    StorageUtil.ClearAllPrefix("TTONSceneChange_")
EndFunction
