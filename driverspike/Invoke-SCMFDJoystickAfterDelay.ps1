param(
    [ValidateRange(0, 60)]
    [int]$DelaySeconds = 5,

    [ValidateSet("button-down", "button-up", "axis", "release-all", "stats")]
    [string]$Command = "stats",

    [int]$Button = 1,
    [string]$Axis = "x",
    [int]$Value = 0
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$client = Join-Path $root "x64\Debug\TestEnumeratorAndClient.exe"

if (-not (Test-Path -LiteralPath $client)) {
    throw "Client not found: $client"
}

Write-Host "Running joystick command '$Command' in $DelaySeconds seconds. Focus joy.cpl now."
Start-Sleep -Seconds $DelaySeconds

switch ($Command) {
    "button-down" { & $client --joy-button $Button down; break }
    "button-up" { & $client --joy-button $Button up; break }
    "axis" { & $client --joy-axis $Axis $Value; break }
    "release-all" { & $client --joy-release-all; break }
    "stats" { & $client --joy-stats; break }
}
