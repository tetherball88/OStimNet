scriptname TTON_Events

Function RegisterEvents() global
    RegisterEventSchemaSex()
EndFunction

Function RegisterEventSchemaSex() global
    
    ; int res = SkyrimnetApi.RegisterEventSchema("tton_sex", \
    ; "Sex", "Sex related events", \
    ; "[{\"name\": \"description\", \"type\": 0, \"required\": true, \"description\": \"Sex scene description.\"}]", \
    ; "{\"recent_events\":\"{{description}}\",\"raw\":\"{{description}}\",\"compact\":\"{{description}}\",\"verbose\":\"{{description}}\"}", false, 0)

    ; TTON_Debug.trace("RegisterEventSchemSex:"+res)
EndFunction

Function RegisterSexEvent(string content) global
    ; int res = SkyrimNetApi.RegisterEvent("tton_sex", "{\"description\": \""+content+"\"}", none, none)
    ; TTON_Debug.trace("RegisterSexEvent:"+res)
    ; SkyrimNetApi.RegisterEvent("custom", content, Game.GetPlayer(), none)
EndFunction
