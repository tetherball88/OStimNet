---
name: ostimnet-release
description: >-
  Prepare, bump, and verify a new OStimNet version release (vX.Y.Z). Activate when asked to prepare a release, bump version numbers, generate or finalize CHANGELOG.md for a release, audit release readiness (.dll, Magelight UI, Papyrus .pex, manifest.json, requirements), or prepare GitHub/Nexus release notes.
---

# OStimNet Version Release Runbook

Use this workflow whenever preparing a new OStimNet release (`vX.Y.Z`).

## How OStimNet Releases Work

- Pushing a `v*` tag (`.\tag-push.bat vX.Y.Z`) triggers [.github/workflows/release-zip.yml](../../../.github/workflows/release-zip.yml) on `ubuntu-latest`.
- The workflow **does not compile code in CI**; it directly packages the committed `Interface/`, `Magelight/`, `Scripts/`, `SKSE/`, and `TT_OStimNet.esp` directories into `OStimNet_vX.Y.Z.zip`.
- On a live tag push (or `workflow_dispatch` with `dry_run: false`), the workflow:
  1. Extracts the `## vX.Y.Z` section from `CHANGELOG.md` (and converts it to plain text for Nexus).
  2. Creates the GitHub Release with `OStimNet_vX.Y.Z.zip` attached.
  3. Renames the archive to `OStimNet_X.Y.Z.zip` and publishes the new main file + changelog to **Nexus Mods** via `Nexus-Mods/upload-action@v1.0.0-beta.11` using the `nexusmods` GitHub Environment (`secrets.NEXUSMODS_API_KEY`, `vars.NEXUSMODS_MOD_ID` / `secrets.NEXUSMODS_MOD_ID`, `vars.NEXUSMODS_FILE_ID` / `secrets.NEXUSMODS_FILE_ID`).
- You can run a manual `workflow_dispatch` with `dry_run: true` on GitHub Actions to test zip creation and changelog extraction without publishing to GitHub Releases or Nexus Mods.
- Every compiled artifact (`SKSE/Plugins/OStimNet.dll`, `Magelight/OStimNet/views/OStimNet/*`, and `Scripts/*.pex`) **must** be up to date and committed before tagging.

---

## Release Preparation Steps

### 1. Inspect Changes Since Previous Release

1. Find the latest release tag and review everything queued since that tag:
   ```powershell
   git tag --list "v*" --sort=-creatordate | Select-Object -First 5
   git log <prev_tag>..HEAD --stat
   git status -s
   ```
2. Determine the target semantic version `vX.Y.Z` (e.g., `v2.7.0`).

### 2. Bump Version Metadata

Run [prepare_release.ps1](./scripts/prepare_release.ps1) with `-ApplyVersion`:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ".agents/skills/ostimnet-release/scripts/prepare_release.ps1" -Version vX.Y.Z -ApplyVersion
```

This updates the version strings across:
- `SKSE/Plugins/SkyrimNet/external/tetherball88.ostimnet/manifest.json` (`"version"`)
- `SKSE_Source/CMakeLists.txt` (`project(OStimNet VERSION ... LANGUAGES CXX)`)
- `SKSE_Source/version.rc` (`FILEVERSION`, `PRODUCTVERSION`, `FileVersion`, `ProductVersion`)

### 3. Update `CHANGELOG.md` & Audit Requirements

1. Prepend a `## vX.Y.Z` section (`### Added`, `### Changed`, `### Fixed`) above the previous release in `CHANGELOG.md`, matching existing formatting (bold lead-ins with em-dashes and concise user-facing descriptions).
2. Inspect commits since `<prev_tag>` for added, removed, or bumped dependencies and integrations:
   - Core dependencies (`SkyrimNet`, `OStim Standalone`, `OStim Navigator`, `OStim Hot Swap`, `OCum Ascended`, `Magelight UI`, `PapyrusUtil SE`, `po3 Papyrus Extender`).
   - Optional integrations (e.g., `OVirginity Reflowered`, `Fertility Mode`).
3. Update `README.md` and `NEXUS_DESCRIPTION.bbcode` if any requirement, minimum version, or URL changed, and note any updates needed in the **Nexus Mods Requirements** widget.

### 4. Rebuild Compiled Binaries & Audit Papyrus

1. Activate [ostimnet-build](../ostimnet-build/SKILL.md) and rebuild using `-Reconfigure` so the updated `version.rc` / `CMakeLists.txt` version metadata is embedded in `OStimNet.dll`:
   ```powershell
   # Standard C++ release build (reconfigured for version.rc)
   powershell -NoProfile -ExecutionPolicy Bypass -File ".agents/skills/ostimnet-build/scripts/build.ps1" -Target dll -Reconfigure

   # Use -Target all -Reconfigure if ui-src/ changed since <prev_tag>
   powershell -NoProfile -ExecutionPolicy Bypass -File ".agents/skills/ostimnet-build/scripts/build.ps1" -Target all -Reconfigure
   ```
2. Verify `SkyrimSE.exe` is closed before final commit/tag so `SKSE/Plugins/OStimNet.dll` receives the post-build copy (a locked DLL leaves `SKSE/Plugins/OStimNet.dll` stale on disk).
3. **Papyrus rule (`AGENTS.md`):** Do **not** compile `.psc` → `.pex` scripts unless explicitly requested by the user. `prepare_release.ps1` performs a read-only check for missing or stale `.pex` files; if any `.psc` files were modified without recompiled `.pex` counterparts, list those exact scripts so the user can compile them before tagging.

### 5. Run Full Pre-Release Audit

Re-run [prepare_release.ps1](./scripts/prepare_release.ps1) in audit mode:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ".agents/skills/ostimnet-release/scripts/prepare_release.ps1" -Version vX.Y.Z
```

Confirm all automated checks report `[PASS]` (or surface any remaining `[FAIL]` items such as uncompiled `.pex` files or a locked `OStimNet.dll`).

### 6. Hand-off Checklist (Never Auto-Commit or Auto-Tag)

Do **not** commit, push, or run `tag-push.bat` unless the user explicitly asks you to. Instead, provide:
1. The `prepare_release.ps1` audit summary (`PASS` / `WARN` / `FAIL`).
2. Any manual actions required before tagging (compiling flagged `.pex` files, closing `SkyrimSE.exe` if DLL copy was blocked, updating the Nexus Mods Requirements widget / description if dependencies changed).
3. Suggested release commit & tag commands (pushing the tag automatically creates the GitHub Release and deploys to Nexus Mods; optionally run GitHub Actions `workflow_dispatch` with `dry_run: true` first to verify packaging):
   ```powershell
   git commit -m "release vX.Y.Z"
   git push
   .\tag-push.bat vX.Y.Z
   ```
