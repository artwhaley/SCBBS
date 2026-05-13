#requires -version 5.1
<#
.SYNOPSIS
  Rebuild the SCMFD keyboard driver and test client, then remove old installs and stage + install SCMFD_Keyboard_Root.

.DESCRIPTION
  Finds driverspike.sln next to this project, then builds the driver and test-client project files directly with full paths (VS2022 only).
  Does not require Administrator for the build. Deploy steps still require elevation.

.PARAMETER Configuration
  Debug or Release (passed to MSBuild and used for package layout).

.PARAMETER SkipBuild
  Skip MSBuild; only deploy using outputs already on disk.

.PARAMETER MSBuildPath
  Full path to MSBuild.exe (optional; must be a VS2022 (17.x) MSBuild path).

.PARAMETER SolutionPath
  Full path to driverspike.sln (optional; default: ..\driverspike.sln from this script).

.PARAMETER VisualStudioVersionForWdk
  MSBuild property VisualStudioVersion passed to the solution so WDK loads Microsoft.DriverKit.Build.Tasks.(version).dll.
  Default: auto-detect VS2022-era WDK task versions only (17.0 preferred, then 16.0).

.PARAMETER WhatIf
  Show resolved paths and planned steps only.
#>
[CmdletBinding(SupportsShouldProcess = $true)]
param(
    [ValidateSet('Debug', 'Release')]
    [string] $Configuration = 'Debug',
    [switch] $SkipBuild,
    [string] $MSBuildPath = '',
    [string] $SolutionPath = '',
    [string] $VisualStudioVersionForWdk = ''
)

$ProjectDir = $PSScriptRoot
. (Join-Path $ProjectDir 'SCMFD_Keyboard_Root.InfHelpers.ps1')

function Resolve-DriverspikeSolutionPath {
    param(
        [Parameter(Mandatory)][string]$ProjectDir,
        [string]$Override
    )
    if ($Override) {
        $p = [IO.Path]::GetFullPath($Override)
        if (-not (Test-Path -LiteralPath $p -PathType Leaf)) {
            Write-Error "Solution not found: $p"
            exit 1
        }
        return $p
    }
    $slnDir = Split-Path -Parent $ProjectDir
    $p = [IO.Path]::GetFullPath((Join-Path $slnDir 'driverspike.sln'))
    if (-not (Test-Path -LiteralPath $p -PathType Leaf)) {
        Write-Error "Could not find driverspike.sln at: $p"
        exit 1
    }
    return $p
}

function Find-MsBuildExe {
    param([string]$Override)
    if ($Override) {
        $p = [IO.Path]::GetFullPath($Override)
        if (-not (Test-Path -LiteralPath $p -PathType Leaf)) {
            Write-Error "MSBuild not found: $p"
            exit 1
        }
        if ($p -match '\\Microsoft Visual Studio\\18\\' -or $p -match '\\2026\\') {
            Write-Error "Refusing non-VS2022 MSBuild path: $p"
            exit 1
        }
        return $p
    }
    $vswhere = [IO.Path]::GetFullPath((Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'))
    if (Test-Path -LiteralPath $vswhere) {
        $found = @(& $vswhere -latest -products * -version '[17.0,18.0)' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' 2>$null) |
            Where-Object { $_ -and (Test-Path -LiteralPath $_) } |
            Select-Object -First 1
        if ($found) { return [IO.Path]::GetFullPath($found.Trim()) }
    }
    $roots = @(
        (Join-Path $env:ProgramFiles 'Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe'),
        (Join-Path $env:ProgramFiles 'Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe'),
        (Join-Path $env:ProgramFiles 'Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe'),
        (Join-Path $env:ProgramFiles 'Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe'),
        (Join-Path $env:ProgramFiles 'Microsoft Visual Studio\2022\Preview\MSBuild\Current\Bin\MSBuild.exe')
    )
    foreach ($r in $roots) {
        if (Test-Path -LiteralPath $r) { return [IO.Path]::GetFullPath($r) }
    }
    return $null
}

function Get-InstalledDriverKitTaskDllVersions {
    $kitsRoot = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\build'
    if (-not (Test-Path -LiteralPath $kitsRoot)) { return @() }
    $dlls = Get-ChildItem -LiteralPath $kitsRoot -Recurse -Filter 'Microsoft.DriverKit.Build.Tasks.*.dll' -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -notmatch 'PackageVerifier' }
    $vers = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
    foreach ($d in $dlls) {
        if ($d.Name -match '^Microsoft\.DriverKit\.Build\.Tasks\.(.+)\.dll$') {
            [void]$vers.Add($Matches[1].Trim())
        }
    }
    $vers | Sort-Object {
        try { [version]$_ } catch { [version]'0.0' }
    } -Descending
}

function Test-DriverKitTaskDllVersionInstalled {
    param([Parameter(Mandatory)][string]$Version)
    $kitsRoot = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\build'
    if (-not (Test-Path -LiteralPath $kitsRoot)) { return $false }
    $pat = "Microsoft.DriverKit.Build.Tasks.$Version.dll"
    $hit = Get-ChildItem -LiteralPath $kitsRoot -Recurse -Filter $pat -ErrorAction SilentlyContinue | Select-Object -First 1
    return ($null -ne $hit)
}

function Resolve-VisualStudioVersionForWdk {
    param([string]$Explicit)
    if ($Explicit) {
        $x = $Explicit.Trim()
        try {
            if ([version]$x -gt [version]'17.9') {
                Write-Error "VisualStudioVersionForWdk must be VS2022-era (<=17.x). Got: $x"
                exit 1
            }
        } catch {}
        return $x
    }
    $fromEnv = $env:WDK_DRIVERKIT_TASKS_VS_VERSION, $env:SCMFD_KEYBOARD_ROOT_WDK_TASKS_VS_VERSION | Where-Object { $_ } | Select-Object -First 1
    if ($fromEnv) {
        $e = $fromEnv.Trim()
        try {
            if ([version]$e -le [version]'17.9') { return $e }
        } catch {}
    }
    $installed = @(Get-InstalledDriverKitTaskDllVersions)
    if ($installed.Count -eq 0) { return $null }
    $vs2022Compatible = @($installed | Where-Object {
            try { [version]$_ -le [version]'17.9' } catch { $false }
        })
    if ($vs2022Compatible.Count -eq 0) { return $null }
    $pref17 = $vs2022Compatible | Where-Object { $_ -like '17.*' } | Select-Object -First 1
    if ($pref17) { return $pref17 }
    return ($vs2022Compatible | Select-Object -First 1)
}

function Invoke-DriverspikeRebuild {
    param(
        [Parameter(Mandatory)][string]$MsBuildExe,
        [Parameter(Mandatory)][string]$SolutionFile,
        [Parameter(Mandatory)][string]$Configuration,
        [string]$VisualStudioVersionForWdk
    )
    $solutionDir = Split-Path -Parent $SolutionFile
    $projects = @(
        (Join-Path $solutionDir 'SCMFD_Keyboard_Root\SCMFD_Keyboard_Root.vcxproj'),
        (Join-Path $solutionDir 'TestEnumeratorAndClient\TestEnumeratorAndClient.vcxproj')
    )
    $vsProp = @()
    if ($VisualStudioVersionForWdk) {
        $vsProp = @("/p:VisualStudioVersion=$VisualStudioVersionForWdk")
        Write-Host "WDK DriverKit tasks: using /p:VisualStudioVersion=$VisualStudioVersionForWdk (Microsoft.DriverKit.Build.Tasks.$VisualStudioVersionForWdk.dll)"
    }
    Write-Host "MSBuild: $MsBuildExe"
    foreach ($project in $projects) {
        if (-not (Test-Path -LiteralPath $project -PathType Leaf)) {
            Write-Error "Project not found: $project"
            exit 1
        }
        Write-Host "Project: $project"
        Write-Host "Command: $MsBuildExe `"$project`" /t:Rebuild /p:Configuration=$Configuration /p:Platform=x64 /m /v:minimal /nr:true $($vsProp -join ' ')"
        $old = $ErrorActionPreference
        $ErrorActionPreference = 'Continue'
        try {
            & $MsBuildExe $project /t:Rebuild /p:Configuration=$Configuration /p:Platform=x64 /m /v:minimal /nr:true @vsProp
        }
        finally {
            $ErrorActionPreference = $old
        }
        if ($LASTEXITCODE -ne 0) {
            Write-Error "MSBuild Rebuild failed for $project (exit code $LASTEXITCODE). Fix errors above, or open the log from Visual Studio."
            exit $LASTEXITCODE
        }
    }
}

function Test-Administrator {
    $p = [Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
    return $p.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Find-DevconExe {
    $direct = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\Tools\x64\devcon.exe'
    if (Test-Path -LiteralPath $direct) { return $direct }
    $root = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\Tools'
    if (-not (Test-Path -LiteralPath $root)) { return $null }
    $hit = Get-ChildItem -LiteralPath $root -Filter 'devcon.exe' -Recurse -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($hit) { return $hit.FullName }
    return $null
}

function Invoke-Devcon {
    param(
        [Parameter(Mandatory)][string]$DevconPath,
        [Parameter(Mandatory)][string[]]$Arguments
    )
    $old = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        & $DevconPath @Arguments 2>&1 | ForEach-Object { Write-Host $_ }
    }
    finally {
        $ErrorActionPreference = $old
    }
}

function Remove-SCMFD_Keyboard_RootPnPDevices {
    param([string]$DevconPath)
    if (-not $DevconPath) { Write-Warning 'devcon.exe not found; skipping hardware remove (remove phantom devices manually if needed).'; return }
    foreach ($args in @(
            @('remove', 'root\SCMFD_Keyboard_Root'),
            @('remove', '@root\SCMFD_Keyboard_Root'),
            @('remove', 'ROOT\SCMFD_Keyboard_Root')
        )) {
        try {
            Invoke-Devcon -DevconPath $DevconPath -Arguments $args
        }
        catch { Write-Warning "devcon $($args -join ' '): $_" }
    }
}

function Remove-SCMFD_Keyboard_RootFromDriverStore {
    $lines = @()
    try {
        $lines = @(pnputil.exe /enum-drivers 2>&1)
    }
    catch {
        Write-Warning "pnputil /enum-drivers failed: $_"
        return
    }
    $pub = $null
    $orig = $null
    $prov = $null
    $packages = [System.Collections.Generic.List[object]]::new()
    foreach ($line in $lines) {
        if ($line -match 'Published Name\s*:\s*(oem\d+\.inf)') {
            if ($pub) {
                $packages.Add([PSCustomObject]@{ OEM = $pub; OriginalName = $orig; Provider = $prov })
            }
            $pub = $Matches[1]
            $orig = $null
            $prov = $null
        }
        elseif ($line -match 'Original Name\s*:\s*(.+)$') { $orig = $Matches[1].Trim() }
        elseif ($line -match 'Provider Name\s*:\s*(.+)$') { $prov = $Matches[1].Trim() }
    }
    if ($pub) {
        $packages.Add([PSCustomObject]@{ OEM = $pub; OriginalName = $orig; Provider = $prov })
    }
    $toDelete = $packages | Where-Object {
        ($null -ne $_.OriginalName -and $_.OriginalName -match '(?i)^scmfd_keyboard_root\.inf$') -or
        ($null -ne $_.OriginalName -and $_.OriginalName -match '(?i)^driver\.inf$' -and $_.Provider -match '(?i)SpikeTest') -or
        ($null -ne $_.Provider -and $_.Provider -match '(?i)scmfd_keyboard_root')
    }
    foreach ($oem in (($toDelete | Select-Object -ExpandProperty OEM -Unique) | Sort-Object {
                if ($_ -match 'oem(\d+)\.inf') { [int]$Matches[1] } else { 0 }
            } -Descending)) {
        try {
            Write-Host "pnputil /delete-driver $oem /uninstall /force"
            & pnputil.exe /delete-driver $oem /uninstall /force 2>&1 | ForEach-Object { Write-Host $_ }
        }
        catch { Write-Warning "Could not delete $oem : $_" }
    }
}

function Resolve-SCMFD_Keyboard_RootPackage {
    param(
        [Parameter(Mandatory)][string]$ProjectDir,
        [Parameter(Mandatory)][string]$Configuration
    )
    $sln = Split-Path -Parent $ProjectDir
    $outDirs = @(
        (Join-Path $sln "x64\$Configuration\SCMFD_Keyboard_Root"),
        (Join-Path $sln "x64\$Configuration"),
        (Join-Path $ProjectDir "x64\$Configuration"),
        (Join-Path $ProjectDir "x64\$Configuration\SCMFD_Keyboard_Root")
    ) | Where-Object { Test-Path -LiteralPath $_ }

    $packages = @{}
    foreach ($od in $outDirs) {
        foreach ($inf in (Get-SCMFD_Keyboard_RootInfCandidates -OutDir $od -ProjectDir $ProjectDir -Configuration $Configuration)) {
            if ((Split-Path -Leaf $inf) -ne 'SCMFD_Keyboard_Root.inf') { continue }
            $d = [IO.Path]::GetFullPath((Split-Path -Parent $inf))
            $cat = Join-Path $d 'SCMFD_Keyboard_Root.cat'
            $dll = Join-Path $d 'SCMFD_Keyboard_Root.dll'
            if (-not (Test-Path -LiteralPath $cat)) { continue }
            if (-not (Test-Path -LiteralPath $dll)) { continue }
            $packages[$d] = [IO.Path]::GetFullPath($inf)
        }
    }

    if ($packages.Count -eq 0) { return $null }

    $preferred = [IO.Path]::GetFullPath((Join-Path $sln "x64\$Configuration\SCMFD_Keyboard_Root"))
    if ($packages.ContainsKey($preferred)) {
        return @{ Directory = $preferred; Inf = $packages[$preferred] }
    }

    $pick = $packages.GetEnumerator() | Sort-Object { $_.Key.Length } -Descending | Select-Object -First 1
    return @{ Directory = $pick.Key; Inf = $pick.Value }
}

# --- 1) Rebuild (no admin) ---
$sln = Resolve-DriverspikeSolutionPath -ProjectDir $ProjectDir -Override $SolutionPath
if (-not $SkipBuild) {
    $msb = Find-MsBuildExe -Override $MSBuildPath
    if (-not $msb) {
        Write-Error @"
Could not find MSBuild.exe. Install Visual Studio 2022 (Desktop development with C++) or Build Tools, or pass -MSBuildPath with the full path to MSBuild.exe.
vswhere expected at: $([IO.Path]::GetFullPath((Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe')))
"@
        exit 1
    }
    $wdkVs = Resolve-VisualStudioVersionForWdk -Explicit $VisualStudioVersionForWdk
    if (-not $wdkVs) {
        Write-Error @"
No Microsoft.DriverKit.Build.Tasks.*.dll found under: $([IO.Path]::GetFullPath((Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\build')))

Install the Windows Driver Kit (WDK) and the WDK Visual Studio extension (version that matches your Visual Studio / Build Tools). See:
  https://learn.microsoft.com/windows-hardware/drivers/download-the-wdk

For VS2022 you should have e.g. ...\Windows Kits\10\build\10.0.xxxxx\bin\Microsoft.DriverKit.Build.Tasks.17.0.dll
or older compatible 16.0.

If you need to force a specific DLL version, pass -VisualStudioVersionForWdk 17.0 (or 16.0),
or set environment variable WDK_DRIVERKIT_TASKS_VS_VERSION.
"@
        exit 1
    }
    if (-not (Test-DriverKitTaskDllVersionInstalled -Version $wdkVs)) {
        $avail = @(Get-InstalledDriverKitTaskDllVersions)
        Write-Error "Requested WDK task version $wdkVs is not installed. Available: $($avail -join ', ')"
        exit 1
    }
    Write-Host "Resolved WDK task assembly version for MSBuild: $wdkVs"
    if ($PSCmdlet.ShouldProcess($sln, "MSBuild Rebuild ($Configuration|x64)")) {
        Invoke-DriverspikeRebuild -MsBuildExe $msb -SolutionFile $sln -Configuration $Configuration -VisualStudioVersionForWdk $wdkVs
    }
}
else {
    Write-Host 'SkipBuild: not invoking MSBuild.'
}

# --- 2) Package + deploy ---
$pkg = Resolve-SCMFD_Keyboard_RootPackage -ProjectDir $ProjectDir -Configuration $Configuration
if (-not $pkg) {
    Write-Error @"
Could not find a complete package (SCMFD_Keyboard_Root.inf + .cat + .dll).
$(if (-not $SkipBuild) { 'MSBuild reported success but outputs are missing; check project Output Directory.' } else { "Build the solution first, or omit -SkipBuild." })
Checked under:
  $([IO.Path]::GetFullPath((Join-Path (Split-Path -Parent $ProjectDir) "x64\$Configuration\SCMFD_Keyboard_Root")))
  $([IO.Path]::GetFullPath((Join-Path (Split-Path -Parent $ProjectDir) "x64\$Configuration")))
  $([IO.Path]::GetFullPath((Join-Path $ProjectDir "x64\$Configuration")))
"@
    exit 1
}

$inf = $pkg.Inf
$dir = $pkg.Directory
Write-Host "Package directory: $dir"
Write-Host "INF:             $inf"

if (-not $PSCmdlet.ShouldProcess($inf, 'Remove old devices, refresh driver store, install SCMFD_Keyboard_Root')) {
    exit 0
}

if (-not (Test-Administrator)) {
    Write-Error 'Deploy steps require an elevated PowerShell (Administrator). Re-run this script as admin, or use -SkipBuild with an existing package after building from a dev prompt.'
    exit 1
}

$devcon = Find-DevconExe
if (-not $devcon) {
    Write-Warning 'devcon.exe not found under Windows Kits\10\Tools. Install the WDK tools or Windows SDK with devcon.'
}

Remove-SCMFD_Keyboard_RootPnPDevices -DevconPath $devcon
Remove-SCMFD_Keyboard_RootFromDriverStore

Write-Host "pnputil /add-driver `"$inf`" /install"
& pnputil.exe /add-driver $inf /install 2>&1 | ForEach-Object { Write-Host $_ }

if ($devcon) {
    Write-Host "devcon install `"$inf`" root\SCMFD_Keyboard_Root"
    Invoke-Devcon -DevconPath $devcon -Arguments @('install', $inf, 'root\SCMFD_Keyboard_Root')
}

Write-Host 'Redeploy finished. If the driver still fails, check %windir%\INF\setupapi.dev.log for the last SCMFD_Keyboard_Root section.'
