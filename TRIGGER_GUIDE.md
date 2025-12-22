# OStimNet Trigger Configuration Guide

A practical guide to customizing and creating triggers for NPC reactions during intimate encounters.

## [What Are Triggers?](https://goncalo22.github.io/SkyrimNet-GamePlugin/Features/customtriggers)

Triggers tell NPCs how to react to OStim events. When something happens (scene starts, position changes, someone climaxes), the trigger fires and generates an AI response.

You configure triggers through the **SkyrimNet Web Dashboard** - no manual YAML editing required!

## Accessing the Web Dashboard

1. Start Skyrim with SkyrimNet running
2. Open your browser and go to: `http://localhost:8008`
3. Navigate to **Automation** → **Triggers**

You'll see all your triggers listed with stats showing Total Triggers, Enabled/Disabled counts, and System Status.

## Quick Start: Editing Existing Triggers

### Example: Making Climax Reactions More Frequent

**Step 1:** In the trigger list, find `tton_sex_climax` and click the **Edit** button

**Step 2:** You'll see the trigger editor with these sections:

- **Name:** `tton_sex_climax`
- **Priority:** `10` (highest - orgasms are important)
- **Description:** "NPC's reaction when climaxed"
- **Event Type:** `tton_event`
- **Event Conditions:** Filters for `type = climax` and `skipTrigger = false`
- **Response Type:** Direct Narration
- **Audience:** Nearby NPCs
- **Response Content:** The AI prompt template
- **Probability:** `1` (100%)
- **Cooldown:** `30` seconds
- **Enabled:** Checked ✓

### Key Settings You Can Change:

**Cooldown (seconds)** - How long before this trigger can fire again
- `30` = Can fire once every 30 seconds
- `0` = No cooldown (fires every time)
- `120` = Once every 2 minutes

**Probability (0-1)** - Chance the trigger fires when conditions are met
- `1` = Always fires (100%)
- `0.5` = 50% chance
- `0.25` = 25% chance

**Priority** - Which trigger wins when multiple match
- Higher numbers = more important
- `10` = High priority (climax reactions)
- `3` = Medium priority (decline reactions)
- `1` = Low priority (position changes)

**Enabled** - Checkbox to turn the trigger on or off
- Checked ✓ = Active
- Unchecked = Disabled

### Editing the AI Prompt

The **Response Content** box contains the AI prompt template. Click in the text area to edit:

**Current climax prompt:**
```
{{ originator }} makes a comment about climax. If they climax - focus on {{ originator }}. If somebody else cum on/in them make a comment about it. If somebody else cum but not on them make a comment about that situation.
```

**More explicit version:**
```
{{ originator }} reacts to the orgasm with explicit detail. If {{ originator }} climaxed, describe their physical sensations. If someone came on/in {{ originator }}, describe how they feel about it.
```

**More subtle version:**
```
{{ originator }} makes a brief, breathless comment about the climax, focusing on emotion over graphic detail.
```

**Step 3:** After making changes, click **Update Trigger** at the bottom

**Step 4:** Changes take effect immediately - test in-game!

## Available Template Variables

Click the **{{ }} Variables** button in the editor to see available variables. You can use these in Response Content:

- `{{ originator }}` - The NPC who will speak (automatically selected)
- `{{ event_json.msg }}` - The event description from the game
- `{{ event_json.type }}` - Event type (sex_start, climax, etc.)
- `{{ event_json.threadID }}` - OStim thread ID

## Understanding OStimNet Trigger Types

### 1. Scene Start (`tton_sex_start`)
Fires when NPCs begin an intimate scene.

**Settings:**
- Priority: `2`
- Cooldown: `30s`
- Audience: `Nearby NPCs`

**Customization ideas:**
- Lower probability for shy characters (0.5 = 50% chance)
- Increase cooldown to reduce initial chatter

### 2. Position Change (`tton_sex_change`)
Fires when the scene changes to a new position.

**Settings:**
- Priority: `1`
- Cooldown: `30s`
- Audience: `Nearby NPCs`

**Customization ideas:**
- Increase cooldown to 60-90s to reduce spam
- Lower probability to 0.3-0.5 for quieter scenes

### 3. Climax (`tton_sex_climax`)
Fires when an NPC orgasms.

**Settings:**
- Priority: `10`
- Cooldown: `30s`
- Audience: `Nearby NPCs`

**Why high priority?** Ensures climax reactions override position changes.

### 4. Scene End (`tton_sex_stop`)
Fires when the scene finishes.

**Settings:**
- Priority: `1`
- Cooldown: `0` (no cooldown needed)
- Response Type: `Diary Entry` (saved to memory, not spoken)
- Audience: `Nearby NPCs`

**Special:** Uses `{{ event_json.msg }}` which contains summary.

### 5. Action Decline (`tton_action_decline`)
Fires when player refuses an NPC's request.

**Settings:**
- Priority: `3`
- Cooldown: `30s`
- Audience: `Nearby NPCs`

**Smart AI prompt:**
```
Declining isn't bad on its own. Evaluate recent events and dialogues to make decision if decline has bad intetion or simply "not right at this moment". {{ originator }} makes comment based on if decline was in bad faith or not.
```

This tells the AI to consider context before reacting negatively.

## Creating a New Trigger

### Example: Quiet Position Changes

Let's create a trigger that only comments on position changes 30% of the time with a longer cooldown.

**Step 1:** In the trigger dashboard, click **Create Trigger**

**Step 2:** Fill in the basic fields:

- **Name:** `tton_sex_change_quiet`
- **Priority:** `1`
- **Description:** "Less frequent reactions to position changes"
- **Event Type:** Select `tton_event` from dropdown

**Step 3:** Add Event Conditions (click **Add Condition** twice):

**Condition 1:**
- Field: `type`
- Operator: `equals`
- Value: `sex_change`

**Condition 2:**
- Field: `skipTrigger`
- Operator: `equals`
- Value: `false`

**Step 4:** Configure Response:

- **Response Type:** Direct Narration
- **Audience:** Nearby NPCs
- **Response Content:**
```
{{ originator }} briefly comments on the new position, keeping it short.
```

**Step 5:** Set Probability and Cooldown:

- **Probability:** `0.3` (30% chance)
- **Cooldown:** `90` seconds
- **Enabled:** Checked ✓

**Step 6:** Click **Create Trigger**

**Step 7:** Disable the original `tton_sex_change` trigger:
- Find it in the list
- Click **Edit**
- Uncheck **Enabled**
- Click **Update Trigger**

**Step 8:** Test in-game! Changes are immediate.

## Common Customizations

### Make NPCs Less Chatty
```yaml
probability: 0.3        # Only 30% chance to comment
cooldownSeconds: 120    # Wait 2 minutes between comments
```

### Make NPCs More Explicit
```yaml
response:
  content: "{{ originator }} describes the sensation in graphic detail, using explicit language."
```

### Different Reactions for Different Events
Create multiple triggers with different probabilities:

**tton_climax_loud.yaml** (50% chance):
```yaml
probability: 0.5
content: "{{ originator }} cries out loudly during climax..."
```

**tton_climax_quiet.yaml** (50% chance):
```yaml
probability: 0.5
content: "{{ originator }} gasps quietly, trying to stay composed..."
```

### Personality-Based Reactions
```yaml
content: "If {{ originator }} is shy or reserved, make a brief embarrassed comment. If confident or assertive, make a bold declaration. {{ originator }} reacts to climaxing."
```

The AI reads character bios, so this actually works!

## Testing Your Triggers

1. **Save changes:** Always click **Update Trigger** or **Create Trigger**
2. **No restart needed:** Changes take effect immediately in-game
3. **Use the dashboard:** Trigger list shows Enabled/Disabled counts at the top
4. **Check System Status:** Dashboard shows if trigger system is "Active"
5. **Test in-game:** Start a scene and observe NPC reactions
6. **Check logs:** Look in `Data/SKSE/Plugins/SkyrimNet/logs/` for errors

## Troubleshooting

**Trigger not firing:**
- Check **Enabled** checkbox is checked ✓
- Verify **Probability** isn't too low (try `1` for testing)
- Check if in-game mute setting is on (triggers respect `skipTrigger`)
- Verify **Event Type** is `tton_event`
- Check **Event Conditions** match the event (type equals climax, etc.)

**Multiple triggers firing:**
- Check **Priority** - highest number wins when multiple match
- Only one trigger per event type should be enabled
- Different triggers can fire for different event types

**AI response doesn't match prompt:**
- The LLM has creative freedom - it interprets your prompt
- Try being more specific in **Response Content**
- Check if character bio contradicts the prompt
- Remember: NPCs stay in character even with explicit prompts

**Can't see changes:**
- Did you click **Update Trigger**?
- Refresh the dashboard page
- Check the enabled/disabled count updated

## Advanced: Conditional Triggers

You can create multiple triggers for the same event type with different conditions.

### Only React to First Climax

Create a trigger with an additional condition:

**Add Condition:**
- Field: `msg`
- Operator: `contains`
- Value: `first time`

This checks if the event message contains "first time".

## Best Practices

1. **Start conservative:** Lower probabilities (0.3-0.5) and longer cooldowns (60-90s) prevent spam
2. **Test incrementally:** Change one setting at a time, test in-game, adjust
3. **Disable, don't delete:** Uncheck **Enabled** instead of deleting triggers you might want later
4. **Match tone:** Keep **Response Content** prompts consistent with your playthrough style
5. **Respect priority:** High priority = important events (climax: 10 > start: 2 > change: 1)
6. **Always include skipTrigger:** This condition respects player mute preferences
7. **Use the Variables button:** Click **{{ }} Variables** to see what's available
8. **Watch character count:** The editor shows character count - keep prompts focused

## Template Variable Reference

Available in all OStimNet triggers:

| Variable | Description | Example |
|----------|-------------|---------|
| `{{ originator }}` | The NPC speaking | "Lydia" |
| `{{ event_json.type }}` | Event type | "climax" |
| `{{ event_json.msg }}` | Event description | "Lydia reached orgasm..." |
| `{{ event_json.threadID }}` | OStim thread number | 0 |
| `{{ event_json.declinedAction }}` | Declined action (decline events only) | "StopSex" |

## Examples From The Wild

### Rare But Impactful Climax Reactions

**Settings:**
- Name: `tton_climax_rare`
- Priority: `10`
- Probability: `0.2` (20% chance)
- Cooldown: `60` seconds
- Response Content:
```
{{ originator }} has an intense, memorable reaction to climax that will stick with them.
```

### Position Changes Only for Major Transitions

**Settings:**
- Name: `tton_change_major`
- Priority: `1`
- Probability: `0.4` (40% chance)
- Cooldown: `120` seconds
- Response Content:
```
{{ originator }} comments only if the position change is significantly different from before.
```

### Empathetic Decline Handling

**Settings:**
- Name: `tton_decline_empathy`
- Priority: `4`
- Probability: `1`
- Cooldown: `30` seconds
- Response Content:
```
{{ originator }} accepts the decline gracefully, showing understanding and respect for boundaries.
```

---

*Happy customizing! The web UI makes it easy - start with small tweaks and experiment.*
