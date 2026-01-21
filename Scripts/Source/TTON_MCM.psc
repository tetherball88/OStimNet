Scriptname TTON_MCM extends SKI_ConfigBase

int property oid_SettingsClearData auto
int property oid_SettingsExportData auto
int property oid_SettingsImportData auto

int property oid_EnableStartSexConfirmationModal auto
int property oid_EnableStartAffectionateConfirmationModal auto
int property oid_EnableChangePositionConfirmationModal auto
int property oid_EnableAddNewActorsConfirmationModal auto
int property oid_EnableStopSexConfirmationModal auto
int property oid_UseDefaultOStimSceneStart auto
int property oid_PrioritizePlayerThreadComments auto

int property oid_SexCommentsGenderWeight auto

int property oid_DeniesCooldown auto
int property oid_AffectionSceneDuration auto

int property oid_SpectatorsEnabled auto
int property oid_MaxSpectatorsOverall auto
int property oid_MaxSpectatorsPerThread auto
int property oid_SpectatorCommentWeight auto
int property oid_SpectatorScanInterval auto
int property oid_ClearSpectatorsData auto
int property oid_StopSpectatorScan auto

int property oid_Mute auto
int property oid_MuteHotkey auto

string selectedPage


Event OnConfigInit()
    ModName = "OStimNet"
    Pages = new string[1]

    Pages[0] = "Settings"
    RegisterMuteHotkey()
EndEvent

Event OnConfigOpen()
    RegisterMuteHotkey()
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
    if(Game.GetModByName("SkyrimNet_Sexlab.esp") != 255)
        AddHeaderOption("NOTE: SkyrimNet_Sexlab detected.")
        AddHeaderOption("Its framework setting controls which mod starts scenes.")
    endif
    AddHeaderOption("Player Consent & Control")
    oid_EnableStartSexConfirmationModal = AddToggleOption("Confirm before sex scenes:", TTON_JData.GetConfirmStartSexScenes())
    oid_EnableStartAffectionateConfirmationModal = AddToggleOption("Confirm before affection scenes:", TTON_JData.GetConfirmStartAffectionScenes())
    oid_EnableStopSexConfirmationModal = AddToggleOption("Confirm before stopping scenes:", TTON_JData.GetConfirmStopSexScenes())
    oid_UseDefaultOStimSceneStart = AddToggleOption("Use default OStim start scene:", TTON_JData.GetUseOStimDefaultStartSelection())

    AddHeaderOption("Scene Management")
    oid_EnableChangePositionConfirmationModal = AddToggleOption("Confirm scene position changes:", TTON_JData.GetConfirmChangeScenePosition())
    oid_EnableAddNewActorsConfirmationModal = AddToggleOption("Confirm adding new actors:", TTON_JData.GetConfirmAddNewActors())

    AddHeaderOption("Spectators")
    oid_SpectatorsEnabled = AddToggleOption("Enable spectators:", TTON_JData.GetSpectatorsEnabled())
    if(TTON_JData.GetSpectatorsEnabled())
        float maxOverall = TTON_JData.GetMcmMaxSpectatorsOverall() as float
        oid_MaxSpectatorsOverall = AddSliderOption("Max spectators overall:", maxOverall)
        float maxPerThread = TTON_JData.GetMcmMaxSpectatorsPerThread() as float
        oid_MaxSpectatorsPerThread = AddSliderOption("Max spectators per thread:", maxPerThread)
        float spectatorWeight = TTON_JData.GetMcmSpectatorCommentWeight() as float
        oid_SpectatorCommentWeight = AddSliderOption("Comment weight (0=Participant, 100=Spectator):", spectatorWeight)
        float scanInterval = TTON_JData.GetMcmSpectatorScanInterval() as float
        oid_SpectatorScanInterval = AddSliderOption("Scan interval (seconds):", scanInterval)
        oid_ClearSpectatorsData = AddTextOption("", "Clear spectators data")
        oid_StopSpectatorScan = AddTextOption("", "Stop spectator scan loop")
    endif


    AddHeaderOption("Affection Scenes")
    float affectionDuration = TTON_JData.GetMcmAffectionDuration() as float
    oid_AffectionSceneDuration = AddSliderOption("Affection scene duration (seconds):", affectionDuration)
EndFunction

Function RenderRightColumn()
    AddHeaderOption("Immersion & Feedback")
    float frequency = TTON_JData.GetMcmCommentsFrequency() as float
    float genderWeight = TTON_JData.GetMcmCommentsGenderWeight() as float

    oid_SexCommentsGenderWeight = AddSliderOption("Gender preference (0=Male, 100=Female):", genderWeight)
    oid_Mute = AddToggleOption("Mute(events are still logged):", TTON_JData.GetMuteSetting())
    oid_MuteHotkey = AddKeyMapOption("Hotkey for mute:", TTON_JData.GetMuteHotkey())
    oid_PrioritizePlayerThreadComments = AddToggleOption("Prioritize player scenes for comments:", TTON_JData.GetPrioritizePlayerThreadComments())

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
    elseif(option == oid_UseDefaultOStimSceneStart)
        SetToggleOptionValue(oid_UseDefaultOStimSceneStart, TTON_JData.ToggleMcmCheckbox("useOStimDefaultStartSelection", 0))
    elseif(option == oid_Mute)
        SetToggleOptionValue(oid_Mute, TTON_JData.ToggleMuteSetting())
    elseif(option == oid_PrioritizePlayerThreadComments)
        SetToggleOptionValue(oid_PrioritizePlayerThreadComments, TTON_JData.ToggleMcmCheckbox("prioritizePlayerThreadComments", 1))
    elseif(option == oid_SpectatorsEnabled)
        SetToggleOptionValue(oid_SpectatorsEnabled, TTON_JData.ToggleSpectatorsEnabled())
        ForcePageReset()
    elseif(option == oid_ClearSpectatorsData)
        bool yes = ShowMessage("Are you sure you want to clear all spectators data?")
        if(yes)
            TTON_Spectators.ClearAllSpectators()
        endif
    elseif(option == oid_StopSpectatorScan)
        if(TTON_Spectators.IsScanLoopRunning())
            TTON_Spectators.StopScanLoop()
            ShowMessage("Spectator scan loop stopped.")
        else
            ShowMessage("Spectator scan loop is not running.")
        endif
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
    elseif(option == oid_UseDefaultOStimSceneStart)
        SetInfoText("Starts default OStim scene(select furniture, select additional actors, start from idle scene).")
    elseif(option == oid_SexCommentsGenderWeight)
        SetInfoText("Controls which gender is more likely to speak. 50 = equal chance for both genders.")
    elseif(option == oid_AffectionSceneDuration)
        SetInfoText("How long affectionate scenes last before automatically ending.")
    elseif(option == oid_DeniesCooldown)
        SetInfoText("How long each NPC waits before asking again after you refuse their request.")
    elseif(option == oid_Mute)
        SetInfoText("Mutes npcs and doesn't allow them to auto-speak on OStim events like scene change/start/end/climax. Good for allowing your own narration or searching/navigating.")
    elseif(option == oid_MuteHotkey)
        SetInfoText("Toggle Mute in-game without entering MCM.")
    elseif(option == oid_PrioritizePlayerThreadComments)
        SetInfoText("Prioritize comments from the player's thread in scenes and mute NPCs from other threads..")
    elseif(option == oid_SpectatorsEnabled)
        SetInfoText("Enable NPCs to become spectators when they discover their significant others in intimate scenes.")
    elseif(option == oid_MaxSpectatorsOverall)
        SetInfoText("Maximum number of spectators allowed across all active threads.")
    elseif(option == oid_MaxSpectatorsPerThread)
        SetInfoText("Maximum number of spectators allowed per individual thread.")
    elseif(option == oid_SpectatorCommentWeight)
        SetInfoText("Controls who comments more often. 0 = only participants, 100 = only spectators, 50 = equal chance.")
    elseif(option == oid_SpectatorScanInterval)
        SetInfoText("How often to scan for potential spectators (in seconds). Lower values find spectators faster but use more resources.")
    elseif(option == oid_ClearSpectatorsData)
        SetInfoText("Removes all active spectators and clears spectator-related storage data.")
    elseif(option == oid_StopSpectatorScan)
        SetInfoText("Manually stops the spectator scan loop if it's stuck running.")
    endif
endevent

event OnOptionSliderOpen(int a_option)
    float startValue
    float defaultValue
    float startRange
    float endRange
    float interval
    if(a_option == oid_SexCommentsGenderWeight)
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
    elseif(a_option == oid_MaxSpectatorsOverall)
        startValue = TTON_JData.GetMcmMaxSpectatorsOverall() as float
        defaultValue = 5.0
        startRange = 0
        endRange = 20
        interval = 1
    elseif(a_option == oid_MaxSpectatorsPerThread)
        startValue = TTON_JData.GetMcmMaxSpectatorsPerThread() as float
        defaultValue = 2.0
        startRange = 0
        endRange = 10
        interval = 1
    elseif(a_option == oid_SpectatorCommentWeight)
        startValue = TTON_JData.GetMcmSpectatorCommentWeight() as float
        defaultValue = 50.0
        startRange = 0
        endRange = 100
        interval = 1
    elseif(a_option == oid_SpectatorScanInterval)
        startValue = TTON_JData.GetMcmSpectatorScanInterval() as float
        defaultValue = 10.0
        startRange = 5
        endRange = 60
        interval = 5
    endif

    SetSliderDialogStartValue(startValue)
    SetSliderDialogDefaultValue(defaultValue)
    SetSliderDialogRange(startRange, endRange)
    SetSliderDialogInterval(interval)
endEvent
event OnOptionSliderAccept(int a_option, float a_value)
    SetSliderOptionValue(a_option, a_value)
    if(a_option == oid_SexCommentsGenderWeight)
        TTON_JData.SetMcmCommentsGenderWeight(a_value as int)
    elseif(a_option == oid_AffectionSceneDuration)
        TTON_JData.SetMcmAffectionDuration(a_value as int)
    elseif(a_option == oid_DeniesCooldown)
        TTON_JData.SetMcmDenyCooldown(a_value as int)
    elseif(a_option == oid_MaxSpectatorsOverall)
        TTON_JData.SetMcmMaxSpectatorsOverall(a_value as int)
    elseif(a_option == oid_MaxSpectatorsPerThread)
        TTON_JData.SetMcmMaxSpectatorsPerThread(a_value as int)
    elseif(a_option == oid_SpectatorCommentWeight)
        TTON_JData.SetMcmSpectatorCommentWeight(a_value as int)
    elseif(a_option == oid_SpectatorScanInterval)
        TTON_JData.SetMcmSpectatorScanInterval(a_value as int)
    endif
endEvent

event OnOptionDefault(int a_option)
	if(a_option == oid_SexCommentsGenderWeight)
        SetSliderDialogStartValue(50)
        TTON_JData.SetMcmCommentsGenderWeight(50)
    elseif(a_option == oid_AffectionSceneDuration)
        SetSliderDialogStartValue(30)
        TTON_JData.SetMcmAffectionDuration(30)
    elseif(a_option == oid_DeniesCooldown)
        SetSliderDialogStartValue(20)
        TTON_JData.SetMcmDenyCooldown(20)
    elseif(a_option == oid_MuteHotkey)
        TTON_JData.SetMuteHotkey(-1)
        SetKeyMapOptionValue(oid_MuteHotkey, -1)
        RegisterMuteHotkey()
    elseif(a_option == oid_MaxSpectatorsOverall)
        SetSliderDialogStartValue(5)
        TTON_JData.SetMcmMaxSpectatorsOverall(5)
    elseif(a_option == oid_MaxSpectatorsPerThread)
        SetSliderDialogStartValue(2)
        TTON_JData.SetMcmMaxSpectatorsPerThread(2)
    elseif(a_option == oid_SpectatorCommentWeight)
        SetSliderDialogStartValue(50)
        TTON_JData.SetMcmSpectatorCommentWeight(50)
    elseif(a_option == oid_SpectatorScanInterval)
        SetSliderDialogStartValue(10)
        TTON_JData.SetMcmSpectatorScanInterval(10)
    endif
endEvent

Function RegisterMuteHotkey()
    UnregisterForAllKeys()
    int keyCode = TTON_JData.GetMuteHotkey()
    if(keyCode > 0)
        RegisterForKey(keyCode)
    endif
EndFunction

event OnOptionKeyMapChange(int option, int keyCode, string conflictControl, string conflictName)
    if(option == oid_MuteHotkey)
        int finalKeyCode = keyCode
        if(keyCode == 0)
            finalKeyCode = -1
        endif
        TTON_JData.SetMuteHotkey(finalKeyCode)
        SetKeyMapOptionValue(option, finalKeyCode)
        RegisterMuteHotkey()
    endif
endevent

Event OnKeyDown(int keyCode)
    if UI.IsTextInputEnabled()
        return
    endif
    int configuredKey = TTON_JData.GetMuteHotkey()
    if(keyCode != configuredKey || keyCode <= 0)
        return
    endif

    bool isPaused = TTON_JData.ToggleMuteSetting()
    if(oid_Mute > 0)
        SetToggleOptionValue(oid_Mute, isPaused)
    endif
    if(isPaused)
        Debug.Notification("OStimNet NPCs are muted.")
    else
        Debug.Notification("OStimNet NPCs are talking.")
    endif
EndEvent
