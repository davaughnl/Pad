[CmdletBinding()]
param([Parameter(Mandatory)][string]$Installer, [Parameter(Mandatory)][string]$Version)
$ErrorActionPreference = 'Stop'
$target = Join-Path $env:RUNNER_TEMP 'pad-install-test'
if (Test-Path $target) { Remove-Item $target -Recurse -Force }
$key = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\Pad'
function Run($exe, $argline) {
    # Process.WaitForExit waits for the installer only; Start-Process -Wait would also wait for the relaunched Pad.
    $p = [System.Diagnostics.Process]::Start($exe, $argline)
    if (-not $p.WaitForExit(120000)) { $p.Kill(); throw "$exe timed out" }
    if ($p.ExitCode -ne 0) { throw "$exe exited $($p.ExitCode)" }
}
# Silent install (the same path the in-app updater uses).
Run $Installer "/S /D=$target"
Start-Sleep 3
Get-Process pad -ErrorAction SilentlyContinue | Stop-Process -Force
foreach ($f in 'bin\pad.exe','bin\SDL2.dll','bin\libssl-1_1-x64.dll','bin\libcrypto-1_1-x64.dll','driver\interception.dll','driver\install-interception.exe','uninstall.exe','licenses\Pad-GPL-3.0.txt') {
    if (-not (Test-Path (Join-Path $target $f))) { throw "missing after install: $f" }
}
$reg = Get-ItemProperty $key
if ($reg.DisplayVersion -ne $Version) { throw "DisplayVersion $($reg.DisplayVersion) != $Version" }
# Settings written by the user must survive an upgrade and uninstall.
New-Item -ItemType Directory -Force (Join-Path $target 'bin\profiles') | Out-Null
Set-Content (Join-Path $target 'bin\profiles\keep-me.gamecontroller.amgp') 'user data'
# Upgrade in place.
Run $Installer "/S /D=$target"
Start-Sleep 3
Get-Process pad -ErrorAction SilentlyContinue | Stop-Process -Force
if (-not (Test-Path (Join-Path $target 'bin\profiles\keep-me.gamecontroller.amgp'))) { throw 'upgrade removed user data' }
# Uninstall.
Run (Join-Path $target 'uninstall.exe') "/S _?=$target"
if (Test-Path (Join-Path $target 'bin\pad.exe')) { throw 'pad.exe still present after uninstall' }
if (-not (Test-Path (Join-Path $target 'bin\profiles\keep-me.gamecontroller.amgp'))) { throw 'uninstall removed user data' }
if (Test-Path $key) { throw 'uninstall registry key still present' }
Write-Host 'Installer test passed: silent install, upgrade, uninstall, user data kept.'
