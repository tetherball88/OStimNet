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
    string[] sceneTags = OMetadata.GetSceneTags(sceneId)
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

; Starts a new OStim scene with the given actors and tags
; @param actors Array of actors to include in the scene
; @param tags Optional scene tags or actions to filter by
; @returns Thread ID of the new scene, or -1 if failed
int function StartOstim(actor[] actors, string tags = "") global
    actors = OActorUtil.Sort(actors, OActorUtil.EmptyArray())
    bool someActorsBusy = false
    int i = 0
    while(i < actors.Length)
        if(OActor.IsInOStim(actors[i]))
            someActorsBusy = true
        endif
        i += 1
    endwhile
    if(someActorsBusy)
        TTON_Debug.warn("StartOstim: tried to start new thread with actors who are busy in another thread.")
        return -1
    endif
    int builderID = OThreadBuilder.create(actors)
    string newScene = getSceneByActionsOrTags(actors, tags, true)

    if(!UserHasScenesForActors(actors))
        TTON_Debug.warn("StartOstim: can't start new thread, user doesn't have suitable scene for this set of actors")
    endif
    OThreadBuilder.SetStartingAnimation(builderID, newScene)
    int newThreadID = OThreadBuilder.Start(builderID)

    return newThreadID
endfunction

; Changes the animation speed of an ongoing OStim scene
; @param ThreadId The ID of the thread to modify
; @param speed Direction to change speed ("increase" or "decrease")
Function OStimChangeSpeed(int ThreadId, string speed) global
    int currentSpeed = OThread.GetSpeed(ThreadId)
    string sceneId = OThread.GetScene(ThreadId)
    int maxSpeed = OMetadata.GetMaxSpeed(sceneId)
    int minSpeed = OMetadata.GetDefaultSpeed(sceneId)
    if(speed == "increase" && currentSpeed < maxSpeed)
        currentSpeed += 1
    elseif(speed == "decrease" && currentSpeed > minSpeed)
        currentSpeed -= 1
    endif
    OThread.SetSpeed(ThreadId, currentSpeed)
EndFunction

; Finds an appropriate scene based on actors and specified tags
; @param actors Array of actors to find a scene for
; @param tags CSV string of action types or scene tags to search for
; @param useRandom Whether to fall back to a random scene if no matching scene is found
; @returns Scene ID if found, empty string if no suitable scene
string function getSceneByActionsOrTags(actor[] actors, string tags, bool useRandom = false) global
    string newScene
    if tags != ""
        newScene = OLibrary.GetRandomSceneWithAnyActionCSV(actors, tags)
        if(newScene == "")
        newScene = OLibrary.GetRandomSceneWithAnySceneTagCSV(actors, tags)
        endif
    else
        TTON_Debug.Debug("No OStim tags provided")
    endif

    if(newScene == "")
        if(useRandom)
        newScene = OLibrary.GetRandomScene(actors)
        else
        TTON_Debug.Debug("No OStim scene found for: " + tags)
        endif
    else
        TTON_Debug.Debug("Found " + tags + " scene: " + newScene)
    endif

    return newScene
endfunction

; Checks if there are any available scenes for the given actor combination
; @param actors Array of actors to check scenes for
; @returns True if at least one suitable scene exists
bool Function UserHasScenesForActors(Actor[] actors) global
    return OLibrary.GetRandomSceneSuperloadCSV(actors, AnyActionType = "sexual") != ""
EndFunction

; Adds new actors to an ongoing OStim scene
; @param ThreadID The ID of the thread to modify
; @param newActors Array of actors to add to the scene
function AddActorsToActiveThread(int ThreadID, actor[] newActors) global
    if(!OThread.IsRunning(ThreadID))
        return
    endif
    actor[] currentActors = OThread.GetActors(ThreadID)
    int totalActors = currentActors.Length + newActors.Length
    if totalActors <= 5
        int i = 0
        int addedActors = 0
        while(i < newActors.Length)
            currentActors = PapyrusUtil.PushActor(currentActors, newActors[i])
            i += 1
        endwhile
        if(!UserHasScenesForActors(OActorUtil.Sort(currentActors, OActorUtil.EmptyArray())))
            TTON_Debug.warn("AddActorsToActiveThread: Don't stop ongoing thread. User doesn't have suitable scene for new set of actors.")
            return
        endif
        OThread.Stop(ThreadID)
        while(OThread.isRunning(ThreadID))
            Utility.Wait(0.2)
        endwhile
        StartOstim(currentActors)
    else
        TTON_Debug.warn("AddActorsToActiveThread: New set of actors exceed 5 actors in one scene. New actors length: " + totalActors)
    endif
endfunction

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
string Function GetActorsNamesComaSeparated(Actor[] actors) global
    return PapyrusUtil.StringJoin(OActorUtil.ActorsToNames(actors), ", ")
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
        description = SkyrimNetApi.ParseString(template, "sceneData", "{\"actors\": "+actorsStrArr+"}")
    endif

    if(!description)
      description = TTON_Utils.GenerateOStimSceneDescription(sceneId, actors)
    endif

    return description
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

    string actorsStrArr = GetActorsNamesJson(actors, ThreadID, true)

    string description = GetSceneDescription(sceneId, actors)

    return "{\"isInThisScene\": "+isInThisScene+", \"sceneId\": \""+sceneId+"\", \"actors\": "+actorsStrArr+",  \"description\": \""+description+"\"}"
EndFunction


; Checks if an actor is eligible for OStim scenes
; @param npc The actor to check for eligibility
; @returns True if the actor is eligible (not a child and passes OStim verification)
bool Function IsOStimEligible(Actor npc) global
    return !OUtils.IsChild(npc) && OActor.VerifyActors(OActorUtil.ToArray(npc))
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
    string res = GetActorName(npc) + " ejaculated on/in "
    bool hadLocations = false
    ; those who can cum inside
    if OActor.HasSchlong(npc)
        string sceneId = OThread.GetScene(ThreadID)
        int actorPosition = GetActorAnimationIndex(npc, OThread.GetActorPosition(ThreadID, npc), sceneId)
        int[] Actions = OMetadata.FindActionsSuperloadCSVv2(sceneId, ActorPositions = actorPosition, AnyCustomStringListRecord = ";cum")
        int i = 0

        string locations = ""

        While (i < Actions.Length)
            String[] Slots = OMetadata.GetCustomActionTargetStringList(sceneId, Actions[i], "cum")
            Actor Target = OThread.GetActor(ThreadID, OMetadata.GetActionTarget(SceneID, Actions[i]))
            locations += GetActorName(Target) + "'s " + OCSV.ToCSVList(Slots) + ";"
            hadLocations = true
            i += 1
        EndWhile

        res += locations
    endif

    if(!hadLocations)
        return ""
    endif

    return res
EndFunction
