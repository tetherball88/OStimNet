scriptname OStimNet

;==========================================================================
; Native functions (implemented in OStimNet SKSE plugin)
;==========================================================================

; Returns the current location generation counter for async evaluation invalidation
int Function GetLocationGeneration() global native

; Returns the scene description for the given thread ID
string Function GetSceneDescription(int ThreadID) global native

; Returns actor names joined naturally: "Lydia", "Lydia and Farengar", "Lydia, Farengar and Ulfric".
; Pass akExclude to omit one actor (e.g. the player). Pass None to include all.
string Function GetActorListString(Actor[] akActors, Actor akExclude = None) global native

; Logs a message via the SKSE logger. level: 0=trace, 1=debug, 2=info, 3=warn, 4=error
Function Log(string msg, int level = 2) global native

; Returns the intent string stored for the thread.
; Returns "" if no intent has been set for that thread.
string Function GetThreadIntent(int ThreadID) global native

; Returns the current sexual phase name for a thread: "undressing", "foreplay", "oral", or "sex".
; Returns "" if phase tracking is disabled for this thread (e.g. dom/aggressive intent,
; or the EnableThreadPhases config setting is off).
string Function GetThreadPhase(int ThreadID) global native

; Returns a comma-separated list of OStim activity keywords for the thread's current phase,
; suitable for passing as the activity filter in TTON_SceneSearch.SceneSexSearch().
; Returns "" if phase tracking is disabled for this thread.
string Function GetThreadPhaseActivityKeywords(int ThreadID) global native

; Returns the first phase name for the given intent ("undressing", "foreplay", "oral", or "sex"),
; applying the EnableUndressingPlayer / EnableUndressingNpc config so Undressing is skipped
; when it is disabled for this thread type.
; Returns "" if the intent has no phase sequence (e.g. "platonic", or phases disabled for it).
string Function GetInitialPhaseByIntent(string intent, bool isPlayerThread) global native

; Returns the main-actor array stored for the given thread.
; Returns an empty array if the thread is not found or has no main actors set.
Actor[] Function GetMainActors(int ThreadID) global native

; Returns the secondary-actor array stored for the given thread.
; Returns an empty array if the thread is not found or has no secondary actors set.
Actor[] Function GetSecondaryActors(int ThreadID) global native

; Updates the stored intent and reshuffles main/secondary actors for a running thread.
; mainActors: actors to assign as the primary role for the new intent.
; Secondary actors are derived automatically from the remaining thread participants.
Function SetThreadIntent(int ThreadID, string intent, Actor[] mainActors) global native

; Call BEFORE OThreadBuilder.Start() when you already know intent, sexual flag,
; and actor roles (e.g. after receiving ostimnet_sexual_evaluation_finished).
; Allocates a claim token in the registry that HandleStart will consume when
; ostim_thread_start fires, bypassing all deferred-start logic.
; Returns the claim token (> 0) to pass to ConfirmThread.
int Function ClaimThread(string intent, bool isSexual, Actor[] mainActors, Actor[] secondaryActors) global native

; Call AFTER OThreadBuilder.Start() returns the real threadID.
; Binds the claim token to the thread. If HandleStart already fired, the thread
; is activated immediately; otherwise the claim is consumed when HandleStart fires.
Function ConfirmThread(int claimToken, int threadID) global native

; Call before triggering an add-actor operation that causes OStim to stop/restart the thread.
; When true, ostimnet_sex_stop is suppressed for that thread end.
; Call with false (or omit) for a genuine stop.
Function SetThreadContinuation(int ThreadID, bool isContinuation) global native

; Call before SetThreadContinuation + the hot-swap for actor-add scenarios.
; Provides LLM-evaluated intent and actor roles so the continuation uses fresh data
; instead of copying the stale sceneDescription and currentPosition from the old thread.
; The swap path (no call to this function) is unaffected.
Function SetContinuationOverride(int ThreadID, string newIntent, Actor[] main, Actor[] secondary) global native

; Called from the Papyrus listener for ocum_applied_cum / ocum_applied_cum_npc_scene.
; Appends cum-applied data to the thread's pending climax batch and resets the debounce timer.
Function OcumApplied(int ThreadID, Actor Orgasmer, Actor Target, float AmountML, string Area, string SceneID) global native

; Called from the Papyrus listener for ocum_squirt.
; Appends a squirt entry to the thread's pending climax batch and resets the debounce timer.
; Type: "spurt" or "flow"
Function OcumSquirt(int ThreadID, Actor akActor, string SceneID, string Type) global native

; Returns the actor in an active OStim thread that the given spectator has a
; romantic/marital relationship with, or None if the actor is not currently tracked as a spectator.
Actor Function GetSpectatorTarget(Actor akSpectator) global native

; Manually registers akSpectator as watching the given OStim thread.
; akTarget: the actor in the thread the spectator is focused on.
;   Pass None to use the first actor in the thread as the default target.
; Fires ostimnet_add_spectator mod event.
Function AddSpectator(Actor akSpectator, int ThreadID, Actor akTarget = None) global native

; Removes akSpectator from spectator tracking and fires ostimnet_remove_spectator.
; Does nothing if the actor is not currently tracked as a spectator.
Function RemoveSpectator(Actor akSpectator) global native

; Builds the tton_event JSON payload for a spectator_fled event.
; Pass ThreadID = -1 if the thread has already ended.
string Function BuildSpectatorFledEventJson(Actor akSpectator) global native

; Sends the "ostimnet_evaluate_prestart_sexual" prompt to the LLM to evaluate roles
; for the given participants before an OStim scene is created.
; Returns 1 if the task was queued successfully, 0 on failure.
; When the LLM finishes, fires mod event "ostimnet_sexual_evaluation_finished":
;   strArg = stringified JSON (numArg=1.0 only):
;     { "intent": "...", "sexualPosition": "...", "sexualActivities": "...",
;       "main": [formId, ...], "secondary": [formId, ...],
;       "originalParticipants": [formId, ...] }
;   numArg = 1.0  scene resolved successfully
;   numArg = 2.0  LLM decided not enough willing participants (scene=null)
;   numArg = 0.0  error (LLM failure or bad response)
int Function EvaluatePreStartSexualScene(Actor[] akParticipants, string asIntent = "", string evalId = "") global native

; Sends the "ostimnet_evaluate_nonsexual_scene" prompt to the LLM to evaluate whether
; a non-sexual scene should start with the given participants.
; asActivity:   optional activity hint for the LLM (e.g. "spar", "meditate"). Pass "" for no hint.
; asIntent:     optional intent hint for the LLM (e.g. "friendly", "hostile"). Pass "" for no hint.
; akInitiator:  optional actor who initiated the scene. Pass None for no initiator.
; Returns 1 if the task was queued successfully, 0 on failure.
; When the LLM finishes, fires mod event "ostimnet_nonsexual_evaluation_finished":
;   strArg = stringified JSON (numArg=1.0 only):
;     { "start": true, "intent": "...", "activity": "...", "main": [formId, ...], "secondary": [formId, ...] }
;   numArg = 1.0  start=true, scene should proceed
;   numArg = 2.0  start=false, LLM decided scene should not start
;   numArg = 0.0  error (LLM failure or bad response)
int Function EvaluateNonSexualScene(Actor[] akParticipants, string asActivity = "", string asIntent = "", Actor akInitiator = None, string evalId = "") global native

; Sends the "ostimnet_evaluate_join_ongoing_sex" prompt to the LLM to evaluate whether
; akJoiner should be allowed to join an existing running thread.
; threadID: the OStim thread ID the actor wants to join.
; Returns 1 if the task was queued successfully, 0 on failure.
; When the LLM finishes, fires mod event "ostimnet_join_sex_evaluation_finished":
;   strArg = stringified JSON (numArg=1.0 only):
;     { "threadID": int, "joinerFormID": uint, "intent": "...",
;       "main": [formId, ...], "secondary": [formId, ...] }
;   numArg = 1.0  accepted — joiner may be added
;   numArg = 2.0  declined — current participants refused
;   numArg = 0.0  error (LLM failure or bad response)
int Function EvaluateJoinOngoingSex(Actor akJoiner, int threadID, string evalId = "") global native

; Sends the "ostimnet_evaluate_invite_to_sex" prompt to the LLM to evaluate whether
; akInvitees should be invited into an existing running thread by akInviter.
; threadID: the OStim thread ID to invite actors into.
; Returns 1 if the task was queued successfully, 0 on failure.
; When the LLM finishes, fires mod event "ostimnet_invite_sex_evaluation_finished":
;   strArg = stringified JSON (numArg=1.0 only):
;     { "threadID": int, "inviterFormID": uint, "intent": "...",
;       "main": [formId, ...], "secondary": [formId, ...] }
;   numArg = 1.0  accepted — at least one invitee may join
;   numArg = 2.0  declined — no invitees willing or accepted
;   numArg = 0.0  error (LLM failure or bad response)
int Function EvaluateInviteToSex(Actor akInviter, Actor[] akInvitees, int threadID, string evalId = "") global native

; Shows a 3-button confirmation dialog for an LLM-initiated action.
; Checks per-action cooldowns before showing. Blocks until the player responds.
; Cooldown is applied to all mainActors on result.
; Returns:
;    0 = player accepted
;    1 = player declined
;    2 = player chose "Decline (Explain)" — TriggerTextInput fired automatically
;    3 = cooldown still active (modal not shown)
;    4 = player not among actors (modal not shown)
;    5 = intent is "aggressive" or "dom" (modal not shown)
int Function ShowConfirmationModal(string actionType, Actor[] mainActors, Actor[] secondaryActors, string paramsJson) Global Native

; Checks if the action is on cooldown for any of the given actors.
; If not on cooldown, sets the normal cooldown for all actors immediately and returns 0.
; Call at the start of every action execute function, before SetActorsPending and before ShowConfirmationModal.
; Returns:
;    0 = ok (cooldown set, action may proceed)
;    1 = blocked (cooldown still active, action should be skipped)
int Function CheckAndSetActionCooldown(string actionType, Actor[] actors) global native

; Returns true if asPosition is a canonical position name or a known alias (case-insensitive).
; Use this to validate user/LLM-supplied position strings before passing them to OStim APIs.
bool Function IsValidPosition(string asPosition) global native

; Scans the provided scene-tag array and returns the first canonical sexual-position
; string found (e.g. "missionary", "doggystyle"). Returns "" if no position tag is present.
string Function GetSexualPositionFromTags(string[] tags) global native

; Builds a tton_event JSON payload for a spectator_added event.
; threadID is resolved from the spectator's tracked entry; msg is built from actor display names;
; skipTrigger is taken from the plugin's mute setting. Returns "" if the spectator is not tracked.
; Returns the JSON string ready to pass to SkyrimNetApi.RegisterEvent.
string Function BuildSpectatorAddedEventJson(Actor spectator) global native


;==========================================================================
; JSON utility natives — dot-notated path access into any JSON string
;
; Path examples:
;   "reasoning"      -> top-level string key
;   "agressor.0"     -> first element of the "agressor" array (0-based)
;   "nested.key.sub" -> nested object traversal
;
; Usage example (iterating FormID array from ostimnet_sexual_evaluation_finished):
;   int len = OStimNet.JsonArrayLength(akStrArg, "agressor")
;   int i = 0
;   while i < len
;       int formId = OStimNet.JsonGetInt(akStrArg, "agressor." + i, 0)
;       i += 1
;   endwhile
;==========================================================================

; Returns the string value at akPath, or akDefault if missing / wrong type.
string Function JsonGetString(string akJson, string akPath, string akDefault = "") global native

; Returns the int value at akPath, or akDefault if missing / wrong type.
; Float values in JSON are truncated to int.
int Function JsonGetInt(string akJson, string akPath, int akDefault = 0) global native

; Returns the float value at akPath, or akDefault if missing / wrong type.
float Function JsonGetFloat(string akJson, string akPath, float akDefault = 0.0) global native

; Returns the bool value at akPath, or akDefault if missing / wrong type.
bool Function JsonGetBool(string akJson, string akPath, bool akDefault = false) global native

; Returns the length of the JSON array at akPath, or -1 if not found / not an array.
; Pass akPath = "" to query the root value itself.
int Function JsonArrayLength(string akJson, string akPath = "") global native

; Resolves a single uint32 FormID at akPath to a live Actor reference.
; Returns None if the value is missing, zero, or doesn't resolve to an Actor.
Actor Function JsonGetActor(string akJson, string akPath = "") global native

; Resolves a JSON array of uint32 FormIDs at akPath to live Actor references.
; Elements that don't resolve to an Actor are omitted from the result.
; Pass akPath = "" to treat the root JSON value itself as the array.
Actor[] Function JsonGetActorArray(string akJson, string akPath = "") global native
