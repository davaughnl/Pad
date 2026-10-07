[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$BuildDir,
    [Parameter(Mandatory)][string]$QtDir,
    [Parameter(Mandatory)][string]$SdlDir,
    [string]$OutputDir = 'dist',
    [Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{40}$')][string]$Revision,
    [string]$OpenSslDir,
    [string]$InterceptionZip,
    [ValidatePattern('^([0-9]+\.[0-9]+\.[0-9]+)?$')][string]$Version = ''
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$source = (Resolve-Path "$PSScriptRoot/../..").Path
$build = (Resolve-Path $BuildDir).Path
$qt = (Resolve-Path $QtDir).Path
$sdl = (Resolve-Path $SdlDir).Path
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
$output = (Resolve-Path $OutputDir).Path
$name = if ($Version) { "Pad-$Version-Windows-x64" } else { "Pad-$($Revision.Substring(0, 8))-Windows-x64" }
$stage = Join-Path $output $name
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
$bin = Join-Path $stage 'bin'
$data = Join-Path $stage 'share/antimicrox'
New-Item -ItemType Directory -Path $bin, "$data/translations", "$bin/profiles", "$bin/share/antimicrox/translations", "$bin/share/qt/translations", "$stage/licenses" -Force | Out-Null
Copy-Item "$build/bin/pad.exe" $bin
Copy-Item "$sdl/lib/x64/SDL2.dll" $bin
& "$qt/bin/windeployqt.exe" --release --no-compiler-runtime --no-opengl-sw --no-system-d3d-compiler "$bin/pad.exe"
if ($LASTEXITCODE -ne 0) { throw 'windeployqt failed' }
# Release link-time optimization can remove an unused linked Qt module from
# the import table, so windeployqt may omit it. Include the declared Qt runtime
# set explicitly, keeping the distribution's dependency contract predictable.
foreach ($module in @('Core', 'Gui', 'Widgets', 'Network', 'Concurrent')) {
    Copy-Item "$qt/bin/Qt5$module.dll" $bin -Force
}
# App-local MSVC runtime: the recipient does not need Visual Studio or an installer.
if (-not $env:VCToolsRedistDir) { throw 'Run in an MSVC developer environment (VCToolsRedistDir missing)' }
$crt = Get-ChildItem "$env:VCToolsRedistDir/x64" -Directory -Filter 'Microsoft.VC*.CRT' | Sort-Object Name -Descending | Select-Object -First 1
if (-not $crt) { throw 'MSVC x64 CRT directory not found' }
Copy-Item "$($crt.FullName)/*.dll" $bin
# Qt 5.15 loads OpenSSL 1.1.1 at run time for HTTPS (in-app updates).
if (-not $OpenSslDir) { throw 'OpenSslDir is required: HTTPS updates need libssl/libcrypto' }
$ssl = (Resolve-Path $OpenSslDir).Path
foreach ($dll in @('libssl-1_1-x64.dll', 'libcrypto-1_1-x64.dll')) {
    $found = Get-ChildItem $ssl -Recurse -Filter $dll | Select-Object -First 1
    if (-not $found) { throw "$dll not found under $ssl" }
    Copy-Item $found.FullName $bin
}
$sslLicense = Get-ChildItem $ssl -Recurse -Include 'LICENSE*','license*' -File -ErrorAction SilentlyContinue | Select-Object -First 1
if ($sslLicense) { Copy-Item $sslLicense.FullName "$stage/licenses/OpenSSL-LICENSE.txt" }
# Optional experimental driver mode: bundle the Interception driver files. Pad never installs them by itself.
if ($InterceptionZip) {
    $expected = 'ad038963d6413055765128b0b931f6e765147c9916dba79e65d872b261f9af10'
    $actual = (Get-FileHash $InterceptionZip -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $expected) { throw "Interception.zip hash mismatch: $actual" }
    $x = Join-Path $output 'interception-extract'
    if (Test-Path $x) { Remove-Item $x -Recurse -Force }
    Expand-Archive $InterceptionZip -DestinationPath $x
    New-Item -ItemType Directory -Path "$stage/driver" -Force | Out-Null
    Copy-Item "$x/Interception/library/x64/interception.dll" "$stage/driver/"
    Copy-Item "$x/Interception/command line installer/install-interception.exe" "$stage/driver/"
    Copy-Item "$x/Interception/licenses/non-commercial-usage/LGPL 3.0.txt" "$stage/licenses/Interception-LGPL-3.0.txt"
    Remove-Item $x -Recurse -Force
}
$translations = @(Get-ChildItem "$build/share/antimicrox/translations" -Filter '*.qm')
if ($translations.Count -eq 0) { throw 'No compiled Pad translations; build the updateqm target first' }
$translations | Copy-Item -Destination "$data/translations"
# Initial loading uses ../share; language reload on Windows uses bin/share.
$translations | Copy-Item -Destination "$bin/share/antimicrox/translations"
Copy-Item "$qt/translations/qt_*.qm" "$bin/share/qt/translations"
Copy-Item "$qt/translations/qtbase_*.qm" "$bin/share/qt/translations"
Copy-Item "$source/share/gamecontrollerdb_windows.txt" "$data/gamecontrollerdb.txt"
Copy-Item "$source/share/LICENSE_SDL_GameControllerDB" "$stage/licenses/"
Copy-Item "$source/LICENSE" "$stage/licenses/Pad-GPL-3.0.txt"
Copy-Item "$source/UPSTREAM-DEVELOPMENT.txt" "$stage/licenses/"
Copy-Item "$source/packaging/windows/Qt-LGPL-3.0.txt" "$stage/licenses/"
Copy-Item "$source/src/pad/OFL.txt" "$stage/licenses/Geist-OFL.txt"
Copy-Item "$source/src/pad/LUCIDE-LICENSE.txt" "$stage/licenses/Lucide-ISC.txt"
Copy-Item "$sdl/LICENSE.txt" "$stage/licenses/SDL2.txt"
Copy-Item "$source/packaging/windows/README-portable.txt" "$stage/README.txt"
Copy-Item "$source/packaging/windows/THIRD-PARTY.txt" "$stage/licenses/"
# Allow both Qt's deployed plugin layout and explicit relocated translation data.
@'
[Paths]
Prefix=.
Plugins=.
Translations=share/qt/translations
'@ | Set-Content "$bin/qt.conf" -Encoding utf8
@{
    product = 'Pad'; revision = $Revision; configuration = 'Release'; architecture = 'x64'
    qt = '5.15.2'; sdl = '2.32.10'; compiler = 'MSVC / Visual Studio 2022'; updates = $true; version = $(if ($Version) { $Version } else { 'development' })
} | ConvertTo-Json | Set-Content "$stage/build-info.json" -Encoding utf8
$required = @('pad.exe', 'SDL2.dll', 'Qt5Core.dll', 'Qt5Gui.dll', 'Qt5Widgets.dll', 'Qt5Network.dll', 'Qt5Concurrent.dll', 'platforms/qwindows.dll', 'vcruntime140.dll', 'msvcp140.dll', 'libssl-1_1-x64.dll', 'libcrypto-1_1-x64.dll')
foreach ($file in $required) {
    if (-not (Test-Path (Join-Path $bin $file))) { throw "Missing portable dependency: $file" }
}
$archive = Join-Path $output "$name.zip"
if (Test-Path $archive) { Remove-Item $archive -Force }
Compress-Archive -Path $stage -DestinationPath $archive -CompressionLevel Optimal
$hash = (Get-FileHash $archive -Algorithm SHA256).Hash.ToLowerInvariant()
"$hash  $name.zip" | Set-Content "$archive.sha256" -Encoding ascii
Write-Host "Portable package: $archive"
