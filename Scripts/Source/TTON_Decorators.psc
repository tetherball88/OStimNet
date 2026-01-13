scriptname TTON_Decorators

;==========================================================================
; Decorator Registration
;==========================================================================

; Registers all template decorators with the SkyrimNet API
; These decorators provide dynamic data for use in LLM templates
Function RegisterDecorators() global
    SkyrimNetApi.RegisterDecorator("tton_get_nearby_ostim_furniture", "TTON_Decorators", "GetNearbyFurniture")
EndFunction

string Function GetNearbyFurniture(Actor npc) global
    StorageUtil.ClearObjStringListPrefix(npc, "TTON_OStimFurniture")
    string furnitureList = ""

    ObjectReference[] availableFurniture = OFurniture.FindFurniture(2, npc, 1000.0, 100.0)
    int j = 0

    while(j < availableFurniture.Length)
        StorageUtil.StringListAdd(npc, "TTON_OStimFurniture", OFurniture.GetFurnitureType(availableFurniture[j]), false)
        j += 1
    endwhile

    string json = "{\"furniture\": \""+PapyrusUtil.StringJoin(StorageUtil.StringListToArray(npc, "TTON_OStimFurniture"))+"\"}"
    StorageUtil.ClearObjStringListPrefix(npc, "TTON_OStimFurniture")
    return json
EndFunction
