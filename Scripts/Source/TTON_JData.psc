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

;/
  Returns the key for initial data in JContainers.
/;
string Function GetInitialDataKey() global
    return GetStaticKey() + ".initialData"
EndFunction

;/ ==============================
   SECTION: Object Top level
============================== /;
;/
  Clears all MARAS data from JContainers.
/;
Function Clear() global
    JDB_solveObjSetter(GetNamespaceKey(), JMap_object())
    ImportStaticData()
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

;/
  Imports static data (initialData.json) into JContainers.
/;
Function ImportStaticData() global
    int JRoot = JDB_solveObj(GetNamespaceKey())
    if(JRoot == 0)
        JDB_solveObjSetter(GetNamespaceKey(), JMap_object())
    endif
    JDB_solveObjSetter(GetStaticKey() + ".initialData", JValue_readFromFile("Data/SKSE/Plugins/OStimNet/initialData.json"), true)
    ImportAnimationsDescriptions()
    ClearTmpData()
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
    return JDB_solveForm(GetInitialDataKey() + ".player") as Actor
EndFunction

int Function GetActions() global
    return JDB_solveObj(GetInitialDataKey() + ".actions")
EndFunction

string Function NextAction(string previousKey = "") global
    return JMap_nextKey(GetActions(), previousKey)
EndFunction

string Function GetActionName(string actionName) global
    return JDB_solveStr(GetInitialDataKey() + ".actions." + actionName + ".name")
EndFunction

string Function GetActionScriptFileName(string actionName) global
    return JDB_solveStr(GetInitialDataKey() + ".actions." + actionName + ".scriptFileName")
EndFunction

string Function GetActionIsEligible(string actionName) global
    return JDB_solveStr(GetInitialDataKey() + ".actions." + actionName + ".isEligible")
EndFunction

string Function GetActionExecute(string actionName) global
    return JDB_solveStr(GetInitialDataKey() + ".actions." + actionName + ".execute")
EndFunction

bool Function GetActionAddTags(string actionName) global
    return JDB_solveInt(GetInitialDataKey() + ".actions." + actionName + ".addTags") == 1
EndFunction

string Function GetActionDescription(string actionName) global
    return JDB_solveStr(GetInitialDataKey() + ".actions." + actionName + ".description")
EndFunction

string Function GetActionParameters(string actionName) global
    return JDB_solveStr(GetInitialDataKey() + ".actions." + actionName + ".parameters")
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

; if not set - default true
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

Function SetMcmCommentsDistance(int value) global
    SetMcmInt("sexCommentsDistance", value)
EndFunction

int Function GetMcmCommentsDistance() global
    int val = GetMcmInt("sexCommentsDistance")
    if(val == -1)
        val = 768
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

bool Function GetConfirmChangeScenePosition() global
    return GetMcmCheckbox("confirmChangeScene", 1)
EndFunction

bool Function GetConfirmAddNewActors() global
    return GetMcmCheckbox("confirmAddActors", 1)
EndFunction

Function SetMcmContinueNarrationChance(int value) global
    SetMcmInt("continueNarrationChance", value)
EndFunction

int Function GetMcmContinueNarrationChance() global
    int val = GetMcmInt("continueNarrationChance")
    if(val == -1)
        val = 50
    endif

    return val
EndFunction

;/ ==============================
   SECTION: Temporary data which cleans on each load
============================== /;

Function ClearTmpData() global
    int JTmp = JMap_object()
    JMap_setObj(JTmp, "threads", JMap_object())
    JMap_setObj(JTmp, "denies", JFormMap_object())
    JDB_solveObjSetter(GetNamespaceKey() + ".tmp", JTmp, true)
EndFunction

;/ ==============================
   SECTION: Threads manager
============================== /;

Function SetThreadInt(int ThreadID, string propName, int value = 1) global
    JDB_solveIntSetter(GetNamespaceKey() + ".tmp.threads." + ThreadID + "." + propName, value, true)
EndFunction

bool Function GetThreadBool(int ThreadID, string propName) global
    return JDB_solveInt(GetNamespaceKey() + ".tmp.threads." + ThreadID + "." + propName) == 1
EndFunction

int Function GetThreadInt(int ThreadID, string propName, int DefaultValue = 0) global
    return JDB_solveInt(GetNamespaceKey() + ".tmp.threads." + ThreadID + "." + propName, DefaultValue)
EndFunction

Function SetThreadForced(int ThreadID, int value = 1) global
    SetThreadInt(ThreadID, "forced", value)
EndFunction

bool Function IsThreadForced(int ThreadID) global
    return GetThreadBool(ThreadID, "forced")
EndFunction

Function SetThreadAddNewActors(int ThreadID, int value = 1) global
    SetThreadInt(ThreadID, "addingNewActors", value)
EndFunction

bool Function GetThreadAddNewActors(int ThreadID) global
    return GetThreadBool(ThreadID, "addingNewActors")
EndFunction

Function SetThreadContinuationFrom(int OldThreadID, int NewThreadID) global
    SetThreadInt(NewThreadID, "continuationFrom", OldThreadID)
EndFunction

int Function GetThreadContinuationFrom(int ThreadID) global
    return GetThreadInt(ThreadID, "continuationFrom", -1)
EndFunction

Function SetThreadAffectionOnly(int ThreadID, int val = 1) global
    SetThreadInt(ThreadID, "affectionOnly", val)
EndFunction

bool Function GetThreadAffectionOnly(int ThreadID) global
    return GetThreadInt(ThreadID, "affectionOnly") == 1
EndFunction

;/ ==============================
   SECTION: Sex comments
============================== /;

Function SetTimer(string propertyName) global
    JDB_solveFltSetter(GetNamespaceKey() + ".tmp" + propertyName, Utility.GetCurrentRealTime(), true)
EndFunction

float Function GetTimer(string propertyName) global
    return JDB_solveFlt(GetNamespaceKey() + ".tmp" + propertyName)
EndFunction

bool Function HasCooldownPassed(string propertyName, float cooldown) global
    float now = Utility.GetCurrentRealTime()
    float diff = now - GetTimer(propertyName)

    return cooldown <= diff
EndFunction

Function SetLastCommentTime() global
    SetTimer(".sexComment")
EndFunction

float Function GetLastCommentTime() global
    return GetTimer(".sexComment")
EndFunction

bool Function CanMakeLastComment() global
    return HasCooldownPassed(".sexComment", TTON_JData.GetMcmCommentsFrequency())
EndFunction

Function SetDeclineActionCooldown(Actor akActor, string actionName) global
    StorageUtil.SetFloatValue(akActor, "deny." + actionName, Utility.GetCurrentRealTime())
EndFunction

float Function GetDeclineActionCooldown(Actor akActor, string actionName) global
    return StorageUtil.GetFloatValue(akActor, "deny." + actionName)
EndFunction

bool Function CanUseActionAfterDecline(Actor akActor, string actionName) global
    float lastDeclineTime = GetDeclineActionCooldown(akActor, actionName)
    if(lastDeclineTime == 0.0)
        return true
    endif

    float now = Utility.GetCurrentRealTime()
    float diff = now - lastDeclineTime

    return TTON_JData.GetMcmDenyCooldown() <= diff
EndFunction
