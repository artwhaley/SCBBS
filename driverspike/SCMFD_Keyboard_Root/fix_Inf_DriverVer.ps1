param(
    [Parameter(Mandatory = $false)]
    [string] $OutDir = '',
    [Parameter(Mandatory = $false)]
    [string] $ProjectDir = '',
    [Parameter(Mandatory = $false)]
    [string] $Configuration = 'Debug'
)
$here = $PSScriptRoot
. (Join-Path $here 'SCMFD_Keyboard_Root.InfHelpers.ps1')
if (-not $OutDir) { $OutDir = (Get-Location).Path }
if (-not $ProjectDir) { $ProjectDir = $here }
$changed = Repair-AllSCMFD_Keyboard_RootInfs -OutDir $OutDir -ProjectDir $ProjectDir -Configuration $Configuration
foreach ($p in $changed) {
    Write-Host "INF normalized: $p"
}
