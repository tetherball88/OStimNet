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

## How It Works

1. When an OStim scene starts and spectators are enabled, the scan loop begins
2. Every N seconds ([configurable](./Config.md#spectator-scan-interval-seconds-default-5s)), the system scans for NPCs within 3000([configurable](./Config.md#spectator-scan-radius-default-3000-units)) units of the player
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

