# Papyrus - AI-Enhanced Intimate Encounters 🔥

An advanced mod that integrates AI-powered functionality with OStim to create dynamic, context-aware intimate encounters through natural conversation and intelligent scene management.

## ✨ Key Features

- 🤖 **AI-Powered Interactions**: Experience intimate encounters through natural conversation with NPCs
- 🎭 **Context-Aware Scenes**: Dynamic scene selection based on relationships and surroundings
- 🔄 **Intelligent Scene Management**: Smooth transitions and participant management during encounters
- 📝 **Detailed Animation Descriptions**: Comprehensive animation metadata for improved AI understanding
- 🎮 **Immersive Gameplay**: Seamless integration that respects character personalities and relationships
- 👀 **Environmental Awareness**: NPCs react and respond to nearby intimate encounters
- 🔄 **Multi-Scene Support**: Handle multiple simultaneous scenes across the game world
- 🎯 **Event-Driven Triggers**: SkyrimNet trigger system enables configurable NPC reactions to intimate events
- ⚡ **Performance Optimized**: Storage-based caching system for fast template rendering

## 🚀 Recent Major Features

### v0.6.0 - Trigger System Architecture
- 🎯 **SkyrimNet Trigger Integration**: YAML-based trigger configurations for NPC reactions
  - Sex start, position change, climax, scene end, and action decline triggers
  - Server-side configurability without script recompilation
  - Intelligent speaker selection (e.g., orgasming NPC comments on their own climax)
  - Event-specific cooldowns, priorities, and probabilities
- 💾 **Storage Layer Architecture**: Pre-computed decorator data for performance
- 📋 **Unified Event Schema**: Single `tton_event` schema with type field for all OStim events
- 🎨 **Template Modernization**: Direct PapyrusUtil access in Jinja2 templates
  - Eliminated complex JSON-building decorators
  - Flexible data access for template authors

### v0.1.0+ - Core Systems
- 🪑 **Environmental Interaction**: Start sex action now considers nearby furniture options
- ✅ **Player Confirmations**: Added confirmation messages for key OStim actions:
  - 🏁 Starting intimate encounters
  - 🔄 Changing scenes
  - 👋 Inviting or joining other NPCs
  - 🛑 Ending encounters
- ⚙️ **Customizable Notifications**: Toggle which confirmation messages you want to see through MCM
- 👥 **Improved Multi-Actor Handling**: Better management when inviting or joining NPCs to ongoing scenes
- ⏳ **Reaction Cooldowns**: Adjustable timers prevent NPCs from repeatedly requesting the same action

## ⚙️ Configuration

### MCM Settings (In-Game)
- 📱 **Confirmation Messages**: Enable/disable various confirmation dialogs: `Enable start sex confirmation modal`/`Enable change scene confirmation modal`/`Add another actors confirmation modal`/`Enable stop sex confirmation modal`
- 👫 **Gender Preference for Comments**: Set probability weights for which gender is selected for commentary: `Which gender has more chances to start comment`
- 🔒 **Action Denial Cooldown**: Prevent repeated requests from NPCs after player declines an action: `Action cooldown after player's deny`. It works per npc, if npc1 one wanted to perform some OStimNet action but was denied by player - npc1 won't be able to use this action for next X seconds. Meanwhile if npc2 will try same action it will be available for them.

### Trigger Configuration (Server-Side YAML)
Located in `SKSE/Plugins/SkyrimNet/config/triggers/`, you can customize:
- ⏰ **Cooldown Timers**: Adjust `cooldownSeconds` to control reaction frequency
- 🎲 **Trigger Probability**: Set `probability` (0.0-1.0) for how often reactions fire
- 📊 **Priority Levels**: Modify `priority` to control trigger execution order (higher = more important)
- 🎯 **Event Filtering**: Edit `schemaConditions` to fine-tune when triggers activate
- 📝 **Response Templates**: Customize AI prompts in the `response.content` field

Available trigger files:
- `tton_sex_start.yaml` - Reactions when scenes begin
- `tton_sex_change.yaml` - Comments on position changes
- `tton_sex_climax.yaml` - High-priority orgasm reactions
- `tton_sex_stop.yaml` - Scene end diary entries
- `tton_action_decline.yaml` - Player refusal evaluations

## 📝 Usage Notes

- 🔄 When inviting/joining NPCs, the system treats it as the same encounter thread rather than generating separate stop/start events
- 🚫 The mod tracks when players deny an NPC's request to stop an encounter as an event
- 🔁 Too low deny cooldown setting can lead to NPCs spamming the same action again and again (if they are stubborn enough)
- 🎯 Trigger system respects mute settings - events still fire but won't generate AI reactions when muted
- 💾 Sexual behavior data is cached for performance - updates occur during scene lifecycle events
- 🔧 Server admins can tune trigger behavior without waiting for mod updates

## 🛠️ For Modders & Template Authors

### Direct PapyrusUtil Access
Templates can now directly read cached storage data:
```jinja
{% set threadID = papyrus_util("GetIntValue", npc.UUID, "TTONDec_ThreadParticipant", -1) %}
```

### Storage Key Prefixes
- `TTONDec_ActiveOStimThreads` - List of active thread IDs
- `TTONDec_Thread{ID}_*` - Thread-specific data (actors, description, sexual status)

### Event Schema
All OStim events use the `tton_event` schema with fields:
- `type` - Event type (sex_start, sex_change, climax, sex_stop, action_decline)
- `msg` - Human-readable event description
- `threadID` - OStim thread identifier
- `skipTrigger` - Boolean to bypass trigger processing
- `declinedAction` - (Optional) Specific action that was declined
