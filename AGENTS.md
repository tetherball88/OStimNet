# AGENTS.md

## Build & Verification Policy

- **C++ & Magelight UI Build Without Path-Hunting:** Never search the filesystem for `cmake.exe`, Visual Studio, `vcpkg`, or `CommonLibSSE`. Activate [.agents/skills/ostimnet-build](.agents/skills/ostimnet-build/SKILL.md) and run:
  `powershell -NoProfile -ExecutionPolicy Bypass -File ".agents/skills/ostimnet-build/scripts/build.ps1" -Target dll` (or `-Target all` when `ui-src/` also changed; add `-Reconfigure` when changing CMake files).
- **Locked Target DLL (`SkyrimSE.exe` Open):** If Skyrim is running, `SKSE/Plugins/OStimNet.dll` is locked by Windows, causing CMake's `POST_BUILD` copy (`MSB3073`) to fail *after* `SKSE_Source/build/release/Release/OStimNet.dll` compiles and links cleanly. `build.ps1` detects this automatically—treat compilation as **succeeded**, never retry in a loop, and never kill `SkyrimSE.exe`; simply note that copying the new DLL awaits closing Skyrim.
- **Papyrus Scripts (`.psc` -> `.pex`):** Do NOT compile Papyrus scripts unless explicitly asked by the user.
- **Version Release Preparation:** When preparing a new `vX.Y.Z` release, bumping versions, or auditing release readiness, activate [.agents/skills/ostimnet-release](.agents/skills/ostimnet-release/SKILL.md) and run `prepare_release.ps1`.
- **Magelight UI Modals Only:** All confirmations, retry/ignore prompts, and scene/role dialogs must use `OStimNet::UI::ModalManager` <-> `ui-src` (`<ModalHost />`), never native `RE::MessageBoxMenu` or `UIExtensions`.

## Coding Guidelines & Conventions

- **No Unsolicited Alias Functions:** Do NOT create alias functions, duplicate alternative names, or unsolicited wrapper functions for APIs / Papyrus native functions unless explicitly asked by the user. Implement only the exact requested function names.
- **Minimal & Clean APIs:** Keep Papyrus native declarations and C++ bindings 1:1 and strictly aligned with specified requirements without adding extra naming variations.
- **PCH & Header Hygiene:** Every `.cpp` under `SKSE_Source/src/` must begin with `#include "PCH.h"`, and headers under `SKSE_Source/src/` must never contain `using namespace`.

## SkyrimNet Implementation Guides

Refer to [`docs\SkyrimNet`](docs/SkyrimNet) whenever implementing or modifying SkyrimNet concepts:
- `docs\SkyrimNet\WORKFLOW_ACTIONS.md` — Defining and implementing SkyrimNet actions (`.yaml`, parameters, eligibility rules, Papyrus execution).
- `docs\SkyrimNet\WORKFLOW_PROMPTS.md` — Prompt templates, evaluations, Jev decision templates (`[ state ]`, `[ question ]`, `[ criteria ]`), and LLM calls.
- `docs\SkyrimNet\WORKFLOW_TRIGGERS.md` — SkyrimNet events and triggers.
- `docs\SkyrimNet\WORKFLOW_MOD_INTEGRATION.md` — Mod integration patterns with SkyrimNet.
- `docs\SkyrimNet\prompts-and-decorators.md` & `docs\SkyrimNet\prompt-file-syntax.md` — Inja template syntax and custom decorators.
- `docs\SkyrimNet\CONTENT_ROOTS.md` & `docs\SkyrimNet\MIGRATING_TO_BETA25.md` — External content root layout (`SKSE/Plugins/SkyrimNet/external/...`) and Beta 25 conventions.
