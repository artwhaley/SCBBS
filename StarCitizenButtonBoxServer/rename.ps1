Set-Location "c:\Users\artwh\Documents\custom-driver\StarCitizenButtonBoxServer"

$replacements = @{
    "ButtonBoxWebSocketServer" = "ButtonBoxServer"
    "MediaTransportManager" = "MediaHandler"
    "MediaPlaybackSnapshot" = "MediaSnapshot"
    "SendMediaUpdateToAsync" = "SendMediaSnapshot"
    "TrySendTransportCommandAsync" = "HandleAction"
    "BuildSnapshotAsync" = "GetSnapshotAsync"
    "PushSnapshotAsync" = "PublishSnapshotAsync"
    "BroadcastMediaUpdate" = "BroadcastMedia"
    "RestartWebSocketAfterSave" = "RestartServer"
    "StartupWorkAsync" = "InitializeAsync"
}

Get-ChildItem -Filter "*.cs" -File | ForEach-Object {
    $file = $_.FullName
    $text = Get-Content $file -Raw
    foreach ($key in $replacements.Keys) {
        $text = $text.Replace($key, $replacements[$key])
    }
    # Inline ShortPeerId
    $text = $text.Replace("var peer = ShortPeerId(socket);", 'var peer = socket.ConnectionInfo.Id.ToString("N")[..8];')
    $text = $text -replace 'static string ShortPeerId\(IWebSocketConnection socket\) =>\s*socket\.ConnectionInfo\.Id\.ToString\("N"\)\[\.\.8\];', ''
    
    $text | Set-Content $file
}
