scriptname TTON_Decorators

;==========================================================================
; Decorator Registration
;==========================================================================

; Registers all template decorators with the SkyrimNet API
; These decorators provide dynamic data for use in LLM templates
Function RegisterDecorators() global
    SkyrimNetApi.RegisterDecorator("tton_get_speaking_npc_sex_action_info", "TTON_Decorators", "GetSpeakingNpcStartSexActionInfo")
    SkyrimNetApi.RegisterDecorator("tton_get_speaking_npc_sex_join_action_info", "TTON_Decorators", "GetSpeakingNpcJoinSexActionInfo")
    SkyrimNetApi.RegisterDecorator("tton_get_speaking_npc_social_action_info", "TTON_Decorators", "GetSpeakingNpcSocialActionInfo")
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
