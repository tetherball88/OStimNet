# Thread Phases

When **Enable thread phases** is on in the configuration, sexual encounters progress gradually through logical stages rather than picking random animations. Each thread's intent defines exactly which phases it goes through. The scene progresses forward only — it will never revert to a previous phase.

---

## The Phases

1. **Undressing**
   - Stripping and undressing animations.
2. **Foreplay**
   - Non-penetrative stimulation (handjob, footjob, fingering, toying, foreplay).
3. **Oral**
   - Oral sex (blowjob, cunnilingus, rimjob).
4. **Sex**
   - Penetrative intercourse (vaginalsex, analsex).

---

## Phase Progression by Intent

Different dynamics skip straight to different phases. For example, a romantic encounter starts fully clothed and progresses through all steps, while an aggressive encounter goes straight to intercourse.

- **Romantic**: Undressing → Foreplay → Oral → Sex
- **Lustful**: Foreplay → Oral → Sex
- **Transactional**: Oral → Sex
- **Dom**: Sex
- **Aggressive**: Sex
- **None / No Intent**: Phase tracking is disabled; scene selection uses original random behaviour.

---

## How it works

OStimNet uses OStim Navigator's scene tags to detect the phase of any given scene.

When the LLM (via Game Master or Actions) changes a scene's position, OStimNet enforces the current phase by filtering for appropriate activity keywords. The Game Master can choose to advance the phase when appropriate.

Because phase logic depends on accurate scene tagging, custom animations must be correctly tagged with `intercourse`, `oral`, `penilestimulation`, `fingering`, `toying`, or `clitoralstimulation` to be properly recognized.
