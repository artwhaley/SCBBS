param(
    [int]$DelaySeconds = 5,
    [string]$Usage = "0x04",
    [string]$Modifier = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$client = Join-Path $root "x64\Debug\TestEnumeratorAndClient.exe"

if (-not (Test-Path -LiteralPath $client)) {
    throw "Client not found: $client"
}

Write-Host "Injecting usage $Usage in $DelaySeconds seconds. Focus the target window now."
Start-Sleep -Seconds $DelaySeconds

if ([string]::IsNullOrWhiteSpace($Modifier)) {
    & $client --inject $Usage
} else {
    & $client --inject $Usage $Modifier
}
