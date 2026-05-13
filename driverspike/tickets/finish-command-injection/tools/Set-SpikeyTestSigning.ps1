#requires -version 5.1
<#
.SYNOPSIS
Enable, disable, or inspect Windows test-signing mode for the driverspike runtime session.

.DESCRIPTION
This is a small wrapper around bcdedit. It does not restart unless -Restart is supplied.

Examples:
  .\Set-SpikeyTestSigning.ps1 -Status
  .\Set-SpikeyTestSigning.ps1 -Enable
  .\Set-SpikeyTestSigning.ps1 -Enable -Restart
  .\Set-SpikeyTestSigning.ps1 -Disable
#>

[CmdletBinding(SupportsShouldProcess = $true)]
param(
    [switch] $Enable,
    [switch] $Disable,
    [switch] $Status,
    [switch] $Restart
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Test-Administrator {
    $principal = [Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Show-TestSigningStatus {
    Write-Host 'Current bcdedit test-signing state:'
    $output = & bcdedit.exe /enum 2>&1
    if ($LASTEXITCODE -ne 0) {
        $output | ForEach-Object { Write-Host $_ }
        Write-Host "bcdedit exited with code $LASTEXITCODE. Run from elevated PowerShell if access was denied."
        return
    }
    $output | Select-String -Pattern 'testsigning' -CaseSensitive:$false | ForEach-Object {
        Write-Host $_.Line
    }
    if (-not ($output | Select-String -Pattern 'testsigning' -CaseSensitive:$false)) {
        Write-Host '(No testsigning line found in bcdedit output.)'
    }
}

$modeCount = @($Enable, $Disable, $Status | Where-Object { $_ }).Count
if ($modeCount -eq 0) {
    $Status = $true
    $modeCount = 1
}
if ($modeCount -gt 1) {
    throw 'Choose only one of -Enable, -Disable, or -Status.'
}

if ($Status) {
    Show-TestSigningStatus
    return
}

if (-not (Test-Administrator)) {
    throw 'Changing test-signing mode requires an elevated PowerShell.'
}

if ($Enable) {
    if ($PSCmdlet.ShouldProcess('BCD store', 'Enable Windows test-signing mode')) {
        & bcdedit.exe /set testsigning on
        if ($LASTEXITCODE -ne 0) {
            throw "bcdedit /set testsigning on failed with exit code $LASTEXITCODE. If Secure Boot is enabled, firmware policy may block test signing."
        }
    }
}
elseif ($Disable) {
    if ($PSCmdlet.ShouldProcess('BCD store', 'Disable Windows test-signing mode')) {
        & bcdedit.exe /set testsigning off
        if ($LASTEXITCODE -ne 0) {
            throw "bcdedit /set testsigning off failed with exit code $LASTEXITCODE."
        }
    }
}

Show-TestSigningStatus

if ($Restart) {
    if ($PSCmdlet.ShouldProcess('computer', 'Restart now')) {
        shutdown.exe /r /t 0
    }
}
else {
    Write-Host ''
    Write-Host 'Restart is required for the change to take effect.'
    Write-Host 'Run with -Restart, or restart manually when ready.'
}
