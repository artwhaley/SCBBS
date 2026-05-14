#requires -version 5.1
<#
.SYNOPSIS
  Destructive cleanup: remove SCMFD Joystick root-enumerated devices and matching driver-store packages.

.DESCRIPTION
  Run in an elevated PowerShell. Targets the same scope as the Phase 2 audit:
  - PnP devices: FriendlyName like *SCMFD Joystick* under ROOT\*
  - pnputil packages: SCMFD_Joystick_Root.inf (any case), OR any package whose provider matches SCMFD_Joystick_Root.

  Order: remove devices first, then delete driver packages (newest OEM first).

.PARAMETER WhatIf
  List actions only; does not remove devices or delete packages.

.EXAMPLE
  .\Cleanup-SCMFD_Joystick_RootEnvironment.ps1

.EXAMPLE
  .\Cleanup-SCMFD_Joystick_RootEnvironment.ps1 -WhatIf
#>
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
    return $packages
}

function Get-CleanupTargetPackages {
    $packages = @(Get-PnputilDriverPackages)
    $packages | Where-Object {
        ($null -ne $_.OriginalName -and $_.OriginalName -match '(?i)^scmfd_joystick_root\.inf$') -or
        ($null -ne $_.Provider -and $_.Provider -match '(?i)scmfd_joystick_root')
    }
}

function Get-OemNumericSortKey {
    param([string]$OemInf)
    if ($OemInf -match 'oem(\d+)\.inf') {
        return [int]$Matches[1]
    }
    return 0
}

function Remove-PnpDeviceInstance {
    <#
      Win10 19045 / many PS 5.1 hosts: Remove-PnpDevice cmdlet is missing.
      pnputil /remove-device works from Win10 2004+ (incl. build 19045).
    #>
    param(
        [Parameter(Mandatory)][string]$InstanceId
    )
    $removed = $false
    if (Get-Command Remove-PnpDevice -ErrorAction SilentlyContinue) {
        try {
            Remove-PnpDevice -InstanceId $InstanceId -Confirm:$false -ErrorAction Stop
            $removed = $true
        }
        catch {
            Write-Warning "Remove-PnpDevice failed for ${InstanceId}: $($_.Exception.Message)"
        }
    }
    if (-not $removed) {
        Write-Host "pnputil /remove-device `"$InstanceId`""
        $out = & pnputil.exe /remove-device $InstanceId 2>&1
        $out | ForEach-Object { Write-Host $_ }
        if ($LASTEXITCODE -ne 0) {
            Write-Warning "pnputil /remove-device exit code $LASTEXITCODE for $InstanceId"
        }
        else {
            $removed = $true
        }
    }
    return $removed
}

$destructive = -not $WhatIfPreference
if ($destructive -and -not (Test-Administrator)) {
    Write-Error 'Destructive steps require an elevated PowerShell (Run as administrator). Use -WhatIf to preview without admin.'
    exit 1
}

Write-Host '=== Devices to remove (ROOT\* , FriendlyName *SCMFD Joystick*) ===' -ForegroundColor Cyan
$devices = @(Get-PnpDevice -ErrorAction SilentlyContinue |
    Where-Object { $_.FriendlyName -like '*SCMFD Joystick*' -and $_.InstanceId -like 'ROOT\*' })
if ($devices.Count -eq 0) {
    Write-Host '(none found)'
}
else {
    $devices | Select-Object InstanceId, FriendlyName, Status, Problem | Format-Table -AutoSize
}

Write-Host '=== Driver-store packages to delete ===' -ForegroundColor Cyan
$pkgs = @(Get-CleanupTargetPackages | Sort-Object { Get-OemNumericSortKey $_.OEM } -Descending)
if ($pkgs.Count -eq 0) {
    Write-Host '(none matched filters)'
}
else {
    $pkgs | Select-Object OEM, OriginalName, Provider | Format-Table -AutoSize
}

if ($WhatIfPreference) {
    Write-Host 'WhatIf: no devices removed and no packages deleted.' -ForegroundColor Yellow
    exit 0
}

foreach ($d in $devices) {
    $id = $d.InstanceId
    if ($PSCmdlet.ShouldProcess($id, 'Remove device (pnputil or Remove-PnpDevice)')) {
        if (Remove-PnpDeviceInstance -InstanceId $id) {
            Write-Host "Removed device: $id" -ForegroundColor Green
        }
    }
}

foreach ($p in $pkgs) {
    $oem = $p.OEM
    if ($PSCmdlet.ShouldProcess($oem, 'pnputil /delete-driver /uninstall /force')) {
        try {
            Write-Host "pnputil /delete-driver $oem /uninstall /force"
            & pnputil.exe /delete-driver $oem /uninstall /force 2>&1 | ForEach-Object { Write-Host $_ }
        }
        catch {
            Write-Warning "pnputil delete failed for ${oem}: $($_.Exception.Message)"
        }
    }
}

Write-Host 'Cleanup finished. Re-run Redeploy-SCMFD_Joystick_Root.ps1 to stage a fresh package and install root\SCMFD_Joystick_Root.' -ForegroundColor Green
