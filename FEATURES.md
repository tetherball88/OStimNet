# Papyrus - AI-Enhanced Intimate Encounters 🔥

An advanced mod that integrates AI-powered functionality with OStim to create dynamic, context-aware intimate encounters through natural conversation and intelligent scene management.

## ✨ Key Features

- 🤖 **AI-Powered Interactions**: Experience intimate encounters through natural conversation with NPCs
- 🎭 **Context-Aware Scenes**: Dynamic scene selection based on relationships and surroundings
- 🔄 **Intelligent Scene Management**: Smooth transitions and participant management during encounters
- 📊 **Lover's Ledger Integration**: Enhanced relationship tracking and history
- 📝 **Detailed Animation Descriptions**: Comprehensive animation metadata for improved AI understanding
- 🎮 **Immersive Gameplay**: Seamless integration that respects character personalities and relationships
- 👀 **Environmental Awareness**: NPCs react and respond to nearby intimate encounters
- 🔄 **Multi-Scene Support**: Handle multiple simultaneous scenes across the game world

## 🚀 Features Added in v0.1.0+

- 🪑 **Environmental Interaction**: Start sex action now considers nearby furniture options
- ✅ **Player Confirmations**: Added confirmation messages for key OStim actions:
  - 🏁 Starting intimate encounters
  - 🔄 Changing scenes
  - 👋 Inviting or joining other NPCs
  - 🛑 Ending encounters
- ⚙️ **Customizable Notifications**: Toggle which confirmation messages you want to see through MCM
- 💬 **AI Commentary System** (`RequestSexComment`):
  - 🎯 Triggers the AI to generate contextual direct narrations during intimate encounters
  - ⏱️ Activates during scene changes, climax events, after encounters, or when declining certain actions
  - 🎲 Randomly selects one participant as the "speaker" for variety in perspectives
  - ⚖️ Includes configurable gender weighting to control which participants are selected
- 👥 **Improved Multi-Actor Handling**: Better management when inviting or joining NPCs to ongoing scenes
- ⏳ **Reaction Cooldowns**: Adjustable timers prevent NPCs from repeatedly requesting the same action

## ⚙️ Configuration (MCM Settings)

- 📱 **Confirmation Messages**: Enable/disable various confirmation dialogs: `Enable start sex confirmation modal`/`Enable change scene confirmation modal`/`Add another actors confirmation modal`/`Enable stop sex confirmation modal`
- ⏰ **RequestSexComment Cooldown**: Adjust timing between AI narration requests (recommended ~40 seconds depending on your LLM and audio generation setup): `Cooldown between triggering sex comments`
- 👫 **Gender Preference for Comments**: Set probability weights for which gender is selected for commentary: `Which gender has more chances to start comment`
- 🔒 **Action Denial Cooldown**: Prevent repeated requests from NPCs after player declines an action: `Action cooldown after player's deny`. It works per npc, if npc1 one wanted to perform some OStimNet action but was denied by player - npc1 won't be able to use this action for next X seconds. Meanwhile if npc2 will try same action it will be available for them.

## 📝 Usage Notes

- ⚠️ Setting RequestSexComment cooldown too low (or to 0) will create a large queue of narration requests
- 🔄 When inviting/joining NPCs, the system now treats it as same encounter thread rather than generating separate stop/start events
- 🚫 The mod now tracks when players deny an NPC's request to stop an encounter as event
- 🔁 Too low deny cooldown setting can lead to NPCs spamming the same action again and again (if they are stubborn enough)
