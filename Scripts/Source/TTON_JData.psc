scriptname TTON_JData

;/ ==============================
   SECTION: Single Properties
============================== /;
;/
  Returns the player Actor form.
/;
Actor Function GetPlayer() global
    return Game.GetPlayer()
EndFunction

Keyword Function GetSpectatorMarkerKeyword() global
    return Game.GetFormFromFile(0x6, "TT_OstimNet.esp") as Keyword
EndFunction

Keyword Function GetParticipantFurnitureKeyword() global
    return Game.GetFormFromFile(0x17, "TT_OstimNet.esp") as Keyword
EndFunction

Package Function GetParticipantFollowPackage() global
    return Game.GetFormFromFile(0x16, "TT_OstimNet.esp") as Package
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

Faction Function GetOStimPendingFaction() global
    return Game.GetFormFromFile(0x18, "TT_OstimNet.esp") as Faction
EndFunction

