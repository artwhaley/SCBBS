#requires -version 5.1
<#!
.SYNOPSIS
Validate that SCMFD Joystick Root is installed and publishing HID joystick/control collections.
#>

[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Write-Host '=== SCMFD Joystick root devices ===' -ForegroundColor Cyan
$devices = @(Get-PnpDevice -ErrorAction SilentlyContinue | Where-Object {
    $_.FriendlyName -like '*SCMFD Joystick*' -or
    $_.InstanceId -like 'ROOT\SCMFD_Joystick_A*' -or
    $_.InstanceId -like 'ROOT\SCMFD_Joystick_B*' -or
    $_.InstanceId -like 'ROOT\SCMFD_Joystick_Root*'
})
if ($devices.Count -eq 0) {
    Write-Warning 'No SCMFD Joystick root PnP devices found.'
} else {
    $devices | Select-Object InstanceId, FriendlyName, Class, Status, Problem | Format-Table -AutoSize
}

Write-Host '=== SCMFD Joystick HID interfaces ===' -ForegroundColor Cyan
$client = Join-Path $PSScriptRoot '..\x64\Debug\TestEnumeratorAndClient.exe'
if (-not (Test-Path -LiteralPath $client)) {
    Write-Warning "TestEnumeratorAndClient not found at $client; build first."
    return
}

$lines = @(& $client --list-hid)
$matches = @($lines | Select-String -Pattern '\[scmfd-joystick')
if ($matches.Count -eq 0) {
    Write-Warning 'No [scmfd-joystick] or [scmfd-joystick-control] HID interfaces found.'
} else {
    $matches | ForEach-Object { Write-Host $_.Line }
}
