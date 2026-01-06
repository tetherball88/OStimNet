Scriptname TTON_MainController extends Quest

GlobalVariable Property TTON_IsOstimActive  Auto


Event OnInit()
    Maintenance()
EndEvent

Function Maintenance()
    TTON_JData.ImportStaticData()
    TTON_Storage.ClearOnLoad()
    TTON_Events.RegisterEventSchema()

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

    TTON_Storage.StartThread(ThreadID)

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

Event OStimEnd(string eventName, string strArg, float numArg, Form sender)
    int ThreadID = numArg as int
    if(OThread.GetThreadCount() == 0)
        TTON_IsOstimActive.SetValue(0.0)
    endif
    TTON_Events.RegisterSexStopEvent(ThreadID)
    Actor player = TTON_JData.GetPlayer()

    ; TODO Uncomment when public version of SkyrimNet will have this function
    ; Utility.Wait(0.2)
    ; Form[] actors = TTON_Storage.GetActors(ThreadID)
    ; int i = 0
    ; while(i < actors.Length)
    ;     Actor current = actors[i] as Actor
    ;     if(current != player)
    ;         SkyrimNetApi.ReinforcePackages(actors[i] as Actor)
    ;     endif
    ;     i += 1
    ; endwhile

    TTON_Storage.EndThread(ThreadID)
EndEvent

Event ThreadFinished(int ThreadID)
    if(!TTON_JData.GetThreadAddNewActors(ThreadID))
        Actor[] actors = TTLL_ThreadsCollector.GetActors(ThreadID)
        int i = 0
        while(i < actors.Length)
            Actor current = actors[i]
            TTON_Storage.UpdateNpcSexualData(current)

            Actor[] lovers = TTLL_Store.GetAllLovers(current)
            int j = 0
            while(j < lovers.Length)
                TTON_Storage.UpdateNpcLoverSexualData(current, lovers[j])
                j += 1
            endwhile
            i += 1
        endwhile
    endif

    TTON_JData.SetThreadAffectionOnly(ThreadId, 0)
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
        Form[] climaxedActors = StorageUtil.FormListToArray(none, "TTONClimax_Thread" + ThreadID + "_ClimaxedActors")
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
