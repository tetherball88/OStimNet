scriptname TTON_OStimOptions

Function ChangeIntentRomantic(string stateVal) global
    ChangeIntent("romantic")
EndFunction

Function ChangeIntentLustful(string stateVal) global
    ChangeIntent("lustful")
EndFunction

Function ChangeIntentTransactional(string stateVal) global
    ChangeIntent("transactional")
EndFunction

Function ChangeIntentDom(string stateVal) global
    ChangeIntent("dom")
EndFunction

Function ChangeIntentAggressive(string stateVal) global
    ChangeIntent("aggressive")
EndFunction

Function ChangeIntent(string intent) global
    OStimNet.ShowThreadRolesModal(0, intent)
EndFunction

Function ChangeActorsRoles(string stateVal) global
    OStimNet.ShowThreadRolesModal(0, "")
EndFunction
