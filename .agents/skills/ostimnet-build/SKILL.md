---
name: ostimnet-build
description: >-
  Build OStimNet.dll SKSE C++ plugin and/or the Magelight UI frontend (ui-src) without path-hunting. Activate whenever compiling C++, adding new C++ source files, verifying C++ changes, or building ui-src.
---

# OStimNet Build Runbook

Never search the filesystem for `cmake.exe`, Visual Studio, `vcpkg`, or `CommonLibSSE`. Use the exact one-command pipeline below from the mod root (`E:\Skyrim\development\mods\OStimNet Dev`).

## 1. Build Commands

Run [build.ps1](./scripts/build.ps1):

```powershell
# Build SKSE C++ plugin (OStimNet.dll) after hygiene checks
powershell -NoProfile -ExecutionPolicy Bypass -File ".agents/skills/ostimnet-build/scripts/build.ps1" -Target dll

# Build both OStimNet.dll and the Magelight UI frontend (ui-src -> Magelight/OStimNet/views/OStimNet/)
powershell -NoProfile -ExecutionPolicy Bypass -File ".agents/skills/ostimnet-build/scripts/build.ps1" -Target all

# Build Magelight UI frontend only
powershell -NoProfile -ExecutionPolicy Bypass -File ".agents/skills/ostimnet-build/scripts/build.ps1" -Target ui

# Force CMake reconfigure (use after editing SKSE_Source/CMakeLists.txt or CMakePresets.json)
powershell -NoProfile -ExecutionPolicy Bypass -File ".agents/skills/ostimnet-build/scripts/build.ps1" -Target dll -Reconfigure
```

Use `-Preset debug` to build the Debug configuration (`SKSE_Source/build/debug/Debug/OStimNet.dll` + `.pdb`).

### Underlying Toolchain Paths (Reference Only)
- **CMake binary:** `C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`
- **Required env vars:**
  - `COMMONLIB_SSE_FOLDER = E:/Skyrim/development/mods/_Sources/CommonLibSSE-NG`
  - `VCPKG_ROOT = C:\Program Files\Microsoft Visual Studio\2022\Community\VC\vcpkg`
- **Build outputs:**
  - Plugin DLL (`release` preset): `SKSE_Source/build/release/Release/OStimNet.dll` (copied post-build to `SKSE/Plugins/OStimNet.dll`)
  - Magelight UI view: `Magelight/OStimNet/views/OStimNet/index.html`

### Locked Target DLL Behavior (Important)
When `SkyrimSE.exe` is running, Windows locks `SKSE/Plugins/OStimNet.dll`.
- Compiling and linking `SKSE_Source/build/release/Release/OStimNet.dll` still succeeds, and **only** the CMake `POST_BUILD` `copy_if_different` step (`MSB3073` / `Error copying file`) fails.
- `build.ps1` detects this automatically, prints a `[WARN]`, and exits `0`.
- **Never** retry the build in a loop and **never** terminate `SkyrimSE.exe`—simply note to the user that compilation succeeded and deployment copy to `SKSE/Plugins/OStimNet.dll` awaits closing Skyrim.

## 2. Papyrus Scripts (`.psc` -> `.pex`)
- Do **not** compile Papyrus scripts (`skyrimse.ppj` / Pyro) unless explicitly requested by the user.
