[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$BuildDir,
    [Parameter(Mandatory)][string]$QtDir,
    [Parameter(Mandatory)][string]$SdlDir,
    [string]$OutputDir = 'dist',
    [Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{40}$')][string]$Revision
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$source = (Resolve-Path "$PSScriptRoot/../..").Path
$build = (Resolve-Path $BuildDir).Path
$qt = (Resolve-Path $QtDir).Path
$sdl = (Resolve-Path $SdlDir).Path
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
$output = (Resolve-Path $OutputDir).Path
$name = "Pad-$($Revision.Substring(0, 8))-Windows-x64"
$stage = Join-Path $output $name
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
$bin = Join-Path $stage 'bin'
$data = Join-Path $stage 'share/antimicrox'
New-Item -ItemType Directory -Path $bin, "$data/translations", "$bin/profiles", "$bin/share/antimicrox/translations", "$bin/share/qt/translations", "$stage/licenses" -Force | Out-Null
Copy-Item "$build/bin/pad.exe" $bin
Copy-Item "$sdl/lib/x64/SDL2.dll" $bin
& "$qt/bin/windeployqt.exe" --release --no-compiler-runtime --no-opengl-sw --no-system-d3d-compiler "$bin/pad.exe"
if ($LASTEXITCODE -ne 0) { throw 'windeployqt failed' }
# App-local MSVC runtime: the recipient does not need Visual Studio or an installer.
if (-not $env:VCToolsRedistDir) { throw 'Run in an MSVC developer environment (VCToolsRedistDir missing)' }
$crt = Get-ChildItem "$env:VCToolsRedistDir/x64" -Directory -Filter 'Microsoft.VC*.CRT' | Sort-Object Name -Descending | Select-Object -First 1
if (-not $crt) { throw 'MSVC x64 CRT directory not found' }
Copy-Item "$($crt.FullName)/*.dll" $bin
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
    qt = '5.15.2'; sdl = '2.32.10'; compiler = 'MSVC / Visual Studio 2022'; updates = $false
} | ConvertTo-Json | Set-Content "$stage/build-info.json" -Encoding utf8
$required = @('pad.exe', 'SDL2.dll', 'Qt5Core.dll', 'Qt5Gui.dll', 'Qt5Widgets.dll', 'Qt5Network.dll', 'Qt5Concurrent.dll', 'platforms/qwindows.dll', 'vcruntime140.dll', 'msvcp140.dll')
foreach ($file in $required) {
    if (-not (Test-Path (Join-Path $bin $file))) { throw "Missing portable dependency: $file" }
}
$archive = Join-Path $output "$name.zip"
if (Test-Path $archive) { Remove-Item $archive -Force }
Compress-Archive -Path $stage -DestinationPath $archive -CompressionLevel Optimal
$hash = (Get-FileHash $archive -Algorithm SHA256).Hash.ToLowerInvariant()
"$hash  $name.zip" | Set-Content "$archive.sha256" -Encoding ascii
Write-Host "Portable package: $archive"
