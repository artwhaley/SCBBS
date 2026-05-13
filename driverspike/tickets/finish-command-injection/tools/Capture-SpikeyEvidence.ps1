#requires -version 5.1
<#
.SYNOPSIS
Capture non-destructive diagnostics for the driverspike finish-command-injection session.

.DESCRIPTION
Creates a timestamped evidence folder and records:
- test-signing / Secure Boot state
- package signature status
- Spikey PnP state
- binding validation output
- HID listing
- control --stats output
- optional inject output

This script does not build, deploy, enable, disable, remove, or install drivers.
#>

[CmdletBinding()]
param(
    [string] $Tag = 'capture',

    [switch] $IncludeInject,

    [ValidateRange(0, 255)]
    [int] $InjectUsage = 0x04,

    [ValidateRange(0, 255)]
    [int] $Modifier = 0
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Continue'

$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))
$EvidenceRoot = Join-Path $Root 'tickets\finish-command-injection\evidence'
$SafeTag = if ([string]::IsNullOrWhiteSpace($Tag)) { 'capture' } else { $Tag -replace '[^A-Za-z0-9_.-]', '-' }
$Stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$OutDir = Join-Path $EvidenceRoot "$Stamp-$SafeTag"
New-Item -ItemType Directory -Path $OutDir -Force | Out-Null

$TranscriptPath = Join-Path $OutDir 'session.log'

function Section {
    param([Parameter(Mandatory)][string]$Name)
    Write-Host ''
    Write-Host "==== $Name ===="
}

function Run-Step {
    param(
        [Parameter(Mandatory)][string]$Name,
        [Parameter(Mandatory)][scriptblock]$Script
    )

    Section $Name
    try {
        & $Script
    }
    catch {
        Write-Warning "$Name failed: $_"
    }
}

Start-Transcript -Path $TranscriptPath -Force | Out-Null

try {
    Section 'Capture Metadata'
    Write-Host "Root: $Root"
    Write-Host "Evidence: $OutDir"
    Write-Host "Tag: $Tag"
    Write-Host "Time: $(Get-Date -Format o)"
    Write-Host "User: $env:USERDOMAIN\$env:USERNAME"
    $principal = [Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
    Write-Host "IsAdmin: $($principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator))"

    Run-Step 'BCDEdit Entries' {
        & bcdedit.exe /enum
    }

    Run-Step 'Secure Boot' {
        try {
            $secureBoot = Confirm-SecureBootUEFI
            Write-Host "Confirm-SecureBootUEFI: $secureBoot"
        }
        catch {
            Write-Host "Confirm-SecureBootUEFI unavailable or not applicable: $_"
        }
    }

    Run-Step 'Build Outputs' {
        Get-ChildItem -LiteralPath (Join-Path $Root 'x64\Debug') -ErrorAction SilentlyContinue |
            Select-Object Name, Length, LastWriteTime |
            Format-Table -AutoSize
    }

    Run-Step 'Authenticode Signatures' {
        $paths = @(
            (Join-Path $Root 'x64\Debug\TheSpikeyDriver.dll'),
            (Join-Path $Root 'x64\Debug\TheSpikeyDriver\thespikeydriver.cat'),
            (Join-Path $Root 'x64\Debug\TestEnumeratorAndClient.exe'),
            (Join-Path $Root 'x64\Debug\KeystrokeInjectorGui.exe')
        )
        foreach ($path in $paths) {
            if (Test-Path -LiteralPath $path) {
                Write-Host "Signature: $path"
                Get-AuthenticodeSignature -LiteralPath $path |
                    Format-List Status, StatusMessage, SignerCertificate
            }
            else {
                Write-Host "Missing: $path"
            }
        }
    }

    Run-Step 'Spikey PnP Devices' {
        Get-PnpDevice -ErrorAction SilentlyContinue |
            Where-Object {
                $_.FriendlyName -like '*Spikey*' -or
                $_.InstanceId -like 'ROOT\TheSpikeyDriver*' -or
                $_.InstanceId -like 'ROOT\HIDCLASS*'
            } |
            Sort-Object Class, FriendlyName, InstanceId |
            Format-List Status, Class, FriendlyName, InstanceId, Problem, Present
    }

    Run-Step 'Keyboard HIDClass Children' {
        Get-CimInstance Win32_PnPEntity -ErrorAction SilentlyContinue |
            Where-Object {
                ($_.PNPClass -eq 'Keyboard' -or $_.Caption -like '*Keyboard*') -and
                ($_.PNPDeviceID -like 'HID\HIDCLASS*' -or $_.PNPDeviceID -like '*VID_DEED*')
            } |
            Select-Object Name, PNPDeviceID, Service, Status, ConfigManagerErrorCode |
            Format-List
    }

    Run-Step 'Driver Binding Validation Script' {
        $script = Join-Path $Root 'TheSpikeyDriver\Validate-TheSpikeyDriverBinding.ps1'
        if (Test-Path -LiteralPath $script) {
            & $script
        }
        else {
            Write-Host "Missing validation script: $script"
        }
    }

    Run-Step 'TestEnumeratorAndClient --list-hid' {
        $client = Join-Path $Root 'x64\Debug\TestEnumeratorAndClient.exe'
        if (Test-Path -LiteralPath $client) {
            & $client --list-hid
        }
        else {
            Write-Host "Missing client: $client"
        }
    }

    Run-Step 'TestEnumeratorAndClient --stats' {
        $client = Join-Path $Root 'x64\Debug\TestEnumeratorAndClient.exe'
        if (Test-Path -LiteralPath $client) {
            & $client --stats
        }
        else {
            Write-Host "Missing client: $client"
        }
    }

    if ($IncludeInject) {
        Run-Step "TestEnumeratorAndClient --inject 0x$('{0:X2}' -f $InjectUsage) 0x$('{0:X2}' -f $Modifier)" {
            $client = Join-Path $Root 'x64\Debug\TestEnumeratorAndClient.exe'
            if (Test-Path -LiteralPath $client) {
                & $client --inject ("0x{0:X2}" -f $InjectUsage) ("0x{0:X2}" -f $Modifier)
            }
            else {
                Write-Host "Missing client: $client"
            }
        }
    }
}
finally {
    Stop-Transcript | Out-Null
    Write-Host "Evidence written to: $OutDir"
}
