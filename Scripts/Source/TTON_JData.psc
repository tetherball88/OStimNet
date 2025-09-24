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
EndFunction

Function ImportAnimationsDescriptions() global
    JDB_solveObjSetter(GetStaticKey() + ".animationsDescriptions", JValue_readFromFile("Data/SKSE/Plugins/OStimNet/animationsDescriptions/ostimDescriptions.json"), true)
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
