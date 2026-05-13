param(
    [int]$DelaySeconds = 5,
    [string]$Usage = "0x04",
    [int]$DurationMs = 50
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$client = Join-Path $root "x64\Debug\TestEnumeratorAndClient.exe"

if (-not (Test-Path -LiteralPath $client)) {
    throw "Client not found: $client"
}

Write-Host "Injecting usage $Usage in $DelaySeconds seconds. Focus the target window now."
Start-Sleep -Seconds $DelaySeconds

& $client --tap $Usage $DurationMs
