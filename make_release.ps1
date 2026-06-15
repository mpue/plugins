<#
.SYNOPSIS
    One-shot Windows release: builds every plugin (VST3, Release) and produces a
    single Inno Setup installer that lets the end user choose which plugins to
    install. Windows counterpart to make_release.sh.

.DESCRIPTION
    Thin wrapper around build_all.ps1 + create_installer.ps1.

.PARAMETER Version
    Installer version (first positional arg). Default: 1.0.0.

.PARAMETER Plugins
    Subset of plugins to build and package. Default: all. A single plugin
    produces an installer named after that plugin (e.g. Pike-1.0.0.exe).

.PARAMETER Name
    Override the installer name/title (forwarded to create_installer.ps1).

.PARAMETER Each
    Build one standalone installer per selected plugin (forwarded to
    create_installer.ps1).

.PARAMETER Projucer
    Path to Projucer.exe (forwarded to build_all.ps1).

.PARAMETER JuceModules
    Path to JUCE\modules (forwarded to build_all.ps1).

.PARAMETER AsioSdk
    Optional ASIO SDK 'common' header path (forwarded to build_all.ps1).

.PARAMETER Iscc
    Path to ISCC.exe (forwarded to create_installer.ps1).

.PARAMETER Clean
    Clean each project before building.

.PARAMETER SkipBuild
    Reuse existing artifacts; skip the compilation phase.

.EXAMPLE
    .\make_release.ps1 2.1.0
.EXAMPLE
    .\make_release.ps1 2.1.0 -Plugins AF-1,Lupo -Projucer F:\devel\JUCE8\Projucer.exe
.EXAMPLE
    .\make_release.ps1 2.1.0 -Plugins Pike           # single-plugin installer
.EXAMPLE
    .\make_release.ps1 2.1.0 -Each                   # one installer per plugin
.EXAMPLE
    .\make_release.ps1 2.1.0 -SkipBuild
#>
[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [string]   $Version = '1.0.0',
    [string[]] $Plugins,
    [string[]] $Exclude = @('CrashTestDummy'),
    [string]   $Name,
    [switch]   $Each,
    [string]   $Projucer,
    [string]   $JuceModules,
    [string]   $AsioSdk,
    [string]   $Iscc,
    [switch]   $Clean,
    [switch]   $SkipBuild
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

if ($SkipBuild) {
    Write-Host "==> -SkipBuild set, skipping compilation step"
} else {
    Write-Host "===================================================================="
    Write-Host "  Phase 1/2 - Building plugins (VST3, Release)"
    Write-Host "===================================================================="
    $buildArgs = @{}
    if ($Plugins)     { $buildArgs.Plugins     = $Plugins }
    if ($Exclude)     { $buildArgs.Exclude     = $Exclude }
    if ($Projucer)    { $buildArgs.Projucer    = $Projucer }
    if ($JuceModules) { $buildArgs.JuceModules = $JuceModules }
    if ($AsioSdk)     { $buildArgs.AsioSdk     = $AsioSdk }
    if ($Clean)       { $buildArgs.Clean       = $true }

    & (Join-Path $ScriptDir 'build_all.ps1') @buildArgs
    if ($LASTEXITCODE -ne 0) { throw "build_all.ps1 failed (exit $LASTEXITCODE)." }
}

Write-Host ""
Write-Host "===================================================================="
Write-Host "  Phase 2/2 - Creating installer ($Version)"
Write-Host "===================================================================="
$instArgs = @{ Version = $Version }
if ($Plugins) { $instArgs.Plugins = $Plugins }
if ($Exclude) { $instArgs.Exclude = $Exclude }
if ($Name)    { $instArgs.Name    = $Name }
if ($Each)    { $instArgs.Each    = $true }
if ($Iscc)    { $instArgs.Iscc    = $Iscc }

& (Join-Path $ScriptDir 'create_installer.ps1') @instArgs
if ($LASTEXITCODE -ne 0) { throw "create_installer.ps1 failed (exit $LASTEXITCODE)." }
