[CmdletBinding()]
param([Parameter(Mandatory)][string]$Destination)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$version = '2.32.10'
$uri = "https://github.com/libsdl-org/SDL/releases/download/release-$version/SDL2-devel-$version-VC.zip"
$expected = 'af347939395a58b365846aaea27391e69f9ec9d4dd650d6ac40802159b418a6e'
$archive = Join-Path $env:TEMP "SDL2-devel-$version-VC.zip"
Invoke-WebRequest -Uri $uri -OutFile $archive
if ((Get-FileHash $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expected) {
    throw 'SDL2 archive SHA256 mismatch'
}
$unpack = "$Destination-unpack"
if (Test-Path $Destination) { throw "Destination already exists: $Destination" }
if (Test-Path $unpack) { Remove-Item $unpack -Recurse -Force }
Expand-Archive $archive -DestinationPath $unpack
Move-Item (Join-Path $unpack "SDL2-$version") $Destination
# The project's FindSDL2 expects include/SDL2/SDL.h, unlike the VC archive.
New-Item -ItemType Directory -Path "$Destination/include/SDL2" -Force | Out-Null
Copy-Item "$Destination/include/*.h" "$Destination/include/SDL2/"
Remove-Item $unpack -Recurse -Force
Write-Host "SDL2 $version verified and prepared at $Destination"
