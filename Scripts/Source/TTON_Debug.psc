scriptname TTON_Debug

Function log(string msg) global
    MiscUtil.PrintConsole(msg)
EndFunction

Function trace(string msg) global
    log("[OStimNet(trace)]: " + msg)
EndFunction

Function debug(string msg) global
    log("[OStimNet(debug)]: " + msg)
EndFunction

Function info(string msg) global
    log("[OStimNet(info)]: " + msg)
EndFunction

Function warn(string msg) global
    log("[OStimNet(warn)]: " + msg)
EndFunction

Function err(string msg) global
    log("[OStimNet(err)]: " + msg)
EndFunction