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
    OStimNet.SetThreadIntent(0, intent, SelectMainActors(intent, maxMainActors, 0))
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
    Actor[] newMainActors = SelectMainActors(stateVal, maxMainActors, 0)
    if(newMainActors == none)
        Debug.Notification("No changes made to actors roles.")
        return
    endif
    OStimNet.SetThreadIntent(0, intent, newMainActors)
EndFunction

Actor[] Function SelectMainActors(string intent, int maxMainActors, int threadID = 0) global
    ClearSelectedMainActors(threadID)
    int i = 0
    int attempts = maxMainActors + 2

    while (i < maxMainActors && attempts > 0)
        bool firstTime = CountNewMainActors(threadID) == 0
        int choice = ShowOptionsMenu(firstTime, intent, maxMainActors, threadID)
        TTON_Debug.debug("Iteration: " + i + ", attempts: " + attempts + ", Option menu choice: " + choice + ", selected actors count: " + CountNewMainActors(threadID))

        if(choice == -1 || choice == 1) ; exit loop if menu was cancelled or closed
            TTON_Debug.debug("Returning selected actors: cancelled or closed")
            return none ; cancelled
        elseif(choice == 0)
            ; ignore we just loop again and show the menu again
        elseif(choice == 2)
            TTON_Debug.debug("Finish selected with " + CountNewMainActors(threadID) + " actors selected.")
            return GetSelectedMainActors(threadID) ; finish
        elseif(choice == 3) ; progress loop only if an actor was selected
            i += 1
        endif
        attempts -= 1
    endwhile
    TTON_Debug.debug("Returning selected actors: none")
    return none
EndFunction

; Returns: -1 = cancelled, 0 = header clicked (re-show), 1 = keep current, 2 = finish, 3 = actor selected
int Function ShowOptionsmenu(bool firstTime, string intent, int maxMainActors, int threadID = 0) global
    Actor[] allActors = OThread.GetActors(threadID)
    Actor[] currentMainActors = OStimNet.GetMainActors(threadID)

    string mainActorsRole = ""
    if(intent == "platonic" || intent == "romantic" || intent == "lustful")
        mainActorsRole = "initiators"
    elseif(intent == "transactional")
        mainActorsRole = "service receivers"
    elseif(intent == "dom")
        mainActorsRole = "dominant actors"
    elseif(intent == "aggressive")
        mainActorsRole = "aggressors"
    endif

    UIListMenu listMenu = UIExtensions.GetMenu("UIListMenu", true) as UIListMenu

    Actor[] selectedActors = GetSelectedMainActors(threadID)

    ; Item 0: header — clicking it is treated as a no-op and the menu is shown again
    listMenu.AddEntryItem("Select " + mainActorsRole + " (" + (selectedActors.Length) + "/" + maxMainActors + "):")

    ; Item 1: cancel/keep current (first open) or finish (at least one actor already selected)
    if(firstTime)
        if(currentMainActors.Length > 0)
            listMenu.AddEntryItem("Keep current main actors.")
        else
            listMenu.AddEntryItem("Cancel")
        endif
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
    while (i < optionsActors.Length)
        listMenu.AddEntryItem(TTON_Utils.GetActorName(optionsActors[i]))
        i += 1
    endwhile
    listMenu.OpenMenu()
    int choice = listMenu.GetResultInt()

    TTON_Debug.debug("Option menu choice: " + choice)

    if(choice == 0)
        return 0 ; header clicked
    endif

    if(choice == 1)
        if(firstTime)
            return 1 ; keep current or cancel
        else
            return 2 ; finish
        endif
    endif

    if(choice > 1)
        AddSelectedMainActor(optionsActors[choice - 2], threadID)
        if(maxMainActors == 1 || (selectedActors.Length + 1) >= maxMainActors)
            return 2 ; finish
        else
            return 3 ; actor selected
        endif
    endif
    ; If cancelled or closed returns -1
    return choice
EndFunction

; Returns: 1 = Sexual, 2 = Non-Sexual, -1 = Cancelled
int Function SelectThreadType() global
    UIListMenu listMenu = UIExtensions.GetMenu("UIListMenu", true) as UIListMenu
    listMenu.AddEntryItem("Select Thread Type:")
    listMenu.AddEntryItem("Sexual")
    listMenu.AddEntryItem("Non-Sexual")

    int choice = 0
    while (choice == 0)
        listMenu.OpenMenu()
        choice = listMenu.GetResultInt()
    endwhile

    if (choice == 1)
        return 1
    elseif (choice == 2)
        return 2
    endif

    return -1
EndFunction

string Function SelectSexualIntent() global
    UIListMenu listMenu = UIExtensions.GetMenu("UIListMenu", true) as UIListMenu
    listMenu.AddEntryItem("Select Scene Intent:")
    listMenu.AddEntryItem("Romantic")
    listMenu.AddEntryItem("Lustful")
    listMenu.AddEntryItem("Transactional")
    listMenu.AddEntryItem("Dom")
    listMenu.AddEntryItem("Aggressive")
    listMenu.AddEntryItem("Let AI decide")

    int choice = 0
    while (choice == 0)
        listMenu.OpenMenu()
        choice = listMenu.GetResultInt()
    endwhile

    if (choice == 1)
        return "romantic"
    elseif (choice == 2)
        return "lustful"
    elseif (choice == 3)
        return "transactional"
    elseif (choice == 4)
        return "dom"
    elseif (choice == 5)
        return "aggressive"
    elseif (choice == 6)
        return "ai"
    endif

    return ""
EndFunction

string Function SelectNonSexualIntent() global
    UIListMenu listMenu = UIExtensions.GetMenu("UIListMenu", true) as UIListMenu
    listMenu.AddEntryItem("Select Scene Intent:")
    listMenu.AddEntryItem("Platonic")
    listMenu.AddEntryItem("Romantic")

    int choice = 0
    while (choice == 0)
        listMenu.OpenMenu()
        choice = listMenu.GetResultInt()
    endwhile

    if (choice == 1)
        return "platonic"
    elseif (choice == 2)
        return "romantic"
    endif

    return ""
EndFunction

string Function SelectIntent() global
    int threadType = SelectThreadType()
    if (threadType == 1)
        return SelectSexualIntent()
    elseif (threadType == 2)
        return SelectNonSexualIntent()
    endif
    return ""
EndFunction

Function SetupExternalThread(int threadID) global
    Actor[] actors = OThread.GetActors(threadID)

    if(actors == none || actors.Length == 0)
        TTON_Debug.debug("SetupExternalThread: No actors found for thread " + threadID)
        return
    endif

    bool isSexual = true
    string intent = ""
    bool decided = false

    while (!decided)
        int threadType = SelectThreadType()
        if (threadType == -1)
            Debug.Notification("OStimNet: Scene setup cancelled.")
            OStimNet.CancelExternalThread(threadID)
            return
        endif

        isSexual = (threadType == 1)
        if (isSexual)
            intent = SelectSexualIntent()
        else
            intent = SelectNonSexualIntent()
        endif

        if (intent != "")
            decided = true
        endif
    endwhile

    if(intent == "ai")
        Debug.Notification("OStimNet: Delegating scene evaluation to AI...")
        OStimNet.EvaluateExternalSexualThread(threadID)
        return
    endif

    if(actors.Length == 1)
        Actor[] soloMain = PapyrusUtil.ActorArray(1)
        soloMain[0] = actors[0]
        OStimNet.ClaimExternalThread(threadID, intent, soloMain, isSexual)
        Debug.Notification("OStimNet: Scene configured with " + intent + " intent.")
        return
    endif

    ExternalThreadSelectMainActors(actors, intent, isSexual, threadID)
EndFunction

Function ExternalThreadSelectMainActors(Actor[] actors, string intent, bool isSexual, int threadID) global
    int maxMainActors = actors.Length - 1
    SelectMainActors(intent, maxMainActors, threadID)
    Actor[] newMainActors = GetSelectedMainActors(threadID)
    if(newMainActors == none || newMainActors.Length == 0)
        TTON_Debug.debug("ExternalThreadSelectMainActors: No actors selected for thread " + threadID)
        OStimNet.CancelExternalThread(threadID)
        ClearSelectedMainActors(threadID)
        return
    endif

    ClearSelectedMainActors(threadID)

    TTON_Debug.debug("ExternalThreadSelectMainActors: Claiming external thread " + threadID + " with " + newMainActors.Length + " main actors")
    OStimNet.ClaimExternalThread(threadID, intent, newMainActors, isSexual)
EndFunction

Function AddSelectedMainActor(Actor selectedActor, int threadID = 0) global
    TTON_Debug.debug("Add selected actor: " + TTON_Utils.GetActorName(selectedActor) + ", thread: " + threadID)
    StorageUtil.FormListAdd(none, "TTON_ChangeIntent_SelectedActors_" + threadID, selectedActor, false)
    TTON_Debug.debug("Current selected actors: " + CountNewMainActors(threadID))
EndFunction

Actor[] Function GetSelectedMainActors(int threadID = 0) global
    Form[] selected = StorageUtil.FormListToArray(none, "TTON_ChangeIntent_SelectedActors_" + threadID)
    Actor[] selectedActors = PapyrusUtil.ActorArray(selected.Length)
    int i = 0
    TTON_Debug.debug("Selected actors length: " + selected.Length + ", thread: " + threadID)
    while (i < selected.Length)
        selectedActors[i] = selected[i] as Actor
        TTON_Debug.debug("Selected actor: " + TTON_Utils.GetActorName(selectedActors[i]))
        i += 1
    endwhile
    TTON_Debug.debug("Returning selected actors: " + selectedActors)
    return selectedActors
EndFunction

int Function CountNewMainActors(int threadID = 0) global
    return StorageUtil.FormListCount(none, "TTON_ChangeIntent_SelectedActors_" + threadID)
EndFunction

Function ClearSelectedMainActors(int threadID = 0) global
    TTON_Debug.debug("Clearing selected actors, thread: " + threadID)
    StorageUtil.ClearAllPrefix("TTON_ChangeIntent_SelectedActors_" + threadID)
EndFunction

