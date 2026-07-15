# 🔮 SkyrimNet-OStim Integration 🔥

## Overview

This integration mod creates a powerful bridge between **SkyrimNet** AI framework and **OStim** adult framework for Skyrim, enabling natural language control over intimate encounters. Simply chat with NPCs and the AI will intelligently interpret your dialogue to initiate, modify, or end intimate scenes! 🗣️💬

> ⚠️ **When you see Skyrim message boxes NEVER use mouse to answer them, it will make sometimes your camera to fly away, it is known issue without known solution. Use keyboard hotkeys to select confirmation option.**

> 📝 **NOTE**: This integration does NOT alter the main SkyrimNet prompt to make NPCs more sexually forward or explicit. NPCs will maintain their established personalities and relationship boundaries. The mod simply adds capabilities for the AI to understand and interact with OStim functionality when appropriate within character.

## ✨ Features

- **[📚 Available Actions](docs/Actions.md)**
- **[⚙️ Configuration Settings](docs/Config.md)**
- **[🎭 Intents, Roles, Scenes & Threads](docs/Intents.md)**
- **[⏳ Thread Phases](docs/Phases.md)**
- **[👀 Spectators System](docs/Spectators.md)**
- **[🧠 AI Encounter Evaluations](docs/Evaluations.md)**
- **[📝 Scene Descriptions](docs/Descriptions.md)**
- **[💬 Confirmation Messages](docs/ConfirmationMessages.md)**
- **💦 OCum Integration** - Enhances AI awareness with fluid overlays and generates more immersive climax narratives.

## 📋 Requirements

### [Recommended animation packs](docs/AnimationPacks.md)

- [SkyrimNet](https://github.com/MinLL/SkyrimNet-GamePlugin/tree/main) - Core LLM framework
- [OStim Standalone](https://www.nexusmods.com/skyrimspecialedition/mods/98163) - Adult framework
- [OStim Navigator v2.0.2+](https://github.com/tetherball88/OStimNavigator/releases/latest) - It became core for OStimNet to provide all OStim scenes related data(furniture, scenes, tags, actions, etc...) and scene description building.
- [OStim Hot Swap](https://www.nexusmods.com/skyrimspecialedition/mods/174565) - for joining/inviting to scene.
- [OCum Ascended](https://www.nexusmods.com/skyrimspecialedition/mods/77506) - Uses it to generate better female orgams descriptions, adds descriptions for cum overlay on npcs in their appearance description
- [PapyrusUtil SE](https://www.nexusmods.com/skyrimspecialedition/mods/13048) - Modders Scripting Utility Functions
- [powerofthree's Papyrus Extender](https://www.nexusmods.com/skyrimspecialedition/mods/22854) - Scripting utility
- [UIExtensions](https://www.nexusmods.com/skyrimspecialedition/mods/17561) - for certain communication menus(manually select intent and/or main/secondary actors)

## 🔧 How It Works

The mod creates a seamless bridge between SkyrimNet and OStim through several key systems:

1. **The Game Master AI**: A specialized AI agent that manages the narrative flow of intimate encounters behind the scenes.
   - Evaluates whether characters are genuinely willing based on their personality and relationship dynamics before a scene starts.
   - Automatically assigns distinct roles (e.g., dominant/submissive, initiator/responder) and infers the scene's *Intent* (Romantic, Lustful, Transactional, Dom, Aggressive).
   - Can automatically advance scenes over time, logically progressing them through natural phases (Undressing → Foreplay → Oral → Sex).

2. **Data layer on top of OStim threads** - OStimNet enreaches OStim threads with additional metadata, allowing the AI to understand the context of ongoing encounters and make informed decisions about how to proceed. It adds to each thread intent evaluated by LLM, tracks thread continuation, tracks climax participants and their climax descriptions.

3. **Action Registration**: Registers specialized actions allowing NPCs to control OStim functionality organically.
   - When you chat with NPCs, the AI can trigger new encounters, change positions, adjust the pace, or invite others to join based on the conversation.
   - Player confirmation modals allow you to accept or decline proposed actions, with the AI reacting naturally to your refusal.

4. **Organic Encounters (Location Scanning)**: The world feels alive even when you aren't involved.
   - When entering a new location (like an inn or a town), the AI invisibly scans nearby NPCs and evaluates if a plausible intimate encounter would naturally occur between them based on their relationships and personalities.

5. **Dynamic Spectators System**: NPCs can discover and react to ongoing scenes.
   - NPCs with existing relationships (spouses, lovers) to participants can automatically discover scenes if they are nearby.
   - The AI can also explicitly command an NPC to become a spectator if it fits the narrative (e.g., a jealous ex, a curious bystander). Spectators might comment, watch, or flee in embarrassment.

6. **Context Enrichment & Event Triggers**: NPCs are fully aware of what's happening to them and around them.
   - Exact animation descriptions provide the LLM with details of the ongoing positions and activities.
   - NPCs dynamically comment during scenes (when starting, changing positions, or climaxing) using intelligent speaker selection.
   - Deep OCum integration enables the LLM to recognize physical fluid layers on actors and dramatically improves the detail of climax scene descriptions.

## 📚 Usage Examples

Simply engage in conversation with NPCs and let the AI interpret your dialogue naturally:

- 💬 *"Would you like to join me somewhere more private?"* might trigger `StartNewSex`
- 💬 During an encounter, saying *"Let's try something different"* might trigger `ChangeSexScenePosition`
- 💬 *"Perhaps we should invite Lydia to join us"* could trigger `InviteToYourSex`

## 💡 Tips

- NPCs will respond according to their personality, relationship status, and current situation
- Conversations in appropriate locations (inns, homes) may yield more positive responses
- The integration respects character boundaries established in their AI personality

## ⚠️ Note

This mod is intended for adult users and requires all components to be properly installed and configured. Please refer to individual mod pages for specific installation instructions.

---

*Bringing natural intelligence to intimate encounters in Skyrim!* 🏹❤️
