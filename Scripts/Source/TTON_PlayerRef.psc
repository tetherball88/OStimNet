Scriptname TTON_PlayerRef extends ReferenceAlias

Event OnPlayerLoadGame()
    TTON_MainController mainController = self.GetOwningQuest() as TTON_MainController
    mainController.Maintenance()
EndEvent

Event OnLocationChange(Location akOldLoc, Location akNewLoc)
    if(TTON_JData.GetGmScanOnLocationChange())
        Utility.Wait(2.0) ; wait a moment for the player to fully load into the new location
        TTON_GameMaster gm = self.GetOwningQuest() as TTON_GameMaster
        gm.EvaluateNearbyNpcs()
    endif
EndEvent
