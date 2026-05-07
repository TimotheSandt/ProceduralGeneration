# install.ps1 - Installs the CUI language extension into VS Code
#
# Usage:
#   .\install.ps1                        # default profile (~/.vscode/extensions)
#   .\install.ps1 -Profile "C++"        # named profile
#   .\install.ps1 -List                 # list available profiles

param(
    [string]$Profile = "",
    [switch]$List
)

$extDirName  = "proceduralgeneration.cui-language-0.2.0"
$profilesDir = "$env:APPDATA\Code\User\profiles"
$storageJson = "$env:APPDATA\Code\User\globalStorage\storage.json"

# ── Read profile name→location map from storage.json ──────────────────────────
function Get-Profiles {
    if (-not (Test-Path $storageJson)) { return @() }
    $data = Get-Content $storageJson -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($null -eq $data.userDataProfiles) { return @() }
    return $data.userDataProfiles
}

# ── List profiles ──────────────────────────────────────────────────────────────
if ($List) {
    Write-Host "  (default)  ->  $env:USERPROFILE\.vscode\extensions"
    foreach ($p in (Get-Profiles)) {
        Write-Host "  '$($p.name)'  ->  $profilesDir\$($p.location)\extensions"
    }
    return
}

# ── Resolve target extensions directory ───────────────────────────────────────
if ($Profile -eq "") {
    $extDir = "$env:USERPROFILE\.vscode\extensions"
} else {
    $match = Get-Profiles | Where-Object { $_.name -eq $Profile } | Select-Object -First 1
    if (-not $match) {
        Write-Error "Profile '$Profile' not found."
        Write-Host "Available profiles:"
        foreach ($p in (Get-Profiles)) { Write-Host "  '$($p.name)'" }
        exit 1
    }
    $extDir = "$profilesDir\$($match.location)\extensions"
}

# ── Install files ──────────────────────────────────────────────────────────────
$src  = $PSScriptRoot
$dest = Join-Path $extDir $extDirName

New-Item -ItemType Directory -Force -Path $extDir | Out-Null
foreach ($old in @($dest, (Join-Path $extDir "cui-language"))) {
    if (Test-Path $old) { Remove-Item $old -Recurse -Force }
}
New-Item -ItemType Directory -Force -Path $dest | Out-Null
Copy-Item "$src\*" $dest -Recurse -Force

# ── Register in extensions.json (required for named profiles) ──────────────────
# Named profiles use a registry file; without it VS Code ignores the extension folder.
# Default profile (~/.vscode/extensions) is auto-scanned, so registration is optional there.
if ($Profile -ne "") {
    $regFile = "$env:APPDATA\Code\User\profiles\$($match.location)\extensions.json"

    # Build the path strings VS Code expects
    $fsPath   = $dest.Replace('\', '\\')
    $posixPath = "/" + $dest.Replace('\', '/').TrimStart('/')
    # Percent-encode the drive colon for the external URI
    $external = "file:///" + ($dest.Replace('\', '/') -replace '^([A-Za-z]):', '$1%3A')
    $timestamp = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()

    $entry = @{
        identifier = @{ id = "proceduralgeneration.cui-language" }
        version    = "0.2.0"
        location   = @{ "`$mid" = 1; fsPath = $dest; _sep = 1; external = $external; path = $posixPath; scheme = "file" }
        metadata   = @{
            isApplicationScoped = $false
            isMachineScoped     = $false
            isBuiltin           = $false
            installedTimestamp  = $timestamp
            pinned              = $false
            source              = "vsix"
        }
    }

    if (Test-Path $regFile) {
        $reg = Get-Content $regFile -Raw -Encoding UTF8 | ConvertFrom-Json
        # Remove any existing entry for our extension
        $reg = @($reg | Where-Object { $_.identifier.id -ne "proceduralgeneration.cui-language" })
    } else {
        $reg = @()
    }
    $reg += $entry
    # PowerShell 5.1 Set-Content UTF8 adds a BOM; VS Code requires BOM-free UTF-8
    $json = $reg | ConvertTo-Json -Depth 10
    [System.IO.File]::WriteAllText($regFile, $json, [System.Text.UTF8Encoding]::new($false))
    Write-Host "Registered in: $regFile"
}

Write-Host "Installed to: $dest"
Write-Host "Reload VS Code (Ctrl+Shift+P -> 'Developer: Reload Window') to activate."
