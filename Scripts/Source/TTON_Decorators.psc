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
    int[] threads = new int[6]
    threads[0] = 0
    threads[1] = 1
    threads[2] = 2
    threads[3] = 3
    threads[4] = 4
    threads[5] = 5
    int i = 0
    string res = "{\"scenes\": ["
    bool first = true

    while(i < threads.Length)
        ; this condition isn't needed when OThread.GetAllThreadIDs() is working properly
        if(OThread.IsRunning(threads[i]))
            if(!first)
                res += ","
            endif
            first = false

            res += TTON_Utils.GetOStimSexSceneJson(npc, threads[i])

        endif

        i += 1
    endwhile

    res += "]}"

    return res
EndFunction

; Gets information about the specific OStim scene an NPC is participating in
; @param npc The NPC whose scene information to retrieve
; @returns JSON string containing details about the NPC's current sex scene
string Function GetOStimNpcSexScene(Actor npc) global
    int ThreadId = OActor.GetSceneID(npc)
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
    int solo = TTLL_Store.GetSoloSexCount(npc)
    int exclusive = TTLL_Store.GetExclusiveSexCount(npc)
    int groupCount = TTLL_Store.GetGroupSexCount(npc)
    int total = solo + exclusive + groupCount
    string json = "{"
    json += "\"soloSex\":" + solo + ","
    json += "\"exclusiveSex\":" + exclusive + ","
    json += "\"groupSex\":" + groupCount + ","
    json += "\"sameSexEncounter\":" + TTLL_Store.GetSameSexEncounterCount(npc) + ","
    json += "\"totalEncounters\":" + total + ","

    json += "\"lovers\": ["

    Actor lover = TTLL_Store.NextLover(npc)

    string arraySeparator = ""

    while(lover)
        json += arraySeparator + "{"
        json += "\"name\": \"" + TTLL_Store.GetLoverName(npc, lover) + "\","
        float diff = currentTime - TTLL_Store.GetLoverLastTime(npc, lover)
        json += "\"daysAgo\": " + diff + ","
        json += "\"exclusiveSex\": " + TTLL_Store.GetLoverExclusiveSexCount(npc, lover) + ","
        json += "\"groupSex\": " + TTLL_Store.GetLoverGroupSexCount(npc, lover) + ","
        json += "\"score\": " + TTLL_Store.GetLoverScore(npc, lover)
        json += "}"
        arraySeparator = ","
        lover = TTLL_Store.NextLover(npc, lover)
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
