# Migrating SKSE Projects to Centralized CommonLib via Environment Variable

This guide details how to decouple a project from a CommonLib git submodule (e.g., `extern/CommonLibSSE-NG`) and reference a centralized CommonLib installation using the `COMMONLIB_SSE_FOLDER` environment variable.

---

## 1. Update `CMakeLists.txt`

Replace the hardcoded `extern/CommonLibSSE-NG` path with a check for `ENV{COMMONLIB_SSE_FOLDER}` and configure an explicit binary directory (`clib-build`):

```cmake
# ---------------------------------------------------- #
#                     CommonLib                        #
# ---------------------------------------------------- #

if(NOT DEFINED ENV{COMMONLIB_SSE_FOLDER})
    message(FATAL_ERROR "Missing COMMONLIB_SSE_FOLDER environment variable")
endif()

set(CommonLibPath "$ENV{COMMONLIB_SSE_FOLDER}")
set(CommonLibName "CommonLibSSE")

set(BUILD_TESTS OFF CACHE BOOL "" FORCE)

# Force static linking for all vcpkg dependencies
set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "" FORCE)

add_subdirectory("${CommonLibPath}" ${CMAKE_CURRENT_BINARY_DIR}/_deps/clib-build EXCLUDE_FROM_ALL)
get_target_property(COMMONLIB_SRC_DIR CommonLibSSE SOURCE_DIR)

include("${COMMONLIB_SRC_DIR}/cmake/CommonLibSSE.cmake")
```

---

## 2. Update VS Code Settings (`.vscode/settings.json`)

Ensure the VS Code CMake Tools extension passes `COMMONLIB_SSE_FOLDER` to CMake during configuration:

```json
{
    "cmake.sourceDirectory": "${workspaceFolder}/SKSE_Source",
    "cmake.useCMakePresets": "always",
    "cmake.configureOnOpen": false,
    "cmake.configureEnvironment": {
        "COMMONLIB_SSE_FOLDER": "E:/Skyrim/development/mods/_Sources/CommonLibSSE-NG"
    }
}
```

---

## 3. Update VS Code Tasks (`.vscode/tasks.json`)

Add the `options.env` block with `COMMONLIB_SSE_FOLDER` to all CMake configure and build tasks:

```json
{
    "label": "cmake: Configure (Release)",
    "type": "process",
    "command": "C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe",
    "args": [
        "--preset",
        "release"
    ],
    "options": {
        "cwd": "${workspaceFolder}/SKSE_Source",
        "env": {
            "COMMONLIB_SSE_FOLDER": "E:/Skyrim/development/mods/_Sources/CommonLibSSE-NG"
        }
    },
    "problemMatcher": []
},
{
    "label": "cmake: Build Plugin (Release)",
    "type": "process",
    "command": "C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe",
    "args": [
        "--build",
        "--preset",
        "build-release"
    ],
    "options": {
        "cwd": "${workspaceFolder}/SKSE_Source",
        "env": {
            "COMMONLIB_SSE_FOLDER": "E:/Skyrim/development/mods/_Sources/CommonLibSSE-NG"
        }
    },
    "problemMatcher": [
        "$msCompile"
    ],
    "dependsOn": "cmake: Configure (Release)"
}
```

---

## 4. Remove the Git Submodule

Run the following commands in PowerShell from the project root:

```powershell
# 1. Untrack the submodule from the git index
git rm --cached SKSE_Source/extern/CommonLibSSE-NG

# 2. Remove .gitmodules (if CommonLib was the only submodule)
git rm -f .gitmodules

# 3. Clean local git submodule config (if present)
if ((git config --local --get-regexp submodule) -ne $null) {
    git config --local --remove-section submodule.SKSE_Source/extern/CommonLibSSE-NG
}

# 4. Delete the local submodule files and directory from disk
Remove-Item -Recurse -Force "SKSE_Source/extern"
```

---

## 5. Verify the Setup

1. **Clean prior build cache (optional)**:
   ```powershell
   Remove-Item -Recurse -Force "SKSE_Source/build"
   ```
2. **Configure**: Run the `cmake: Configure (Release)` task in VS Code (or execute `cmake --preset release` with `$env:COMMONLIB_SSE_FOLDER` defined).
3. **Build**: Run the `cmake: Build Plugin (Release)` task and verify the compilation completes successfully.
