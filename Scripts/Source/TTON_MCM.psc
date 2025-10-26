Scriptname TTON_MCM extends SKI_ConfigBase

int property oid_SettingsClearData auto
int property oid_SettingsExportData auto
int property oid_SettingsImportData auto

int property oid_EnableStartSexConfirmationModal auto
int property oid_EnableStartAffectionateConfirmationModal auto
int property oid_EnableChangePositionConfirmationModal auto
int property oid_EnableAddNewActorsConfirmationModal auto
int property oid_EnableStopSexConfirmationModal auto
int property oid_AllowPlayerFurnitureSelection auto

int property oid_SexCommentsFrequency auto
int property oid_SexCommentsGenderWeight auto
int property oid_SexCommentsDistance auto

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
    AddHeaderOption("Player Consent & Control")
    oid_EnableStartSexConfirmationModal = AddToggleOption("Confirm before sex scenes:", TTON_JData.GetConfirmStartSexScenes())
    oid_EnableStartAffectionateConfirmationModal = AddToggleOption("Confirm before affection scenes:", TTON_JData.GetConfirmStartAffectionScenes())
    oid_EnableStopSexConfirmationModal = AddToggleOption("Confirm before stopping scenes:", TTON_JData.GetConfirmStopSexScenes())
    oid_AllowPlayerFurnitureSelection = AddToggleOption("Allow manual bed selection:", TTON_JData.GetAllowPlayerFurnitureSelection())

    AddHeaderOption("Scene Management")
    oid_EnableChangePositionConfirmationModal = AddToggleOption("Confirm scene position changes:", TTON_JData.GetConfirmChangeScenePosition())
    oid_EnableAddNewActorsConfirmationModal = AddToggleOption("Confirm adding new actors:", TTON_JData.GetConfirmAddNewActors())

    float affectionDuration = TTON_JData.GetMcmAffectionDuration() as float
    oid_AffectionSceneDuration = AddSliderOption("Affection scene duration (seconds):", affectionDuration)
EndFunction

Function RenderRightColumn()
    AddHeaderOption("Immersion & Feedback")
    float frequency = TTON_JData.GetMcmCommentsFrequency() as float
    float genderWeight = TTON_JData.GetMcmCommentsGenderWeight() as float
    float distance = TTON_JData.GetMcmCommentsDistance() as float

    oid_SexCommentsFrequency = AddSliderOption("Comment cooldown (seconds):", frequency)
    oid_SexCommentsGenderWeight = AddSliderOption("Gender preference (0=Male, 100=Female):", genderWeight)
    oid_SexCommentsDistance = AddSliderOption("Comment hearing range (units):", distance)

    AddHeaderOption("Behavior & Timing")
    float denyCooldown = TTON_JData.GetMcmDenyCooldown() as float
    oid_DeniesCooldown = AddSliderOption("Deny action cooldown (seconds):", denyCooldown)

    AddHeaderOption("Data Management")
    oid_SettingsExportData = AddTextOption("", "Export data to file")
    oid_SettingsImportData = AddTextOption("", "Import data from file")
    oid_SettingsClearData = AddTextOption("", "Clear all data")
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
        SetToggleOptionValue(oid_EnableStartSexConfirmationModal, TTON_JData.ToggleMcmCheckbox("confirmStartSex", 1))
    elseif(option == oid_EnableStartAffectionateConfirmationModal)
        SetToggleOptionValue(oid_EnableStartAffectionateConfirmationModal, TTON_JData.ToggleMcmCheckbox("confirmStartAffection", 1))
    elseif(option == oid_EnableChangePositionConfirmationModal)
        SetToggleOptionValue(oid_EnableChangePositionConfirmationModal, TTON_JData.ToggleMcmCheckbox("confirmChangeScene", 1))
    elseif(option == oid_EnableAddNewActorsConfirmationModal)
        SetToggleOptionValue(oid_EnableAddNewActorsConfirmationModal, TTON_JData.ToggleMcmCheckbox("confirmAddActors", 1))
    elseif(option == oid_EnableStopSexConfirmationModal)
        SetToggleOptionValue(oid_EnableStopSexConfirmationModal, TTON_JData.ToggleMcmCheckbox("confirmStopSex", 1))
    elseif(option == oid_AllowPlayerFurnitureSelection)
        SetToggleOptionValue(oid_AllowPlayerFurnitureSelection, TTON_JData.ToggleMcmCheckbox("allowPlayerFurnitureSelection", 0))
    endif
endevent

; Highlight
event OnOptionHighlight(int option)
    if(option == oid_SettingsClearData)
        SetInfoText("Permanently removes all OStimNet data from your save file.")
    elseif(option == oid_SettingsExportData)
        SetInfoText("Saves your settings to: Documents\\My Games\\Skyrim Special Edition\\JCUser\\MARAS\\store.json")
    elseif(option == oid_SettingsImportData)
        SetInfoText("Loads settings from: Documents\\My Games\\Skyrim Special Edition\\JCUser\\MARAS\\store.json")
    elseif(option == oid_EnableStartSexConfirmationModal)
        SetInfoText("Ask for permission when NPCs want to start intimate scenes with you.")
    elseif(option == oid_EnableStartAffectionateConfirmationModal)
        SetInfoText("Ask for permission when NPCs want to start non-sexual affectionate scenes with you.")
    elseif(option == oid_EnableChangePositionConfirmationModal)
        SetInfoText("Ask for permission when NPCs want to change positions during your scene.")
    elseif(option == oid_EnableAddNewActorsConfirmationModal)
        SetInfoText("Ask for permission when NPCs want to join your scene or invite others.")
    elseif(option == oid_EnableStopSexConfirmationModal)
        SetInfoText("Ask for permission when NPCs want to end your scene. Refusing marks the scene as forced.")
    elseif(option == oid_AllowPlayerFurnitureSelection)
        SetInfoText("If no furniture selected by LLM, let you manually choose to use bed or not.")
    elseif(option == oid_SexCommentsFrequency)
        SetInfoText("Minimum time between NPC comments during intimate scenes.")
    elseif(option == oid_SexCommentsGenderWeight)
        SetInfoText("Controls which gender is more likely to speak. 50 = equal chance for both genders.")
    elseif(option == oid_SexCommentsDistance)
        SetInfoText("Maximum distance for NPCs to make comments. 0 = unlimited. Line of sight overrides distance.")
    elseif(option == oid_AffectionSceneDuration)
        SetInfoText("How long affectionate scenes last before automatically ending.")
    elseif(option == oid_DeniesCooldown)
        SetInfoText("How long each NPC waits before asking again after you refuse their request.")
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
        defaultValue = 20.0
        startRange = 0
        endRange = 400
        interval = 5
    elseif(a_option == oid_SexCommentsGenderWeight)
        startValue = TTON_JData.GetMcmCommentsGenderWeight() as float
        defaultValue = 50.0
        startRange = 0
        endRange = 100
        interval = 1
    elseif(a_option == oid_SexCommentsDistance)
        startValue = TTON_JData.GetMcmCommentsDistance() as float
        defaultValue = 768.0
        startRange = 0
        endRange = 10240
        interval = 64
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
    elseif(a_option == oid_SexCommentsDistance)
        TTON_JData.SetMcmCommentsDistance(a_value as int)
    elseif(a_option == oid_AffectionSceneDuration)
        TTON_JData.SetMcmAffectionDuration(a_value as int)
    elseif(a_option == oid_DeniesCooldown)
        TTON_JData.SetMcmDenyCooldown(a_value as int)
    endif
endEvent

event OnOptionDefault(int a_option)
	if(a_option == oid_SexCommentsFrequency)
        SetSliderDialogStartValue(20)
        TTON_JData.SetMcmCommentsFrequency(20)
    elseif(a_option == oid_SexCommentsGenderWeight)
        SetSliderDialogStartValue(50)
        TTON_JData.SetMcmCommentsGenderWeight(50)
    elseif(a_option == oid_SexCommentsDistance)
        SetSliderDialogStartValue(2048)
        TTON_JData.SetMcmCommentsDistance(2048)
    elseif(a_option == oid_AffectionSceneDuration)
        SetSliderDialogStartValue(30)
        TTON_JData.SetMcmAffectionDuration(30)
    elseif(a_option == oid_DeniesCooldown)
        SetSliderDialogStartValue(20)
        TTON_JData.SetMcmDenyCooldown(20)
    endif
endEvent
