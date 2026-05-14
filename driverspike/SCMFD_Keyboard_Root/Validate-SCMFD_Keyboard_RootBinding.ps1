#requires -version 5.1
<#
.SYNOPSIS
Validate that SCMFD_Keyboard_Root-like HID entries are using kbdhid and inspect reported HID report sizes.

.DESCRIPTION
Non-destructive checks:
- list keyboard-class PnP devices and show matched service driver
- flag SCMFD_Keyboard_Root VID/PID matches
- attempt to show InputReportByteLength for matching VID/PID via TestEnumeratorAndClient list output
#>

[CmdletBinding()]
param(
    [ValidateRange(1, 65535)]
    [int] $VendorId = 0x5343,

    [ValidateRange(1, 65535)]
    [int] $ProductId = 0x4B42
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script:signedDriverCache = @()

function Get-ServiceForInstance {
    param([string]$InstanceId)

    $service = $null
    $driverName = $null

    try {
        $pnpProperties = Get-PnpDeviceProperty -InstanceId $InstanceId -KeyName 'DEVPKEY_Device_Service' -ErrorAction Stop
        if ($null -ne $pnpProperties) {
            $service = $pnpProperties.Data
        }
    }
    catch {
        if ($script:signedDriverCache.Count -eq 0) {
            try {
                $script:signedDriverCache = @(
                    Get-CimInstance -ClassName Win32_PnPSignedDriver -ErrorAction Stop
                )
            }
            catch {
                $script:signedDriverCache = @()
            }
        }
        if ($script:signedDriverCache.Count -gt 0) {
            $match = $script:signedDriverCache | Where-Object {
                $_.DeviceID -eq $InstanceId -or $_.DeviceID -like "*$InstanceId*"
            } | Select-Object -First 1
            if ($match) {
                $service = $match.Service
                $driverName = $match.DriverName
            }
        }
    }

    [PSCustomObject]@{
        Service = $service
        DriverName = $driverName
    }
}

$vidHex = ('{0:X4}' -f $VendorId)
$pidHex = ('{0:X4}' -f $ProductId)
$vidToken = "VID_$vidHex"
$pidToken = "PID_$pidHex"
$targetLabel = "VID=${vidToken} PID=${pidToken}"

Write-Host ("=== SCMFD Keyboard binding check for {0} ===" -f $targetLabel)
Write-Host "Collecting keyboard PnP devices..."

$keyboardDevices = @(Get-CimInstance -ClassName Win32_PnPEntity -ErrorAction Stop |
    Where-Object { $_.PNPClass -eq 'Keyboard' -or $_.Caption -like '*Keyboard*' })

if ($keyboardDevices.Count -eq 0) {
    Write-Warning "No keyboard-class Win32_PnPEntity entries found."
    return
}

$rows = foreach ($d in $keyboardDevices) {
    $ids = @()
    if ($null -ne $d.HardwareID) { $ids += @($d.HardwareID) }
    if ($null -ne $d.PNPDeviceID) { $ids += $d.PNPDeviceID }
    $idText = ($ids -join ' ')
    $isVidPidSCMFDKeyboard = ($idText -match $vidToken) -and ($idText -match $pidToken)
    $isHidClassInstance = ($d.PNPDeviceID -like 'HID\HIDCLASS*')
    $hasKeyboardUsage = ($idText -match 'UP:0001_U:0006') -or ($idText -match 'HID_DEVICE_SYSTEM_KEYBOARD')
    $isLikelySCMFDKeyboard = $isVidPidSCMFDKeyboard

    $instanceForFilter = if ($d.DeviceID) { $d.DeviceID } else { $d.PNPDeviceID }
    if ([string]::IsNullOrWhiteSpace($instanceForFilter)) {
        $resolvedService = [PSCustomObject]@{ Service = $null; DriverName = $null }
    }
    else {
        $resolvedService = Get-ServiceForInstance -InstanceId $instanceForFilter
    }

    $friendlyName = $null
    $present = $null
    try {
        $pnpDevice = Get-PnpDevice -InstanceId $instanceForFilter -ErrorAction Stop
        $friendlyName = $pnpDevice.FriendlyName
        $present = $pnpDevice.Present
    }
    catch {
        $friendlyName = $null
        $present = $null
    }

    if (-not $isLikelySCMFDKeyboard -and $friendlyName) {
        if ($friendlyName -like '*SCMFD Keyboard*' -or $friendlyName -like '*SCMFD_Keyboard_Root*' -or $friendlyName -like '*virtual HID*') {
            $isLikelySCMFDKeyboard = $true
        }
    }
    if (-not $isLikelySCMFDKeyboard -and $isHidClassInstance -and $hasKeyboardUsage -and ($idText -match 'HID\\SCMFD_Keyboard_Root')) {
        $isLikelySCMFDKeyboard = $true
    }

    $serviceValue = if ($resolvedService.Service) { $resolvedService.Service } else { '<not-found>' }
    $driverNameValue = if ($resolvedService.DriverName) { $resolvedService.DriverName } else { '<not-found>' }

    [PSCustomObject]@{
        InstanceId = $d.PNPDeviceID
        Name = if ($friendlyName) { $friendlyName } else { $d.Name }
        IsSCMFDKeyboard = $isLikelySCMFDKeyboard
        IsVidPidSCMFDKeyboard = $isVidPidSCMFDKeyboard
        IsHidClassInstance = $isHidClassInstance
        Present = $present
        HasKeyboardUsage = $hasKeyboardUsage
        Service = $serviceValue
        ConfigManagerErrorCode = $d.ConfigManagerErrorCode
        HardwareId = $idText
        SignedDriver = $driverNameValue
    }
}

$rows | Format-Table -AutoSize

$scmfdKeyboardRows = @($rows | Where-Object { $_.IsSCMFDKeyboard -eq $true })
if ($scmfdKeyboardRows.Count -eq 0) {
    Write-Warning "No keyboard PnP entry matched VID=${vidToken} PID=${pidToken}."
} else {
    Write-Host "Matched SCMFD Keyboard-like entries:"
    $scmfdKeyboardRows | Format-Table -AutoSize
    $presentSCMFDKeyboardRows = @($scmfdKeyboardRows | Where-Object { $_.Present -eq $true -and $_.ConfigManagerErrorCode -eq 'CM_PROB_NONE' })
    if ($presentSCMFDKeyboardRows.Count -gt 0) {
        Write-Host "Present + healthy SCMFD Keyboard-like entries:"
        $presentSCMFDKeyboardRows | Format-Table -AutoSize
    } else {
        Write-Warning "No present+healthy SCMFD Keyboard-like keyboard entries found."
    }

    $bad = @($scmfdKeyboardRows | Where-Object { $_.Service -ne 'kbdhid' -and $_.Service -ne '<not-found>' })
    if ($bad.Count -gt 0) {
        Write-Warning "Some matched SCMFD Keyboard entries are not bound to kbdhid."
        $bad | ForEach-Object {
            Write-Warning ("  {0} Service={1}" -f $_.InstanceId, $_.Service)
        }
    }

    $usageBad = @($scmfdKeyboardRows | Where-Object { $_.HasKeyboardUsage -ne $true })
    if ($usageBad.Count -gt 0) {
        Write-Warning "Some SCMFD Keyboard-like entries do not advertise keyboard usage IDs."
    }
}

$client = Join-Path $PSScriptRoot '..\x64\Debug\TestEnumeratorAndClient.exe'
if (Test-Path -LiteralPath $client) {
    Write-Host "Running TestEnumeratorAndClient --list-hid for InputReportByteLength correlation:"
    $contractLines = & $client --list-hid
    $matchLines = @($contractLines | Select-String -Pattern ("VID=0x{0} PID=0x{1}" -f $vidHex, $pidHex))
    if ($matchLines.Count -gt 0) {
        $matchLines | ForEach-Object { Write-Host $_.Line }
    } else {
        Write-Warning "No --list-hid line matched VID=0x$vidHex PID=0x$pidHex."
    }
} else {
    Write-Warning "TestEnumeratorAndClient not found at $client; contract check not run."
}

