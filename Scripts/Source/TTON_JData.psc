scriptname TTON_JData

import TTON_JCDomain

;/
  Returns the namespace key for MARAS data in JContainers.
/;
string Function GetNamespaceKey() global
    return ".TT_OStimNet"
EndFunction

;/
  Returns the static key for MARAS static data in JContainers.
/;
string Function GetStaticKey() global
    return GetNamespaceKey() + ".static"
EndFunction

;/ ==============================
   SECTION: Object Top level
============================== /;
;/
  Clears all MARAS data from JContainers.
/;
Function Clear() global
    JDB_solveObjSetter(GetNamespaceKey(), JMap_object())
EndFunction

;/
  Exports MARAS data to a JSON file.
/;
Function ExportData() global
    JValue_writeToFile(JDB_solveObj(GetNamespaceKey()), JContainers.userDirectory() + "OStimeNet/store.json")
EndFunction

;/
  Imports MARAS data from a JSON file.
/;
Function ImportData() global
    int jObj = JValue_readFromFile(JContainers.userDirectory() + "OStimeNet/store.json")
    JDB_solveObjSetter(GetNamespaceKey(), jObj)
EndFunction


int Function LoadMultipleFiles(string folderPath, bool isFormMap = false) global
    int JTarget
    if(isFormMap)
        JTarget = JFormMap_object()
    else
        JTarget = JMap_object()
    endif
    int JFilesMap = JValue_readFromDirectory(folderPath)
    string nextKey = JMap_nextKey(JFilesMap)
    while(nextKey)
      if(isFormMap)
          JFormMap_addPairs(JTarget, JMap_getObj(JFilesMap, nextKey), true)
      else
          JMap_addPairs(JTarget, JMap_getObj(JFilesMap, nextKey), true)
      endif
          nextKey = JMap_nextKey(JFilesMap, nextKey)
    endwhile
    JValue_release(JFilesMap)
    return JTarget
EndFunction

Function ImportAnimationsDescriptions() global
    JDB_solveObjSetter(GetStaticKey() + ".animationsDescriptions", LoadMultipleFiles("Data/SKSE/Plugins/OStimNet/animationsDescriptions"), true)
EndFunction


string Function GetDescription(string sceneId) global
  return JDB_solveStr(GetStaticKey() + ".animationsDescriptions." + sceneId)
EndFunction

;/ ==============================
   SECTION: Single Properties
============================== /;
;/
  Returns the player Actor form.
/;
Actor Function GetPlayer() global
    return Game.GetPlayer()
EndFunction

;/ ==============================
   SECTION: MCM data
============================== /;

bool Function ToggleMcmCheckbox(string propName, int default = -1) global
    bool current = GetMcmCheckbox(propName, default)
    int val = -1
    if(current)
        val = 0
    else
        val = 1
    endif
    JDB_solveIntSetter(GetNamespaceKey() + ".mcm." + propName, val, true)

    return val != 0
EndFunction

; if not set - default false
bool Function GetMcmCheckbox(string propName, int default = -1) global
    bool res = JDB_solveInt(GetNamespaceKey() + ".mcm." + propName, default) != 0
    return res
EndFunction

Function SetMcmInt(string propName, int value) global
    JDB_solveIntSetter(GetNamespaceKey() + ".mcm." + propName, value, true)
EndFunction

int Function GetMcmInt(string propName) global
    return JDB_solveInt(GetNamespaceKey() + ".mcm." + propName, -1)
EndFunction

Function SetMuteHotkey(int value) global
    SetMcmInt("muteHotkey", value)
EndFunction

int Function GetMuteHotkey() global
    return GetMcmInt("muteHotkey")
EndFunction


bool Function GetMuteSetting() global
    return GetMcmCheckbox("muteSetting", 0)
EndFunction

bool Function ToggleMuteSetting() global
    return ToggleMcmCheckbox("muteSetting", 0)
EndFunction


Function SetMcmCommentsFrequency(int value) global
    SetMcmInt("sexCommentsFrequency", value)
EndFunction

int Function GetMcmCommentsFrequency() global
    int val = GetMcmInt("sexCommentsFrequency")
    if(val == -1)
        val = 20
    endif

    return val
EndFunction

Function SetMcmCommentsGenderWeight(int value) global
    SetMcmInt("sexCommentsGenderWeight", value)
EndFunction

int Function GetMcmCommentsGenderWeight() global
    int val = GetMcmInt("sexCommentsGenderWeight")
    if(val == -1)
        val = 50
    endif

    return val
EndFunction

Function SetMcmDenyCooldown(int value) global
    SetMcmInt("denyCooldown", value)
EndFunction

int Function GetMcmDenyCooldown() global
    int val = GetMcmInt("denyCooldown")
    if(val == -1)
        val = 20
    endif

    return val
EndFunction

Function SetMcmAffectionDuration(int value) global
    SetMcmInt("affectionDuration", value)
EndFunction

int Function GetMcmAffectionDuration() global
    int val = GetMcmInt("affectionDuration")
    if(val == -1)
        val = 20
    endif

    return val
EndFunction

; Gets the use OStim default start selection flag
; default is disabled (false)
bool Function GetUseOStimDefaultStartSelection() global
    return GetMcmCheckbox("useOStimDefaultStartSelection", 0)
EndFunction

bool Function GetConfirmStartSexScenes() global
    return GetMcmCheckbox("confirmStartSex", 1)
EndFunction

bool Function GetConfirmStartAffectionScenes() global
    return GetMcmCheckbox("confirmStartAffection", 1)
EndFunction

bool Function GetConfirmStopSexScenes() global
    return GetMcmCheckbox("confirmStopSex", 1)
EndFunction

bool Function GetPrioritizePlayerThreadComments() global
    return GetMcmCheckbox("prioritizePlayerThreadComments", 1)
EndFunction

bool Function GetConfirmChangeScenePosition() global
    return GetMcmCheckbox("confirmChangeScene", 1)
EndFunction

bool Function GetConfirmAddNewActors() global
    return GetMcmCheckbox("confirmAddActors", 1)
EndFunction

;/ ==============================
   SECTION: Spectator Settings
============================== /;

bool Function GetSpectatorsEnabled() global
    return GetMcmCheckbox("spectatorsEnabled", 0)
EndFunction

bool Function ToggleSpectatorsEnabled() global
    return ToggleMcmCheckbox("spectatorsEnabled", 0)
EndFunction

Function SetMcmMaxSpectatorsOverall(int value) global
    SetMcmInt("maxSpectatorsOverall", value)
EndFunction

int Function GetMcmMaxSpectatorsOverall() global
    int val = GetMcmInt("maxSpectatorsOverall")
    if(val == -1)
        val = 5
    endif
    return val
EndFunction

Function SetMcmMaxSpectatorsPerThread(int value) global
    SetMcmInt("maxSpectatorsPerThread", value)
EndFunction

int Function GetMcmMaxSpectatorsPerThread() global
    int val = GetMcmInt("maxSpectatorsPerThread")
    if(val == -1)
        val = 2
    endif
    return val
EndFunction

Function SetMcmSpectatorCommentWeight(int value) global
    SetMcmInt("spectatorCommentWeight", value)
EndFunction

int Function GetMcmSpectatorCommentWeight() global
    int val = GetMcmInt("spectatorCommentWeight")
    if(val == -1)
        val = 50
    endif
    return val
EndFunction

Function SetMcmSpectatorScanInterval(int value) global
    SetMcmInt("spectatorScanInterval", value)
EndFunction

int Function GetMcmSpectatorScanInterval() global
    int val = GetMcmInt("spectatorScanInterval")
    if(val == -1)
        val = 10
    endif
    return val
EndFunction

;/ ==============================
   SECTION: Temporary data which cleans on each load
============================== /;

Function ClearTmpData() global
    int JTmp = JMap_object()
    JMap_setObj(JTmp, "denies", JFormMap_object())
    JDB_solveObjSetter(GetNamespaceKey() + ".tmp", JTmp, true)
EndFunction

Keyword Function GetSpectatorMarkerKeyword() global
    return Game.GetFormFromFile(0x6, "TT_OstimNet.esp") as Keyword
EndFunction

Package Function GetSpectatorFollowPackage() global
    return Game.GetFormFromFile(0x7, "TT_OstimNet.esp") as Package
EndFunction

Package Function GetSpectatorFleeEditorLocPackage() global
    return Game.GetFormFromFile(0x5, "TT_OstimNet.esp") as Package
EndFunction

Package Function GetSpectatorFleeFarPackage() global
    return Game.GetFormFromFile(0x3, "TT_OstimNet.esp") as Package
EndFunction

Faction Function GetSpectatorFaction() global
    return Game.GetFormFromFile(0x8, "TT_OstimNet.esp") as Faction
EndFunction

Faction Function GetSpectatorFleeFaction() global
    return Game.GetFormFromFile(0x2, "TT_OstimNet.esp") as Faction
EndFunction

Faction Function GetPlayerMarriedFaction() global
    return Game.GetFormFromFile(0xc6472, "Skyrim.esm") as Faction
EndFunction

Spell Function GetDeclineSpell(string actionName) global
    int formId = 0
    if(actionName == "StartAffectionScene")
        formId = 0x9
    elseif(actionName == "StartNewSex")
        formId = 0x10
    elseif(actionName == "ChangeSexActivity")
        formId = 0x11
    elseif(actionName == "InviteToYourSex")
        formId = 0x12
    elseif(actionName == "JoinOngoingSex")
        formId = 0x13
    elseif(actionName == "ChangeSexPace")
        formId = 0x14
    elseif(actionName == "StopSex")
        formId = 0x15
    endif

    if(formId == 0)
        return none
    endif

    return Game.GetFormFromFile(formId, "TT_OstimNet.esp") as Spell
EndFunction

;/ ==============================
   SECTION: Sex comments
============================== /;

Function SetDeclineActionCooldown(Actor akActor, string actionName) global
    akActor.AddSpell(GetDeclineSpell(actionName))
EndFunction
