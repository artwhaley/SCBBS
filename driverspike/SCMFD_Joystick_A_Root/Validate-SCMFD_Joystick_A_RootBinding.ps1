#requires -version 5.1
[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$devices = @(Get-PnpDevice -ErrorAction SilentlyContinue | Where-Object {
    $_.InstanceId -like 'ROOT\SCMFD_Joystick_A_Root*' -or
    $_.FriendlyName -like '*SCMFD Joystick A*'
})
$devices | Select-Object InstanceId,FriendlyName,Class,Status,Problem | Format-Table -AutoSize

$client = Join-Path $PSScriptRoot '..\x64\Debug\TestEnumeratorAndClient.exe'
if (Test-Path -LiteralPath $client) {
    & $client --list-hid | Select-String '\[scmfd-joystick' | ForEach-Object { $_.Line }
}
