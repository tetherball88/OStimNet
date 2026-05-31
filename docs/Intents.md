# Intents, Roles, Scenes & Threads

Every OStim thread that OStimNet manages has an **intent** — a label that captures the underlying dynamic between the actors. Intent isn't cosmetic. It determines which animations get picked, which actor plays which role, how the AI evaluates whether each NPC would actually be willing, and how NPCs speak and react during a thread — a dominant NPC talks very differently from a romantic one.

---

## The Six Intents

### Platonic

Non-sexual physical care — a hug, a comforting embrace, a friendly gesture. This intent only applies to [care scenes](Actions.md#startcarescene) and never to sex.

**Roles:** Same initiator/responder structure as romantic.

**Scenes selected:** Care animations only. Hugging is available for both platonic and romantic intent; kissing is romantic only.

---

### Romantic

Two (or more) people who have genuine emotional investment in each other. The scene is as much about connection as it is about sex. Romantic is the only sexual intent that also applies to [care actions](Actions.md#startcarescene) — a hug or a kiss between two people who care about each other uses the same intent label.

**Roles:** The actor(s) who drives the encounter is the **initiator**; the other(s) are **responders**. Neither role implies dominance — it's just about who made the first move.

**Willing if:** The NPC has a positive signal toward their partner — attraction, fondness, curiosity, something that actively draws them in. The absence of a reason to refuse is not enough. An NPC who is merely neutral won't agree to a romantic scene.

**Scenes selected:** Animations tagged romantic, lustful, or platonic(for care actions) — anything with a soft or emotionally open dynamic. Or with no intent tag (generic scenes)

---

### Lustful

Purely physical desire. The actors want each other, but there's no emotional investment required.

**Roles:** Same initiator/responder structure as romantic, but the LLM isn't looking for emotional connection — only physical interest.

**Willing if:** The NPC feels physical attraction strong enough to act on. Mild dislike of the person is acceptable if it's overridden by that attraction. What blocks it is hostility, disgust, or an exclusive emotional commitment to someone else.

**Scenes selected:** Animations tagged lustful or romantic, or with no intent tag (generic scenes).

---

### Transactional

Sex as an exchange. One actor provides a service; the other receives it in return for something material — coin, a favour, leverage, or situational desperation.

**Roles:** The **service recipient** gets what they want; the **service provider** delivers it and expects something in return. The main actor is the recipient.

**Willing if:** The provider has sufficient motivation for the exchange — the material benefit makes sense given their situation. Strong moral opposition can still block it, but it needs to be genuinely strong. The recipient's willingness is evaluated separately (they're usually the one initiating).

**Scenes selected:** Animations tagged transactional, lustful, or romantic, or with no intent tag (generic scenes).

---

### Dom

A consensual dominant/submissive dynamic. All actors have chosen to engage in this specific power exchange.

**Roles:** The **dominant** controls the encounter; the **submissive** yields to them.

**Willing if:** The NPC is not just open to sex in general — they have to be open to *this specific dynamic*. That requires some degree of established trust or relationship with the other actor, a personality that fits the role they'd be playing, and no hard limits or relevant trauma. A random stranger can't easily be talked into a dom scene.

**Scenes selected:** Animations tagged dom, or with no intent tag (generic scenes).

---

### Aggressive

Non-consensual. The aggressor overpowers the victim — the victim's consent is not evaluated, only their ability to resist.

**Roles:** The **aggressor(s)** are the main actors; the **victim(s)** are secondary. The LLM checks that the aggressor is motivated and capable, and that the victim could realistically be overpowered given the situation.

**Willing if:** This intent has no willingness check for the victim. The aggressor needs motivation and a plausible power advantage. This intent can be disabled entirely in [Config.md#intents](Config.md#intents), and a confirmation modal can be shown before it applies.

**Scenes selected:** Animations tagged aggressive, or with no intent tag.

---

## Roles

Every thread has two groups of actors: **main** and **secondary**. What those labels mean depends on the intent:

| Intent | Main actors | Secondary actors |
|--------|-------------|-----------------|
| Platonic / Romantic / Lustful | Initiator(s) | Responder(s) |
| Transactional | Service recipient(s) | Service provider(s) |
| Dom | Dominant(s) | Submissive(s) |
| Aggressive | Aggressor(s) | Victim(s) |

Roles affect more than labelling — the LLM uses them when deciding how they speak, who drives position changes.

---

## Positions & Activities

OStimNet tracks two things about the current animation: the **sexual position** (e.g., missionary, doggy, cowgirl) and the **activity** (e.g., vaginalsex, blowjob, cunnilingus, foreplay, analsex). These are read from OStim's scene tags.

The position and activity are used when:

- **Starting a thread** — the AI picks a starting position and activity that fit the intent and the participants' personalities.
- **Changing a thread** — the [ChangeSexScenePosition](Actions.md#changesexsceneposition) action selects from currently available positions and activities in the OStim scene graph. Only positions reachable from the current scene are offered.
- **AI Thread Advancement** — the Game Master picks a new position and activity on a timer, avoiding repeating the current one (see [Config.md#ai-scene-advancement](Config.md#ai-scene-advancement)).

Available positions at any moment depend on what OStim Navigator has connected to the current scene node. OStimNet can't jump to a position that has no navigation path from where the scene currently is.

---

## Thread Phases

Depending on the intent, sexual encounters can progress gradually through logical stages (undressing → foreplay → oral → sex). For a detailed breakdown of how phases work and how they map to intents, see [Phases.md](Phases.md).

---

## How Intents Get Assigned

### Threads OStimNet starts itself

When an NPC (or location scan) decides to initiate a thread, OStimNet runs a **pre-start evaluation** before the animation even begins. The AI looks at the proposed participants, the suggested intent, and each actor's personality, relationships, and current situation. It confirms who is willing, may adjust the participant list if someone refuses, and locks in the roles before OStim starts the thread.

If the evaluation comes back with too few willing participants, the thread doesn't start at all.

### Threads started by other mods

If OStim starts a thread that OStimNet didn't initiate — triggered by another mod, a script, manually, anything — OStimNet detects it and runs a **retroactive evaluation**. The thread is already playing; the AI assigns intent and roles to the situation as it finds it. There's no willingness check here (the actors are already engaged) — the evaluation is purely about understanding the dynamic so OStimNet can manage the thread correctly going forward.

---

## Manually Changing Intent Mid-Thread

There are two ways to change intent during an active thread without waiting for the AI to decide.

**OStim thread menu (recommended):** Open the OStim in-thread OPTIONS menu and navigate to the **OStimNet** page. From there:

- **Change Intent** — a sub-page with one button per intent (Romantic, Lustful, Transactional, Dom, Aggressive). Tap the one you want and it applies immediately, no evaluation, no confirmation prompt.
- **Change Actor Roles** — lets you pick which actors are main actors. Everyone not selected becomes secondary. Use this to flip who is dominant/submissive, aggressor/victim, etc. without restarting the thread.

**Via the AI:** The LLM can also propose an intent change on its own via the [ChangeSexSceneIntent](Actions.md#changesexsceneintent) action. This goes through the normal evaluation flow — the AI decides if the shift makes sense, and a confirmation modal appears if the player is a participant.
