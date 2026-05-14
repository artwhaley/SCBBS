#requires -version 5.1
[CmdletBinding(SupportsShouldProcess = $true)]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Test-Administrator {
    $p = [Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
    return $p.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Get-PnputilDriverPackages {
    $lines = @(pnputil.exe /enum-drivers 2>&1)
    $pub = $null; $orig = $null; $prov = $null
    $packages = [System.Collections.Generic.List[object]]::new()
    foreach ($line in $lines) {
        if ($line -match 'Published Name\s*:\s*(oem\d+\.inf)') {
            if ($pub) { $packages.Add([PSCustomObject]@{ OEM = $pub; OriginalName = $orig; Provider = $prov }) }
            $pub = $Matches[1]; $orig = $null; $prov = $null
        } elseif ($line -match 'Original Name\s*:\s*(.+)$') { $orig = $Matches[1].Trim() }
        elseif ($line -match 'Provider Name\s*:\s*(.+)$') { $prov = $Matches[1].Trim() }
    }
    if ($pub) { $packages.Add([PSCustomObject]@{ OEM = $pub; OriginalName = $orig; Provider = $prov }) }
    return $packages
}

function Remove-SCMFD_Joystick_B_RootWinmmCache {
    $paths = @(
        'HKLM:\SYSTEM\CurrentControlSet\Control\MediaProperties\PrivateProperties\Joystick\OEM\VID_DEED&PID_F10A',
        'HKCU:\System\CurrentControlSet\Control\MediaProperties\PrivateProperties\Joystick\OEM\VID_DEED&PID_F10A',
        'HKLM:\SYSTEM\CurrentControlSet\Control\MediaProperties\PrivateProperties\Joystick\OEM\VID_DEED&PID_F00A',
        'HKCU:\System\CurrentControlSet\Control\MediaProperties\PrivateProperties\Joystick\OEM\VID_DEED&PID_F00A',
        'HKLM:\SYSTEM\CurrentControlSet\Control\MediaProperties\PrivateProperties\Joystick\OEM\VID_DEED&PID_F00D',
        'HKCU:\System\CurrentControlSet\Control\MediaProperties\PrivateProperties\Joystick\OEM\VID_DEED&PID_F00D',
        'HKLM:\SYSTEM\CurrentControlSet\Control\MediaProperties\PrivateProperties\Joystick\OEM\VID_DEED&PID_FEED',
        'HKCU:\System\CurrentControlSet\Control\MediaProperties\PrivateProperties\Joystick\OEM\VID_DEED&PID_FEED',
        'HKLM:\SYSTEM\CurrentControlSet\Control\MediaProperties\PrivateProperties\Joystick\OEM\VID_5343&PID_4B42',
        'HKCU:\System\CurrentControlSet\Control\MediaProperties\PrivateProperties\Joystick\OEM\VID_5343&PID_4B42'
    )

    foreach ($path in $paths) {
        if (Test-Path $path) {
            Write-Host "Removing WinMM joystick cache: $path"
            Remove-Item -Path $path -Recurse -Force
        }
    }
}

if (-not $WhatIfPreference -and -not (Test-Administrator)) {
    Write-Error 'Run elevated (Administrator) for destructive cleanup, or pass -WhatIf.'
    exit 1
}

$devices = @(Get-PnpDevice -ErrorAction SilentlyContinue | Where-Object {
    $_.InstanceId -like 'ROOT\SCMFD_Joystick_B_Root*' -or
    $_.InstanceId -like 'ROOT\SCMFD_Joystick_B*' -or
    $_.InstanceId -like 'ROOT\SCMFD_Joystick_Root*' -or
    ($_.FriendlyName -like '*SCMFD Joystick B*' -and $_.InstanceId -like 'ROOT\HIDCLASS\*')
})

$pkgs = @(Get-PnputilDriverPackages | Where-Object {
    ($_.OriginalName -match '(?i)^scmfd_joystick_b_root\.inf$') -or
    ($_.OriginalName -match '(?i)^scmfd_joystick_root\.inf$') -or
    ($_.Provider -match '(?i)scmfd_joystick_b_root') -or
    ($_.Provider -match '(?i)scmfd_joystick_root')
})

$devices | Select-Object InstanceId,FriendlyName,Status,Problem | Format-Table -AutoSize
$pkgs | Select-Object OEM,OriginalName,Provider | Format-Table -AutoSize

if ($WhatIfPreference) { exit 0 }

foreach ($d in $devices) { & pnputil.exe /remove-device $d.InstanceId 2>&1 | Out-Host }
foreach ($oem in ($pkgs | Select-Object -ExpandProperty OEM -Unique)) { & pnputil.exe /delete-driver $oem /uninstall /force 2>&1 | Out-Host }
Remove-SCMFD_Joystick_B_RootWinmmCache
