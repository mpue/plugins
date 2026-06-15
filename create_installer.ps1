<#
.SYNOPSIS
    Builds a Windows installer (.exe) that bundles every plugin in this repo as
    a selectable VST3 component. This is the Windows counterpart to
    create_installer.sh (which builds a macOS .pkg).

.DESCRIPTION
    Generates an Inno Setup script per installer (Pueski-Plugins.iss for the
    suite, <plugin>.iss for single-plugin installers) with one installer
    "component" per plugin, so the end user can pick which plugins to install.
    All VST3 plugins install to the shared VST3 folder:

        C:\Program Files\Common Files\VST3

    If the Inno Setup compiler (ISCC.exe) is found, each script is compiled into

        Installer\output\<name>-<version>.exe

    otherwise the .iss is written and you are told how to compile it.

    Requires the plugins to have been built first (use .\build_all.ps1, or
    .\make_release.ps1 which does both). Each <plugin>.vst3 is located by
    searching <plugin>\Builds\VisualStudio2022.

.PARAMETER Version
    Installer version (first positional arg). Default: 1.0.0.

.PARAMETER Plugins
    Subset of plugins to include. Default: all that have a built .vst3.
    A single plugin produces an installer named after that plugin
    (e.g. Pike-1.0.0.exe) instead of the suite installer.

.PARAMETER Name
    Override the installer name/title (output file <Name>-<version>.exe).

.PARAMETER Each
    Build one standalone installer per selected plugin instead of a single
    suite installer.

.PARAMETER Iscc
    Path to ISCC.exe. Falls back to env:ISCC and common install locations.

.EXAMPLE
    .\create_installer.ps1
.EXAMPLE
    .\create_installer.ps1 2.1.0 -Plugins AF-1,Lupo
.EXAMPLE
    .\create_installer.ps1 2.1.0 -Plugins Pike       # single-plugin installer
.EXAMPLE
    .\create_installer.ps1 2.1.0 -Each               # one installer per plugin
#>
[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [string]   $Version = '1.0.0',
    [string[]] $Plugins,
    [string[]] $Exclude = @('CrashTestDummy'),
    [string]   $Name,
    [switch]   $Each,
    [string]   $Iscc
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

# Auto-discover plugin projects (same logic as build_all.ps1).
function Get-AllPlugins {
    Get-ChildItem -Path $ScriptDir -Directory | Where-Object {
        $jucer = Join-Path $_.FullName "$($_.Name).jucer"
        (Test-Path $jucer) -and ((Get-Content -LiteralPath $jucer -Raw) -match 'projectType="audioplug"')
    } | Select-Object -ExpandProperty Name | Sort-Object
}

if ($Plugins) {
    $Plugins = $Plugins | ForEach-Object { $_ -split '[,\s]+' } | Where-Object { $_ }
} else {
    $Plugins = Get-AllPlugins
}
if ($Exclude) {
    $Plugins = $Plugins | Where-Object { $Exclude -notcontains $_ }
}

if ($Each -and $Name) {
    Write-Error "-Name cannot be combined with -Each (each installer is named after its plugin)."
    exit 2
}

$InstallerDir = Join-Path $ScriptDir 'Installer'
$OutputDir    = Join-Path $InstallerDir 'output'
$SuiteName    = 'Pueski-Plugins'
$SuiteTitle   = 'Pueski Plugin Suite'
$Publisher    = 'Matthias Pueski'

New-Item -ItemType Directory -Force -Path $InstallerDir, $OutputDir | Out-Null

# ── Locate each built .vst3 ──────────────────────────────────────────────────
function Find-Vst3 {
    param([string] $Plugin)
    $buildDir = Join-Path $ScriptDir "$Plugin\Builds\VisualStudio2022"
    if (-not (Test-Path $buildDir)) { return $null }
    $hits = Get-ChildItem -Path $buildDir -Recurse -Filter "$Plugin.vst3" -ErrorAction SilentlyContinue
    foreach ($h in $hits) {
        if ($h.PSIsContainer) {
            # A JUCE VST3 bundle: must actually contain the built binary, not
            # just an empty <plugin>.vst3 folder left behind by a failed build.
            $bin = Join-Path $h.FullName "Contents\x86_64-win\$Plugin.vst3"
            if ((Test-Path $bin) -and ((Get-Item $bin).Length -gt 0)) { return $h }
        } elseif ($h.Length -gt 0) {
            return $h
        }
    }
    return $null
}

Write-Host "==> Locating built VST3 artifacts"
$found   = @{}
$missing = @()
foreach ($p in $Plugins) {
    $vst3 = Find-Vst3 -Plugin $p
    if ($vst3) { $found[$p] = $vst3.FullName } else { $missing += $p }
}

if ($found.Count -eq 0) {
    Write-Error "No built .vst3 found for any selected plugin. Run .\build_all.ps1 first."
    exit 1
}
if ($missing.Count -gt 0) {
    Write-Warning "Skipping plugins with no built .vst3: $($missing -join ', ')"
}

# A .vst3 may be a single file or a bundle folder (JUCE produces a folder on
# Windows: <name>.vst3\Contents\x86_64-win\<name>.vst3). Handle both.
function Get-SourceSpec {
    param([string] $Path)        # full path to <plugin>.vst3
    if (Test-Path $Path -PathType Container) {
        # Bundle folder -> copy recursively, recreating the folder name.
        return [pscustomobject]@{ Source = (Join-Path $Path '*'); IsDir = $true }
    } else {
        return [pscustomobject]@{ Source = $Path; IsDir = $false }
    }
}

# ── Locate ISCC (optional — without it only the .iss files are written) ──────
function Resolve-Iscc {
    param([string] $Hint)
    $candidates = @()
    if ($Hint)         { $candidates += $Hint }
    if ($env:ISCC)     { $candidates += $env:ISCC }
    $candidates += @(
        (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'),
        (Join-Path $env:ProgramFiles        'Inno Setup 6\ISCC.exe')
    )
    foreach ($c in $candidates) { if ($c -and (Test-Path $c)) { return (Resolve-Path $c).Path } }
    $cmd = Get-Command ISCC.exe -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    return $null
}
$IsccExe = Resolve-Iscc -Hint $Iscc

# ── Emit (and compile) one installer for a named set of plugins ──────────────
function New-PluginInstaller {
    param(
        [string]   $InstallerName,
        [string]   $AppTitle,
        [string[]] $PluginNames
    )

    $issPath = Join-Path $InstallerDir "$InstallerName.iss"

    $sb = New-Object System.Text.StringBuilder
    [void]$sb.AppendLine("; Auto-generated by create_installer.ps1 -- do not edit by hand.")
    [void]$sb.AppendLine("#define MyAppName `"$AppTitle`"")
    [void]$sb.AppendLine("#define MyAppVersion `"$Version`"")
    [void]$sb.AppendLine("#define MyPublisher `"$Publisher`"")
    [void]$sb.AppendLine("")
    [void]$sb.AppendLine("[Setup]")
    [void]$sb.AppendLine("AppName={#MyAppName}")
    [void]$sb.AppendLine("AppVersion={#MyAppVersion}")
    [void]$sb.AppendLine("AppPublisher={#MyPublisher}")
    [void]$sb.AppendLine("DefaultDirName={commonpf64}\Common Files\VST3")
    [void]$sb.AppendLine("DisableDirPage=yes")
    [void]$sb.AppendLine("DisableProgramGroupPage=yes")
    [void]$sb.AppendLine("ArchitecturesInstallIn64BitMode=x64compatible")
    [void]$sb.AppendLine("ArchitecturesAllowed=x64compatible")
    [void]$sb.AppendLine("PrivilegesRequired=admin")
    [void]$sb.AppendLine("OutputDir=output")
    [void]$sb.AppendLine("OutputBaseFilename=$InstallerName-$Version")
    [void]$sb.AppendLine("Compression=lzma2")
    [void]$sb.AppendLine("SolidCompression=yes")
    [void]$sb.AppendLine("WizardStyle=modern")
    [void]$sb.AppendLine("")

    # Components: one per plugin, all selected by default.
    [void]$sb.AppendLine("[Components]")
    foreach ($p in ($PluginNames | Sort-Object)) {
        $comp = $p -replace '[^A-Za-z0-9_]', '_'
        [void]$sb.AppendLine("Name: `"$comp`"; Description: `"$p (VST3)`"; Types: full custom")
    }
    [void]$sb.AppendLine("")
    [void]$sb.AppendLine("[Types]")
    [void]$sb.AppendLine("Name: `"full`"; Description: `"All plugins`"")
    [void]$sb.AppendLine("Name: `"custom`"; Description: `"Custom selection`"; Flags: iscustom")
    [void]$sb.AppendLine("")

    # Files: copy each .vst3 (file or bundle folder) gated on its component.
    [void]$sb.AppendLine("[Files]")
    foreach ($p in ($PluginNames | Sort-Object)) {
        $comp = $p -replace '[^A-Za-z0-9_]', '_'
        $spec = Get-SourceSpec -Path $found[$p]
        if ($spec.IsDir) {
            # Recreate <plugin>.vst3\... under the VST3 dir.
            [void]$sb.AppendLine("Source: `"$($spec.Source)`"; DestDir: `"{app}\$p.vst3`"; Components: $comp; Flags: recursesubdirs createallsubdirs ignoreversion")
        } else {
            [void]$sb.AppendLine("Source: `"$($spec.Source)`"; DestDir: `"{app}`"; Components: $comp; Flags: ignoreversion")
        }
    }
    [void]$sb.AppendLine("")

    Set-Content -LiteralPath $issPath -Value $sb.ToString() -Encoding UTF8
    Write-Host "==> Wrote $issPath"

    if (-not $IsccExe) { return $issPath }

    Write-Host "==> Compiling installer with $IsccExe"
    & $IsccExe $issPath
    if ($LASTEXITCODE -ne 0) { throw "ISCC failed (exit $LASTEXITCODE)." }
    return (Join-Path $OutputDir "$InstallerName-$Version.exe")
}

# ── Build the requested installer(s) ─────────────────────────────────────────
$selected = @($found.Keys | Sort-Object)
$outputs  = @()

if ($Each) {
    foreach ($p in $selected) {
        $outputs += New-PluginInstaller -InstallerName $p -AppTitle $p -PluginNames @($p)
    }
} elseif ($Name) {
    $outputs += New-PluginInstaller -InstallerName $Name -AppTitle $Name -PluginNames $selected
} elseif ($selected.Count -eq 1) {
    $outputs += New-PluginInstaller -InstallerName $selected[0] -AppTitle $selected[0] -PluginNames $selected
} else {
    $outputs += New-PluginInstaller -InstallerName $SuiteName -AppTitle $SuiteTitle -PluginNames $selected
}

Write-Host ""
Write-Host "===================================================================="
if (-not $IsccExe) {
    Write-Host "  Inno Setup compiler (ISCC.exe) not found — only .iss files written."
    Write-Host "  Install Inno Setup 6 from https://jrsoftware.org/isinfo.php then run:"
    foreach ($o in $outputs) { Write-Host "    ISCC `"$o`"" }
    Write-Host "  (or pass -Iscc <path to ISCC.exe>)."
} elseif ($outputs.Count -eq 1) {
    Write-Host "  Installer ready: $($outputs[0])"
} else {
    Write-Host "  $($outputs.Count) installers ready:"
    foreach ($o in $outputs) { Write-Host "    $o" }
}
Write-Host ""
Write-Host "  Plugins included: $($selected -join ', ')"
Write-Host "  Installs VST3 to: C:\Program Files\Common Files\VST3"
Write-Host "===================================================================="
