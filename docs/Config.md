# Configuration

All settings live in the SkyrimNet mod menu under the **OStimNet** plugin.

---

## General

### Use custom descriptions (default: on)

OStimNet can describe what's happening in a scene in two ways. **Custom descriptions** are hand-authored and generally read more naturally, but use fixed pronouns and anatomy terms — so a strap-on will always be called a penis, for example. **Auto-generated descriptions** are produced by OStim Navigator using OStim's scene tags and actions. They as precise as scenes author made them. They usually less accurate or miss some details but more suitable for player who uses non regular setup like futa, female actors in male roles or don't want **custom descriptions**'s pronounces.

### Care scene duration (seconds) (default: 20s)

How long a non-sexual care scene (a hug, a kiss) lasts before it ends automatically. The default is 20 seconds. This only applies to scenes started by the [StartCareScene](Actions.md#startcarescene) action — the LLM doesn't manually end them.

### OStim scene change debounce (seconds) (default: 2s)

OStimNet waits a short moment before reacting to rapid events so it doesn't fire a dozen eventes in a row when someone changes positions twice in quick succession. 2s work well in practice. Raise them if you're seeing comment spam; lower them if reactions feel sluggish.

### Climax debounce (seconds) (default: 2s)

OStim can fire multiple climax events in a row. If multiple climax events happened in this 2 seconds they all we be fired as single SkyrimNet event describing everyone climax details. OStimNet waits this long before reacting to another climax event. The default of 2 seconds works well in practice. Raise it if you're seeing comment spam; lower it if reactions feel sluggish.

### Speed change debounce (seconds) (default: 5s)

OStim can fire multiple speed change events in a row. OStimNet waits this long before reacting to another speed change event. The default of 5 seconds works well in practice. Raise it if you're seeing comment spam; lower it if reactions feel sluggish.

### Game Master LLM variant (default: `gamemaster_evaluation`)

Which AI profile the Game Master uses when it decides how to advance a scene. The default (`gamemaster_evaluation`) is tuned for scene decisions. Only change this if you have a specific reason — for example, using a lighter model for performance. Keep in mind it requires capable models of processing complex prompts.

### Nearby actors scan radius (default: 2000 units)

When an NPC considers starting a scene, OStimNet scans for potential partners within this distance. The default (2000 units, roughly 28 metres) covers most indoor spaces comfortably. Increase it if NPCs in large open areas seem to ignore each other; decrease it if you want scenes to only happen between NPCs who are physically close.

### Proximity pause radius (default: 200 units)

With OstimNet you have 2 ways to advance npc scenes. Vanilla OStim auto-mode, where scenes changes on timer. Another one is scheduled [AI Scene Advancement](#ai-scene-advancement). There is certain situations when player want to engage with npc scene and not to be interrupted by automatic scene advancement. This setting allows you to pause automatic scene advancement when player is within certain distance from any npc in the scene. Set it to 0 to never pause scenes. 1 game unit ≈ 14.3 mm. Default 200 ≈ 2.8 m.

### Enable thread phases (default: on)

When enabled, sexual scenes progress gradually through phases (undressing → foreplay → oral → sex). Each intent has its own phase sequence — romantic starts with undressing, lustful skips to foreplay, dom/aggressive go straight to sex. When disabled, scene selection uses the original random behaviour.

---

## Hot Keys

### Toggle mute

A hotkey to silence all NPC comments mid-session. Useful if you want a quiet moment without changing the permanent setting. It resets when you reload the game, so your saved **Mute comments** preference is restored.

### Location scan hotkey

Manually fires a location participant scan at any time. Handy if you fast-travelled somewhere and the automatic scan already fired before the NPCs you wanted were in range. Manual scans always bypass the cooldown.

---

## Intents

Scenes and threads in OStim can carry an intent tag — romantic, lustful, transactional, dom, or aggressive — which reflects the dynamic of the animation. OStimNet uses this to pick animations that match the mood of the encounter.

### OStim scenes intent rule (default: on)

When this is on (the default), OStimNet only picks animations that make sense for the chosen intent. A romantic encounter won't pull a rough dom animation; an aggressive scene won't default to gentle lovemaking. The matching is intentionally permissive — romantic allows lustful scenes, transactional allows romantic ones — so you still get variety without jarring mismatches. Turn this off only if you want completely random animation selection regardless of context.

### Enable aggressive intent (default: off)

When enabled, the LLM can start or shift a scene to an aggressive dynamic — for example, if an NPC's personality and the current situation make a non-consensual encounter narratively plausible. **Disable this if you don't want any aggressive scenes under any circumstances.** It acts as a hard block regardless of what the LLM decides.

### Show confirmation modal for actions with aggressive intent (default: off)

If aggressive intent is enabled and aggressive action is performed player won't get confirmation modal. If you want to get confirmation modal for aggressive actions enable this setting. Gives you a chance to refuse even if the LLM decided it made sense.

---

## Comments

### Comments prioritize nearest thread

When multiple scenes are running at once (e.g. the player's scene and a nearby NPC scene), this makes OStimNet prefer the closest scene to the player when deciding who speaks. The player's own scene always wins outright. Without this, whichever scene fires an event first claims the comment slot, which can leave your scene silent while a distant NPC pair chatters away.

### Comment gender priority(0 - always male, 100 - alway female) (default: 50)

When choosing which actor in a scene delivers a comment, this sets how often the female actor is picked over the male. The default 50 is a coin flip. Set it toward 100 if you want women to speak more often, toward 0 for men. Climax comments ignore this setting and use their own smarter logic (who climaxed hardest, who was the target, etc.).

### Mute comments (default: off)

Turns off all NPC scene commentary. It is permanent setting carried over between reloads. Can also be toggled in-game with the mute hotkey without touching this setting permanently.

---

## Spectators

When a scene starts, OStimNet can make nearby NPCs discover and react to it — a jealous spouse walking in, a lover who wasn't expecting this, a stranger who happens to be nearby. See [Spectators.md](Spectators.md) for a full breakdown of how it works.

### Enable spectators (default: on)

Master switch. Turn this off if you want scenes to be entirely private with no automatic interruptions from other NPCs.

### Spectator scan interval (seconds) (default: 5s)

How often OStimNet checks for new potential spectators while a scene is active. Lower values mean a jealous NPC finds out sooner; higher values mean more breathing room before anyone notices. The default of 5 seconds is a reasonable balance.

### Spectator scan radius (default: 3000 units)

How far away a potential spectator can be and still be detected. The default (roughly 43 metres) covers most dungeon rooms and inn interiors. Reduce it if you want only NPCs in the immediate area to notice. 1 game unit ≈ 14.3 mm. Default 3000 ≈ 43 m.

---

## Actions

See [Actions.md](Actions.md) for a full list of what actions the LLM can take.

### Actions cooldown (seconds) (default: 20s)

After an NPC takes an action — say, changing the scene or inviting someone to join — they can't take that same action again for this many seconds. This prevents an NPC from spamming the same move over and over. The default of 20 seconds gives time for the scene to settle before the AI reconsiders. If your LLM or TTS takes longer to produce dialogue increase this cooldown.

### Actions decline cooldown (seconds) (default: 40s)

When you decline an NPC's proposed action, a longer cooldown applies specifically to that NPC for that action. Setting this higher than the normal cooldown makes a refusal feel meaningful — the NPC backs off properly rather than immediately trying again. Default is 40 seconds.

### Confirmation modal for NPC-only scenes (default: off)

When NPCs start a scene among themselves (no player involved), this controls whether you get a confirmation popup first. Off by default — NPC scenes just happen. Turn it on if you want the ability to veto NPC scenes before they start.

---

## Location Scan

Every time you pass through a loading screen into a new area, OStimNet can scan the surroundings and ask the LLM whether any NPCs nearby are likely candidates for an intimate scene. This is what drives "organic" encounters where NPCs approach you or each other shortly after you arrive somewhere.

### Enable location scan (default: on)

Master switch. Disable if you only want scenes to start through direct NPC actions, not from automatic location-entry suggestions.

### Location scan delay (seconds) (default: 8s)

After the loading screen closes, OStimNet waits this many seconds before scanning. This exists because NPCs take a moment to fully spawn and settle into their routines — scanning too early can miss half the room. Increase it (up to 60s) if you're on a slow machine or in a heavily scripted area where NPCs are late to load.

### Location scan cooldown (seconds) (default: 120s)

Minimum time between automatic scans. Prevents every loading screen from producing a suggestion. The default of 120 seconds (2 minutes) means back-to-back fast travel won't spam you with scene prompts. Set it to 0 to remove the cooldown entirely, or raise it if suggestions feel too frequent.

### Location type filters

Controls which types of locations trigger a scan at all. By default, towns, cities, inns, guilds, and unnamed locations are enabled; dungeons are disabled. Enabling dungeons means OStimNet will also scan for willing participants when you enter a dungeon, which can make for interesting encounters but may feel tonally out of place depending on your playstyle.

#### Scan in settlements (default: on)

Should scans fire in towns and cities. This includes any location with a settlement keyword, including villages, towns, and cities.

#### Scan in inns (default: on)

Should scans fire in inns, taverns. This includes any location with an inn keyword.

#### Scan in guilds (default: on)

Should scans fire in guild halls. This includes any location with a guild keyword.

#### Scan in player homes (default: on)

Should scans fire in the player's own home. This includes any location with the `LocTypePlayerHouse` keyword — Breezehome, Honeyside, Hjerim, etc.

#### Scan in dwellings (default: on)

Should scans fire in generic dwellings and houses. This includes any location with a `LocTypeDwelling` or `LocTypeHouse` keyword — NPC homes, farmhouses, and other residential interiors that aren't inns or player-owned.

#### Scan in dungeons (default: off)

Should scans fire in dungeons. This includes any location with a dungeon keyword.

#### Scan in other locations (default: on)

Should scans fire in locations that do not match any recognized type or have no named location.

---

## AI Scene Advancement

Normally, scene changes with OStim auto-mode on scheduled timer. With OStimNet it can be also controlled by an NPC takes the [ChangeSexScenePosition](Actions.md#changesexsceneposition) action. AI Scene Advancement is an alternative: OStimNet advances the scene automatically on a timer, without waiting for the LLM to decide to take an action. It disables OStim's auto-mode. The Game Master picks a new position and activity every N seconds.

### Player thread (default: off)

Whether automatic advancement applies to scenes the player is in. Off by default — most players prefer control over their own scene. Enable it if you want the scene to evolve on its own without any input.

### NPC threads (default: on)

Whether automatic advancement applies to NPC-only scenes. On by default. This makes NPC scenes feel alive and progressing naturally even when the player isn't paying attention.

### Interval (seconds) (default: 60s)

How many seconds between each automatic advancement. The default of 60 seconds gives scenes time to breathe. Lower it (minimum 10s) for faster-paced scenes; raise it (up to 600s) if you want long stretches on the same position before anything changes.

---

## Undressing

### Enable undressing(player thread) (default: on)

Enable undressing phase in player thread.

### Enable undressing(npc thread) (default: on)

Enable undressing phase in npc thread.

### Undressing approach(player thread) (default: OStim)

Select the undressing approach to use in player thread.
- **OStim**: Default undressing approach, second actor undresses themself, second actor undresses first.
- **OARE**: Normal undressing speed, second actor mostly undresses themself, first actor helps with top cloth. Second actor undresses first.
- **OSA**: Slowest undressing, first actor helps second actor to undress all parts of clothing. Second actor helps first actor to undress all parts of clothing. First actor undresses first.
- **OSAFirstFast**: Faster than OSA undressing, first actor helps second actor to undress all parts of clothing. Second actor helps first actor to undress fast.

### Undressing approach(npc thread) (default: OStim)

Select the undressing approach to use in npc-npc thread.
- **OStim**: Default undressing approach, second actor undresses themself, second actor undresses first.
- **OARE**: Normal undressing speed, second actor mostly undresses themself, first actor helps with top cloth. Second actor undresses first.
- **OSA**: Slowest undressing, first actor helps second actor to undress all parts of clothing. Second actor helps first actor to undress all parts of clothing. First actor undresses first.
- **OSAFirstFast**: Faster than OSA undressing, first actor helps second actor to undress all parts of clothing. Second actor helps first actor to undress fast.
