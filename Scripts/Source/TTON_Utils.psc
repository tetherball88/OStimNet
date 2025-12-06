scriptname TTON_Utils

;==========================================================================
; Actor Utility Functions
;==========================================================================

; Gets the appropriate display name for an actor
; @param akActor The actor to get the name for
; @returns The actor's name (base name for player, display name for NPCs)
string Function GetActorName(actor akActor) global
    if akActor == TTON_JData.GetPlayer()
        return akActor.GetActorBase().GetName()
    else
        return akActor.GetDisplayName()
    EndIf
EndFunction


;==========================================================================
; Scene Description Generation
;==========================================================================

; Generates a detailed textual description of an ongoing OStim scene
; @param ThreadID The ID of the thread to describe
; @returns Natural language description of the scene and its participants
string Function GenerateOStimSceneDescription(string sceneId, Actor[] actors) global
    string[] actionTypes = OMetadata.GetActionTypes(sceneId)
    string actorString = GetActorsNamesComaSeparated(actors)

    string result = actorString + " engaged in intimate scene."

    int i = 0

    while(i < actionTypes.Length)
        int actorIdx = OMetadata.GetActionActor(sceneId, i)
        int targetIdx = OMetadata.GetActionTarget(sceneId, i)
        int performerIdx = OMetadata.GetActionPerformer(sceneId, i)
        Actor activeActor = actors[actorIdx]
        Actor target = actors[targetIdx]
        Actor performer = actors[performerIdx]

        string performerTags = ""

        if(performer != activeActor && performer != target)
        performerTags = GetActorTagsCsv(sceneId, actorIdx)
        endif

        result += " " + GetActorName(activeActor) + "("+GetActorTagsCsv(sceneId, actorIdx)+") enacts the " + actionTypes[i] + ", taking the active physical role with " + GetActorName(target) + "("+GetActorTagsCsv(sceneId, targetIdx)+"). The pace and movement are guided by " + GetActorName(performer) + "'s"+performerTags+" lead."

        i += 1
    endwhile

    return result
EndFunction

; Gets a comma-separated list of tags for an actor in a scene
; @param sceneId The ID of the scene
; @param actorIdx The index of the actor in the scene
; @returns CSV string of actor tags
string Function GetActorTagsCsv(string sceneId, int actorIdx) global
    return OCSV.ToCSVList(OMetadata.GetActorTags(sceneId, actorIdx))
EndFunction

;==========================================================================
; OStim Scene Management
;==========================================================================

; Finds an appropriate scene based on actors and specified actions
; @param actors Array of actors to find a scene for
; @param actions CSV string of action types or scene actions to search for
; @param useRandom Whether to fall back to a random scene if no matching scene is found
; @returns Scene ID if found, empty string if no suitable scene
string function getSceneByActions(actor[] actors, string actions, string furn = "none", bool nonSexual = false) global
    string newScene

    if(!nonSexual)
        if(furn != "none")
            if(OFurniture.IsChildOf("bed", furn))
                ; prioritize scenes without standing actors if bed is used
                newScene = OLibrary.GetRandomSceneSuperloadCSV(actors, furn, AnyActionType = actions, ActorTagBlacklistForAll ="standing")
                if(newScene == "")
                    ; while using bed, prioritize finding scene without standing actors over specific action types
                    newScene = OLibrary.GetRandomSceneSuperloadCSV(actors, furn, ActionWhitelistTypes = "sexual", ActorTagBlacklistForAll ="standing")
                endif
            endif
            if(newScene == "")
                ; try to find by action with furniture
                newScene = OLibrary.GetRandomSceneSuperloadCSV(actors, furn, AnyActionType = actions)
                ; still no match, try any sexual scene
                if(newScene == "")
                    newScene = OLibrary.GetRandomSceneSuperloadCSV(actors, furn, ActionWhitelistTypes = "sexual")
                endif
            endif
        else
            ; prioritize scenes with at least one standing actor if no furniture specified
            newScene = OLibrary.GetRandomSceneSuperloadCSV(actors, AnyActionType = actions, ActorTagWhitelistForAny ="standing")
        endif

        ; try to find by action but without furniture
        if(newScene == "")
            newScene = OLibrary.GetRandomSceneSuperloadCSV(actors, AnyActionType = actions)
        endif

        if(newScene == "")
            ; try to find any sexual scene
            newScene = OLibrary.GetRandomSceneSuperloadCSV(actors, ActionWhitelistTypes = "sexual")
        endif
    else
        ; try to find non sexual scene for standing actors
        newScene = OLibrary.GetRandomSceneSuperloadCSV(actors, AnyActionType = actions, ActionBlacklistTypes = "sexual", AllActorTagsForAll = "standing")
        if(newScene == "")
            ; try to find by action but without standing restriction
            newScene = OLibrary.GetRandomSceneSuperloadCSV(actors, AnyActionType = actions, ActionBlacklistTypes = "sexual")
        endif
    endif

    return newScene
endfunction

; Checks if there are any available scenes for the given actor combination
; @param actors Array of actors to check scenes for
; @returns True if at least one suitable scene exists
string Function UserHasScenesForActors(Actor[] actors, string furn = "") global
    string sceneId = OLibrary.GetRandomSceneSuperloadCSV(actors, furn, AnyActionType = "sexual") != ""
    if(sceneId == "")
        return "no-furn"
    else
        return "furn"
    endif

    return "none"
EndFunction

; return changed animation position based on actor's ostim tags or original position
int Function GetActorAnimationIndex(Actor currentA, int position, string sceneId) global
    string[] actorTags = OMetadata.GetActorTags(sceneId, position)
    int k = 0
    while(k < actorTags.Length)
        string tag = actorTags[k]
        if(StringUtil.Find(tag, "animationIndex") == 0)
            if(tag == "animationIndex0")
                return 0
            elseif(tag == "animationIndex1")
                return 1
            elseif(tag == "animationIndex2")
                return 2
            elseif(tag == "animationIndex3")
                return 3
            elseif(tag == "animationIndex4")
                return 4
            endif
        endif
        k += 1
    endwhile

    return position
EndFunction

Actor[] Function SortOstimActorsWithAnimationIndex(Actor[] actors, string sceneId) global
    int aLength = actors.length
    Actor[] sortedActors = PapyrusUtil.ActorArray(aLength)

    int j = 0
    while(j < aLength)
        Actor currentA = actors[j]
        int animationIndex = GetActorAnimationIndex(currentA, j, sceneId)

        sortedActors[animationIndex] = currentA
        j += 1
    endwhile

    return sortedActors
EndFunction

; Converts an array of actors into a JSON array of their names
; @param actors Array of actors to convert
; @returns JSON array string containing actor names
string Function GetActorsNamesJson(Actor[] actors, string sceneId, bool sortByAnimationIndex = false) global
    string actorNames = "["
    int i = 0

    int aLength = actors.length
    Actor[] sortedActors = SortOstimActorsWithAnimationIndex(actors, sceneId)

    while(i < sortedActors.length)
        if(i != 0)
        actorNames += ","
        endif

        actorNames += "\""+TTON_Utils.GetActorName(sortedActors[i])+"\""
        i += 1
    endwhile

    string res = actorNames + "]"

    return res
EndFunction

; Converts an array of actors into a comma-separated string of their names
; @param actors Array of actors to convert
; @returns Comma-separated string of actor names
string Function GetActorsNamesComaSeparated(Actor[] actors, Actor exclude = none) global
    int i = 0
    string res = ""
    while(i < actors.Length)
        if(actors[i] != exclude)
            string actorName = TTON_Utils.GetActorName(actors[i])
            if(actorName != "")
                if(res != "")
                    res += ", "
                endif
                res += TTON_Utils.GetActorName(actors[i])
            endif
        endif
        i += 1
    endwhile
    return res
EndFunction

;==========================================================================
; Scene Data Serialization
;==========================================================================

string Function GetBaseSceneId(string variantId) global
    int idx = StringUtil.Find(variantId, "Swapped")
    if idx != -1
        ; return everything before "Swapped"
        return StringUtil.Substring(variantId, 0, idx)
    endif
    ; no suffix found, return unchanged
    return variantId
EndFunction

string Function GetSceneDescription(string sceneId, Actor[] actors) global
    string baseSceneId = GetBaseSceneId(sceneId)

    string actorsStrArr = GetActorsNamesJson(actors, sceneId, true)

    string template = TTON_JData.GetDescription(baseSceneId)

    string description = ""

    if(template)
        description = SkyrimNetApi.ParseString(template, "scenedata", "{\"actors\": "+actorsStrArr+"}")
    endif

    if(!description)
      description = TTON_Utils.GenerateOStimSceneDescription(sceneId, actors)
    endif

    return description
EndFunction

float Function GetLightFactor(Actor target) global
    float lightLevel = target.GetLightLevel() ; 0 - 150
    if lightLevel < 0.0
        lightLevel = 0.0
    elseif lightLevel > 100.0
        lightLevel = 100.0
    endif

    ; Normalize to 0..1
    float lightNorm = lightLevel / 100.0

    ; Light factor: in pitch black = 0.4x sight, in full light = 1.0x sight
    return 0.4 + (0.6 * lightNorm)
EndFunction

string Function GetVisibility(Actor npc, Actor target) global
    float distance = npc.GetDistance(target)
    bool isInLos = npc.HasLOS(target)
    float hearingDistance = TTON_JData.GetMcmCommentsDistance() ; max distance to hear comments
    if(hearingDistance == 0)
        return "canSeeAndHear"
    endif
    float maxHear = hearingDistance
    float maxSight = hearingDistance * 0.7 ; npc can hear further than see
    maxSight *= GetLightFactor(target)

    bool isInterior = npc.GetParentCell().IsInterior()

    if(isInterior)
        maxHear *= 1.2 ; interiors are usually quieter
    else
        maxHear *= 0.8 ; outdoors are usually noisier
    endif

    if distance > maxHear
        return "none"
    endif

    float sightClose = maxSight * 0.35 ; close sight range
    float sightMid = maxSight * 0.80 ; mid sight range
    float hearClose = maxHear * 0.60 ; close hear range
    float hearFar = maxHear ; far hear range

    if distance <= maxSight
        if distance <= sightClose
            ; very close: always see + hear
            return "canSeeAndHear"
        elseif distance <= sightMid
            ; mid-range: LOS decides if we see
            if isInLos
                return "canSeeAndHear"
            else
                return "canHearClose"
            endif
       else
            ; edge of sight detail range
            if isInLos
                return "canSeeAndHear"
            else
                ; rely purely on hearing band
                if distance <= hearClose
                    return "canHearClose"
                else
                    return "canHearFar"
                endif
            endif
        endif
    endif

    if distance <= hearClose
        return "canHearClose"
    else
        return "canHearFar"
    endif
EndFunction

; Generates a JSON representation of an OStim scene
; @param npc The NPC requesting the scene information
; @param ThreadId The ID of the thread to get information about
; @returns JSON object containing scene details, participants, and description
string Function GetOStimSexSceneJson(Actor npc, int ThreadId) global
    string sceneId = OThread.GetScene(ThreadID)
    ; without Swapped suffix
    string baseSceneId =  GetBaseSceneId(sceneId)
    bool isInThisScene = OThread.GetActorPosition(ThreadId, npc) != -1
    Actor[] actors = OThread.GetActors(ThreadId)

    string description = ""
    string isSexual = TTLL_ThreadsCollector.GetThreadBool(ThreadId, "hadsex")

    string visibility = GetVisibility(npc, actors[0])

    if(visibility == "canSeeAndHear")
        description = GetSceneDescription(sceneId, actors)
    endif

    return "{\"isInThisScene\": "+isInThisScene+", \"sceneId\": \""+sceneId+"\",  \"description\": \""+description+"\", \"isSexual\": "+isSexual+", \"visibility\": \""+visibility+"\" }"
EndFunction


; Checks if an actor is eligible for OStim scenes
; @param npc The actor to check for eligibility
; @returns True if the actor is eligible (not a child and passes OStim verification)
bool Function IsOStimEligible(Actor npc) global
    return !OUtils.IsChild(npc) && OActor.VerifyActors(OActorUtil.ToArray(npc)) && !npc.IsInDialogueWithPlayer()
EndFunction

; Gets an eligible actor from JSON parameters
; @param paramsJson The JSON parameters containing actor information
; @param paramName The name of the parameter containing the actor
; @returns The actor if eligible, none otherwise
Actor Function GetEligibleActorFromParam(string paramsJson, string paramName) global
    Actor npc = SkyrimNetApi.GetJsonActor(paramsJson, paramName, none)
    if(!IsOStimEligible(npc))
        return none
    endif
    return npc
EndFunction

; Determines which speed changes are available for a given scene
; @param SceneId The ID of the current scene
; @param currentSpeed The current speed level
; @returns String indicating available speed changes: "both", "increase", "decrease", or "none"
String Function GetAvailableSpeedDirections(string SceneId, int currentSpeed) global
    string defaultSpeed = OMetadata.GetDefaultSpeed(SceneId)
    string maxSpeed = OMetadata.GetMaxSpeed(SceneId)
    string res = "none"

    if(currentSpeed > defaultSpeed && currentSpeed < maxSpeed)
        res = "both"
    elseif(currentSpeed > defaultSpeed)
        res = "decrease"
    elseif(currentSpeed < maxSpeed)
        res = "increase"
    endif

    return res
EndFunction

string Function GetShclongOrgasmedLocation(Actor npc, int ThreadID) global
    string npcName = GetActorName(npc)
    string res = npcName + " ejaculated "

    ; those who can cum inside
    bool hadLocations = false
    if OActor.HasSchlong(npc)
        string sceneId = OThread.GetScene(ThreadID)
        int actorPosition = GetActorAnimationIndex(npc, OThread.GetActorPosition(ThreadID, npc), sceneId)
        int[] ActionsAsTarget = OMetadata.FindActionsSuperloadCSVv2(sceneId, ParticipantPositionsAny=actorPosition, AnyCustomStringListRecord = ";cum")
        int[] ActionsAsActor = OMetadata.FindActionsSuperloadCSVv2(sceneId, ParticipantPositionsAny=actorPosition, AnyCustomStringListRecord = "cum")
        int i = 0

        while(i < ActionsAsTarget.Length)
            int ActionIndex = ActionsAsTarget[i]
            Actor akTarget = OThread.GetActor(ThreadID, OMetadata.GetActionTarget(sceneId, ActionIndex))
            BuildCumLocations(sceneId, ActionIndex, akTarget, true, npc)
            i += 1
        EndWhile

        i = 0
        while(i < ActionsAsActor.Length)
            int ActionIndex = ActionsAsActor[i]
            Actor akActor = OThread.GetActor(ThreadID, OMetadata.GetActionActor(sceneId, ActionIndex))
            BuildCumLocations(sceneId, ActionIndex, akActor, false, npc)
            i += 1
        EndWhile

        i = 0
        Form[] actors = StorageUtil.FormListToArray(npc, "cumOnNpcs")
        string locations = ""
        while(i < actors.Length)
            Actor cumOnActor = actors[i] as Actor
            String[] SlotsOn = StorageUtil.StringListToArray(cumOnActor, npcName+"_cumOnPlacesOn")
            String[] SlotsIn = StorageUtil.StringListToArray(cumOnActor, npcName+"_cumOnPlacesIn")

            if(SlotsOn.Length > 0)
                locations += "on " + GetActorName(cumOnActor) + "'s "
                hadLocations = true
                locations += OCSV.ToCSVList(SlotsOn)
                if(SlotsIn.Length > 0)
                    locations += " and "
                endif
            endif
            if(SlotsIn.Length > 0)
                locations += "in " + GetActorName(cumOnActor) + "'s "
                hadLocations = true
                locations += OCSV.ToCSVList(SlotsIn)
            endif
            locations += "; "
            i += 1
            StorageUtil.StringListClear(cumOnActor, npcName+"_cumOnPlacesOn")
            StorageUtil.StringListClear(cumOnActor, npcName+"_cumOnPlacesIn")
        endwhile

        res += locations
    endif

    if(!hadLocations)
        return ""
    endif

    StorageUtil.FormListClear(npc, "cumOnNpcs")

    return res
EndFunction

Function BuildCumLocations(string sceneId, int ActionIndex, Actor cumTarget, bool isTarget, Actor cummingNpc) global
    string npcName = GetActorName(cummingNpc)
    String[] Slots
    if(isTarget)
        Slots = OMetadata.GetCustomActionTargetStringList(sceneId, ActionIndex, "cum")
    else
        Slots = OMetadata.GetCustomActionActorStringList(sceneId, ActionIndex, "cum")
    endif
    if(Slots.Length > 0)
        StorageUtil.FormListAdd(cummingNpc, "cumOnNpcs", cumTarget, false)
        int j = 0
        while(j < Slots.Length)
            string slot = Slots[j]
            if(slot == "rectum" || slot == "mouth" || slot == "throat" || slot == "vagina")
                StorageUtil.StringListAdd(cumTarget, npcName+"_cumOnPlacesIn", slot, false)
            else
                StorageUtil.StringListAdd(cumTarget, npcName+"_cumOnPlacesOn", slot, false)
            endif
            j += 1
        endwhile
    endif
EndFunction

bool Function ShowConfirmMessage(string msg, string type) global
    if(!TTON_JData.GetMcmCheckbox(type, 1))
        return true
    endif
    string result = SkyMessage.Show(msg, "Yes", "No")
    if(result != "Yes" && result != "No")
        Debug.Notification("Dynamic Message Box Not Found (Please install Papyrus MessageBox - SKSE NG plugin)")
        return true
    endif

    return result == "Yes"
EndFunction

bool Function RequestSexComment(string msg, Actor[] actors = none, Actor speaker = none, string type = "", bool continueNarration = true) global
    if(!speaker)
        if(!actors || actors.Length == 0)
            TTON_Debug.warn("RequestSexComment: No actors provided to select a speaker from.")
            return false
        endif
        speaker = GetWeightedRandomActorToSpeak(actors)
    endif

    ; Check distance from player if distance limit is set
    string visibility = GetVisibility(TTON_JData.GetPlayer(), speaker)
    if(visibility == "none")
        ; since player can't see or hear the speaker, no comment or log event of OStim scene
        TTON_Debug.info("RequestSexComment: Player cannot hear or see the speaker.")
        return false
    endif

    Actor player = TTON_JData.GetPlayer()
    bool cooldownFree = TTON_JData.CanMakeLastComment()
    bool canMakeComment = cooldownFree && speaker

    int ThreadID = OActor.GetSceneId(speaker)

    TTON_Debug.info("RequestSexComment: canMakeComment=" + canMakeComment + ", visibility=" + visibility)

    if(canMakeComment)
        TTON_JData.SetLastCommentTime()
        SkyrimNetApi.DirectNarration(msg, speaker)
        if(continueNarration)
            int chance = Utility.RandomInt(1, 100)
            if(chance <= TTON_JData.GetMcmContinueNarrationChance())
                Utility.Wait(2)
                SkyrimNetApi.TriggerContinueNarration()
            endif
        endif
    elseif(visibility == "canSeeAndHear")
        if(type == "scene_change" && !TTON_JData.CanTrackSceneChange(ThreadID))
            return false
        endif
        TTON_JData.SetSceneChangeTime(ThreadID)
        SkyrimNetApi.RegisterEvent("tton_event", msg, speaker, none)
    endif
    return false
EndFunction

; if it's player only scene it will return none
; assume that actors are only actors from scene
actor Function GetWeightedRandomActorToSpeak(actor[] actors, form[] actorForms = none) global
    Actor player = TTON_JData.GetPlayer()
    actor[] maleActors = PapyrusUtil.ActorArray(0)
    actor[] femaleActors = PapyrusUtil.ActorArray(0)

    if !actors || actors.Length == 0
        return none
    endif

    int ArrLength

    if(actors && actors.Length > 0)
        ArrLength = actors.Length
    elseif(actorForms && actorForms.Length > 0)
        ArrLength = actorForms.Length
    endif

    int count = 0
    while count < ArrLength
        actor currActor
        if(actors && actors.Length > 0)
            currActor = actors[count]
        elseif(actorForms && actorForms.Length > 0)
            currActor = actorForms[count] as Actor
        endif
        int currSex = GetGender(currActor)
        bool isMuted = false

        if(OActor.IsMuted(currActor))
            isMuted = true
        endif
        if(currActor == player || isMuted)
        ; player doesn't participate in llm talking :)
        ; some actions in ostim prevents actors from talking, which makes sense
        elseif(currSex == 0)
            maleActors = PapyrusUtil.PushActor(maleActors, currActor)
        else
            femaleActors = PapyrusUtil.PushActor(femaleActors, currActor)
        endif

        count += 1
    endwhile

    ; if no female npc, just pick random male. Can be none though if it's player only scene
    if(femaleActors.length == 0 && maleActors.Length > 0)
        return getRandomActor(maleActors)
    endif

    ; if no male npc, just pick random female. Can be none though if it's player only scene
    if(maleActors.length == 0 && femaleActors.Length > 0)
        return getRandomActor(femaleActors)
    endif

    ; pick weighted type of npc who will talk
    bool isFemale = Utility.RandomInt(1, 100) <= TTON_JData.GetMcmCommentsGenderWeight()

    if(isFemale && femaleActors.Length > 0)
        return getRandomActor(femaleActors)
    elseif(!isFemale && maleActors.Length > 0)
        return getRandomActor(maleActors)
    endif

    return none
EndFunction

int Function GetGender(Actor akActor) global
  if !akActor
    return -1 ; Invalid actor
  endif
  return akActor.GetActorBase().GetSex()
endfunction

actor function getRandomActor(actor[] actors) global
  int index = PO3_SKSEFunctions.GenerateRandomInt(0, actors.length - 1)

  return actors[index]
endfunction

Actor[] Function GetAllActorsAfterJoin(int ThreadID, Actor[] newActors) global
    actor[] originalActors = OThread.GetActors(ThreadID)
    actor[] currentActors = originalActors
    int totalActors = currentActors.Length + newActors.Length
    Actor[] eligibleActors = PapyrusUtil.ActorArray(0)

    if(totalActors > 5)
        TTON_Debug.warn("AddActorsToActiveThread: New set of actors exceed 5 actors in one scene. New actors length: " + totalActors)
        return originalActors
    endif

    int i = 0
    int addedActors = 0
    while(i < newActors.Length)
        if(newActors[i] && PapyrusUtil.CountActor(currentActors, newActors[i]) == 0 && IsActorEligibleToJoin(newActors[i]))
            currentActors = PapyrusUtil.PushActor(currentActors, newActors[i])
        endif
        i += 1
    endwhile

    return currentActors
EndFunction

bool Function IsActorEligibleToJoin(Actor akActor) global
    bool isConsidering = StorageUtil.GetIntValue(akActor, "SexInviteConsidering") == 1
    return !OActor.IsInOStim(akActor) && OThread.GetThreadCount() > 0 && TTON_Utils.IsOStimEligible(akActor) && !isConsidering
EndFunction

Function Decline(string actionName, Actor initiator, bool playerInvited) global
    TTON_JData.SetDeclineActionCooldown(initiator, actionName)
    TTON_Events.RegisterDeclineEvent(actionName, initiator, playerInvited)
EndFunction

bool Function Ask(string type, Actor initiator, string question, bool playerInvited = false) global
    bool yes = TTON_Utils.ShowConfirmMessage(question, type)
    if(!yes)
        Decline(type, initiator, playerInvited)
    endif
    return yes
EndFunction
