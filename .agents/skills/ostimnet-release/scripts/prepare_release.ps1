param(
    [Parameter(Mandatory = $true)]
    [string]$Version,

    [string]$BaseTag = "",

    [switch]$ApplyVersion
)

$ErrorActionPreference = "Stop"

$CleanVer = $Version.Trim() -replace '^[vV]', ''
if ($CleanVer -notmatch '^(\d+)\.(\d+)\.(\d+)$') {
    Write-Error "Invalid version '$Version'. Expected format: vX.Y.Z or X.Y.Z (e.g., v2.7.0)."
    exit 1
}

$Major      = $Matches[1]
$Minor      = $Matches[2]
$Patch      = $Matches[3]
$SemVer     = "$Major.$Minor.$Patch"
$Tag        = "v$SemVer"
$CommaVer   = "$Major,$Minor,$Patch,0"
$DotFourVer = "$Major.$Minor.$Patch.0"

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..\..")).Path

if ([string]::IsNullOrWhiteSpace($BaseTag)) {
    $tags = git -C $RepoRoot tag --list "v*" --sort=-creatordate 2>$null
    if ($LASTEXITCODE -eq 0 -and $tags) {
        foreach ($t in $tags) {
            $trimmed = $t.Trim()
            if ($trimmed -and $trimmed -ne $Tag) {
                $BaseTag = $trimmed
                break
            }
        }
    }
}

$script:PassCount = 0
$script:WarnCount = 0
$script:FailCount = 0

function Write-Pass {
    param([string]$Message)
    $script:PassCount++
    Write-Host ("[PASS] {0}" -f $Message) -ForegroundColor Green
}

function Write-Warn {
    param([string]$Message)
    $script:WarnCount++
    Write-Host ("[WARN] {0}" -f $Message) -ForegroundColor Yellow
}

function Write-Fail {
    param([string]$Message)
    $script:FailCount++
    Write-Host ("[FAIL] {0}" -f $Message) -ForegroundColor Red
}

function Write-Utf8NoBom {
    param(
        [string]$Path,
        [string]$Text
    )
    $enc = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, $Text, $enc)
}

Write-Host ("=== OStimNet Release Preparation Audit ({0}) ===" -f $Tag) -ForegroundColor Cyan
if ($BaseTag) {
    Write-Host ("Base tag: {0}" -f $BaseTag)
} else {
    Write-Host "Base tag: (none detected)"
}
Write-Host ""

$ManifestRel   = "SKSE\Plugins\SkyrimNet\external\tetherball88.ostimnet\manifest.json"
$ManifestPath  = Join-Path $RepoRoot $ManifestRel
$CMakePath     = Join-Path $RepoRoot "SKSE_Source\CMakeLists.txt"
$VersionRcPath = Join-Path $RepoRoot "SKSE_Source\version.rc"
$ChangelogPath = Join-Path $RepoRoot "CHANGELOG.md"

# -----------------------------------------------------------------------------
# Optional -ApplyVersion: bump manifest.json, CMakeLists.txt, and version.rc
# -----------------------------------------------------------------------------
if ($ApplyVersion) {
    Write-Host "--> Applying version $SemVer metadata..." -ForegroundColor Cyan

    if (Test-Path $ManifestPath) {
        $manifestRaw = [System.IO.File]::ReadAllText($ManifestPath)
        $newManifest = [regex]::Replace(
            $manifestRaw,
            '("version"\s*:\s*")[^"]*(")',
            ('${1}' + $SemVer + '${2}'),
            1
        )
        if ($newManifest -ne $manifestRaw) {
            Write-Utf8NoBom -Path $ManifestPath -Text $newManifest
            Write-Host ("[UPDATED] {0} -> version = ""{1}""" -f $ManifestRel, $SemVer) -ForegroundColor Green
        } else {
            Write-Host ("[OK] {0} already at ""{1}""" -f $ManifestRel, $SemVer)
        }
    }

    if (Test-Path $CMakePath) {
        $cmakeRaw = [System.IO.File]::ReadAllText($CMakePath)
        $newCmake = [regex]::Replace(
            $cmakeRaw,
            '(project\(\s*OStimNet\s+VERSION\s+)[^\s\)]+(\s+LANGUAGES\s+CXX\s*\))',
            ('${1}' + $SemVer + '${2}'),
            1
        )
        if ($newCmake -ne $cmakeRaw) {
            Write-Utf8NoBom -Path $CMakePath -Text $newCmake
            Write-Host ("[UPDATED] SKSE_Source\CMakeLists.txt -> VERSION {0}" -f $SemVer) -ForegroundColor Green
        } else {
            Write-Host ("[OK] SKSE_Source\CMakeLists.txt already at {0}" -f $SemVer)
        }
    }

    if (Test-Path $VersionRcPath) {
        $rcRaw = [System.IO.File]::ReadAllText($VersionRcPath)
        $newRc = $rcRaw
        $newRc = [regex]::Replace($newRc, '(?m)^(\s*FILEVERSION\s+)\d+,\d+,\d+,\d+', ('${1}' + $CommaVer))
        $newRc = [regex]::Replace($newRc, '(?m)^(\s*PRODUCTVERSION\s+)\d+,\d+,\d+,\d+', ('${1}' + $CommaVer))
        $newRc = [regex]::Replace($newRc, '(VALUE\s+"FileVersion",\s*")[^"]*(")', ('${1}' + $DotFourVer + '${2}'))
        $newRc = [regex]::Replace($newRc, '(VALUE\s+"ProductVersion",\s*")[^"]*(")', ('${1}' + $DotFourVer + '${2}'))
        if ($newRc -ne $rcRaw) {
            Write-Utf8NoBom -Path $VersionRcPath -Text $newRc
            Write-Host ("[UPDATED] SKSE_Source\version.rc -> {0} ({1})" -f $DotFourVer, $CommaVer) -ForegroundColor Green
            Write-Host "[INFO] Run build.ps1 -Target dll -Reconfigure to embed updated version resources into OStimNet.dll." -ForegroundColor Cyan
        } else {
            Write-Host ("[OK] SKSE_Source\version.rc already at {0}" -f $DotFourVer)
        }
    }
    Write-Host ""
}

# -----------------------------------------------------------------------------
# 1. Version Metadata Sync
# -----------------------------------------------------------------------------
Write-Host "1. Version Metadata Sync" -ForegroundColor Cyan

if (Test-Path $ManifestPath) {
    try {
        $manifestJson = [System.IO.File]::ReadAllText($ManifestPath) | ConvertFrom-Json
        if ($manifestJson.version -eq $SemVer) {
            Write-Pass ("{0} version is '{1}'" -f $ManifestRel, $SemVer)
        } else {
            Write-Fail ("{0} version is '{1}' (expected '{2}'). Run with -ApplyVersion." -f $ManifestRel, $manifestJson.version, $SemVer)
        }
    } catch {
        Write-Fail ("{0} failed JSON parse: {1}" -f $ManifestRel, $_.Exception.Message)
    }
} else {
    Write-Fail ("Missing {0}" -f $ManifestRel)
}

if (Test-Path $CMakePath) {
    $cmakeContent = [System.IO.File]::ReadAllText($CMakePath)
    if ($cmakeContent -match 'project\(\s*OStimNet\s+VERSION\s+([^\s\)]+)') {
        $cmakeVer = $Matches[1]
        if ($cmakeVer -eq $SemVer) {
            Write-Pass ("SKSE_Source\CMakeLists.txt project VERSION is '{0}'" -f $SemVer)
        } else {
            Write-Fail ("SKSE_Source\CMakeLists.txt project VERSION is '{0}' (expected '{1}'). Run with -ApplyVersion." -f $cmakeVer, $SemVer)
        }
    } else {
        Write-Fail "Could not parse project(OStimNet VERSION ...) in SKSE_Source\CMakeLists.txt"
    }
} else {
    Write-Fail "Missing SKSE_Source\CMakeLists.txt"
}

if (Test-Path $VersionRcPath) {
    $rcContent = [System.IO.File]::ReadAllText($VersionRcPath)
    $fvMatch  = $rcContent -match ('(?m)^\s*FILEVERSION\s+' + [regex]::Escape($CommaVer) + '\b')
    $pvMatch  = $rcContent -match ('(?m)^\s*PRODUCTVERSION\s+' + [regex]::Escape($CommaVer) + '\b')
    $fvsMatch = $rcContent -match ('VALUE\s+"FileVersion",\s*"' + [regex]::Escape($DotFourVer) + '"')
    $pvsMatch = $rcContent -match ('VALUE\s+"ProductVersion",\s*"' + [regex]::Escape($DotFourVer) + '"')
    if ($fvMatch -and $pvMatch -and $fvsMatch -and $pvsMatch) {
        Write-Pass ("SKSE_Source\version.rc matches '{0}' ({1})" -f $DotFourVer, $CommaVer)
    } else {
        Write-Fail ("SKSE_Source\version.rc does not match '{0}' ({1}). Run with -ApplyVersion." -f $DotFourVer, $CommaVer)
    }
} else {
    Write-Fail "Missing SKSE_Source\version.rc"
}

if (Test-Path $ChangelogPath) {
    $changelogContent = [System.IO.File]::ReadAllText($ChangelogPath)
    if ($changelogContent -match ('(?m)^##\s+v?' + [regex]::Escape($SemVer) + '\b')) {
        Write-Pass ("CHANGELOG.md contains section for {0}" -f $Tag)
    } else {
        Write-Fail ("CHANGELOG.md is missing a '## {0}' section" -f $Tag)
    }
} else {
    Write-Fail "Missing CHANGELOG.md"
}
Write-Host ""

# -----------------------------------------------------------------------------
# 2. Release Zip Required Paths (.github/workflows/release-zip.yml)
# -----------------------------------------------------------------------------
Write-Host "2. Release Zip Required Paths" -ForegroundColor Cyan

$RequiredPaths = @(
    "Interface",
    "Magelight\OStimNet\views\OStimNet\index.html",
    "Scripts",
    "SKSE\Plugins\OStimNet.dll",
    $ManifestRel,
    "TT_OStimNet.esp"
)

foreach ($rel in $RequiredPaths) {
    $full = Join-Path $RepoRoot $rel
    if (Test-Path $full) {
        Write-Pass ("Present: {0}" -f $rel)
    } else {
        Write-Fail ("Missing required release path: {0}" -f $rel)
    }
}
Write-Host ""

# -----------------------------------------------------------------------------
# 3. SKSE Plugin (OStimNet.dll) Freshness, Lock Sync & Embedded Version
# -----------------------------------------------------------------------------
Write-Host "3. SKSE Plugin DLL Checks" -ForegroundColor Cyan

$DeployedDll = Join-Path $RepoRoot "SKSE\Plugins\OStimNet.dll"
$BuiltDll    = Join-Path $RepoRoot "SKSE_Source\build\release\Release\OStimNet.dll"

if (Test-Path $DeployedDll) {
    if (Test-Path $BuiltDll) {
        $deployedHash = (Get-FileHash -Path $DeployedDll -Algorithm SHA256).Hash
        $builtHash    = (Get-FileHash -Path $BuiltDll -Algorithm SHA256).Hash
        if ($deployedHash -eq $builtHash) {
            Write-Pass "SKSE\Plugins\OStimNet.dll matches Release build output (SHA-256)"
        } else {
            Write-Fail "SKSE\Plugins\OStimNet.dll differs from SKSE_Source\build\release\Release\OStimNet.dll (post-build copy was likely blocked by a locked SkyrimSE.exe)"
        }
    } else {
        Write-Warn "SKSE_Source\build\release\Release\OStimNet.dll not present locally; cannot compare build hash"
    }

    $dllTime = (Get-Item $DeployedDll).LastWriteTimeUtc
    $cppSources = Get-ChildItem -Path (Join-Path $RepoRoot "SKSE_Source") -Recurse -File | Where-Object {
        $_.FullName -notmatch '[\\/]build[\\/]' -and
        ($_.Extension -in ".cpp", ".h", ".hpp" -or $_.Name -in "CMakeLists.txt", "version.rc")
    }
    $staleCpp = @($cppSources | Where-Object { $_.LastWriteTimeUtc -gt $dllTime.AddSeconds(2) })
    if ($staleCpp.Count -eq 0) {
        Write-Pass "SKSE\Plugins\OStimNet.dll is newer than all SKSE_Source files"
    } else {
        $names = ($staleCpp | ForEach-Object { $_.Name }) -join ", "
        Write-Fail ("SKSE\Plugins\OStimNet.dll is older than modified C++ source(s): {0}. Run build.ps1 -Target dll." -f $names)
    }

    $fvi = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($DeployedDll)
    $numericVer = "{0}.{1}.{2}.{3}" -f $fvi.FileMajorPart, $fvi.FileMinorPart, $fvi.FileBuildPart, $fvi.FilePrivatePart
    if ($fvi.FileVersion -eq $DotFourVer -and $numericVer -eq $DotFourVer) {
        Write-Pass ("Embedded DLL FileVersion is '{0}'" -f $DotFourVer)
    } else {
        Write-Fail ("Embedded DLL FileVersion is '{0}' (numeric {1}, expected '{2}'). Rebuild with -Target dll -Reconfigure." -f $fvi.FileVersion, $numericVer, $DotFourVer)
    }
}
Write-Host ""

# -----------------------------------------------------------------------------
# 4. Magelight UI Bundle Freshness
# -----------------------------------------------------------------------------
Write-Host "4. Magelight UI Freshness" -ForegroundColor Cyan

$UiSrcDir = Join-Path $RepoRoot "ui-src"
$UiIndex  = Join-Path $RepoRoot "Magelight\OStimNet\views\OStimNet\index.html"
if ((Test-Path $UiSrcDir) -and (Test-Path $UiIndex)) {
    $uiIndexTime = (Get-Item $UiIndex).LastWriteTimeUtc
    $uiFiles = Get-ChildItem -Path $UiSrcDir -Recurse -File | Where-Object {
        $_.FullName -notmatch '[\\/](node_modules|dist)[\\/]'
    }
    $staleUi = @($uiFiles | Where-Object { $_.LastWriteTimeUtc -gt $uiIndexTime.AddSeconds(2) })
    if ($staleUi.Count -eq 0) {
        Write-Pass "Magelight UI bundle is up to date relative to ui-src\"
    } else {
        $names = ($staleUi | ForEach-Object { $_.Name }) -join ", "
        Write-Fail ("Magelight UI bundle is older than ui-src\ file(s): {0}. Run build.ps1 -Target ui." -f $names)
    }
} else {
    Write-Warn "Skipping UI freshness check (ui-src or index.html missing)"
}
Write-Host ""

# -----------------------------------------------------------------------------
# 5. Papyrus .psc -> .pex Audit (Read-Only)
# -----------------------------------------------------------------------------
Write-Host "5. Papyrus .psc -> .pex Audit (read-only)" -ForegroundColor Cyan

$PscDir = Join-Path $RepoRoot "Scripts\Source"
$PexDir = Join-Path $RepoRoot "Scripts"

$DirtyPaths = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::OrdinalIgnoreCase)
$porcelainLines = git -C $RepoRoot status --porcelain 2>$null
if ($LASTEXITCODE -eq 0 -and $porcelainLines) {
    foreach ($line in $porcelainLines) {
        if ($line.Length -ge 4) {
            $relItem = $line.Substring(3).Trim().Trim('"').Replace('/', '\')
            if ($relItem -match ' -> (.+)$') {
                $relItem = $Matches[1].Trim('"').Replace('/', '\')
            }
            [void]$DirtyPaths.Add($relItem)
        }
    }
}

$MissingPex = @()
$StalePex   = @()

if (Test-Path $PscDir) {
    $pscFiles = Get-ChildItem -Path $PscDir -Filter "*.psc" -File
    foreach ($psc in $pscFiles) {
        $pexName = "$($psc.BaseName).pex"
        $pexPath = Join-Path $PexDir $pexName
        $pscRel  = "Scripts/Source/$($psc.Name)"
        $pexRel  = "Scripts/$pexName"

        if (-not (Test-Path $pexPath)) {
            $MissingPex += $psc.Name
            continue
        }

        $pex = Get-Item $pexPath
        if ($psc.LastWriteTimeUtc -gt $pex.LastWriteTimeUtc.AddSeconds(2)) {
            $StalePex += "$($psc.Name) (disk timestamp newer than $pexName)"
            continue
        }

        $pscDirty = $DirtyPaths.Contains($pscRel.Replace('/', '\'))
        $pexDirty = $DirtyPaths.Contains($pexRel.Replace('/', '\'))
        if (-not $pscDirty -and -not $pexDirty) {
            $pscCt = git -C $RepoRoot log -1 --format=%ct -- $pscRel 2>$null
            $pexCt = git -C $RepoRoot log -1 --format=%ct -- $pexRel 2>$null
            if ($pscCt -and $pexCt -and ([long]$pscCt -gt [long]$pexCt)) {
                $StalePex += "$($psc.Name) (committed more recently than $pexName)"
            }
        }
    }

    if ($MissingPex.Count -eq 0 -and $StalePex.Count -eq 0) {
        Write-Pass ("All {0} Papyrus .psc scripts have matching up-to-date .pex files" -f $pscFiles.Count)
    } else {
        foreach ($m in $MissingPex) {
            Write-Fail ("Missing compiled .pex for {0}" -f $m)
        }
        foreach ($s in $StalePex) {
            Write-Fail ("Stale compiled .pex for {0}" -f $s)
        }
    }
} else {
    Write-Warn "Scripts\Source directory not found"
}
Write-Host ""

# -----------------------------------------------------------------------------
# 6. JSON Syntax & Release Zip Hygiene
# -----------------------------------------------------------------------------
Write-Host "6. JSON Syntax & Release Hygiene" -ForegroundColor Cyan

$JsonCheckTargets = @()
if (Test-Path $ManifestPath) { $JsonCheckTargets += (Get-Item $ManifestPath) }
foreach ($dirRel in @("SKSE\Plugins\OStimNet", "SKSE\Plugins\OStim\options")) {
    $dirFull = Join-Path $RepoRoot $dirRel
    if (Test-Path $dirFull) {
        $JsonCheckTargets += Get-ChildItem -Path $dirFull -Filter "*.json" -Recurse -File
    }
}

$BadJson = @()
foreach ($jf in $JsonCheckTargets) {
    try {
        $null = [System.IO.File]::ReadAllText($jf.FullName) | ConvertFrom-Json
    } catch {
        $BadJson += "$($jf.FullName): $($_.Exception.Message)"
    }
}
if ($BadJson.Count -eq 0) {
    Write-Pass ("All {0} checked JSON file(s) parse cleanly" -f $JsonCheckTargets.Count)
} else {
    foreach ($bj in $BadJson) {
        Write-Fail ("Invalid JSON: {0}" -f $bj)
    }
}

$ArtifactExts = @(".pdb", ".iobj", ".ipdb", ".ilk", ".exp", ".lib", ".tmp", ".bak", ".orig", ".log")
$StrayFiles = @()
foreach ($zipFolder in @("Interface", "Magelight", "Scripts", "SKSE")) {
    $folderFull = Join-Path $RepoRoot $zipFolder
    if (Test-Path $folderFull) {
        $hits = Get-ChildItem -Path $folderFull -Recurse -File | Where-Object {
            $_.Extension -in $ArtifactExts
        }
        $StrayFiles += $hits
    }
}
if ($StrayFiles.Count -eq 0) {
    Write-Pass "No stray debug/temp files (.pdb/.ilk/.exp/.lib/.tmp/.bak/.orig/.log) inside release folders"
} else {
    foreach ($sf in $StrayFiles) {
        Write-Fail ("Stray artifact in release folder: {0}" -f $sf.FullName)
    }
}
Write-Host ""

# -----------------------------------------------------------------------------
# 7. Git Context & Summary
# -----------------------------------------------------------------------------
Write-Host "7. Git Context" -ForegroundColor Cyan
if ($BaseTag) {
    Write-Host ("Commits since {0}:" -f $BaseTag)
    $commits = git -C $RepoRoot log "$BaseTag..HEAD" --oneline 2>$null
    if ($commits) {
        $commits | ForEach-Object { Write-Host ("  {0}" -f $_) }
    } else {
        Write-Host "  (none)"
    }
}

Write-Host "Working tree status (git status -s):"
$statusLines = git -C $RepoRoot status -s 2>$null
if ($statusLines) {
    $statusLines | ForEach-Object { Write-Host ("  {0}" -f $_) }
} else {
    Write-Host "  (clean)"
}
Write-Host ""

Write-Host ("Summary: {0} PASS, {1} WARN, {2} FAIL" -f $script:PassCount, $script:WarnCount, $script:FailCount) -ForegroundColor Cyan

if ($script:FailCount -gt 0) {
    exit 1
}
exit 0
