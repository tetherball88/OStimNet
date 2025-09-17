Scriptname TTON_PlayerRef extends ReferenceAlias

Event OnPlayerLoadGame()
    TTON_MainController mainController = self.GetOwningQuest() as TTON_MainController
    mainController.Maintenance()
EndEvent

