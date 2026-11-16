param(
    [ValidateSet("dll", "ui", "all")]
    [string]$Target = "dll",

    [ValidateSet("release", "debug")]
    [string]$Preset = "release",

    [switch]$Reconfigure,

    [switch]$SkipHygieneCheck
)

$ErrorActionPreference = "Stop"

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..\..")).Path
$SkseDir  = Join-Path $RepoRoot "SKSE_Source"
$UiDir    = Join-Path $RepoRoot "ui-src"
$BuildDir = Join-Path $SkseDir "build\$Preset"
$Config   = if ($Preset -eq "debug") { "Debug" } else { "Release" }

if ($Target -eq "dll" -or $Target -eq "all") {
    $CMakeExe = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    if (-not (Test-Path $CMakeExe)) {
        $fallback = Get-Command cmake -ErrorAction SilentlyContinue
        if ($fallback) {
            $CMakeExe = $fallback.Source
        } else {
            Write-Error "CMake executable not found at '$CMakeExe' or on PATH."
            exit 1
        }
    }

    if (-not $env:COMMONLIB_SSE_FOLDER) {
        $env:COMMONLIB_SSE_FOLDER = "E:/Skyrim/development/mods/_Sources/CommonLibSSE-NG"
    }
    if (-not $env:VCPKG_ROOT) {
        $env:VCPKG_ROOT = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\vcpkg"
    }

    # 1. C++ hygiene checks
    if (-not $SkipHygieneCheck) {
        $SrcDir = Join-Path $SkseDir "src"
        if (Test-Path $SrcDir) {
            $CppFiles = Get-ChildItem -Path $SrcDir -Recurse -Filter "*.cpp"
            foreach ($cpp in $CppFiles) {
                $hasPch = Select-String -Path $cpp.FullName -Pattern '^\s*#\s*include\s*"PCH\.h"' -Quiet
                if (-not $hasPch) {
                    Write-Host ("[FAIL] Missing #include ""PCH.h"" in: {0}" -f $cpp.FullName) -ForegroundColor Red
                    exit 1
                }
            }

            $HeaderFiles = Get-ChildItem -Path $SrcDir -Recurse -Include "*.h","*.hpp"
            $UsingNs = $HeaderFiles | Select-String -Pattern '^\s*using\s+namespace\s+'
            if ($UsingNs) {
                Write-Host "[FAIL] 'using namespace' detected in header:" -ForegroundColor Red
                $UsingNs | ForEach-Object { Write-Host ("  {0}:{1}: {2}" -f $_.Path, $_.LineNumber, $_.Line.Trim()) -ForegroundColor Red }
                exit 1
            }
        }

        $AllSkseFiles = Get-ChildItem -Path $SkseDir -Include "*.h","*.hpp","*.cpp" -Recurse | Where-Object {
            $_.FullName -notmatch '[\\/]build[\\/]'
        }
        $LegacyMsgBox = $AllSkseFiles | Select-String -Pattern 'MessageBoxData|MessageBoxMenu|IMessageBoxCallback'
        if ($LegacyMsgBox) {
            Write-Host "[FAIL] Legacy RE::MessageBoxMenu usage detected (use Magelight UI ModalManager instead):" -ForegroundColor Red
            $LegacyMsgBox | ForEach-Object { Write-Host ("  {0}:{1}: {2}" -f $_.Path, $_.LineNumber, $_.Line.Trim()) -ForegroundColor Red }
            exit 1
        }
    }

    # 2. CMake configure & build OStimNet.dll
    Push-Location $SkseDir
    try {
        $CacheFile = Join-Path $BuildDir "CMakeCache.txt"
        if ($Reconfigure -or -not (Test-Path $CacheFile)) {
            Write-Host "==> Configuring CMake preset '$Preset'..." -ForegroundColor Cyan
            & $CMakeExe --preset $Preset
            if ($LASTEXITCODE -ne 0) {
                exit $LASTEXITCODE
            }
        }

        Write-Host "==> Building OStimNet.dll ($Config)..." -ForegroundColor Cyan
        $DllBuiltPath = Join-Path $BuildDir "$Config\OStimNet.dll"
        $PreBuildTime = if (Test-Path $DllBuiltPath) { (Get-Item $DllBuiltPath).LastWriteTimeUtc } else { [datetime]::MinValue }

        $BuildOutput = & $CMakeExe --build $BuildDir --config $Config --target OStimNet 2>&1
        $DllExitCode = $LASTEXITCODE
        $BuildOutput | ForEach-Object { Write-Host $_ }

        if ($DllExitCode -ne 0) {
            $OutputText = ($BuildOutput | Out-String)
            $CompiledLinkLine = $OutputText -match "OStimNet\.vcxproj\s*->\s*.*OStimNet\.dll"
            $CopyFailedLine   = ($OutputText -match "Error copying file") -or ($OutputText -match "MSB3073") -or ($OutputText -match "cannot access the file")
            $DllExistsNow     = Test-Path $DllBuiltPath

            if ($DllExistsNow -and $CopyFailedLine -and ($CompiledLinkLine -or ((Get-Item $DllBuiltPath).LastWriteTimeUtc -ge $PreBuildTime))) {
                Write-Host "[WARN] OStimNet.dll compiled & linked cleanly at '$DllBuiltPath', but POST_BUILD copy to 'SKSE/Plugins/OStimNet.dll' failed (target DLL is likely locked by a running SkyrimSE.exe instance). Treating compilation as SUCCEEDED." -ForegroundColor Yellow
            } else {
                exit $DllExitCode
            }
        }
    } finally {
        Pop-Location
    }
}

# 3. Magelight UI frontend build
if ($Target -eq "ui" -or $Target -eq "all") {
    if (-not (Test-Path $UiDir)) {
        Write-Error "UI source directory not found at '$UiDir'."
        exit 1
    }

    Push-Location $UiDir
    try {
        if (-not (Test-Path (Join-Path $UiDir "node_modules"))) {
            Write-Host "==> Installing UI dependencies (npm install)..." -ForegroundColor Cyan
            npm install
            if ($LASTEXITCODE -ne 0) {
                exit $LASTEXITCODE
            }
        }

        Write-Host "==> Building Magelight UI frontend (npm run build)..." -ForegroundColor Cyan
        npm run build
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    } finally {
        Pop-Location
    }
}
