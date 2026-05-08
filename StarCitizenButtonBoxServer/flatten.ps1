Set-Location $PSScriptRoot

function Merge-CSharpFiles {
    param([string[]]$Files, [string]$OutputFile)
    $usings = @()
    $content = @()
    foreach ($f in $Files) {
        $fileLines = Get-Content $f
        foreach ($line in $fileLines) {
            if ($line -match "^using ") {
                if ($line -notmatch "^using StarCitizenButtonBoxServer\.") {
                    if ($usings -notcontains $line) { $usings += $line }
                }
            } elseif ($line -match "^namespace StarCitizenButtonBoxServer") {
                # skip
            } else {
                $content += $line
            }
        }
    }
    
    $out = @()
    $out += $usings
    $out += ""
    $out += "namespace StarCitizenButtonBoxServer;"
    $out += $content
    $out | Set-Content $OutputFile
}

Merge-CSharpFiles -Files @("Bindings\BindingManager.cs", "Bindings\BindingConfig.cs", "Bindings\CommandConfig.cs") -OutputFile "BindingManager.cs"
Merge-CSharpFiles -Files @("Media\MediaTransportManager.cs", "Media\SystemVolume.cs") -OutputFile "MediaHandler.cs"
Merge-CSharpFiles -Files @("Converters\IntStringConverter.cs") -OutputFile "UiHelpers.cs"
Merge-CSharpFiles -Files @("Server\ButtonBoxWebSocketServer.cs") -OutputFile "WebSocketServer.cs"
Merge-CSharpFiles -Files @("Server\FirewallHelper.cs") -OutputFile "FirewallHelper.cs"
Merge-CSharpFiles -Files @("ViewModels\BindingsEditorViewModel.cs") -OutputFile "BindingsEditorViewModel.cs"

function Remove-SubUsings {
    param([string]$File)
    $lines = Get-Content $File
    $newContent = $lines | Where-Object { $_ -notmatch "^using StarCitizenButtonBoxServer\." }
    $newContent | Set-Content $File
}

Remove-SubUsings "App.xaml.cs"
Remove-SubUsings "BindingsEditorWindow.xaml.cs"
Remove-SubUsings "ServerDashboardWindow.xaml.cs"
Remove-SubUsings "InputHandler.cs"

$xaml = Get-Content "BindingsEditorWindow.xaml"
$xaml = $xaml -replace 'xmlns:vm="clr-namespace:StarCitizenButtonBoxServer\.ViewModels"', 'xmlns:vm="clr-namespace:StarCitizenButtonBoxServer"'
$xaml = $xaml -replace 'xmlns:bind="clr-namespace:StarCitizenButtonBoxServer\.Bindings"', 'xmlns:bind="clr-namespace:StarCitizenButtonBoxServer"'
$xaml = $xaml -replace 'xmlns:conv="clr-namespace:StarCitizenButtonBoxServer\.Converters"', 'xmlns:conv="clr-namespace:StarCitizenButtonBoxServer"'
$xaml | Set-Content "BindingsEditorWindow.xaml"

Remove-Item -Path "Bindings" -Recurse -Force
Remove-Item -Path "Input" -Recurse -Force
Remove-Item -Path "Server" -Recurse -Force
Remove-Item -Path "Media" -Recurse -Force
Remove-Item -Path "Services" -Recurse -Force
Remove-Item -Path "Converters" -Recurse -Force
Remove-Item -Path "ViewModels" -Recurse -Force
