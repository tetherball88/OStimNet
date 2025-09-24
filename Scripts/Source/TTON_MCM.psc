Scriptname TTON_MCM extends SKI_ConfigBase

int property oid_SettingsClearData auto
int property oid_SettingsExportData auto
int property oid_SettingsImportData auto

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
EndFunction

Function RenderLeftColumn()
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
    endif
endevent
