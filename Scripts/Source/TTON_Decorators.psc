scriptname TTON_Decorators

;==========================================================================
; Decorator Registration
;==========================================================================

; Registers all template decorators with the SkyrimNet API
; These decorators provide dynamic data for use in LLM templates
Function RegisterDecorators() global
    SkyrimNetApi.RegisterDecorator("tton_ostim_ongoing_scenes", "TTON_Decorators", "GetOStimOngoingSexScenes")
    SkyrimNetApi.RegisterDecorator("tton_ostim_npc_sex_scene", "TTON_Decorators", "GetOStimNpcSexScene")
    SkyrimNetApi.RegisterDecorator("tton_get_npc_sexual_data", "TTON_Decorators", "GetNpcSexualData")
    SkyrimNetApi.RegisterDecorator("tton_get_speaking_npc_sex_action_info", "TTON_Decorators", "GetSpeakingNpcStartSexActionInfo")
    SkyrimNetApi.RegisterDecorator("tton_get_speaking_npc_sex_join_action_info", "TTON_Decorators", "GetSpeakingNpcJoinSexActionInfo")
    SkyrimNetApi.RegisterDecorator("tton_get_speaking_npc_social_action_info", "TTON_Decorators", "GetSpeakingNpcSocialActionInfo")
EndFunction

;==========================================================================
; Scene Information Decorators
;==========================================================================

; Gets information about all ongoing OStim scenes
; @param npc The NPC requesting scene information
; @returns JSON string containing details about all active sex scenes
string Function GetOStimOngoingSexScenes(Actor npc) global
    ; todo as of now this method doesn't work, check later releases
    ; int[] threads = OThread.GetAllThreadIDs()
    int[] threads = StorageUtil.IntListToArray(none, "StartedOStimThreads")
    int i = 0
    string res = "{\"scenes\": ["

    while(i < threads.Length)
        ; this condition isn't needed when OThread.GetAllThreadIDs() is working properly
        if(OThread.IsRunning(threads[i]))
            string json = TTON_Utils.GetOStimSexSceneJson(npc, threads[i])
            if(json != "")
                StorageUtil.StringListAdd(none, "TTON_TmpScenes", json, false)
            endif
        endif

        i += 1
    endwhile

    res += PapyrusUtil.StringJoin(StorageUtil.StringListToArray(none, "TTON_TmpScenes"), ",") + "]}"

    StorageUtil.ClearAllPrefix("TTON_TmpScenes")

    return res
EndFunction

; Gets information about the specific OStim scene an NPC is participating in
; @param npc The NPC whose scene information to retrieve
; @returns JSON string containing details about the NPC's current sex scene
string Function GetOStimNpcSexScene(Actor npc) global
    int ThreadId = OActor.GetSceneID(npc)
    if(ThreadID == -1)
        return ""
    endif
    return TTON_Utils.GetOStimSexSceneJson(npc, ThreadId)
EndFunction

;==========================================================================
; Sexual History Decorators
;==========================================================================

; Retrieves comprehensive sexual history data for an NPC
; @param npc The NPC whose sexual history to retrieve
; @returns JSON string containing counts of different types of encounters and lover information
string Function GetNpcSexualData(Actor npc) global
    if(npc == none)
        return "{}"
    endif
    float currentTime = Utility.GetCurrentGameTime()
    int solo = TTLL_Store.GetNpcInt(npc, "solosex")
    int exclusive = TTLL_Store.GetNpcInt(npc, "exclusivesex")
    int groupCount = TTLL_Store.GetNpcInt(npc, "groupsex")
    int total = solo + exclusive + groupCount
    string json = "{"
    json += "\"soloSex\":" + solo + ","
    json += "\"exclusiveSex\":" + exclusive + ","
    json += "\"groupSex\":" + groupCount + ","
    json += "\"sameSexEncounter\":" + TTLL_Store.GetNpcInt(npc, "samesexencounter") + ","
    json += "\"totalEncounters\":" + total + ","

    json += "\"lovers\": ["

    Actor[] lovers = TTLL_Store.GetAllLovers(npc)

    string arraySeparator = ""

    int i = 0
    while(i < lovers.Length)
        Actor lover = lovers[i]
        json += arraySeparator + "{"
        json += "\"name\": \"" + TTON_Utils.GetActorName(lover) + "\","
        float diff = currentTime - TTLL_Store.GetLoverFlt(npc, lover, "lasttime")
        json += "\"daysAgo\": " + diff + ","
        json += "\"exclusiveSex\": " + TTLL_Store.GetLoverInt(npc, lover, "exclusivesex") + ","
        json += "\"groupSex\": " + TTLL_Store.GetLoverInt(npc, lover, "groupsex") + ","
        json += "\"score\": " + TTLL_Store.GetLoverScore(npc, lover)
        json += "}"
        arraySeparator = ","
        i += 1
    endwhile

    json += "]"

    json += "}"

    return json
EndFunction

;==========================================================================
; Action Information Decorators
;==========================================================================

; Gets information needed for the StartSex action template
; @param npc The NPC initiating the action
; @returns JSON string with NPC name, nearby potential partners, and available actions
string Function GetSpeakingNpcStartSexActionInfo(Actor npc) global
    return GetSpeakingNpcSexActionInfo(npc)
EndFunction

; Gets information needed for the JoinSex action template
; @param npc The NPC wanting to join an ongoing scene
; @returns JSON string with NPC name, nearby potential partners (in OStim scenes), and available actions
string Function GetSpeakingNpcJoinSexActionInfo(Actor npc) global
    return GetSpeakingNpcSexActionInfo(npc, "only")
EndFunction

; Helper function to get sexual action information for an NPC
; @param npc The NPC to get information for
; @param inOStim Filter for OStim scene participation: "exclude" (not in scene), "only" (in scene), or "all"
; @returns JSON string containing NPC name, filtered list of nearby actors, and available actions
string Function GetSpeakingNpcSexActionInfo(Actor npc, string inOStim = "exclude", bool nonSexual = false) global
    ; searches ostim elgigble actors nearby, unfortunately it also returns disabled not rendered npcs and npcs which already in OStim scenes
    Actor[] actors = OActorUtil.GetActorsInRangeV2(npc, 1000, false, true, true)
    Actor[] filteredActors = PapyrusUtil.ActorArray(0)

    int i = 0

    string actorsString = ""

    while(i < actors.Length)
        Actor currentActor = actors[i]
        bool isActorInOstim = OActor.IsInOStim(currentActor)
        bool ostimCondition = inOStim == "all"
        if(inOstim == "exclude" && !isActorInOstim)
            ostimCondition = true
        endif
        if(inOStim == "only" && isActorInOstim)
            ostimCondition = true
        endif

        if(actorsString != "")
            actorsString += ","
        endif

        if(ostimCondition && currentActor.IsEnabled())
            actorsString += TTON_Utils.GetActorName(currentActor)
        endif
        i+=1
    endwhile

    string nonSexualActions = "cuddling | hugging | frenchkissing | kissing | kissingcheek | kissinghand | kissingneck | pattinghead"
    string sexualActions = "analfingering | analfisting | analsex | analtoying | anilingus | blowjob | boobjob | buttjob | cunnilingus | deepthroat | femalemasturbation | footjob | grindingobject | grindingpenis | grindingthigh | gropingtesticles | handjob | lickingnipple | lickingpenis | lickingtesticles | lickingvagina | malemasturbation | rimjob | rubbingclitoris | rubbingpenisagainstface | suckingnipple | thighjob | tribbing | vaginalfingering | vaginalfisting | vaginalsex | vaginaltoying"
    string availableActions

    if(nonSexual)
        availableActions = nonSexualActions
    else
        availableActions = sexualActions
    endif

    string furnitureList = ""

    if(!nonSexual)
        ObjectReference[] availableFurniture = OFurniture.FindFurniture(2, npc, 1000.0, 100.0)
        int j = 0

        while(j < availableFurniture.Length)
            furnitureList += OFurniture.GetFurnitureType(availableFurniture[j]) + ","
            j += 1
        endwhile
    endif

    return "{\"name\": \""+TTON_Utils.GetActorName(npc)+"\", \"nearbyPotentialPartners\": \""+actorsString+"\", \"actions\": \""+availableActions+"\", \"furniture\": \""+furnitureList+"\"}"
EndFunction

string Function GetSpeakingNpcSocialActionInfo(Actor npc) global
    return GetSpeakingNpcSexActionInfo(npc, nonSexual = true)
EndFunction
