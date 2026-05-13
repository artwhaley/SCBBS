$devcon = Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\Tools" -Filter devcon.exe -Recurse | Where-Object { $_.FullName -match 'x64' } | Select-Object -First 1 -ExpandProperty FullName
if (-not $devcon) { 
    Write-Error "devcon.exe not found. Please ensure WDK is installed."
    exit 1 
}

$devices = Get-CimInstance Win32_PnPEntity | Where-Object { $_.Name -like '*TheSpikeyDriver*' }
if (-not $devices) { 
    Write-Host "No active Spikey devices found to disable."
    exit 0 
}

foreach ($dev in $devices) {
    Write-Host "Disabling $($dev.PNPDeviceID)..."
    & $devcon disable "@$($dev.PNPDeviceID)"
}

Write-Host "Done. The virtual keyboard should be stopped now."
