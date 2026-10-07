[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$StageDir,
    [Parameter(Mandatory)][ValidatePattern('^[0-9]+\.[0-9]+\.[0-9]+$')][string]$Version,
    [string]$OutputDir = 'dist'
)
$ErrorActionPreference = 'Stop'
$source = (Resolve-Path "$PSScriptRoot/../..").Path
$stage = (Resolve-Path $StageDir).Path
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
$output = (Resolve-Path $OutputDir).Path
$makensis = (Get-Command makensis.exe -ErrorAction SilentlyContinue).Source
if (-not $makensis) {
    foreach ($p in @("${env:ProgramFiles(x86)}\NSIS\makensis.exe", "$env:ProgramFiles\NSIS\makensis.exe")) {
        if (Test-Path $p) { $makensis = $p; break }
    }
}
if (-not $makensis) { throw 'makensis.exe not found (install NSIS)' }
$name = "Pad-$Version-Windows-x64-Setup.exe"
$out = Join-Path $output $name
if (Test-Path $out) { Remove-Item $out -Force }
& $makensis /V2 "/DVERSION=$Version" "/DSTAGE=$stage" "/DOUTFILE=$out" "/DIMAGES=$source\src\images" "$source\packaging\windows\installer\pad.nsi"
if ($LASTEXITCODE -ne 0) { throw 'makensis failed' }
$hash = (Get-FileHash $out -Algorithm SHA256).Hash.ToLowerInvariant()
"$hash  $name" | Set-Content "$out.sha256" -Encoding ascii
Write-Host "Installer: $out"
