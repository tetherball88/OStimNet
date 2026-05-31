# Scene Descriptions

OStimNet needs to provide the LLM with a clear understanding of what is physically happening in a scene. It does this through two types of descriptions: **Auto-Generated** and **Custom**.

---

## Auto-Generated Descriptions

By default, OStim Navigator automatically builds a scene description for any animation playing. It constructs this description dynamically by looking at:
- The actors involved in the scene.
- The actors' specific tags (e.g., gender, role).
- The scene's tags (e.g., `vaginalsex`, `missionary`).
- The scene's actions (e.g., who is performing what action on whom).

These auto-generated descriptions are a highly accurate reflection of the scene's raw data. They ensure that even if a custom description doesn't exist for an animation, the LLM still knows exactly what's going on.

---

## Custom Descriptions

While auto-generated descriptions are mechanically precise, they can sometimes lack nuance or read a bit clinically. They also use exact anatomy terms (like "strapon" instead of "penis" depending on the character setup), which some users might prefer to abstract away.

Custom descriptions are hand-authored, natural-language overrides that replace the auto-generated ones. They generally read more organically and can capture the "vibe" or pacing of an animation better than tags alone.

You can toggle between preferring Custom or Auto-generated descriptions in the OStimNet MCM under **General** -> [Use custom descriptions](./Config.md#use-custom-descriptions-default-on).

---

## Creating New Custom Descriptions

You can create or edit your own custom descriptions directly in-game using OStim Navigator's Dev Editor!

### Opening the Editor
1. Open the OStim Options menu using your hotkeys.
2. Navigate to `Options` → `OStim Navigator Options` → `Show Dev Editor`.
3. Switch to the **OStimNet** tab in the editor window.

*(Note: The editor uses the Prisma UI framework. Your mouse cursor will appear. To toggle between UI interaction and normal game camera, press `Num2` by default).*

### Using the Editor
1. Select an animation from the table.
2. Under the **OStimNet** tab, you'll see a preview of the **Auto Description** (which cannot be edited directly).
3. Below that is the **Custom Override** text box. You can write your description manually here.
4. Alternatively, click **Generate** (requires SkyrimNet to be active). A window will pop up where you can give the LLM hints about the animation (e.g., *"female 1's arms are wrapped around male 0's neck"*, *"fast pace"*, *"deep thrusts"*). The LLM will write a natural description for you.
5. Click **Save** when you're happy with the result.

### Where are they saved?
Custom descriptions are saved as JSON files grouped by the animation pack name.
They are located at:
`SKSE/Plugins/OStimNet/animationsDescriptions/YourPackName.json`

If you are an MO2 user, newly created pack files will appear in your **Overwrite** folder under that path, unless the file already exists in the OStimNet mod folder.
