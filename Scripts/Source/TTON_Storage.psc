scriptname TTON_Storage

Function ClearOnLoad() global
    StorageUtil.ClearAllPrefix("TTONDec_")

    ; clear possible values from deny actions functionality, which was moved to spell based approach
    StorageUtil.ClearAllPrefix("deny.StartCareScene")
    StorageUtil.ClearAllPrefix("deny.StartNewSex")
    StorageUtil.ClearAllPrefix("deny.ChangeSexScene")
    StorageUtil.ClearAllPrefix("deny.InviteToYourSex")
    StorageUtil.ClearAllPrefix("deny.JoinOngoingSex")
    StorageUtil.ClearAllPrefix("deny.ChangeSexPace")
    StorageUtil.ClearAllPrefix("deny.StopSex")

    ; clear storage related to bed furniture, which is no longer used in favor of a more generic furniture approach
    StorageUtil.ClearAllPrefix("TTON_FurnitureList")
    StorageUtil.ClearAllPrefix("TTON_FurnitureActor")
    StorageUtil.ClearAllPrefix("TTON_WalkToFurniture")
EndFunction
