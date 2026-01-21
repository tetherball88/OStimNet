# Spectators System

The Spectators system allows NPCs to dynamically discover and react to intimate OStim scenes. NPCs can become spectators through two mechanisms: AI-driven decisions via SkyrimNet actions, or automatic detection of romantic partners in scenes.

## Overview

Spectators are NPCs who watch ongoing OStim scenes. Once an NPC becomes a spectator, they will follow their target using AI packages and can comment on the scene or flee based on SkyrimNet AI decisions.

There are **two ways** an NPC can become a spectator:

### 1. SkyrimNet AI Actions (Any NPC)
The LLM can decide that any eligible NPC should become a spectator by invoking the `SpectatorOfSex` action. This is not limited by relationships - if the AI determines an NPC would realistically watch a scene (curiosity, jealousy, happenstance, etc.), they can become a spectator.

### 2. Auto-Scan System (Relationship-Based)
A background scan periodically checks for NPCs with romantic connections to scene participants. This ensures that spouses, lovers, and courting partners have opportunities to discover their partners in intimate scenes, even if the LLM doesn't explicitly trigger it.

## Relationship Detection (Auto-Scan Only)

The auto-scan checks for the following relationship types (in priority order):

1. **Spouse** - Married partners
2. **Courting** - NPCs currently in a courtship
3. **Lovers** - Any lover relationships

## SkyrimNet Integration

### Actions

The spectator system registers two SkyrimNet actions that the LLM can invoke for **any eligible NPC**:

#### SpectatorOfSex
Allows the LLM to make any NPC become a spectator of an ongoing scene. The AI decides when this action makes narrative sense - it could be a jealous ex, a curious bystander, a nosy neighbor, or anyone the LLM determines would realistically watch the scene.

| Property | Value |
|----------|-------|
| Name | `SpectatorOfSex` |
| Script | `TTON_Actions` |
| Eligibility | `SpectatorOfSexIsEligible` |
| Execute | `SpectatorOfSexActionExecute` |
| Parameters | `{"target": "Person in sex scene to watch (spectate)"}` |

**Eligibility Requirements:**
- Spectators can still be added (not at max capacity)
- SexLab is not in charge of the framework
- Actor is not busy with other scenes
- Actor is OStim eligible
- Actor is not already in the spectator faction

**Note:** There is no relationship requirement - any NPC meeting these criteria can be made a spectator by the LLM.

#### SpectatorOfSexFlee
Allows the LLM to make a spectator flee from the scene they are watching. The AI decides when fleeing makes sense - embarrassment, fear of being caught, emotional distress, etc.

| Property | Value |
|----------|-------|
| Name | `SpectatorOfSexFlee` |
| Script | `TTON_Actions` |
| Eligibility | `SpectatorOfSexFleeIsEligible` |
| Execute | `SpectatorOfSexFleeActionExecute` |
| Parameters | None |

**Eligibility Requirements:**
- Actor is in the spectator faction (currently a spectator)
- Actor is not already in the flee faction (not already fleeing)

### Events

The system fires the following SkyrimNet events:

#### spectator_added
Fired when an NPC successfully becomes a spectator.

**Message format:** `"{SpectatorName} decided to watch {ActorNames} having intimate encounter."`

**Event data:**
- Type: `spectator_added`
- ThreadID: The OStim thread being watched
- Speaking Actor: The spectator

#### spectator_fled
Fired when a spectator flees from the scene.

**Message format:** `"{SpectatorName} was watching {ActorNames} having intimate encounter but fled the scene."`

**Event data:**
- Type: `spectator_fled`
- ThreadID: The OStim thread that was being watched
- Speaking Actor: The fleeing spectator

### Triggers

Spectator events respect the following trigger conditions:
- **Mute setting**: Events won't trigger speech if muted
- **Player thread priority**: Non-player thread events can be suppressed
- **Skip trigger flag**: Prevents duplicate triggers during scene changes

## Auto Scan System

In addition to LLM-driven spectator creation, the system includes an automatic scanning mechanism that specifically looks for NPCs with **romantic relationships** to scene participants. This ensures dramatic encounters (spouse catching partner cheating, etc.) happen reliably without depending solely on LLM decisions.

### How It Works

1. When an OStim scene starts and spectators are enabled, the scan loop begins
2. Every N seconds (configurable), the system scans for NPCs within 800 units of the player
3. For each NPC found:
   - Check if they're not already in an OStim scene
   - Check if they're not already a spectator
   - **Check relationships** for spouses, courting partners, or lovers in active scenes
4. If a matching romantic partner is found, attempt to make the NPC a spectator
5. The spectator must successfully approach within 310 units (with up to 12 attempts, 5 seconds apart)
6. The loop continues while:
   - There are active OStim threads
   - Spectators are enabled in MCM
   - The scan loop hasn't been manually stopped
   - There's capacity for more spectators

### Difference from SkyrimNet Actions

| Mechanism | Who Can Become Spectator | Trigger |
|-----------|-------------------------|---------|
| SkyrimNet Action | Any eligible NPC | LLM decision |
| Auto Scan | Only NPCs with romantic ties to participants | Automatic periodic check |

Both mechanisms work together - the auto-scan ensures relationship-based drama while SkyrimNet actions allow for broader AI-driven spectator scenarios.

### Scan Loop Control

The scan loop can be:
- **Started automatically** when scenes begin
- **Stopped automatically** when all scenes end or spectators are disabled
- **Stopped manually** via MCM button (useful if the loop gets stuck)

## MCM Options

All spectator settings are found in the **Spectators** section of the OStimNet MCM.

### Enable spectators
Toggle to enable or disable the entire spectator system.
- **Default:** Enabled
- When disabled, spectator actions are not registered with SkyrimNet

### Max spectators overall
Maximum number of spectators allowed across all active OStim threads.
- **Range:** 0-20
- **Default:** 5
- **Interval:** 1

### Max spectators per thread
Maximum number of spectators allowed for each individual OStim thread.
- **Range:** 0-10
- **Default:** 2
- **Interval:** 1

### Comment weight (0=Participant, 100=Spectator)
Controls the likelihood of spectators vs participants being chosen to comment during scenes.
- **Range:** 0-100
- **Default:** 50
- **Interval:** 1
- At 0: Only scene participants comment
- At 50: Equal chance for both
- At 100: Only spectators comment

### Scan interval (seconds)
How often the auto-scan searches for potential spectators.
- **Range:** 5-60 seconds
- **Default:** 10 seconds
- **Interval:** 5
- Lower values detect spectators faster but use more system resources

### Clear spectators data
Button to remove all active spectators and clear spectator-related storage data.
- Shows confirmation dialog before executing
- Useful for troubleshooting or resetting stuck spectators

### Stop spectator scan loop
Button to manually stop the spectator scan loop if it becomes stuck.
- Shows status message indicating if the loop was running or not
- Useful for troubleshooting when the scan loop doesn't stop automatically

## AI Packages

Spectators use custom AI packages to control their behavior:

| Package | Purpose |
|---------|---------|
| Spectator Follow | Makes the spectator follow and watch their target |
| Spectator Flee (Editor Location) | Makes the spectator flee to their home location |
| Spectator Flee (Far) | Alternative flee package for NPCs without valid editor locations(any npc will run to Ivarstead map marker regardles of current location) |

### Cleanup

Spectator data is cleaned up:
- When a spectator is removed individually
- When spectators are cleared for a specific thread (scene ends)
- When all spectators are cleared (via MCM or save game load)
- Package overrides and faction memberships are properly removed
