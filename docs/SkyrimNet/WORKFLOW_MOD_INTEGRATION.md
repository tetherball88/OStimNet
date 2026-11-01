# SkyrimNet Mod Integration Workflow

> **Purpose:** This is a comprehensive workflow for AI assistants to help users integrate Skyrim mods with SkyrimNet, covering triggers, actions, and prompts.

## Overview

A complete mod integration typically involves:
1. **Understanding the mod** - What it does, how it works
2. **Triggers** - Detecting when mod events happen
3. **Actions** - Enabling NPCs to use mod functions
4. **Prompts** - Adding mod awareness to character context

Not every mod needs all three. The workflow helps determine what's appropriate.

---

## Key Principle: Source Code vs MCP

| Source | Purpose | When to Use |
|--------|---------|-------------|
| **Source Code** (Papyrus .psc files) | *Understanding* - What the mod does, how it works, why | Read first to understand logic, flow, and intentions |
| **MCP Tools** | *Truth* - What's actually loaded in-game, exact IDs | **ALWAYS** use for editor IDs, function signatures, event names |

> **⚠️ The MCP is the source of truth.** Source code may be outdated, modified, or different from what's running. Always verify via MCP before creating triggers, actions, or prompts.

---

## PHASE 1: Mod Discovery and Understanding

### Step 1.1: Gather Mod Information

**ASK THE USER:**
- "What mod are you integrating?"
- "What does this mod do? What gameplay it adds?"
- "Is it currently running in-game?"
- "Do you have the mod's source code in the workspace? (Papyrus scripts, etc.)"

### Step 1.2: Check for Source Code in Workspace

**IMPORTANT:** Source code is invaluable for understanding mod internals.

**If source is available:**
- Read Papyrus scripts (.psc files) to understand function signatures, logic, and flow
- Look for script comments and documentation
- Identify quest structures, faction usage, global variables
- Understand event flow and function purposes

**If source is NOT available:**
- Search the web for mod documentation, Nexus page, source code
- Ask user to provide any available documentation
- Prepare to explore via MCP tools

> **⚠️ CRITICAL:** Source code helps you *understand* the mod, but **MCP tools are the source of truth** for live game data. Always verify editor IDs, function names, and current state via MCP—the source may be outdated or different from what's loaded.

### Step 1.3: Identify the Mod's Plugin

**Query loaded plugins:**

```
mcp_skyrimnet-mcp_get_plugins
```

**Find the mod's .esp/.esm/.esl file.** Note the exact filename for filtering queries.

### Step 1.4: Explore Mod Data via MCP (Source of Truth)

**Run these queries to understand what the mod exposes:**

**Quests (for actions):**
```
mcp_skyrimnet-mcp_get_quests:
  plugin: "ModName.esp"
```

**Magic Effects (for triggers/prompts):**
```
mcp_skyrimnet-mcp_get_magic_effects:
  plugin: "ModName.esp"
```

**Factions (for eligibility/prompts):**
```
mcp_skyrimnet-mcp_get_factions:
  plugin: "ModName.esp"
```

**Global Variables (for state tracking):**
```
mcp_skyrimnet-mcp_get_globals:
  plugin: "ModName.esp"
```

**Keywords (for categorization):**
```
mcp_skyrimnet-mcp_get_keywords:
  plugin: "ModName.esp"
```

### Step 1.5: Check Live Events

**If the mod fires events, capture them:**

```
mcp_skyrimnet-mcp_get_monitored_events:
  count: 100
  event_type: "mod_event"
```

Look for events with the mod's prefix in `event_name`.

### Step 1.6: Explore Quest Scripts via MCP (for actions)

**If the mod has quests with scripts:**

```
mcp_skyrimnet-mcp_get_quest_scripts:
  quest_editor_id: "ModQuestID"
```

Then get functions:
```
mcp_skyrimnet-mcp_get_quest_script_functions:
  quest_editor_id: "ModQuestID"
  script_name: "ModScriptName"
```

### Step 1.7: Present Findings to User

**SUMMARIZE:**
- What the mod exposes (quests, factions, effects, globals, events)
- What integration points are available
- Initial recommendations for what to create

**ASK:** "Based on what I found, here's what we could integrate. What would you like to focus on?"

---

## PHASE 2: Determine Integration Scope

### Step 2.1: Integration Decision Matrix

| Mod Feature | Triggers | Actions | Prompts |
|-------------|----------|---------|---------|
| Fires SKSE ModEvents | ✅ Event detection | ❌ | ⚠️ Maybe context |
| Has callable functions | ❌ | ✅ NPC behaviors | ❌ |
| Applies magic effects | ✅ Effect detection | ❌ | ✅ Status awareness |
| Uses factions for state | ⚠️ Maybe | ✅ Eligibility | ✅ Role awareness |
| Has global variables | ✅ Value thresholds | ✅ Eligibility | ✅ State awareness |
| Visible gameplay changes | ✅ Detection | ⚠️ Maybe | ✅ Context |

### Step 2.2: Prioritize Integration Points

**HIGH VALUE (do first):**
- Events that significantly impact gameplay
- Functions that create meaningful NPC behaviors
- State that affects how characters should act

**MEDIUM VALUE:**
- Secondary events (nice to detect but not critical)
- Situational actions
- Additional context

**LOW VALUE:**
- Internal/debug events
- State that doesn't affect roleplay
- Redundant information

### Step 2.3: Confirm Scope with User

**PRESENT:**
- Recommended integration priorities
- Estimated number of files to create
- What each integration adds

**ASK:** "Does this scope look right? Any features you want to prioritize or skip?"

---

## PHASE 3: Create Triggers (if applicable)

> **Reference:** See `WORKFLOW_TRIGGERS.md` for detailed trigger creation.

### Step 3.1: Identify Trigger Candidates

**Good trigger candidates:**
- Mod events that represent significant moments
- Spell casts with mod-specific spells
- Effect applications that change character state
- Quest stages that mark progression

### Step 3.2: For Each Trigger

1. **Query specific events** with `get_monitored_events` filtered by type
2. **Identify the exact fields** to match on
3. **Choose response type** (thought, narration, diary, etc.)
4. **Design content template** with appropriate variables
5. **Set probability/cooldown** to prevent spam
6. **Validate with** `validate_custom_trigger`

### Step 3.3: Trigger Naming Convention

```
modname_feature_event.yaml
```

Examples:
- `deviousfollower_spanking_event.yaml`
- `survival_cold_exposure.yaml`
- `bathing_wash_complete.yaml`

---

## PHASE 4: Create Actions (if applicable)

> **Reference:** See `WORKFLOW_ACTIONS.md` for detailed action creation.

### Step 4.1: Identify Action Candidates

**Good action candidates:**
- Functions that perform visible gameplay actions
- Functions that NPCs would logically use in dialogue
- Functions with simple parameter requirements

**Avoid:**
- Internal state management functions
- Functions requiring complex object parameters
- Debug/testing functions

### Step 4.2: For Each Action

1. **Get function signature** with `get_quest_script_functions`
2. **Map parameters** (speaker, dynamic, static)
3. **Design eligibility rules** using appropriate decorators
4. **Write clear description** for LLM decision-making
5. **Create event string** for logging
6. **Validate with** `validate_custom_action`

### Step 4.3: Action Naming Convention

```
ModName_ActionDescription.yaml
```

Examples:
- `DF_DemandOral.yaml`
- `Survival_SetupCamp.yaml`
- `Bathing_WashSelf.yaml`

---

## PHASE 5: Create Prompts (if applicable)

> **Reference:** See `WORKFLOW_PROMPTS.md` for detailed prompt creation.

### Step 5.1: Identify Prompt Candidates

**Good prompt additions:**
- Status effects that affect character behavior
- Faction memberships with roleplay implications
- Global state that characters would be aware of

### Step 5.2: For Each Prompt Submodule

1. **Identify detection method** (effect, faction, global, keyword)
2. **Query exact editor IDs** from MCP
3. **Design conditional content** for different states
4. **Handle render modes** (full, thoughts, etc.)
5. **Validate with** `validate_prompt`

### Step 5.3: Prompt File Naming

For character bio submodules:
```
prompts/submodules/character_bio/0XXX_modname.prompt
```

Number ranges:
- `0000-0099`: Core info
- `0100-0299`: Basic character data
- `0300-0499`: Mod integrations
- `0500-0699`: Advanced state
- `0700+`: Special conditions

---

## PHASE 6: Testing and Refinement

### Step 6.1: Validation Checklist

**For each file created, verify:**

- [ ] **Triggers**: `validate_custom_trigger` returns `valid: true`
- [ ] **Actions**: `validate_custom_action` returns `valid: true`  
- [ ] **Prompts**: `validate_prompt` returns `valid: true`

### Step 6.2: Cross-Reference Check

**Ensure consistency:**
- Triggers use the same event names the mod actually fires
- Actions reference correct quest/script/function names
- Prompts check for effects/factions that actually exist

### Step 6.3: Present Complete Integration

**SUMMARIZE for user:**
- All files created
- What each file does
- How they work together

**ASK:** "Here's the complete integration. Would you like to test any specific part or make adjustments?"

---

## Complete Example: Devious Follower Integration

### Mod Overview
Devious Follower is a follower mod where a companion manipulates the player into debt and degrading deals.

### Discovery Results
- **Plugin**: `DeviousFollowers.esp`
- **Main Quest**: `_DTools`
- **Key Factions**: `_DMaster` (active follower), `_DFSexRule`, `_DFSpankRule`, etc.
- **Key Globals**: `_DflowDebt`, `_DWill`, `_DFBoredom`
- **Mod Events**: `DF-Spank`, `DF-Enslave`, etc.

### Integration Plan

**Triggers Created:**
1. `df_spanking_event.yaml` - Detects when player is spanked
2. `df_enslavement.yaml` - Detects enslavement events
3. `df_scene_start.yaml` - Detects intimate scenes

**Actions Created:**
1. `df_demand_oral.yaml` - NPC demands oral service
2. `df_demand_sex.yaml` - NPC demands sex
3. `df_add_debt.yaml` - NPC adds to player's debt
4. `df_trigger_punishment.yaml` - NPC punishes player

**Prompts Created:**
1. `0301_devious_follower.prompt` - Comprehensive DF master awareness
   - Tracks debt level, control level, active deals
   - Adjusts NPC demeanor based on progression

### Sample Trigger

```yaml
name: "df_spanking_event"
description: "Trigger when player receives spanking from Devious Follower"

eventCriteria:
  eventType: "mod_event"
  schemaConditions:
    - fieldPath: "event_name"
      operator: "equals"
      value: "DF-Spank"
      caseSensitive: true

response:
  type: "direct_narration"
  content: "*{{ player_name }} is bent over and spanked, {{ player_name }}'s cheeks reddening under the punishment.*"

audience: "nearby_npcs"
enabled: true
probability: 1.0
cooldownSeconds: 60
priority: 5
```

### Sample Action

```yaml
name: "DF_DemandOral"
description: "Put {{ player.name }}'s mouth to proper use. Use when you want to assert dominance."

questEditorId: "_DTools"
scriptName: "_dftools"
executionFunctionName: "SexOral"

parameterMapping:
  - type: "speaker"

eligibilityRules:
  - conditions:
      - decoratorName: "is_in_faction"
        arguments: ["currentActor", "_DMaster"]
        comparisonOperator: "=="
        expectedValue: true
    logicalOperator: "AND"
    required: true
  - conditions:
      - decoratorName: "is_in_faction"
        arguments: ["player", "_DFSexRule"]
        comparisonOperator: "=="
        expectedValue: true
      - decoratorName: "get_global_value"
        arguments: ["_DWill"]
        comparisonOperator: "<="
        expectedValue: 2
    logicalOperator: "OR"
    required: true

eventString: "{{ actor.name }} used {{ player.name }}'s mouth."
isShortLived: false

enabled: true
defaultPriority: 5
```

---

## MCP Tools Quick Reference

### Discovery Tools

| Tool | Use For |
|------|---------|
| `get_plugins` | List all loaded mods |
| `get_quests` | Find mod quests |
| `get_quest_scripts` | List scripts on quest |
| `get_quest_script_functions` | **Get callable functions** |
| `get_quest_script_properties` | Get script state |
| `get_magic_effects` | Find mod effects |
| `get_factions` | Find mod factions |
| `get_globals` | Find mod globals |
| `get_keywords` | Find mod keywords |
| `get_monitored_events` | **Capture live events** |
| `get_decorators` | Find prompt functions |

### Validation Tools

| Tool | Use For |
|------|---------|
| `validate_custom_trigger` | **Validate trigger YAML** |
| `validate_custom_action` | **Validate action YAML** |
| `validate_prompt` | **Validate prompt template** |

### Testing Tools

| Tool | Use For |
|------|---------|
| `execute_quest_function` | Test functions (careful!) |
| `get_quest_property_value` | Check runtime state |
| `get_player_info` | Check player state |
| `get_nearby_actors` | Check nearby NPCs |

---

## Master Workflow Checklist

### Phase 1: Discovery
- [ ] Identified mod plugin name
- [ ] Queried quests, effects, factions, globals
- [ ] Checked for mod events
- [ ] Explored script functions (if applicable)
- [ ] Summarized findings for user

### Phase 2: Scope
- [ ] Determined what to integrate (triggers/actions/prompts)
- [ ] Prioritized integration points
- [ ] Confirmed scope with user

### Phase 3: Triggers
- [ ] Created trigger files for significant events
- [ ] Validated all triggers
- [ ] Set appropriate cooldowns/probabilities

### Phase 4: Actions  
- [ ] Created action files for NPC behaviors
- [ ] Designed proper eligibility rules
- [ ] Validated all actions

### Phase 5: Prompts
- [ ] Created prompt submodules for awareness
- [ ] Handled different render modes
- [ ] Validated all prompts

### Phase 6: Review
- [ ] Cross-referenced all files for consistency
- [ ] Presented complete integration to user
- [ ] Made requested adjustments

