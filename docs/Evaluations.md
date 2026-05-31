# LLM Evaluations

OStimNet relies heavily on SkyrimNet's LLM to evaluate situations and make decisions. These evaluations happen at various stages, from the moment an action is considered (Action Parameters) to specialized pre-scene and mid-scene custom evaluations.

---

## 1. Action Parameter Evaluations

When the LLM decides to perform an OStimNet action, it must evaluate and provide dynamic parameters required by that action. These are defined in the action YAML configurations.

### `intent` Parameter
Evaluated during actions like `StartNewSex` and `ChangeSexSceneIntent`. The LLM considers the actors' relationships, recent events, and the situation to determine the narrative dynamic (e.g., romantic, lustful, transactional, dom, aggressive). For start actions, it must also provide justification and a participant list.

### `targetActors` Parameter
Evaluated during actions like `InviteToYourSex`. The LLM scans the surroundings and selects which specific nearby NPC(s) would make sense to invite based on the ongoing scene and relationship dynamics.

### `speed` Parameter
Evaluated during `ChangeSexScenePace`. The LLM decides whether to "increase" or "decrease" the pace based on the narrative intensity of the ongoing encounter.

---

## 2. Custom Game Master Evaluations

Once an action is initiated or an event occurs, OStimNet's Game Master takes over. The Game Master uses a set of specialized custom prompts (located in `SKSE/Plugins/SkyrimNet/prompts/ostimnet_evaluations`) to manage the granular details of the encounter.

### Pre-Start Sexual Evaluation (`ostimnet_evaluate_prestart_sexual`)
When a sexual scene is initiated by an AI action (e.g. `StartNewSex`), this evaluation runs before the scene actually begins.
- **Willingness Filter:** Checks if all proposed actors are truly willing given the determined intent and their personality profiles.
- **Role Assignment:** Divides the willing participants into `mainActors` and `secondaryActors` based on the intent's dynamic (e.g. initiators vs responders, dominant vs submissive).
- **Scene Details:** Selects the initial sexual position and specific activity (foreplay, oral, sex) that narratively fits the participants.

### External Sexual Thread Evaluation (`ostimnet_evaluate_external_sexual_thread`)
Runs when a sexual scene is started by an external source (another mod, or the player directly through OStim), rather than by the AI. 
- **No Willingness Check:** The scene is already happening, so consent/willingness is bypassed.
- **Intent Inference:** The AI reads the situation and infers what the intent of this ongoing scene *must* be.
- **Role Assignment:** It assigns `mainActors` and `secondaryActors` to properly track the scene moving forward.

### Location Scan Evaluation (`ostimnet_scan_location`)
Used by the [Location Scan](./Config.md#location-scan) feature when the player enters a new area [or using hotkey](./Config.md#location-scan-hotkey).
- **Organic Encounters:** Evaluates all nearby NPCs to see if a plausible intimate encounter would organically occur right now, without any direct initiation.
- **Comprehensive Setup:** Determines intent, participants, roles, position, and furniture.
- **Priority:** Prioritizes NPC life. The player is evaluated equally alongside other NPCs, not automatically included unless an NPC has a genuine reason to pursue them.

### Scene Advancement Evaluation (`ostimnet_evaluate_scene_advance`)
Used by the scheduled [AI Scene Advancement](./Config.md#ai-scene-advancement) feature (if enabled in settings).
- **Periodic Checks:** Runs on a timer while a scene is active.
- **Natural Progression:** Chooses the next sexual position and/or activity that naturally advances the scene, avoiding repeating the current state.
- **Phase Awareness:** Takes into account the current Thread Phase (if enabled) to ensure the scene progresses logically.

### Invite to Sex Evaluation (`ostimnet_evaluate_invite_to_sex`)
Runs when an actor in an ongoing scene invites bystanders to join (`InviteToYourSex`).
- **Bystander Willingness:** Evaluates whether the invited bystanders are willing to join the specific dynamic of the current scene.
- **Role Integration:** Determines what roles the new participants will take upon joining.

### Join Ongoing Sex Evaluation (`ostimnet_evaluate_join_ongoing_sex`)
Runs when a bystander attempts to join an ongoing scene on their own (`JoinOngoingSex`).
- **Mutual Consent:** Evaluates if the bystander wants to join, and crucially, if the current participants are willing to let them join.

### Non-Sexual Scene Evaluation (`ostimnet_evaluate_nonsexual_scene`)
Used for care scenes initiated by `StartCareScene` (hugging, kissing).
- **Platonic vs Romantic:** Evaluates willingness based on the softer dynamics of platonic or romantic intents.
- **No Positions:** Focuses on assigning initiator/responder roles without selecting complex sexual positions.

---

These evaluations work together to ensure that every encounter feels narratively grounded, respects character boundaries, and progresses naturally over time.
