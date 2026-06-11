<#
.SYNOPSIS
    Builds all JUCE plugins in this repository as VST3 in Release configuration
    on Windows. This is the Windows counterpart to build_all.sh (which builds
    AU + VST3 on macOS via xcodebuild).

.DESCRIPTION
    Unlike macOS, the Visual Studio solutions are NOT committed and most .jucer
    files only carry a macOS (XCODE_MAC) exporter. For every selected plugin the
    script therefore:

        1. ensures a <VS2022> exporter exists in the .jucer (injected if missing,
           with module paths derived from the project's own MODULES list),
        2. runs Projucer --resave to (re)generate Builds\VisualStudio2022\<plugin>.sln,
        3. builds the <plugin>_VST3.vcxproj with MSBuild (Release|x64),
        4. verifies that <plugin>.vst3 was produced.

    AU does not exist on Windows, so VST3 is the only format.

.PARAMETER Plugins
    Comma/space separated subset of plugins to build. Default: all.

.PARAMETER Configuration
    MSBuild configuration. Default: Release.

.PARAMETER Platform
    MSBuild platform. Default: x64.

.PARAMETER Projucer
    Path to Projucer.exe. Falls back to env:JUCE_PROJUCER, then a search of
    common locations under F:\devel, D:\devel and the JUCE checkout.

.PARAMETER JuceModules
    Path to the JUCE 'modules' folder written into the injected VS2022 exporter.
    Default: auto-detected (..\JUCE\modules, then F:\devel\JUCE8\modules).

.PARAMETER AsioSdk
    Optional path to an ASIO SDK 'common' folder (added as headerPath). Only
    needed if a plugin references the ASIO headers.

.PARAMETER Clean
    Run an MSBuild clean before building each project.

.PARAMETER Jobs
    Value for MSBuild /maxcpucount (parallel build). Default: number of cores.

.EXAMPLE
    .\build_all.ps1
.EXAMPLE
    .\build_all.ps1 -Plugins AF-1,Lupo -Projucer F:\devel\JUCE8\Projucer.exe
.EXAMPLE
    .\build_all.ps1 -Clean -JuceModules F:\devel\JUCE8\modules
#>
[CmdletBinding()]
param(
    [string[]] $Plugins,
    [string[]] $Exclude = @('CrashTestDummy'),
    [string]   $Configuration = 'Release',
    [ValidateSet('x64', 'Win32', 'ARM64')]
    [string]   $Platform = 'x64',
    [string]   $Projucer,
    [string]   $JuceModules,
    [string]   $AsioSdk,
    [switch]   $Clean,
    [int]      $Jobs
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

# Auto-discover every plugin project: an immediate subdir <name> containing
# <name>.jucer with projectType="audioplug". This keeps the list from drifting
# as plugins are added/removed.
function Get-AllPlugins {
    Get-ChildItem -Path $ScriptDir -Directory | Where-Object {
        $jucer = Join-Path $_.FullName "$($_.Name).jucer"
        (Test-Path $jucer) -and ((Get-Content -LiteralPath $jucer -Raw) -match 'projectType="audioplug"')
    } | Select-Object -ExpandProperty Name | Sort-Object
}

# Allow "AF-1,Lupo" as a single element too.
if ($Plugins) {
    $Plugins = $Plugins | ForEach-Object { $_ -split '[,\s]+' } | Where-Object { $_ }
} else {
    $Plugins = Get-AllPlugins
}

# Apply exclusions (e.g. test-only projects like CrashTestDummy).
if ($Exclude) {
    $Plugins = $Plugins | Where-Object { $Exclude -notcontains $_ }
}

# ── Locate MSBuild via vswhere ───────────────────────────────────────────────
function Resolve-MSBuild {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path $vswhere) {
        # Prefer the 64-bit MSBuild (Bin\amd64) so a 64-bit target isn't built
        # with the 32-bit toolchain.
        $msbuild = & $vswhere -latest -prerelease -requires Microsoft.Component.MSBuild `
                    -find 'MSBuild\**\Bin\amd64\MSBuild.exe' | Select-Object -First 1
        if (-not $msbuild) {
            $msbuild = & $vswhere -latest -prerelease -requires Microsoft.Component.MSBuild `
                        -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
        }
        if ($msbuild -and (Test-Path $msbuild)) { return $msbuild }
    }
    $cmd = Get-Command msbuild.exe -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    throw "MSBuild.exe not found. Install Visual Studio (with the MSBuild component) or add it to PATH."
}

# ── Locate Projucer ──────────────────────────────────────────────────────────
function Resolve-Projucer {
    param([string] $Hint)
    $candidates = @()
    if ($Hint)               { $candidates += $Hint }
    if ($env:JUCE_PROJUCER)  { $candidates += $env:JUCE_PROJUCER }
    $candidates += @(
        'D:\devel\JUCE8\extras\Projucer\Builds\VisualStudio2026\x64\Release\App\Projucer.exe',
        'F:\devel\JUCE8\Projucer.exe',
        'F:\devel\JUCE8\extras\Projucer\Builds\VisualStudio2022\x64\Release\App\Projucer.exe',
        'D:\devel\JUCE\Projucer.exe'
    )
    foreach ($c in $candidates) { if ($c -and (Test-Path $c)) { return (Resolve-Path $c).Path } }

    # Last resort: shallow search of likely roots.
    foreach ($root in @('F:\devel','D:\devel','C:\devel')) {
        if (Test-Path $root) {
            $hit = Get-ChildItem -Path $root -Recurse -Filter 'Projucer.exe' `
                    -ErrorAction SilentlyContinue -Depth 5 | Select-Object -First 1
            if ($hit) { return $hit.FullName }
        }
    }
    throw "Projucer.exe not found. Pass -Projucer <path> or set `$env:JUCE_PROJUCER."
}

# ── Locate JUCE modules folder ───────────────────────────────────────────────
function Resolve-JuceModules {
    param([string] $Hint)
    $candidates = @()
    if ($Hint) { $candidates += $Hint }
    $candidates += @(
        (Join-Path (Split-Path -Parent $ScriptDir) 'JUCE\modules'),
        'D:\devel\JUCE8\modules',
        'F:\devel\JUCE8\modules',
        'F:\devel\JUCE\modules'
    )
    foreach ($c in $candidates) { if ($c -and (Test-Path $c)) { return (Resolve-Path $c).Path } }
    throw "JUCE 'modules' folder not found. Pass -JuceModules <path to JUCE\modules>."
}

# ── Ensure a VS2022 exporter exists in the .jucer ────────────────────────────
function Ensure-VS2022Exporter {
    param(
        [string] $JucerPath,
        [string] $ModulesPath,
        [string] $AsioPath
    )
    [xml]$doc = Get-Content -LiteralPath $JucerPath -Raw

    $project = $doc.SelectSingleNode('/JUCERPROJECT')
    if (-not $project) { throw "$JucerPath has no <JUCERPROJECT> root." }

    $exportFormats = $project.SelectSingleNode('EXPORTFORMATS')
    if (-not $exportFormats) {
        $exportFormats = $doc.CreateElement('EXPORTFORMATS')
        [void]$project.AppendChild($exportFormats)
    }

    if ($exportFormats.SelectSingleNode('VS2022')) { return $false }   # already present

    # Collect the module IDs the project uses.
    $moduleIds = @($project.SelectNodes('MODULES/MODULE') | ForEach-Object { $_.GetAttribute('id') })

    $vs = $doc.CreateElement('VS2022')
    $vs.SetAttribute('targetFolder', 'Builds/VisualStudio2022')

    $configs = $doc.CreateElement('CONFIGURATIONS')

    $debug = $doc.CreateElement('CONFIGURATION')
    $debug.SetAttribute('isDebug', '1')
    $debug.SetAttribute('name', 'Debug')
    if ($AsioPath) { $debug.SetAttribute('headerPath', $AsioPath) }
    [void]$configs.AppendChild($debug)

    $release = $doc.CreateElement('CONFIGURATION')
    $release.SetAttribute('isDebug', '0')
    $release.SetAttribute('name', 'Release')
    $release.SetAttribute('optimisation', '3')         # full optimisation
    $release.SetAttribute('useRuntimeLibDLL', '0')     # static runtime -> no VC++ redist dependency
    if ($AsioPath) { $release.SetAttribute('headerPath', $AsioPath) }
    [void]$configs.AppendChild($release)

    [void]$vs.AppendChild($configs)

    $modulePaths = $doc.CreateElement('MODULEPATHS')
    foreach ($id in $moduleIds) {
        $mp = $doc.CreateElement('MODULEPATH')
        $mp.SetAttribute('id', $id)
        $mp.SetAttribute('path', $ModulesPath)
        [void]$modulePaths.AppendChild($mp)
    }
    [void]$vs.AppendChild($modulePaths)

    [void]$exportFormats.AppendChild($vs)
    $doc.Save($JucerPath)
    return $true
}

# ── Setup ────────────────────────────────────────────────────────────────────
Write-Host "==> Resolving toolchain"
$MSBuild     = Resolve-MSBuild
$ProjucerExe = Resolve-Projucer  -Hint $Projucer
$ModulesPath = Resolve-JuceModules -Hint $JuceModules
if (-not $Jobs -or $Jobs -lt 1) { $Jobs = [Environment]::ProcessorCount }

Write-Host "    MSBuild   : $MSBuild"
Write-Host "    Projucer  : $ProjucerExe"
Write-Host "    JUCE mods : $ModulesPath"
Write-Host "    Config    : $Configuration|$Platform   (jobs: $Jobs)"
if ($AsioSdk) { Write-Host "    ASIO SDK  : $AsioSdk" }

# Validate selection.
foreach ($p in $Plugins) {
    $jucer = Join-Path $ScriptDir "$p\$p.jucer"
    if (-not (Test-Path $jucer)) {
        throw "Plugin '$p' not found (no $p\$p.jucer)."
    }
}

# ── Build loop ───────────────────────────────────────────────────────────────
$succeeded = New-Object System.Collections.Generic.List[string]
$failed    = New-Object System.Collections.Generic.List[string]
$startTime = Get-Date

function Build-One {
    param([string] $Plugin)

    $pluginDir = Join-Path $ScriptDir $Plugin
    $jucer     = Join-Path $pluginDir "$Plugin.jucer"
    $buildDir  = Join-Path $pluginDir 'Builds\VisualStudio2022'

    Write-Host ""
    Write-Host "==> [$Plugin] preparing project"

    # 1. Make sure a VS2022 exporter is present.
    if (Ensure-VS2022Exporter -JucerPath $jucer -ModulesPath $ModulesPath -AsioPath $AsioSdk) {
        Write-Host "    + injected VS2022 exporter into $Plugin.jucer"
    }

    # 2. Generate / refresh the Visual Studio solution.
    Write-Host "    resaving with Projucer ..."
    & $ProjucerExe --resave $jucer 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Write-Host "    x Projucer --resave failed (exit $LASTEXITCODE)"
        $failed.Add("$Plugin/VST3") | Out-Null
        return
    }

    # 3. Build only the VST3 chain. The JUCE solution expresses the dependency
    #    graph (VST3 -> SharedCode + VST3 Manifest Helper) at the .sln level, not
    #    inside the .vcxproj. Building the VST3 .vcxproj alone therefore fails
    #    (LNK1181: <plugin>.lib missing, and the post-build manifest step can't
    #    find juce_vst3_helper.exe). Build the solution but TARGET only the VST3
    #    project so MSBuild pulls in exactly its dependencies -- and not the
    #    Standalone target.
    $sln = Get-ChildItem -Path $buildDir -Filter '*.sln' -ErrorAction SilentlyContinue |
           Select-Object -First 1
    if (-not $sln) {
        Write-Host "    x no .sln found under $buildDir"
        $failed.Add("$Plugin/VST3") | Out-Null
        return
    }

    # Pull the exact VST3 project display name out of the .sln, e.g. "AF-1 - VST3"
    # (the line referencing *_VST3.vcxproj but not the manifest helper).
    $slnText = Get-Content -LiteralPath $sln.FullName -Raw
    $m = [regex]::Match(
        $slnText,
        'Project\([^)]*\)\s*=\s*"([^"]+)",\s*"[^"]*_VST3\.vcxproj"')
    if (-not $m.Success) {
        Write-Host "    x could not find a VST3 project in $($sln.Name)"
        $failed.Add("$Plugin/VST3") | Out-Null
        return
    }
    # MSBuild solution-target sanitisation: . % $ @ ; ( ) ' -> _
    $target = ([regex]::Replace($m.Groups[1].Value, "[.%`$@;()']", '_'))

    $log = Join-Path $buildDir "$Plugin-VST3.log"
    Write-Host "    building '$($m.Groups[1].Value)' from $($sln.Name)  (log: $log)"

    $common = @(
        $sln.FullName,
        "/p:Configuration=$Configuration",
        "/p:Platform=$Platform",
        "/m:$Jobs",
        '/nologo',
        '/verbosity:minimal'
    )
    # Solution-target syntax: "<Project>" builds, "<Project>:Rebuild"/":Clean"
    # for those. A "<Project>:Build" suffix is NOT valid (MSB4057).
    $msbuildTarget = if ($Clean) { "$target`:Rebuild" } else { $target }
    & $MSBuild @common "/t:$msbuildTarget" *> $log
    if ($LASTEXITCODE -ne 0) {
        Write-Host "    x FAILED -- see $log"
        $failed.Add("$Plugin/VST3") | Out-Null
        return
    }

    # 4. Verify the artifact landed.
    $artifact = Get-ChildItem -Path $buildDir -Recurse -Filter "$Plugin.vst3" -ErrorAction SilentlyContinue |
                Select-Object -First 1
    if (-not $artifact) {
        Write-Host "    x artifact missing: $Plugin.vst3"
        $failed.Add("$Plugin/VST3") | Out-Null
        return
    }
    Write-Host "    + $($artifact.FullName)"
    $succeeded.Add("$Plugin/VST3") | Out-Null
}

foreach ($plugin in $Plugins) {
    try { Build-One -Plugin $plugin }
    catch {
        Write-Host "    x $plugin : $($_.Exception.Message)"
        $failed.Add("$plugin/VST3") | Out-Null
    }
}

# ── Report ───────────────────────────────────────────────────────────────────
$elapsed = [int]((Get-Date) - $startTime).TotalSeconds
Write-Host ""
Write-Host "===================================================================="
Write-Host "  Build summary  (${elapsed}s)"
Write-Host "===================================================================="
Write-Host "  Succeeded: $($succeeded.Count)"
foreach ($s in $succeeded) { Write-Host "    + $s" }
Write-Host "  Failed:    $($failed.Count)"
foreach ($f in $failed) { Write-Host "    x $f" }
Write-Host "===================================================================="

if ($failed.Count -gt 0) { exit 1 }
