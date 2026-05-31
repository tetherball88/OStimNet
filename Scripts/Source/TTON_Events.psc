scriptname TTON_Events

Function RegisterEvents() global
    ; tmp disabled events they don't make sense currently with direct narration logging additional data instead of events
    RegisterEventSchema()
    ClearStorage()
EndFunction

Function ClearStorage() global
    StorageUtil.ClearAllPrefix(GetSkipKey(-1))
    StorageUtil.ClearAllPrefix(GetIgnoreKey(-1))
EndFunction

string Function GetSkipKey(int ThreadID) global
    string prefix = "TTON_Thread_SkipSceneChangeTrigger_"
    if(ThreadID == -1)
        return prefix
    endif
    return prefix + ThreadID
EndFunction

string Function GetIgnoreKey(int ThreadID) global
    string prefix = "TTON_Thread_IgnoreSceneChangeEvent_"
    if(ThreadID == -1)
        return prefix
    endif
    return prefix + ThreadID
EndFunction

Function RegisterEventSchema() global
    string name = "OStimNet Event"
    string description = "Happens on different occasions, OStim start/end/scene change and others"
    string jsonParams = "[" \
        + "{\"name\": \"tton_type\", \"type\": 0, \"required\": true, \"description\": \"Type of OStimNet event.\"}," \
        + "{\"name\": \"declinedAction\", \"type\": 0, \"required\": false, \"description\": \"If player declined what kind of declanation it was.\"}," \
        + "{\"name\": \"msg\", \"type\": 0, \"required\": true, \"description\": \"Event message to send.\"}," \
        + "{\"name\": \"intent\", \"type\": 0, \"required\": true, \"description\": \"Intent of the thread. platonic|romantic|lustful|transactional|dom|aggressive\"}," \
        + "{\"name\": \"mainActorNames\", \"type\": 0, \"required\": true, \"description\": \"Main actors in thread, their role based on intent: initiators, service providers, dom, agressors\"}," \
        + "{\"name\": \"secondaryActorNames\", \"type\": 0, \"required\": true, \"description\": \"Secondary actors in thread, their role based on intent: accepting, service receivers, sub, victims\"}," \
        + "{\"name\": \"isSexual\", \"type\": 2, \"required\": true, \"description\": \"Indicates if the scene is sexual.\"}," \
        + "{\"name\": \"skipTrigger\", \"type\": 2, \"required\": false, \"description\": \"If trigger should be skipped.\"}," \
        + "{\"name\": \"threadID\", \"type\": 1, \"required\": true, \"description\": \"OStim Thread ID.\"}" \
        +"]"
        string renderTemplateCompact = "\"{{render_template(\\\"helpers/ostimnet_event_compact\\\")}}\""
        string renderTemplate = "\"{{render_template(\\\"helpers/ostimnet_event_verbose\\\")}}\""
    string renderParams = "{\"recent_events\":"+renderTemplateCompact+",\"raw\":"+renderTemplate+",\"compact\":"+renderTemplateCompact+",\"verbose\":"+renderTemplate+"}"
    SkyrimnetApi.RegisterEventSchema("tton_event", name, description, jsonParams, renderParams, false, 0)
EndFunction

Function GameMasterMatchMakerEvent(string msg, Actor initiator) global
    string jsonData = "{"
    jsonData += "\"tton_type\": \"matchmaker_event\""
    jsonData += ",\"msg\": \"" + msg + "\""
    jsonData += ",\"threadID\": 0"
    jsonData += ",\"intent\": \"\""
    jsonData += ",\"mainActorNames\": \"\""
    jsonData += ",\"secondaryActorNames\": \"\""
    jsonData += ",\"skipTrigger\": false"
    jsonData += ",\"isSexual\": false"
    jsonData += "}"
    SkyrimNetApi.RegisterEvent("tton_event", jsonData, initiator, none)
EndFunction
