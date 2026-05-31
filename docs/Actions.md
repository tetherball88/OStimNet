# 🎬 SkyrimNet Actions ✨

OStimNet registers a set of [SkyrimNet actions](https://goncalo22.github.io/SkyrimNet-GamePlugin/Features/action-system) that the LLM can invoke for eligible NPCs. 🧠 Each action has an eligibility check that runs before the LLM is asked to consider it — if the check fails, the action is invisible to the LLM entirely! 🙈

Actions are subject to a per-actor cooldown. ⏳ If the player declines a proposed action, a separate (longer) decline cooldown applies. 🥶 Both cooldowns are configurable in the mod's settings menu under [Actions](Config.md#actions). ⚙️

---

## 🙋 Player Confirmation

When an NPC proposes an action that involves you, a confirmation modal lets you **Accept**, **Cancel** (silently dismiss), or **Decline with explanation** (tell the LLM exactly why you're saying no!). 🛑✋

For full details on how these messages work, your options, and the related settings, check out the [**Confirmation Messages Guide**](ConfirmationMessages.md)! 💬✨

---

## 🚀 Starting Scenes

These actions are available to any NPC not currently in a scene. 🧍

### 💖 StartNewSex

Initiates a brand-new sex scene. The LLM selects partners, furniture, and intent. Not used to join an existing scene. 🛏️

**Eligibility Requirements:**
- 🟢 OStimNet global flag is active(toggle in SkyrimNet Sexlab integration)
- 👤 Actor is not already in a scene
- 👶 Actor is not a child
- ⚔️ Actor is not in combat
- ⏳ Action is not on cooldown

---

### 🤗 StartCareScene

Initiates a non-sexual physical caring scene (hugging, kissing). Not for affectionate dialogue — only for actual physical care animations! 🫂💕

**Eligibility Requirements:**
- 🟢 OStimNet global flag is active
- 👶 Actor is not a child
- ⚔️ Actor is not in combat
- ⏳ Action is not on cooldown

**Notes:**
- 🫂 Hugging is available for both romantic and platonic intent
- 💋 Kissing is only available for romantic intent
- ⏱️ Care scenes end automatically after a configurable duration (default: 20 seconds)

---

### 🏃‍♀️ JoinOngoingSex

An NPC who is not in a scene joins an existing active sex scene as a full participant! 👯‍♂️

**Eligibility Requirements:**
- 🟢 OStimNet global flag is active
- 👶 Actor is not a child
- ⚔️ Actor is not in combat
- 🎭 At least one active scene exists
- ⏳ Action is not on cooldown

---

## 🎬 During a Scene

These actions are only available to NPCs who are already active participants in a sex scene. 🎭

### 🔄 ChangeSexScenePosition

Switches the current sex scene to a different position, activity, or both! Either parameter can be omitted to keep the current value. 🤸

**Eligibility Requirements:**
- 🎭 Actor is in an active sexual scene
- ⏳ Action is not on cooldown

---

### ⏩ ChangeSexScenePace

Increases or decreases the current animation speed (intensity/pace) of the scene. 🏎️💨

**Eligibility Requirements:**
- 🎭 Actor is in an active sexual scene
- ⚙️ The scene has more than one available speed (i.e., there is room to change)
- ⏳ Action is not on cooldown

---

### 🎭 ChangeSexSceneIntent

Shifts the current scene to a different intent, changing its dynamic (e.g., from lustful to dom or from romantic to aggressive). 😈💕

**Eligibility Requirements:**
- 🎭 Actor is in an active sexual scene
- ⏳ Action is not on cooldown

**Notes:**
- 🛡️ Aggressive intent can be disabled globally in the mod settings under [**Intents**](Config.md#intents)
- 🛑 A confirmation modal for aggressive intent is shown to the player by default (configurable in [**Intents**](Config.md#intents))

---

### 💌 InviteToYourSex

An NPC already in a scene invites one or more bystanders to join as active participants! 🎉

**Eligibility Requirements:**
- 🎭 Actor is in an active sexual scene
- 👥 The scene currently has fewer than 5 participants
- ⏳ Action is not on cooldown

---

### 🛑 StopSex

Permanently ends the current sex scene with no intention to continue. Not used for pauses or transitions — only for a full stop! ⛔

**Eligibility Requirements:**
- 🎭 Actor is in an active sexual scene
- ⏳ Action is not on cooldown

---

## 👀 Spectating

These actions let NPCs outside of a scene watch it, and let existing spectators leave. 🍿

### 😲 SpectatorOfSex

Makes an NPC become a spectator of an ongoing scene. There is no relationship requirement — the LLM can send any eligible NPC to watch based on narrative context (curiosity, jealousy, happenstance, etc.). 🫣

**Eligibility Requirements:**
- 🎭 At least one active scene exists
- 👤 Actor is not already a participant in any scene
- 👀 Actor is not already a spectator
- 👶 Actor is not a child
- ⚔️ Actor is not in combat

**Notes:**
- 🚶‍♂️ Once a spectator, the NPC follows their target using AI packages and may comment on or react to the scene
- 🔍 The auto-scan system can also create spectators automatically for NPCs with relationship ties to scene participants (see [Spectators.md](Spectators.md))

---

### 🏃💨 SpectatorOfSexFlee

Makes a current spectator stop watching and leave the scene. The LLM decides when fleeing is appropriate — embarrassment, discomfort, fear of being caught, etc. 😳

**Eligibility Requirements:**
- 👀 Actor is currently a spectator (in the spectator faction)
- 🏃💨 Actor is not already in the process of fleeing

---

## 🙅‍♀️ Action Decline

When the player declines a proposed action during a scene, nearby NPCs receive a `tton_action_decline` event. 📡 The LLM reacts naturally in the moment without repeating the attempt or planning ahead. A separate, configurable decline cooldown prevents the same action from being re-proposed immediately. See [**Actions**](Config.md#actions) in the config reference! 🥶

---

## 🎭 SkyrimNet Triggers

Triggers are the way OStimNet tells NPCs how to react to events that happen during scenes! When something significant occurs (like a scene starting or someone climaxing), the trigger fires and tells the LLM to generate a fun, in-character response. 🗣️✨ 

You can configure and completely customize these through the **SkyrimNet Web Dashboard** (check out the full [SkyrimNet Triggers Documentation](https://goncalo22.github.io/SkyrimNet-GamePlugin/Features/customtriggers) for the nitty-gritty details!). 🛠️

Here are all the specific OStimNet triggers that fire for each major event (check your dashboard to toggle them!):

### 🎬 Scene Start (`tton_sex_start`)
Fires the moment NPCs begin an intimate scene!
- **Default Status:** Enabled ✅
- **Priority:** `2`
- **Cooldown:** `30s`

### 🤸 Position / Pace Change (`tton_sex_position_change`)
Fires when the scene transitions to a new position OR the pace changes! 🔄
- **Default Status:** Enabled ✅
- **Priority:** `1`
- **Cooldown:** `20s`

### 🎆 Climax (`tton_sex_climax`)
Fires when an NPC orgasms! This one has a high priority so it overrides things like position changes. 💥
- **Default Status:** Enabled ✅
- **Priority:** `10`
- **Cooldown:** `30s`

### 🛑 Scene End (Comment) (`tton_sex_stop_comment`)
Provides a spoken reaction when the scene completely finishes.
- **Default Status:** Disabled ❌ (You can enable it if you want them to talk afterwards!)
- **Priority:** `2`
- **Cooldown:** `30s`

### 📓 Scene End (Diary) (`tton_sex_stop_diary`)
Generates a **Diary Entry** saved to the NPC's memory instead of spoken dialogue when the scene ends! ✍️
- **Default Status:** Disabled ❌ (Enable this for deep memory building!)
- **Priority:** `1`
- **Cooldown:** `0s`

### 🙅‍♀️ Action Decline (`tton_action_decline`)
Fires when the player refuses an NPC's request! 💬
- **Default Status:** Enabled ✅
- **Priority:** `3`
- **Cooldown:** `30s`

### 👀 Spectator Added (`tton_spectator_added`)
Fires when a new spectator begins watching the scene! 🫣
- **Default Status:** Enabled ✅
- **Priority:** `1`
- **Cooldown:** `30s`

### 🏃💨 Spectator Fled (`tton_spectator_fled`)
Fires when a spectator decides to leave the scene! 😳
- **Default Status:** Disabled ❌
- **Priority:** `1`
- **Cooldown:** `30s`
