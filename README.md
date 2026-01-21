# 🔮 SkyrimNet-OStim Integration 🔥

## Overview

This integration mod creates a powerful bridge between **SkyrimNet** AI framework and **OStim** adult framework for Skyrim, enabling natural language control over intimate encounters. Simply chat with NPCs and the AI will intelligently interpret your dialogue to initiate, modify, or end intimate scenes! 🗣️💬

> ⚠️ **IMPORTANT**: This mod is currently in development and suggested for testing purposes only! Expect potential bugs and ongoing changes.

> 📝 **NOTE**: This integration does NOT alter the main SkyrimNet prompt to make NPCs more sexually forward or explicit. NPCs will maintain their established personalities and relationship boundaries. The mod simply adds capabilities for the AI to understand and interact with OStim functionality when appropriate within character.

## ✨ Features

**[📚 View complete features documentation](FEATURES.md)**

## 📋 Requirements

- [SkyrimNet](https://github.com/MinLL/SkyrimNet-GamePlugin/tree/main) - Core LLM framework
- [OStim Standalone](https://www.nexusmods.com/skyrimspecialedition/mods/98163) - Adult framework
- [JContainers](https://www.nexusmods.com/skyrimspecialedition/mods/16495) - For handling scene descriptions
- [PapyrusUtil SE](https://www.nexusmods.com/skyrimspecialedition/mods/13048) - Modders Scripting Utility Functions
- [powerofthree's Papyrus Extender](https://www.nexusmods.com/skyrimspecialedition/mods/22854) - Scripting utility
- ⚠️~~[Lover's Ledger](https://www.nexusmods.com/skyrimspecialedition/mods/158983) - For enhanced relationship statistics~~ Not needed since version `v0.7.0` Lover's Ledger prompts moved to separate integration [Lover's Neural Ledger](https://github.com/tetherball88/Lovers-Neural-Ledger)
- ⚠️ [Papyrus MessageBox - SKSE NG](https://www.nexusmods.com/skyrimspecialedition/mods/83578) new from version `v0.1.0`

## 🔧 How It Works

The mod enhances SkyrimNet's capabilities in three key ways:

1. **Action Registration**: Several specialized actions are registered with SkyrimNet, allowing the AI to control OStim functionality through natural dialogue. When you chat with NPCs, the AI analyzes the conversation and can trigger appropriate intimate scenes or modifications.

2. **Context Enrichment**:
   - Comprehensive animation descriptions help the LLM understand what's happening during scenes
   - NPCs are made aware of nearby intimate encounters, allowing for realistic reactions
   - The mod alters conversation context when NPCs are involved in intimate encounters
   - SkyrimNet prompts are enhanced with scene-relevant information without making NPCs artificially forward
   - Pre-computed sexual behavior statistics and lover insights cached for performance

3. **Event-Driven Triggers**: The mod leverages SkyrimNet's trigger system for dynamic NPC reactions:
   - NPCs comment when scenes start, positions change, or climax occurs
   - Configurable YAML triggers allow server-side customization without script changes
   - Intelligent speaker selection ensures reactions come from the most relevant NPC
   - Diary entries automatically generated when scenes end

## 🎬 Available Actions

The integration provides NPCs with these capabilities:

### 1. `StartNewSex` 🆕
Initiates a new intimate encounter with selected partners. The AI considers personality, relationships, and available nearby characters.

### 2. `ChangeSexActivity` 🔄
Allows modification of the current activity during an ongoing encounter when an NPC desires a different position.

### 3. `InviteToYourSex` ➕
Enables NPCs to invite others to join an ongoing encounter, considering relationship dynamics and potential interest.

### 4. `JoinOngoingSex` 🤝
Allows NPCs to join encounters they are currently observing, considering relationships with participants.

### 5. `ChangeSexPace` ⏩
Adjusts the rhythm and intensity of the current activity while maintaining the same position.

### 6. `StopSex` ⏹️
Ends the current encounter for various reasons including discomfort or changing circumstances.

### 7. `SpectatorOfSex` 👀 (Optional)
Allows the AI to make any eligible NPC become a spectator of an ongoing scene. The AI decides when this makes narrative sense - a jealous ex, curious bystander, or anyone who would realistically watch.

### 8. `SpectatorOfSexFlee` 🏃 (Optional)
Allows a spectator to flee from the scene they are watching due to embarrassment, fear, or emotional distress.

## 👀 Spectators System (Optional)

The Spectators system allows NPCs to dynamically discover and react to intimate scenes. This feature is **disabled by default** and can be enabled in MCM.

**[📖 View complete Spectators documentation](docs/Spectators.md)**

### How NPCs Become Spectators

1. **AI-Driven**: The LLM can decide any eligible NPC should watch a scene (no relationship required)
2. **Auto-Scan**: Background scan finds NPCs with romantic ties (spouse, lover, courting) to scene participants

### Key Features

- Spectators follow and watch their target using AI packages
- Can comment on scenes or flee based on AI decisions
- Configurable limits (max spectators overall and per thread)
- Adjustable comment weight between participants and spectators

## 📚 Usage Examples

Simply engage in conversation with NPCs and let the AI interpret your dialogue naturally:

- 💬 *"Would you like to join me somewhere more private?"* might trigger `StartNewSex`
- 💬 During an encounter, saying *"Let's try something different"* might trigger `ChangeSexActivity`
- 💬 *"Perhaps we should invite Lydia to join us"* could trigger `InviteToYourSex`

## 💡 Tips

- NPCs will respond according to their personality, relationship status, and current situation
- Conversations in appropriate locations (inns, homes) may yield more positive responses
- The integration respects character boundaries established in their AI personality

## 🔧 Advanced Configuration

### Trigger System (v0.6.0+)
Server administrators can customize NPC reaction behavior by editing YAML files in `SKSE/Plugins/SkyrimNet/config/triggers/`:

- **tton_sex_start.yaml** - Controls how NPCs react when they begin intimate scenes
- **tton_sex_change.yaml** - Manages position change commentary
- **tton_sex_climax.yaml** - High-priority orgasm reactions
- **tton_sex_stop.yaml** - Scene ending diary generation
- **tton_action_decline.yaml** - Player refusal evaluation

Each trigger supports:
- Cooldown timers (`cooldownSeconds`)
- Probability settings (`probability`: 0.0-1.0)
- Priority levels (`priority`: higher = more important)
- Custom AI prompts (`response.content`)

### For Template Authors
Templates can access cached data directly via PapyrusUtil:
```jinja
{% set threadID = papyrus_util("GetIntValue", npc.UUID, "TTONDec_ThreadParticipant", -1) %}
{% set lovers = papyrus_util("GetFormList", actorUUID, "TTONDec_SexualData_Lovers") %}
```

See [FEATURES.md](FEATURES.md) for complete documentation.

## 🚀 Future Development

- Make OStim running threads finder more dynamic when OStim bug is fixed
- Trigger dynamic profile update after scene as significant event
- Additional trigger types for more granular reactions

## ⚠️ Note

This mod is intended for adult users and requires all components to be properly installed and configured. Please refer to individual mod pages for specific installation instructions.

---

*Bringing natural intelligence to intimate encounters in Skyrim!* 🏹❤️
