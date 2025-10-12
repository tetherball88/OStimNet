Scriptname TTON_MCM extends SKI_ConfigBase

int property oid_SettingsClearData auto
int property oid_SettingsExportData auto
int property oid_SettingsImportData auto

int property oid_EnableStartSexConfirmationModal auto
int property oid_EnableStartAffectionateConfirmationModal auto
int property oid_EnableChangePositionConfirmationModal auto
int property oid_EnableAddNewActorsConfirmationModal auto
int property oid_EnableStopSexConfirmationModal auto

int property oid_SexCommentsFrequency auto
int property oid_SexCommentsGenderWeight auto

int property oid_DeniesCooldown auto
int property oid_AffectionSceneDuration auto

string selectedPage

Event OnConfigInit()
    ModName = "OStimNet"
    Pages = new string[1]

    Pages[0] = "Settings"
EndEvent

Event OnPageReset(string page)
    if(page == "Settings")
        RenderPage()
    endif
EndEvent

Function RenderPage()
    SetCursorFillMode(TOP_TO_BOTTOM)
    RenderLeftColumn()
    SetCursorPosition(1)
    RenderRightColumn()
EndFunction

Function RenderLeftColumn()
    AddHeaderOption("Confirmation messages: ")
    oid_EnableStartSexConfirmationModal = AddToggleOption("Enable start sex confirmation modal:", TTON_JData.GetMcmCheckbox("confirmStartSex"))
    oid_EnableStartAffectionateConfirmationModal = AddToggleOption("Enable start affectionate confirmation modal:", TTON_JData.GetMcmCheckbox("confirmStartAffection"))
    oid_EnableChangePositionConfirmationModal = AddToggleOption("Enable change scene confirmation modal:", TTON_JData.GetMcmCheckbox("confirmChangeScene"))
    oid_EnableAddNewActorsConfirmationModal = AddToggleOption("Add another actors confirmation modal:", TTON_JData.GetMcmCheckbox("confirmAddActors"))
    oid_EnableStopSexConfirmationModal = AddToggleOption("Enable stop sex confirmation modal:", TTON_JData.GetMcmCheckbox("confirmStopSex"))

    AddHeaderOption("Sex comments: ")
    float frequency = TTON_JData.GetMcmCommentsFrequency() as float
    float genderWeight = TTON_JData.GetMcmCommentsGenderWeight() as float

    oid_SexCommentsFrequency = AddSliderOption("Cooldown between triggering sex comments:", frequency)
    oid_SexCommentsGenderWeight = AddSliderOption("Which gender has more chances to start comment:", genderWeight)

    AddHeaderOption("Affection scenes: ")
    float affectionDuration = TTON_JData.GetMcmAffectionDuration() as float
    oid_AffectionSceneDuration = AddSliderOption("Duration of affection scene:", affectionDuration)

    AddHeaderOption("Denies: ")
    float denyCooldown = TTON_JData.GetMcmDenyCooldown() as float
    oid_DeniesCooldown = AddSliderOption("Action cooldown after player's deny:", denyCooldown)
EndFunction

Function RenderRightColumn()
    AddHeaderOption("JContainer export/import: ")
    oid_SettingsExportData = AddTextOption("", "Export data to file")
    oid_SettingsImportData = AddTextOption("", "Import data from file")
    oid_SettingsClearData = AddTextOption("", "Clear whole data")
EndFunction

; Select
event OnOptionSelect(int option)
    if (oid_SettingsClearData == option)
        bool yes = ShowMessage("Are you sure you want to clear all data?")
        if(yes)
            TTON_JData.Clear()
        endif
    elseif (oid_SettingsExportData == option)
        TTON_JData.ExportData()
    elseif (oid_SettingsImportData == option)
        TTON_JData.ImportData()
    elseif(option == oid_EnableStartSexConfirmationModal)
        SetToggleOptionValue(oid_EnableStartSexConfirmationModal, TTON_JData.ToggleMcmCheckbox("confirmStartSex"))
    elseif(option == oid_EnableStartAffectionateConfirmationModal)
        SetToggleOptionValue(oid_EnableStartAffectionateConfirmationModal, TTON_JData.ToggleMcmCheckbox("confirmStartAffection"))
    elseif(option == oid_EnableChangePositionConfirmationModal)
        SetToggleOptionValue(oid_EnableChangePositionConfirmationModal, TTON_JData.ToggleMcmCheckbox("confirmChangeScene"))
    elseif(option == oid_EnableAddNewActorsConfirmationModal)
        SetToggleOptionValue(oid_EnableAddNewActorsConfirmationModal, TTON_JData.ToggleMcmCheckbox("confirmAddActors"))
    elseif(option == oid_EnableStopSexConfirmationModal)
        SetToggleOptionValue(oid_EnableStopSexConfirmationModal, TTON_JData.ToggleMcmCheckbox("confirmStopSex"))
    endif
endevent

; Highlight
event OnOptionHighlight(int option)
    if(option == oid_SettingsClearData)
        SetInfoText("Clears whole data from save")
    elseif(option == oid_SettingsExportData)
        SetInfoText("Exports json data to file in Documents\\My Games\\Skyrim Special Edition\\JCUser\\MARAS\\store.json")
    elseif(option == oid_SettingsImportData)
        SetInfoText("Imports data from file in Documents\\My Games\\Skyrim Special Edition\\JCUser\\MARAS\\store.json")
    elseif(option == oid_EnableStartSexConfirmationModal)
        SetInfoText("Toggle confirmation modal. When OStim scene is initiated by npc on player - it will ask player's confirmation.")
    elseif(option == oid_EnableStartAffectionateConfirmationModal)
        SetInfoText("Toggle confirmation modal. When affectionate non-sexual scene is initiated by npc on player - it will ask player's confirmation.")
    elseif(option == oid_EnableChangePositionConfirmationModal)
        SetInfoText("Toggle confirmation modal. When in player's scene npc initiate change scene - it will ask player's confirmation.")
    elseif(option == oid_EnableAddNewActorsConfirmationModal)
        SetInfoText("Toggle confirmation modal. When in player's scene npc will invite somebody else, or outside npc will decide to join - it will ask player's confirmation.")
    elseif(option == oid_EnableStopSexConfirmationModal)
        SetInfoText("Toggle confirmation modal. When npc decides to stop player's OStim scene - it will ask player's confirmation. If player say no - scene will be marked as forced(not agressive though)")
    elseif(option == oid_SexCommentsFrequency)
        SetInfoText("Set in seconds interval between sex comments can be triggered during OStim events")
    elseif(option == oid_SexCommentsGenderWeight)
        SetInfoText("Set chances of specific gender to make comments during OStim scenes. 0 - always male, 100 - always female, 50 - 50%/50% that male/female will be selected. Works only if scene has more than one gender.")
    elseif(option == oid_AffectionSceneDuration)
        SetInfoText("Set duration in seconds for affectionate non-sexual scenes. Scene stops automatically when time expires.")
    elseif(option == oid_DeniesCooldown)
        SetInfoText("Set cooldown in seconds for actions which were rejected by player before they become available again. Works individually for each asking npc.")
    endif
endevent

event OnOptionSliderOpen(int a_option)
    float startValue
    float defaultValue
    float startRange
    float endRange
    float interval
    if(a_option == oid_SexCommentsFrequency)
        startValue = TTON_JData.GetMcmCommentsFrequency() as float
        defaultValue = 40.0
        startRange = 0
        endRange = 400
        interval = 1
    elseif(a_option == oid_SexCommentsGenderWeight)
        startValue = TTON_JData.GetMcmCommentsGenderWeight() as float
        defaultValue = 50.0
        startRange = 0
        endRange = 100
        interval = 1
    elseif(a_option == oid_AffectionSceneDuration)
        startValue = TTON_JData.GetMcmAffectionDuration() as float
        defaultValue = 20.0
        startRange = 5
        endRange = 120
        interval = 5
    elseif(a_option == oid_DeniesCooldown)
        startValue = TTON_JData.GetMcmDenyCooldown() as float
        defaultValue = 20.0
        startRange = 0
        endRange = 120
        interval = 1
    endif

    SetSliderDialogStartValue(startValue)
    SetSliderDialogDefaultValue(defaultValue)
    SetSliderDialogRange(startRange, endRange)
    SetSliderDialogInterval(interval)
endEvent

event OnOptionSliderAccept(int a_option, float a_value)
    SetSliderOptionValue(a_option, a_value)
    if(a_option == oid_SexCommentsFrequency)
        TTON_JData.SetMcmCommentsFrequency(a_value as int)
    elseif(a_option == oid_SexCommentsGenderWeight)
        TTON_JData.SetMcmCommentsGenderWeight(a_value as int)
    elseif(a_option == oid_AffectionSceneDuration)
        TTON_JData.SetMcmAffectionDuration(a_value as int)
    elseif(a_option == oid_DeniesCooldown)
        TTON_JData.SetMcmDenyCooldown(a_value as int)
    endif
endEvent

event OnOptionDefault(int a_option)
	if(a_option == oid_SexCommentsFrequency)
        SetSliderDialogStartValue(40)
        TTON_JData.SetMcmCommentsFrequency(40)
    elseif(a_option == oid_SexCommentsGenderWeight)
        SetSliderDialogStartValue(50)
        TTON_JData.SetMcmCommentsGenderWeight(50)
    elseif(a_option == oid_AffectionSceneDuration)
        SetSliderDialogStartValue(30)
        TTON_JData.SetMcmAffectionDuration(30)
    elseif(a_option == oid_DeniesCooldown)
        SetSliderDialogStartValue(20)
        TTON_JData.SetMcmDenyCooldown(20)
    endif
endEvent
