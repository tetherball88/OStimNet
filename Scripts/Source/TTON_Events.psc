scriptname TTON_Events

Function RegisterEvents() global
    ; tmp disabled events they don't make sense currently with direct narration logging additional data instead of events
    RegisterEventSchema()
EndFunction

Function RegisterEventSchema() global
    string name = "OStimNet Event"
    string description = "Happens on different occasions, OStim start/end/scene change and others"
    string jsonParams = "[" \
        + "{\"name\": \"type\", \"type\": 0, \"required\": true, \"description\": \"Type of OStimNet event.\"}," \
        + "{\"name\": \"declinedAction\", \"type\": 0, \"required\": false, \"description\": \"If player declined what kind of declanation it was.\"}," \
        + "{\"name\": \"msg\", \"type\": 0, \"required\": true, \"description\": \"Event message to send.\"}," \
        + "{\"name\": \"skipTrigger\", \"type\": 2, \"required\": false, \"description\": \"If trigger should be skipped.\"}," \
        + "{\"name\": \"threadID\", \"type\": 1, \"required\": true, \"description\": \"OStim Thread ID.\"}" \
        +"]"
    string renderParams = "{\"recent_events\":\"{{msg}}\",\"raw\":\"{{msg}}\",\"compact\":\"{{msg}}\",\"verbose\":\"{{msg}}\"}"
    SkyrimnetApi.RegisterEventSchema("tton_event", name, description, jsonParams, renderParams, false, 0)
EndFunction

Function RegisterSexStartEvent(int ThreadID) global
    Actor[] actors = OThread.GetActors(ThreadID)
    Actor weightedActor = TTON_Utils.GetWeightedRandomActorToSpeak(actors)

    TTON_Debug.debug("RegisterSexStartEvent:"+ weightedActor.GetDisplayName())

    string sceneId = OThread.GetScene(ThreadID)
    string sceneDesc = TTON_Utils.GetSceneDescription(sceneId, actors)
    string actorsNames = TTON_Storage.GetActorsNames(ThreadID)
    string msg = actorsNames+" have begun a new intimate encounter while "+sceneDesc
    bool shouldSkipNonPlayerThreads = TTON_Utils.ShouldPrioritizePlayerThreadComments(ThreadID)
    bool skipTrigger = TTON_JData.GetMuteSetting() || shouldSkipNonPlayerThreads

    string jsonData = BuildJson("sex_start", msg, ThreadID, skipTrigger)
    SkyrimNetApi.RegisterEvent("tton_event", jsonData, weightedActor, none)
    ; StorageUtil.SetIntValue(none, "TTON_Thread"+ThreadID+"_SkipSceneChangeTrigger", 1)
EndFunction

Function RegisterSexChangeEvent(int ThreadID) global
    Actor[] actors = OThread.GetActors(ThreadID)
    Actor weightedActor = TTON_Utils.GetWeightedRandomActorToSpeak(actors, ThreadID = ThreadID)

    string sceneId = OThread.GetScene(ThreadID)
    string sceneDesc = TTON_Utils.GetSceneDescription(sceneId, actors)
    string actorsNames = TTON_Storage.GetActorsNames(ThreadID)
    string msg = "Participants "+actorsNames+" changed position to: "+sceneDesc
    bool shouldSkipNonPlayerThreads = TTON_Utils.ShouldPrioritizePlayerThreadComments(ThreadID)

    bool skipTriggerOnce = StorageUtil.GetIntValue(none, "TTON_Thread"+ThreadID+"_SkipSceneChangeTrigger", 0) == 1
    bool skipTrigger = skipTriggerOnce || TTON_JData.GetMuteSetting() || shouldSkipNonPlayerThreads
    string jsonData = BuildJson("sex_change", msg, ThreadID, skipTrigger)

    if(skipTriggerOnce)
        StorageUtil.SetIntValue(none, "TTON_Thread"+ThreadID+"_SkipSceneChangeTrigger", 0)
    endif

    SkyrimNetApi.RegisterEvent("tton_event", jsonData, weightedActor, none)
EndFunction

Function RegisterSexClimaxEvent(int ThreadID, Form[] orgasmedActors) global
    string msg = ""

    if(orgasmedActors.Length > 1)
        msg = "Multiple participants reached climax: "
    endif

    int i = 0
    while(i < orgasmedActors.Length)
        if(i != 0)
            msg += "; "
        endif
        Actor orgasmedActor = orgasmedActors[i] as Actor
        string climaxLocations = TTON_Utils.GetShclongOrgasmedLocation(orgasmedActor, ThreadID)
        if(climaxLocations != "")
            msg += climaxLocations
        else
            msg += TTON_Utils.GetActorName(orgasmedActor) + " reached orgasm."
        endif
        i += 1
    endwhile

    Actor speakingActor = TTON_Utils.GetWeightedRandomActorToSpeak(actorForms = orgasmedActors, ThreadID = ThreadID)

    bool shouldSkipNonPlayerThreads = TTON_Utils.ShouldPrioritizePlayerThreadComments(ThreadID)
    bool skipTrigger = TTON_JData.GetMuteSetting() || shouldSkipNonPlayerThreads
    string jsonData = BuildJson("climax", msg, ThreadID, skipTrigger)
    SkyrimNetApi.RegisterEvent("tton_event", jsonData, speakingActor, none)
    StorageUtil.SetIntValue(none, "TTON_Thread"+ThreadID+"_SkipSceneChangeTrigger", 1)
EndFunction

Function RegisterSpectatorAddedEvent(Actor spectator, int ThreadID) global
    string actorsNames = TTON_Storage.GetActorsNames(ThreadID)
    string spectatorName = TTON_Utils.GetActorName(spectator)
    string msg = spectatorName + " decided to watch " + actorsNames + " having intimate encounter."
    bool shouldSkipNonPlayerThreads = TTON_Utils.ShouldPrioritizePlayerThreadComments(ThreadID)

    bool skipTriggerOnce = StorageUtil.GetIntValue(none, "TTON_Thread"+ThreadID+"_SkipSceneChangeTrigger", 0) == 1
    bool skipTrigger = skipTriggerOnce || TTON_JData.GetMuteSetting() || shouldSkipNonPlayerThreads
    string jsonData = BuildJson("spectator_added", msg, ThreadID, skipTrigger)

    SkyrimNetApi.RegisterEvent("tton_event", jsonData, spectator, none)
EndFunction

Function RegisterSpectatorFledEvent(Actor spectator, int ThreadID) global
    string actorsNames = TTON_Storage.GetActorsNames(ThreadID)
    string spectatorName = TTON_Utils.GetActorName(spectator)
    string msg
    if(ThreadID == -1)
        msg = spectatorName + " was watching an intimate encounter but fled the scene."
    else
        msg = spectatorName + " was watching " + actorsNames + " having intimate encounter but fled the scene."
    endif

    bool shouldSkipNonPlayerThreads = TTON_Utils.ShouldPrioritizePlayerThreadComments(ThreadID)

    bool skipTriggerOnce = StorageUtil.GetIntValue(none, "TTON_Thread"+ThreadID+"_SkipSceneChangeTrigger", 0) == 1
    bool skipTrigger = skipTriggerOnce || TTON_JData.GetMuteSetting() || shouldSkipNonPlayerThreads
    string jsonData = BuildJson("spectator_fled", msg, ThreadID, skipTrigger)

    SkyrimNetApi.RegisterEvent("tton_event", jsonData, spectator, none)
EndFunction

Function RegisterSexStopEvent(int ThreadID, actor[] actors) global
    Actor weightedActor = TTON_Utils.GetWeightedRandomActorToSpeak(actors, ThreadID = ThreadID)
    string msg = "Participants "+TTON_Utils.GetActorsNamesComaSeparated(actors) + " finished their intimate encounter."

    bool shouldSkipNonPlayerThreads = TTON_Utils.ShouldPrioritizePlayerThreadComments(ThreadID)
    bool skipTrigger = TTON_JData.GetMuteSetting() || shouldSkipNonPlayerThreads
    string jsonData = BuildJson("sex_stop", msg, ThreadID, skipTrigger)
    SkyrimNetApi.RegisterEvent("tton_event", jsonData, weightedActor, none)
EndFunction

Function RegisterDeclineEvent(string actionName, Actor initiator, bool isPlayerInvited = false) global
    Actor player = TTON_JData.GetPlayer()
    string id = JString.generateUUID()
    string type = actionName + "_declined"
    string description = "Happens when npc wants to stop OStim sex scene but Player declined"
    string initiatorName = TTON_Utils.GetActorName(initiator)
    string playerName = TTON_Utils.GetActorName(player)
    string msg
    if(actionName == "StopSex")
        msg = initiatorName+" attempted to end sexual activity with "+playerName+", but "+playerName+" insisted on continuing."
    elseif(actionName == "ChangeSexPace")
        msg = initiatorName+" wanted to change the pace of sexual activity with "+playerName+", but "+playerName+" refused the change."
    elseif(actionName == "JoinOngoingSex")
        msg = initiatorName+" wanted to join ongoing sexual activity with "+playerName+", but "+playerName+" refused."
    elseif(actionName == "InviteToYourSex")
        if(isPlayerInvited)
            msg = initiatorName+" invited "+playerName+" to join their sexual activity, but "+playerName+" declined the invitation."
        else
            msg = initiatorName+" invited more people to ongoing sex with "+playerName+", but "+playerName+" refused to add more people."
        endif
    elseif(actionName == "ChangeSexActivity")
        msg = initiatorName+" wanted to change the sexual activity with "+playerName+", but "+playerName+" refused the change."
    elseif(actionName == "StartAffectionScene")
        msg = initiatorName+" wanted to start an affection scene with "+playerName+", but "+playerName+" declined the proposal."
    elseif(actionName == "StartSexAction")
        msg = initiatorName+" wanted to start a new sexual encounter with "+playerName+", but "+playerName+" declined the proposal."
    endif
    bool skipTrigger = TTON_JData.GetMuteSetting()
    string jsonData = BuildJson("action_decline", msg, 0, skipTrigger, actionName)
    SkyrimNetApi.RegisterEvent("tton_event", jsonData, player, initiator)
EndFunction

string Function BuildJson(string type, string msg, int ThreadID, bool skipTrigger, string declinedAction = "") global
    string jsonData = "{"
    jsonData += "\"type\": \""+type+"\""
    jsonData += ",\"msg\": \""+msg+"\""
    jsonData += ",\"threadID\": "+ThreadID
    jsonData += ",\"skipTrigger\": "

    if(skipTrigger)
        jsonData += "true"
    else
        jsonData += "false"
    endif

    if(declinedAction != "")
        jsonData += ", \"declinedAction\": \""+declinedAction+"\""
    endif

    jsonData += "}"
    return jsonData
EndFunction
