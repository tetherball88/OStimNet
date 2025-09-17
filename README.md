# 🔮 SkyrimNet-OStim Integration 🔥

## Overview

This integration mod creates a powerful bridge between **SkyrimNet** AI framework and **OStim** adult framework for Skyrim, enabling natural language control over intimate encounters. Simply chat with NPCs and the AI will intelligently interpret your dialogue to initiate, modify, or end intimate scenes! 🗣️💬

> ⚠️ **IMPORTANT**: This mod is currently in development and suggested for testing purposes only! Expect potential bugs and ongoing changes.

> 📝 **NOTE**: This integration does NOT alter the main SkyrimNet prompt to make NPCs more sexually forward or explicit. NPCs will maintain their established personalities and relationship boundaries. The mod simply adds capabilities for the AI to understand and interact with OStim functionality when appropriate within character.

## ✨ Features

- 🤖 AI-powered intimate encounters through natural conversation
- 🎭 Context-aware scene selection based on relationships and surroundings
- 🔄 Dynamic scene transitions and participant management
- 📊 Integration with Lover's Ledger for improved relationship tracking
- 📝 Comprehensive animation descriptions for enhanced AI understanding
- 🎮 Seamless gameplay integration that respects character personalities
- 👀 Other NPCs aware of nearby sexual encounters
- 🔄 Support for multiple simultaneous scenes

## 📋 Requirements

- [SkyrimNet](https://github.com/MinLL/SkyrimNet-GamePlugin/tree/main) - Core LLM framework
- [OStim Standalone](https://www.nexusmods.com/skyrimspecialedition/mods/98163) - Adult framework
- [JContainers](https://www.nexusmods.com/skyrimspecialedition/mods/16495) - For handling scene descriptions
- [PapyrusUtil SE](https://www.nexusmods.com/skyrimspecialedition/mods/13048) - Modders Scripting Utility Functions
- [powerofthree's Papyrus Extender](https://www.nexusmods.com/skyrimspecialedition/mods/22854) - Scripting utility
- [Lover's Ledger](https://www.nexusmods.com/skyrimspecialedition/mods/158983) - For enhanced relationship statistics

## 🔧 How It Works

The mod enhances SkyrimNet's capabilities in two key ways:

1. **Action Registration**: Several specialized actions are registered with SkyrimNet, allowing the AI to control OStim functionality through natural dialogue. When you chat with NPCs, the AI analyzes the conversation and can trigger appropriate intimate scenes or modifications.

2. **Context Enrichment**:
   - Comprehensive animation descriptions help the LLM understand what's happening during scenes
   - NPCs are made aware of nearby intimate encounters, allowing for realistic reactions
   - The mod alters conversation context when NPCs are involved in intimate encounters
   - SkyrimNet prompts are enhanced with scene-relevant information without making NPCs artificially forward

## 🎬 Available Actions

The integration provides NPCs with these capabilities:

### 1. `StartNewSex` 🆕
Initiates a new intimate encounter with selected partners. The AI considers personality, relationships, and available nearby characters.

### 2. `ChangeSexPosition` 🔄
Allows modification of the current activity during an ongoing encounter when an NPC desires a different position.

### 3. `InviteToYourSex` ➕
Enables NPCs to invite others to join an ongoing encounter, considering relationship dynamics and potential interest.

### 4. `JoinOngoingSex` 🤝
Allows NPCs to join encounters they are currently observing, considering relationships with participants.

### 5. `ChangeSexPace` ⏩
Adjusts the rhythm and intensity of the current activity while maintaining the same position.

### 6. `StopSex` ⏹️
Ends the current encounter for various reasons including discomfort or changing circumstances.

## 📚 Usage Examples

Simply engage in conversation with NPCs and let the AI interpret your dialogue naturally:

- 💬 *"Would you like to join me somewhere more private?"* might trigger `StartNewSex`
- 💬 During an encounter, saying *"Let's try something different"* might trigger `ChangeSexPosition`
- 💬 *"Perhaps we should invite Lydia to join us"* could trigger `InviteToYourSex`

## 💡 Tips

- NPCs will respond according to their personality, relationship status, and current situation
- Conversations in appropriate locations (inns, homes) may yield more positive responses
- The integration respects character boundaries established in their AI personality

## 🚀 Future Development

- Make OStim running threads finder more dynamic when OStim bug is fixed(right now it looks only at thread ids 0, 1, 2, 3, 4)
- More dynamic registering events(when I figure out how they work)
- Trigger dynamic profile update after scene as significant event

## ⚠️ Note

This mod is intended for adult users and requires all components to be properly installed and configured. Please refer to individual mod pages for specific installation instructions.

---

*Bringing natural intelligence to intimate encounters in Skyrim!* 🏹❤️
