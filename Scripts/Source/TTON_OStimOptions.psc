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
    MiscUtil.PrintConsole("Changing intent to: " + intent )
    string currentIntent = OStimNet.GetThreadIntent(0)
    if(currentIntent == intent)
        Debug.Notification("Intent is already " + intent + ", no changes made.")
        return none
    endif
    Actor[] actors = OThread.GetActors(0)
    int maxMainActors = actors.Length - 1
    if(maxMainActors < 1) ; there should be at least 1 actor for secondary role
        Debug.Notification("Not enough actors in the scene to change roles.")
        return none
    endif
    OStimNet.SetThreadIntent(0, intent, SelectMainActors(intent, maxMainActors))
EndFunction

Function ChangeActorsRoles(string stateVal) global
    MiscUtil.PrintConsole("Changing actors roles to: " + stateVal )
    Actor[] actors = OThread.GetActors(0)
    string intent = OStimNet.GetThreadIntent(0)
    TTON_Debug.debug("Current intent: " + intent)
    int maxMainActors = actors.Length - 1
    if(maxMainActors < 1) ; there should be at least 1 actor for secondary role
        Debug.Notification("Not enough actors in the scene to change roles.")
        return
    endif
    Actor[] newMainActors = SelectMainActors(stateVal, maxMainActors)
    if(newMainActors == none)
        Debug.Notification("No changes made to actors roles.")
        return
    endif
    OStimNet.SetThreadIntent(0, intent, newMainActors)
EndFunction

Actor[] Function SelectMainActors(string intent, int maxMainActors) global
    clearSelectedMainActors()
    int i = 0
    int attempts = maxMainActors + 2

    while (i < maxMainActors && attempts > 0)
        bool firstTime = StorageUtil.FormListCount(none, "TTON_ChangeIntent_SelectedActors") == 0
        int choice = ShowOptionsMenu(firstTime, intent, maxMainActors)
        TTON_Debug.debug("Iteration: " + i + ", attempts: " + attempts + ", Option menu choice: " + choice + ", selected actors count: " + StorageUtil.FormListCount(none, "TTON_ChangeIntent_SelectedActors"))

        if(choice == -1 || choice == 1) ; exit loop if menu was cancelled or closed
            return none ; cancelled
        elseif(choice == 0)
            ; ignore we just loop again and show the menu again
        elseif(choice == 2)
            TTON_Debug.debug("Finish selected with " + StorageUtil.FormListCount(none, "TTON_ChangeIntent_SelectedActors") + " actors selected.")
            return GetSelectedMainActors() ; finish
        elseif(choice == 3) ; progress loop only if an actor was selected
            i += 1
        endif
    endwhile
EndFunction

; Returns: -1 = cancelled, 0 = header clicked (re-show), 1 = keep current, 2 = finish, 3 = actor selected
int Function ShowOptionsmenu(bool firstTime, string intent, int maxMainActors) global
    Actor[] allActors = OThread.GetActors(0)
    Actor[] currentMainActors = OStimNet.GetMainActors(0)

    string mainActorsRole = ""
    if(intent == "romantic" || intent == "lustful")
        mainActorsRole = "initiators"
    elseif(intent == "transactional")
        mainActorsRole = "service receivers"
    elseif(intent == "dom")
        mainActorsRole = "dominant actors"
    elseif(intent == "aggressive")
        mainActorsRole = "agressors"
    endif

    UIListMenu listMenu = UIExtensions.GetMenu("UIListMenu", true) as UIListMenu

    Actor[] selectedActors = GetSelectedMainActors()

    ; Item 0: header — clicking it is treated as a no-op and the menu is shown again
    listMenu.AddEntryItem("Select " + mainActorsRole + " (" + (selectedActors.Length) + "/" + maxMainActors + "):")

    ; Item 1: keep current (first open) or finish (at least one actor already selected)
    if(firstTime)
        listMenu.AddEntryItem("Keep current main actors.")
    else
        listMenu.AddEntryItem("Finish")
    endif

    Debug.Notification("Select new main actors("+mainActorsRole+") for intent: " + intent)
    int i = 0

    Actor[] optionsActors = PapyrusUtil.GetDiffActor(allActors, selectedActors)
    if(optionsActors.Length == 0)
        Debug.Notification("No more actors available to select.")
        return 2 ; finish since there are no more actors to select
    endif
    ; Items 2+: actors still available to select (optionsActors, not allActors)
    while (i < allActors.Length)
        listMenu.AddEntryItem(TTON_Utils.GetActorName(allActors[i]))
        i += 1
    endwhile
    listMenu.OpenMenu()
    int choice = listMenu.GetResultInt()

    TTON_Debug.debug("Option menu choice: " + choice)

    if(choice == 0)
        return 0 ; header clicked
    endif

    if(choice == 1)
        return 1 ; keep current
    endif

    if(choice > 1)
        AddSelectedMainActor(optionsActors[choice - 2])
        if(maxMainActors == 1)
            return 2 ; finish
        else
            return 3 ; actor selected
        endif
    endif
    ; If cancelled or closed returns -1
    return choice
EndFunction

Function AddSelectedMainActor(Actor selectedActor) global
    TTON_Debug.debug("Add selected actor: " + TTON_Utils.GetActorName(selectedActor))
    StorageUtil.FormListAdd(none, "TTON_ChangeIntent_SelectedActors", selectedActor, false)
    TTON_Debug.debug("Current selected actors: " + StorageUtil.FormListCount(none, "TTON_ChangeIntent_SelectedActors"))
EndFunction

Actor[] Function GetSelectedMainActors() global
    Form[] selected = StorageUtil.FormListToArray(none, "TTON_ChangeIntent_SelectedActors")
    Actor[] selectedActors = PapyrusUtil.ActorArray(selected.Length)
    int i = 0
    TTON_Debug.debug("Selected actors length: " + selected.Length)
    while (i < selected.Length)
        selectedActors[i] = selected[i] as Actor
        TTON_Debug.debug("Selected actor: " + TTON_Utils.GetActorName(selectedActors[i]))
        i += 1
    endwhile
    return selectedActors
EndFunction

Function ClearSelectedMainActors() global
    StorageUtil.ClearAllPrefix("TTON_ChangeIntent_SelectedActors")
EndFunction
