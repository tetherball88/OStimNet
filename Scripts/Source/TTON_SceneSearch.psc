scriptname TTON_SceneSearch

; Returns blacklisted intents for selected thread intent.
; For example: If intent romantic we don't want to show aggressive or dom scenes.
string Function BlacklistOtherIntents(string intent) global
    if(intent == "platonic")
        return "romantic,lustful,transactional,dom,aggressive"
    elseif(intent == "romantic")
        return "platonic,dom,aggressive,transactional"
    elseif(intent == "lustful")
        return "platonic,dom,aggressive,transactional"
    elseif(intent == "transactional")
        return "platonic,romantic,aggressive"
    elseif(intent == "dom")
        return "platonic,romantic,transactional,aggressive"
    elseif(intent == "aggressive")
        return "platonic,romantic,lustful,transactional,dom"
    endif

    return ""
EndFunction

; actors - array of actors to search for scene
; forceFurn - if true, will only search for scenes with the specified furniture type, never relax furniture filter
; furnType - furniture type to search for, if empty will search for scene without furniture
; actionType - action type to search for, vaginalsex, blodjob, etc...
; sexualPositions - sexual positions to search for, missionary, doggy, etc... Represented in ostim scene json in scene tags
; threadIntent - intent of the scene, romantic, lustful, transactional, dom, aggressive. Intents are stored in scene tags in ostim scene json.
; isStart - if searching for thread start or scene change. If thread start and no furniture try to find standing, otherwise we don't care about actor positional tags.
; isFemDom - if true, means female actors are dominant, if false means male actors are dominant. Only used if intent is dom or aggressive.
string Function SceneSexSearch(Actor[] actors, bool forceFurn = false, string furnType = "", string actionType = "", string sexualPositions = "", string threadIntent = "", bool isStart = false, bool isFemDom = false, bool isPlayerThread = false, bool usePhase = false) global
    TTON_Debug.debug("SceneSearch called with parameters: forceFurn=" + forceFurn + ", furnType=" + furnType + ", actionType=" + actionType + ", sexualPositions=" + sexualPositions + ", threadIntent=" + threadIntent + ", isStart=" + isStart + ", isFemDom=" + isFemDom)
    bool useIntent = SkyrimNetApi.GetConfigBool("Plugin_OStimNet", "tton.settings.ostimScenesIntentRule", true)
    string ActorTagBlacklistForAll = ""
    string AnyActorTagForAny = ""
    string AnyActionType = ""
    string AllSceneTags = ""
    string SceneTagBlacklist = ""
    string AnySceneTag = ""
    if(threadIntent != "romantic" && threadIntent != "lustful" && threadIntent != "transactional" && threadIntent != "dom" && threadIntent != "aggressive")
        TTON_Debug.warn("Invalid thread intent: " + threadIntent + ", ignoring intent requirement...")
        threadIntent = ""
    endif

    string phase = OStimNet.GetInitialPhaseByIntent(threadIntent, isPlayerThread)


    bool isDomOrAggressive = (threadIntent == "dom" || threadIntent == "aggressive")

    ; build per-actor "standing" tag string (e.g. "standing;standing" for 2 actors) for stance filters
    ; and performer index lists (sorted actor order matches OStim's internal assignment)
    string standingPerActor = ""
    int actorIdx = 0
    string AnyActionPerformer = ""
    string maleIndices = ""
    string femaleIndices = ""
    Actor[] sortedActors = OActorUtil.Sort(actors, OActorUtil.EmptyArray())
    while actorIdx < actors.Length
        if actorIdx > 0
            standingPerActor += ";"
        endif
        standingPerActor += "standing"
        if(isDomOrAggressive)
            if(sortedActors[actorIdx].GetActorBase().GetSex() == 0)
                maleIndices += actorIdx + ","
            else
                femaleIndices += actorIdx + ","
            endif
        endif
        actorIdx += 1
    endwhile

    ; performer indices used as fallback dom check when scene tags (dom/femdom) are absent
    if(isDomOrAggressive)
        if(isFemDom)
            AnyActionPerformer = femaleIndices
        else
            AnyActionPerformer = maleIndices
        endif
    endif

    if(furnType == "bed")
        ActorTagBlacklistForAll = standingPerActor
    elseif(furnType == "")
        AnyActorTagForAny = standingPerActor
    endif

    if(actionType != "")
        AnyActionType = actionType
    else
        AnyActionType = "sexual"
    endif

    string excludeFemdom = ""

    if(useIntent && threadIntent != "")
        SceneTagBlacklist = BlacklistOtherIntents(threadIntent)
        if(isDomOrAggressive)
            if(isFemDom)
                AllSceneTags += "femdom"
            else
                excludeFemdom = ",femdom"
            endif
        endif
    endif

    if(sexualPositions != "" && OStimNet.IsValidPosition(sexualPositions))
        AnySceneTag = sexualPositions
    endif

    string sceneId = ""

    if(usePhase && (phase == "undressing" || phase == "foreplay"))
        TTON_Debug.debug("Searching for foreplay scene with parameters: FurnitureType=" + furnType + ", ActorTagBlacklistForAll=" + ActorTagBlacklistForAll + ", AnyActorTagForAny=" + AnyActorTagForAny + ", ActionType=" + AnyActionType + ", SceneTagBlacklist=" + SceneTagBlacklist + excludeFemdom + ", AnySceneTag=" + AnySceneTag + ", AllSceneTags=" + AllSceneTags)
        sceneId = OLibrary.GetRandomSceneSuperloadCSV(actors, \
            FurnitureType = furnType, \
            ActorTagBlacklistForAll = ActorTagBlacklistForAll, \
            AnyActorTagForAny = AnyActorTagForAny, \
            AnyActionType = "sexual", \
            SceneTagBlacklist = SceneTagBlacklist + excludeFemdom, \
            AllSceneTags = AllSceneTags, \
            ActionBlacklistTypes =  "oral,intercourse" \
        )
        if(sceneId)
            return sceneId
        endif
    endif

    if(usePhase && (phase == "undressing" || phase == "foreplay" || phase == "oral"))
        TTON_Debug.debug("Searching for oral scene with parameters: FurnitureType=" + furnType + ", ActorTagBlacklistForAll=" + ActorTagBlacklistForAll + ", AnyActorTagForAny=" + AnyActorTagForAny + ", ActionType=" + AnyActionType + ", SceneTagBlacklist=" + SceneTagBlacklist + excludeFemdom + ", AnySceneTag=" + AnySceneTag + ", AllSceneTags=" + AllSceneTags)
        sceneId = OLibrary.GetRandomSceneSuperloadCSV(actors, \
            FurnitureType = furnType, \
            ActorTagBlacklistForAll = ActorTagBlacklistForAll, \
            AnyActorTagForAny = AnyActorTagForAny, \
            AnyActionType = "oral", \
            SceneTagBlacklist = SceneTagBlacklist + excludeFemdom, \
            AllSceneTags = AllSceneTags \
        )
        if(sceneId)
            return sceneId
        endif
    endif

    TTON_Debug.debug("Searching for scene with parameters: FurnitureType=" + furnType + ", ActorTagBlacklistForAll=" + ActorTagBlacklistForAll + ", AnyActorTagForAny=" + AnyActorTagForAny + ", ActionType=" + AnyActionType + ", SceneTagBlacklist=" + SceneTagBlacklist + excludeFemdom + ", AnySceneTag=" + AnySceneTag + ", AllSceneTags=" + AllSceneTags)
    sceneId = OLibrary.GetRandomSceneSuperloadCSV(actors, \
        FurnitureType = furnType, \
        ActorTagBlacklistForAll = ActorTagBlacklistForAll, \
        AnyActorTagForAny = AnyActorTagForAny, \
        AnyActionType = AnyActionType, \
        SceneTagBlacklist = SceneTagBlacklist + excludeFemdom, \
        AnySceneTag = AnySceneTag, \
        AllSceneTags = AllSceneTags \
    )

    if(sceneId)
        return sceneId
    endif

    ; relaxation step 1 - priority differs by context:
    ;   isStart=true:  relax sexual positions first so actor stance preference (standing) is kept longer
    ;   isStart=false: relax actor stance first so scene changes can flow freely between positions
    bool relaxedInStep2 = false
    if(isStart)
        if(AnySceneTag != "")
            TTON_Debug.warn("No scene found with all requirements: FurnitureType=" + furnType + ", SexualPositions=" + sexualPositions + ", ThreadIntent=" + threadIntent + ", relaxing sexual positions requirement...")
            AnySceneTag = ""
            relaxedInStep2 = true
        endif
    else
        if(ActorTagBlacklistForAll != "" || AnyActorTagForAny != "")
            TTON_Debug.warn("No scene found with all requirements: FurnitureType=" + furnType + ", ActionType=" + actionType + ", SexualPositions=" + sexualPositions + ", ThreadIntent=" + threadIntent + ", relaxing actor stance requirement...")
            ActorTagBlacklistForAll = ""
            AnyActorTagForAny = ""
            relaxedInStep2 = true
        endif
    endif

    ; relaxation step 1 search - skipped if nothing changed (would duplicate call 1)
    if(relaxedInStep2)
        TTON_Debug.debug("Searching for scene with relaxed requirements: FurnitureType=" + furnType + ", ActorTagBlacklistForAll=" + ActorTagBlacklistForAll + ", AnyActorTagForAny=" + AnyActorTagForAny + ", ActionType=" + AnyActionType + ", SceneTagBlacklist=" + SceneTagBlacklist + excludeFemdom + ", AnySceneTag=" + AnySceneTag + ", AllSceneTags=" + AllSceneTags)
        sceneId = OLibrary.GetRandomSceneSuperloadCSV(actors, \
            FurnitureType = furnType, \
            ActorTagBlacklistForAll = ActorTagBlacklistForAll, \
            AnyActorTagForAny = AnyActorTagForAny, \
            AnyActionType = AnyActionType, \
            SceneTagBlacklist = SceneTagBlacklist + excludeFemdom, \
            AnySceneTag = AnySceneTag, \
            AllSceneTags = AllSceneTags \
        )

        if(sceneId)
            return sceneId
        endif
    else
        TTON_Debug.debug("Skipping relaxation step 1 search since no requirements were relaxed")
    endif

    ; relaxation step 2 - the opposite of step 1:
    ;   isStart=true:  now relax actor stance too, allowing floor scenes if no standing scene was found
    ;   isStart=false: now relax sexual positions too, position can follow after stance is free
    ; also switches dom check from scene tags to performer indices (drops femdom tag / excludeFemdom)
    bool relaxedInStep3 = false
    if(isStart)
        if(ActorTagBlacklistForAll != "" || AnyActorTagForAny != "")
            TTON_Debug.warn("No scene found without sexual position filter: FurnitureType=" + furnType + ", ActionType=" + actionType + ", ThreadIntent=" + threadIntent + ", relaxing actor stance requirement...")
            ActorTagBlacklistForAll = ""
            AnyActorTagForAny = ""
            relaxedInStep3 = true
        endif
    else
        if(AnySceneTag != "")
            TTON_Debug.warn("No scene found without actor stance filter: FurnitureType=" + furnType + ", SexualPositions=" + sexualPositions + ", ThreadIntent=" + threadIntent + ", relaxing sexual positions requirement...")
            AnySceneTag = ""
            relaxedInStep3 = true
        endif
    endif

    ; relaxation step 2 search - all positional filters dropped; dom/aggressive uses performer check instead of scene tags
    ; always runs for dom/aggressive intent (performer is a new filter dimension), skipped otherwise if nothing changed
    if(AnyActionPerformer != "" || relaxedInStep2 || relaxedInStep3)
        TTON_Debug.debug("Searching for scene with all positional filters relaxed: FurnitureType=" + furnType + ", ActionType=" + AnyActionType + ", SceneTagBlacklist=" + SceneTagBlacklist + ", AnySceneTag=" + AnySceneTag + ", AnyActionPerformer=" + AnyActionPerformer)
        sceneId = OLibrary.GetRandomSceneSuperloadCSV(actors, \
            FurnitureType = furnType, \
            AnyActionType = AnyActionType, \
            SceneTagBlacklist = SceneTagBlacklist, \
            AnySceneTag = AnySceneTag, \
            AnyActionPerformer = AnyActionPerformer \
        )

        if(sceneId)
            return sceneId
        endif
    else
        TTON_Debug.debug("Skipping relaxation step 2 search since no requirements were relaxed and no performer check is needed")
    endif

    ; try relax action type requirement if no scene found
    if(actionType != "")
        TTON_Debug.warn("No scene found with all positional filters relaxed: FurnitureType=" + furnType + ", ActionType=" + actionType + ", AnyActionPerformer=" + AnyActionPerformer + ", SceneTagBlacklist=" + SceneTagBlacklist)
        AnyActionType = "sexual"
        sceneId = OLibrary.GetRandomSceneSuperloadCSV(actors, \
            FurnitureType = furnType, \
            AnyActionType = AnyActionType, \
            AnyActionPerformer = AnyActionPerformer, \
            SceneTagBlacklist = SceneTagBlacklist \
        )

        if(sceneId)
            return sceneId
        endif
    endif

    ; try performer with no intent blacklist - for packs without intent scene tags but with correct performer roles
    if(AnyActionPerformer != "" && useIntent && threadIntent != "")
        TTON_Debug.warn("No scene found with performer + intent requirements: FurnitureType=" + furnType + ", AnyActionType=" + AnyActionType + ", AnyActionPerformer=" + AnyActionPerformer)
        sceneId = OLibrary.GetRandomSceneSuperloadCSV(actors, \
            FurnitureType = furnType, \
            AnyActionType = AnyActionType, \
            AnyActionPerformer = AnyActionPerformer \
        )

        if(sceneId)
            return sceneId
        endif
    else
        TTON_Debug.debug("Skipping performer search without intent blacklist since either no performer check is needed or intent requirement is not used")
    endif

    ; try relax intent requirement if no scene found
    if(useIntent && threadIntent != "")
        TTON_Debug.warn("No scene found with action type and positional filters relaxed: FurnitureType=" + furnType + ", AnyActionType=" + AnyActionType)
        SceneTagBlacklist = ""
        sceneId = OLibrary.GetRandomSceneSuperloadCSV(actors, \
            FurnitureType = furnType, \
            AnyActionType = AnyActionType \
        )

        if(sceneId)
            return sceneId
        endif
    endif

    ; try relax furniture type requirement if no scene found
    if(!forceFurn && furnType != "")
        TTON_Debug.warn("No scene found with all other requirements relaxed: AnyActionType=" + AnyActionType)
        sceneId = OLibrary.GetRandomSceneSuperloadCSV(actors, \
            AnyActionType = AnyActionType \
        )

        if(sceneId)
            return sceneId
        endif
    endif

    TTON_Debug.err("No scene found with all requirements relaxed")

    return ""
EndFunction


string Function SearchCareScene(Actor[] actors, string intent = "", string originalActivity = "", Actor initiator = none, string actionfallback = "") global
    string ActionBlacklistTypes = "sexual"
    if(intent == "platonic")
        ActionBlacklistTypes += ",romantic"
    endif

    string activity = originalActivity

    if(activity == "kissing")
        activity = "kissing,frenchkissing"
    elseif(activity == "hugging")
        activity = "hugging,cuddling"
        ActionBlacklistTypes += ",kissing,frenchkissing"
    endif

    string initatorIndex
    if(actors[0] == initiator)
        initatorIndex = "0"
    else
        initatorIndex = "1"
    endif
    string ActionWhitelistActors = initatorIndex
    string AllActorTagsForAll = "standing"

    string SceneTagBlacklist = ""
    if(intent != "")
        SceneTagBlacklist = BlacklistOtherIntents(intent)
    endif

    string sceneId = SearchCareSceneBase(actors, \
        ActionWhitelistActors = initatorIndex, \
        AnyActionType = activity, \
        AllSceneTags = intent, \
        SceneTagBlacklist = SceneTagBlacklist, \
        AllActorTagsForAll = AllActorTagsForAll, \
        ActionBlacklistTypes = ActionBlacklistTypes \
    )

    if(sceneId)
        return sceneId
    endif

    if(actionfallback != "")
        sceneId = SearchCareSceneBase(actors, \
            AnyActionType = actionfallback, \
            AllActorTagsForAll = AllActorTagsForAll, \
            SceneTagBlacklist = SceneTagBlacklist, \
            ActionBlacklistTypes = ActionBlacklistTypes \
        )
        if(sceneId)
            return sceneId
        endif
    endif

    if(originalActivity != "hugging")
        sceneId = SearchCareSceneBase(actors, \
            AnyActionType = "hugging,cuddling", \
            AllActorTagsForAll = AllActorTagsForAll, \
            SceneTagBlacklist = SceneTagBlacklist, \
            ActionBlacklistTypes = ActionBlacklistTypes \
        )
        if(sceneId)
            return sceneId
        endif
    endif

    ; search with at least some actors standing
    sceneId = SearchCareSceneBase(actors, \
        ActionWhitelistActors = initatorIndex, \
        AnyActionType = activity, \
        ActorTagWhitelistForAny = AllActorTagsForAll, \
        ActionBlacklistTypes = ActionBlacklistTypes \
    )

    if(sceneId)
        return sceneId
    endif

    TTON_Debug.warn("No care scene found, try whitelisted and blacklisted actions only")
    sceneId = SearchCareSceneBase(actors, \
        AnyActionType = activity, \
        ActionBlacklistTypes = ActionBlacklistTypes \
    )

    if(sceneId)
        return sceneId
    endif

    TTON_Debug.warn("No care scene found with all requirements relaxed to not sexual")
    sceneId = SearchCareSceneBase(actors, \
        ActionBlacklistTypes = ActionBlacklistTypes \
    )

    return sceneId
EndFunction

string Function SearchCareSceneBase(Actor[] actors, string ActionWhitelistActors = "", string AnyActionType = "", string AllSceneTags = "", string SceneTagBlacklist = "", string AllActorTagsForAll = "", string ActionBlacklistTypes = "", string ActorTagWhitelistForAny = "") global
    TTON_Debug.debug("Searching for care scene with requirements: ActionWhitelistActors=" + ActionWhitelistActors + ", AnyActionType=" + AnyActionType + ", AllSceneTags=" + AllSceneTags + ", SceneTagBlacklist=" + SceneTagBlacklist + ", AllActorTagsForAll=" + AllActorTagsForAll + ", ActionBlacklistTypes=" + ActionBlacklistTypes)
    return OLibrary.GetRandomSceneSuperloadCSV(actors, \
        ActionWhitelistActors = ActionWhitelistActors, \
        AnyActionType = AnyActionType, \
        AllSceneTags = AllSceneTags, \
        SceneTagBlacklist = SceneTagBlacklist, \
        AllActorTagsForAll = AllActorTagsForAll, \
        ActionBlacklistTypes = ActionBlacklistTypes, \
        ActorTagWhitelistForAny = ActorTagWhitelistForAny \
    )
EndFunction
