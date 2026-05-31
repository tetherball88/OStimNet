scriptname TTON_Debug

Function trace(string msg) global
    OStimNet.Log(msg, 0)
EndFunction

Function debug(string msg) global
    OStimNet.Log(msg, 1)
EndFunction

Function info(string msg) global
    OStimNet.Log(msg, 2)
EndFunction

Function warn(string msg) global
    OStimNet.Log(msg, 3)
EndFunction

Function err(string msg) global
    OStimNet.Log(msg, 4)
EndFunction
