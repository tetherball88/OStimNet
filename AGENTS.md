# AGENTS.md

## Coding Guidelines & Conventions

- **Do Not Build Unless Asked:** Do NOT run C++/CMake builds or compile Papyrus (`.psc` -> `.pex`) scripts unless explicitly asked by the user.
- **No Unsolicited Alias Functions:** Do NOT create alias functions, duplicate alternative names, or unsolicited wrapper functions for APIs / Papyrus native functions unless explicitly asked by the user. Implement only the exact requested function names.
- **Minimal & Clean APIs:** Keep Papyrus native declarations and C++ bindings 1:1 and strictly aligned with specified requirements without adding extra naming variations.

## SkyrimNet Implementation Guides

Refer to [`docs\SkyrimNet`](docs/SkyrimNet) whenever implementing or modifying SkyrimNet concepts:
- `docs\SkyrimNet\WORKFLOW_ACTIONS.md` — Defining and implementing SkyrimNet actions (`.yaml`, parameters, eligibility rules, Papyrus execution).
- `docs\SkyrimNet\WORKFLOW_PROMPTS.md` — Prompt templates, evaluations, Jev decision templates (`[ state ]`, `[ question ]`, `[ criteria ]`), and LLM calls.
- `docs\SkyrimNet\WORKFLOW_TRIGGERS.md` — SkyrimNet events and triggers.
- `docs\SkyrimNet\WORKFLOW_MOD_INTEGRATION.md` — Mod integration patterns with SkyrimNet.
- `docs\SkyrimNet\prompts-and-decorators.md` & `docs\SkyrimNet\prompt-file-syntax.md` — Inja template syntax and custom decorators.
- `docs\SkyrimNet\CONTENT_ROOTS.md` & `docs\SkyrimNet\MIGRATING_TO_BETA25.md` — External content root layout (`SKSE/Plugins/SkyrimNet/external/...`) and Beta 25 conventions.
