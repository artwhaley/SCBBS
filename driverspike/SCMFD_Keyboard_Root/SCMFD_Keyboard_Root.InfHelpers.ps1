# Shared by fix_Inf_DriverVer.ps1 (MSBuild) and Redeploy-SCMFD_Keyboard_Root.ps1.
# Normalizes stamped INFs for packaging consistency (DriverVer spacing only).

function Get-SCMFD_Keyboard_RootInfCandidates {
    param(
        [Parameter(Mandatory)][string]$OutDir,
        [Parameter(Mandatory)][string]$ProjectDir,
        [Parameter()][string]$Configuration = 'Debug'
    )
    $set = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $tryAdd = {
        param([string]$p)
        if (-not $p) { return }
        $full = $null
        try { $full = (Resolve-Path -LiteralPath $p -ErrorAction Stop).Path } catch { return }
        if (Test-Path -LiteralPath $full -PathType Leaf) { [void]$set.Add($full) }
    }

    foreach ($p in @(
            (Join-Path $OutDir 'SCMFD_Keyboard_Root.inf'),
            (Join-Path $OutDir 'SCMFD_Keyboard_Root\SCMFD_Keyboard_Root.inf')
        )) { & $tryAdd $p }

    $parent = Split-Path -Parent $OutDir
    if ($parent) {
        foreach ($p in @(
                (Join-Path $parent 'SCMFD_Keyboard_Root\SCMFD_Keyboard_Root.inf'),
                (Join-Path $parent 'SCMFD_Keyboard_Root.inf')
            )) { & $tryAdd $p }
    }

    foreach ($p in @(
            (Join-Path $ProjectDir "x64\$Configuration\SCMFD_Keyboard_Root.inf"),
            (Join-Path $ProjectDir "x64\$Configuration\SCMFD_Keyboard_Root\SCMFD_Keyboard_Root.inf")
        )) { & $tryAdd $p }

    $slnParent = Split-Path -Parent $ProjectDir
    if ($slnParent) {
        foreach ($p in @(
                (Join-Path $slnParent "x64\$Configuration\SCMFD_Keyboard_Root\SCMFD_Keyboard_Root.inf"),
                (Join-Path $slnParent "x64\$Configuration\SCMFD_Keyboard_Root.inf")
            )) { & $tryAdd $p }
    }

    $set | Sort-Object
}

function Normalize-SCMFD_Keyboard_RootInfText {
    param([Parameter(Mandatory)][string]$Text)
    $t = [regex]::Replace($Text, 'DriverVer\s*=\s*', 'DriverVer=')
    $t
}

function Repair-SCMFD_Keyboard_RootInfFile {
    param([Parameter(Mandatory)][string]$LiteralPath)
    if (-not (Test-Path -LiteralPath $LiteralPath -PathType Leaf)) { return $false }
    $bytes = [IO.File]::ReadAllBytes($LiteralPath)
    $utf16 = [Text.Encoding]::Unicode
    if ($bytes.Length -ge 2 -and $bytes[0] -eq 0xFF -and $bytes[1] -eq 0xFE) {
        $t = $utf16.GetString($bytes)
        $enc = $utf16
    }
    elseif ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) {
        $t = [Text.Encoding]::UTF8.GetString($bytes, 3, $bytes.Length - 3)
        $enc = [Text.Encoding]::UTF8
    }
    elseif ($bytes.Length -ge 6 -and $bytes[1] -eq 0 -and $bytes[3] -eq 0 -and $bytes[5] -eq 0) {
        # StampInf output: UTF-16 LE without BOM (ASCII INF with NUL high bytes).
        $t = $utf16.GetString($bytes)
        $enc = $utf16
    }
    else {
        $t = [Text.Encoding]::UTF8.GetString($bytes)
        $enc = [Text.Encoding]::UTF8
    }

    $t2 = Normalize-SCMFD_Keyboard_RootInfText $t
    if ($t2 -eq $t) { return $false }
    [IO.File]::WriteAllText($LiteralPath, $t2, $enc)
    return $true
}

function Repair-AllSCMFD_Keyboard_RootInfs {
    param(
        [Parameter(Mandatory)][string]$OutDir,
        [Parameter(Mandatory)][string]$ProjectDir,
        [Parameter()][string]$Configuration = 'Debug'
    )
    $changed = @()
    foreach ($p in (Get-SCMFD_Keyboard_RootInfCandidates -OutDir $OutDir -ProjectDir $ProjectDir -Configuration $Configuration)) {
        if (Repair-SCMFD_Keyboard_RootInfFile -LiteralPath $p) { $changed += $p }
    }
    return @($changed)
}
