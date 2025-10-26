scriptname TTON_Events

Function RegisterEvents() global
    RegisterEventSchemaSexStart()
    RegisterEventSchemaSexStop()
    RegisterEventSchemaSexClimax()
EndFunction

Function RegisterEventSchemaSexStart() global
    string type = "tton_sex_start"
    string name = "Sex Start"
    string description = "Happens when OStim sex scene started"
    string msg = "INTIMATE ACTIVITY INITIATED: Participants {{actors}} engaging together."
    string jsonParams = "[{\"name\": \"actors\", \"type\": 0, \"required\": true, \"description\": \"Participants in this scene.\"}]"
    string renderParams = "{\"recent_events\":\""+msg+"\",\"raw\":\""+msg+"\",\"compact\":\""+msg+"\",\"verbose\":\""+msg+"\"}"
    SkyrimnetApi.RegisterEventSchema(type, name, description, jsonParams, renderParams, false, 0)
EndFunction

Function RegisterEventSchemaSexStop() global
    string type = "tton_sex_stop"
    string name = "Sex Stop"
    string description = "Happens when OStim finishes"
    string jsonParams = "[{\"name\": \"msg\", \"type\": 0, \"required\": true, \"description\": \"Message to be sent with event.\"}]"
    string renderParams = "{\"recent_events\":\"{{msg}}\",\"raw\":\"{{msg}}\",\"compact\":\"{{msg}}\",\"verbose\":\"{{msg}}\"}"
    SkyrimnetApi.RegisterEventSchema(type, name, description, jsonParams, renderParams, false, 0)
EndFunction

Function RegisterEventSchemaSexClimax() global
    string type = "tton_sex_climax"
    string name = "Sex Climax"
    string description = "Happens when one of OStim actors reach climax"
    string jsonParams = "[{\"name\": \"msg\", \"type\": 0, \"required\": true, \"description\": \"Message to be sent with event.\"}]"
    string renderParams = "{\"recent_events\":\"{{msg}}\",\"raw\":\"{{msg}}\",\"compact\":\"{{msg}}\",\"verbose\":\"{{msg}}\"}"
    SkyrimnetApi.RegisterEventSchema(type, name, description, jsonParams, renderParams, false, 0)
EndFunction

Function RegisterSexStartEvent(int ThreadID) global
    string type = "tton_sex_start"
    Actor[] actors = OThread.GetActors(ThreadID)
    string jsonData = "{\"actors\": \""+TTON_Utils.GetActorsNamesComaSeparated(actors)+"\"}"
    SkyrimNetApi.RegisterEvent(type, jsonData, actors[0], none)
EndFunction

Function RegisterSexChangeEvent(int ThreadID) global
    string id = JString.generateUUID()
    string type = "tton_sex_change"
    string description = "Happens when OStim sex scene changed"
    Actor[] actors = OThread.GetActors(ThreadID)
    string msg = "SEXUAL POSITION CHANGED: Participants "+TTON_Utils.GetActorsNamesComaSeparated(actors)+" changed position."
    SkyrimNetApi.RegisterShortLivedEvent(id, type, description, msg, 20 * 1000, actors[0], none)
EndFunction

Function RegisterSexClimaxEvent(int ThreadID, Actor orgasmedActor) global
    string type = "tton_sex_climax"
    string description = "Happens when one of OStim actors reach climax"
    string climaxLocations = TTON_Utils.GetShclongOrgasmedLocation(orgasmedActor, ThreadID)
    Actor[] actors = OThread.GetActors(ThreadID)
    string msg = "CLIMAX OCCURRED: Participant "+TTON_Utils.GetActorName(orgasmedActor)
    if(climaxLocations != "")
        msg += " " + climaxLocations
    else
        msg += " reached orgasm"
    endif

    msg +=  " during sexual activity with " + TTON_Utils.GetActorsNamesComaSeparated(actors)

    bool requestedComment = TTON_Utils.RequestSexComment(msg, speaker = orgasmedActor, ignoreCooldown = true)

    if(!requestedComment)
        string renderParams = "{\"recent_events\":\"{{msg}}\",\"raw\":\"{{msg}}\",\"compact\":\"{{msg}}\",\"verbose\":\"{{msg}}\"}"
        SkyrimNetApi.RegisterEvent(type, "{ \"msg\": \""+msg+"\" }", orgasmedActor, none)
    endif
EndFunction

Function RegisterSexStopEvent(int ThreadID) global
    bool hadSex = TTLL_OstimThreadsCollector.GetHadSex(ThreadID)

    Form[] actorsForms = TTLL_OstimThreadsCollector.GetActorsForms(ThreadID)
    Actor[] actors = PapyrusUtil.ActorArray(actorsForms.Length)
    int i = 0
    string climaxedActors = ""
    while(i < actors.Length)
        actors[i] = actorsForms[i] as Actor
        if(TTLL_OstimThreadsCollector.GetOrgasmed(ThreadID, actors[i]))
            climaxedActors += TTON_Utils.GetActorName(actors[i]) + ","
        endif

        i += 1
    endwhile

    climaxedActors += ""

    string msg = ""

    if(hadSex)
        msg += "INTIMATE "
    else
        msg += "SEXUAL "
    endif
    msg += "ACTIVITY CONCLUDED: Participants "+TTON_Utils.GetActorsNamesComaSeparated(actors)+" ended their encounter"
    string LastSexualSceneId = TTLL_OstimThreadsCollector.GetLastSexualSceneId(ThreadID)

    if(LastSexualSceneId)
        msg += " while " + TTON_Utils.GetSceneDescription(LastSexualSceneId, actors)
    endif

    if(hadSex)
        if(climaxedActors != "")
            msg += ", with " + climaxedActors + " having reached orgasm."
        else
            msg += "."
        endif
    else
        msg += " without sexual activities."
    endif

    string type = "tton_sex_stop"
    string jsonData = "{\"msg\": \""+msg+"\"}"
    SkyrimNetApi.RegisterEvent(type, jsonData, actors[0], none)
    TTON_Utils.RequestSexComment(TTON_Utils.GetActorsNamesComaSeparated(actors) + " just finished their sexual encounter. Based on the recent context, generate a single in-character, post-sex comment that reflects how the encounter likely went.", actors, none, true)
EndFunction

Function RegisterStopSexDeniedEvent(int ThreadID, Actor initiator) global
    Actor player = TTON_JData.GetPlayer()
    string id = JString.generateUUID()
    string type = "tton_sex_stop_denied"
    string description = "Happens when npc wants to stop OStim sex scene but Player denied"
    Actor[] actors = OThread.GetActors(ThreadID)
    string msg = "SEX STOP DENIED: " + TTON_Utils.GetActorName(initiator) + " attempted to end sexual activity, but "+TTON_Utils.GetActorName(player)+" insisted on continuing."
    SkyrimNetApi.RegisterShortLivedEvent(id, type, description, msg, 20 * 1000, actors[0], none)
EndFunction
