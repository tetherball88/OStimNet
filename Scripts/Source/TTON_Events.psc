scriptname TTON_Events

Function RegisterEvents() global
    ; tmp disabled events they don't make sense currently with direct narration logging additional data instead of events
    ; RegisterEventSchemaSexStart()
    ; RegisterEventSchemaSexSceneChange()
    ; RegisterEventSchemaSexStop()
    ; RegisterEventSchemaSexClimax()
EndFunction

; Function RegisterEventSchemaSexStart() global
;     string type = "tton_sex_start"
;     string name = "Sex Start"
;     string description = "Happens when OStim sex scene started"
;     string jsonParams = "[{\"name\": \"msg\", \"type\": 0, \"required\": true, \"description\": \"Event message to send.\"}]"
;     string renderParams = "{\"recent_events\":\"{{msg}}\",\"raw\":\"{{msg}}\",\"compact\":\"{{msg}}\",\"verbose\":\"{{msg}}\"}"
;     SkyrimnetApi.RegisterEventSchema(type, name, description, jsonParams, renderParams, false, 0)
; EndFunction

; Function RegisterEventSchemaSexStop() global
;     string type = "tton_sex_stop"
;     string name = "Sex Stop"
;     string description = "Happens when OStim finishes"
;     string jsonParams = "[{\"name\": \"msg\", \"type\": 0, \"required\": true, \"description\": \"Message to be sent with event.\"}]"
;     string renderParams = "{\"recent_events\":\"{{msg}}\",\"raw\":\"{{msg}}\",\"compact\":\"{{msg}}\",\"verbose\":\"{{msg}}\"}"
;     SkyrimnetApi.RegisterEventSchema(type, name, description, jsonParams, renderParams, false, 0)
; EndFunction

; Function RegisterEventSchemaSexClimax() global
;     string type = "tton_sex_climax"
;     string name = "Sex Climax"
;     string description = "Happens when one of OStim actors reach climax"
;     string jsonParams = "[{\"name\": \"msg\", \"type\": 0, \"required\": true, \"description\": \"Message to be sent with event.\"}]"
;     string renderParams = "{\"recent_events\":\"{{msg}}\",\"raw\":\"{{msg}}\",\"compact\":\"{{msg}}\",\"verbose\":\"{{msg}}\"}"
;     SkyrimnetApi.RegisterEventSchema(type, name, description, jsonParams, renderParams, false, 0)
; EndFunction

; Function RegisterEventSchemaSexSceneChange() global
;     string type = "tton_sex_change_long"
;     string name = "Sex Scene Change"
;     string description = "Happens when OStim sex scene changed"
;     string jsonParams = "[{\"name\": \"msg\", \"type\": 0, \"required\": true, \"description\": \"Message to be sent with event.\"}]"
;     string renderParams = "{\"recent_events\":\"{{msg}}\",\"raw\":\"{{msg}}\",\"compact\":\"{{msg}}\",\"verbose\":\"{{msg}}\"}"
;     SkyrimnetApi.RegisterEventSchema(type, name, description, jsonParams, renderParams, true, 30)
; EndFunction

; Function RegisterSexStartEvent(int ThreadID) global
;     string type = "tton_sex_start"
;     Actor[] actors = OThread.GetActors(ThreadID)
;     string sceneId = OThread.GetScene(ThreadID)
;     string sceneDesc = TTON_Utils.GetSceneDescription(sceneId, actors)
;     string msg = "SEXUAL ACTIVITY INITIATED: "+TTON_Utils.GetActorsNamesComaSeparated(actors)+" have begun a new intimate encounter while "+sceneDesc
;     string jsonData = "{\"msg\": \""+msg+"\"}"
;     SkyrimNetApi.RegisterEvent(type, jsonData, actors[0], none)
; EndFunction

; Function RegisterSexChangeEvent(int ThreadID) global
;     string id = JString.generateUUID()
;     string type = "tton_sex_change_long"
;     string description = "Happens when OStim sex scene changed"
;     Actor[] actors = OThread.GetActors(ThreadID)
;     string sceneId = OThread.GetScene(ThreadID)
;     string sceneDesc = TTON_Utils.GetSceneDescription(sceneId, actors)
;     string msg = "SEXUAL POSITION CHANGED: Participants "+TTON_Utils.GetActorsNamesComaSeparated(actors)+" changed position to: "+sceneDesc
;     ; SkyrimNetApi.RegisterEvent(type, "{\"msg\": \""+msg+"\"}", actors[0], none)
;     ; SkyrimNetApi.RegisterShortLivedEvent(id, type, description, msg, 60 * 1000, actors[0], none)

; EndFunction

Function RegisterSexClimaxEvent(int ThreadID, Actor orgasmedActor) global
    ; string type = "tton_sex_climax"
    ; string description = "Happens when one of OStim actors reach climax"
    ; string climaxLocations = TTON_Utils.GetShclongOrgasmedLocation(orgasmedActor, ThreadID)
    ; Actor[] actors = OThread.GetActors(ThreadID)
    ; string msg = "CLIMAX OCCURRED: "
    ; if(climaxLocations != "")
    ;     msg += climaxLocations
    ; else
    ;     msg += TTON_Utils.GetActorName(orgasmedActor) + " reached orgasm during sexual activity with " + TTON_Utils.GetActorsNamesComaSeparated(actors, orgasmedActor)
    ; endif

    ; string renderParams = "{\"recent_events\":\"{{msg}}\",\"raw\":\"{{msg}}\",\"compact\":\"{{msg}}\",\"verbose\":\"{{msg}}\"}"
    ; SkyrimNetApi.RegisterEvent(type, "{ \"msg\": \""+msg+"\" }", orgasmedActor, none)

    ; bool requestedComment = TTON_Utils.RequestSexComment(msg, speaker = orgasmedActor, ignoreCooldown = true)
EndFunction

Function RegisterSexStopEvent(int ThreadID) global
    ; bool hadSex = TTLL_OstimThreadsCollector.GetHadSex(ThreadID)

    ; Form[] actorsForms = TTLL_OstimThreadsCollector.GetActorsForms(ThreadID)
    ; Actor[] actors = PapyrusUtil.ActorArray(actorsForms.Length)
    ; int i = 0
    ; string climaxedActors = ""
    ; while(i < actors.Length)
    ;     actors[i] = actorsForms[i] as Actor
    ;     if(TTLL_OstimThreadsCollector.GetOrgasmed(ThreadID, actors[i]))
    ;         climaxedActors += TTON_Utils.GetActorName(actors[i]) + ","
    ;     endif

    ;     i += 1
    ; endwhile

    ; climaxedActors += ""

    ; string msg = ""

    ; if(hadSex)
    ;     msg += "INTIMATE "
    ; else
    ;     msg += "SEXUAL "
    ; endif
    ; msg += "ACTIVITY CONCLUDED: Participants "+TTON_Utils.GetActorsNamesComaSeparated(actors)
    ; string LastSexualSceneId = TTLL_OstimThreadsCollector.GetLastSexualSceneId(ThreadID)

    ; if(hadSex)
    ;     if(climaxedActors != "")
    ;         msg += ", with " + climaxedActors + " having reached orgasm."
    ;     else
    ;         msg += "."
    ;     endif
    ; else
    ;     if(LastSexualSceneId)
    ;         msg +=  TTON_Utils.GetSceneDescription(LastSexualSceneId, actors)
    ;     endif
    ;     msg += " without sexual activities."
    ; endif

    ; string last5Scenes = TTON_Utils.Get5LastScenesInThread(ThreadID)
    ; if(last5Scenes != "")
    ;     msg += " During encounter participant engaged in such scenes: \\n- "
    ;     msg += TTON_Utils.Get5LastScenesInThread(ThreadID)
    ; endif

    ; string type = "tton_sex_stop"
    ; string jsonData = "{\"msg\": \""+msg+"\"}"
    ; ; SkyrimNetApi.RegisterEvent(type, jsonData, actors[0], none)
    ; TTON_Utils.RequestSexComment(msg, actors, none, true)
EndFunction

Function RegisterDeclineEvent(string actionName, Actor initiator, bool isPlayerInvited = false) global
    Actor player = TTON_JData.GetPlayer()
    string id = JString.generateUUID()
    string type = "tton_" + actionName + "_declined"
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
    SkyrimNetApi.RegisterShortLivedEvent(id, type, description, msg, 120 * 1000, player, initiator)
EndFunction

