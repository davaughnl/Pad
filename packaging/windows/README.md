# Windows portable build

`Pad Build` targets `main` and pull requests into `main`. It does not change the
upstream workflows that still target `master`. Its Windows job uses Qt 5.15.2,
SDL 2.32.10 (SHA256-checked), VS2022 MSVC x64, Ninja, and a Release build.

Run the scripts from a VS2022 x64 developer PowerShell with Qt5 installed:

1. `./packaging/windows/get-sdl2.ps1 -Destination C:/deps/sdl2`
2. Configure as in `.github/workflows/pad-build.yml` with your Qt and SDL paths.
3. `cmake --build build --target antimicrox updateqm --parallel 4`
4. `./packaging/windows/package.ps1 -BuildDir build -QtDir C:/Qt/5.15.2/msvc2019_64 -SdlDir C:/deps/sdl2 -Revision (git rev-parse HEAD)`
5. `./packaging/windows/smoke-test.ps1 -Archive (Get-ChildItem dist/*.zip | Select-Object -First 1).FullName`

The `antimicrox` CMake target produces `pad.exe`. Packaging deliberately stages
files instead of calling `cmake --install` / CPack because inherited Windows
install rules require MinGW DLLs even for MSVC. A single-config Ninja build also
avoids the inherited rules' `bin` vs `bin/Release` mismatch. No root CMake changes
are needed for this lane.

The zip contains app-local MSVC DLLs, Qt DLLs and plugins, SDL2, translations,
controller DB, notices, and build revision. The workflow uploads the zip and
SHA256 only after testing the extracted archive with a PATH that excludes build
dependencies. Smoke evidence includes stdout/stderr, result.txt, and runner
pixels. A human must review those pixels; CI does not verify visual quality.

No signing, public release creation, macOS build, or update checks are included.
Artifacts expire after 30 days; a successful workflow is a development package,
not a permanent public release. Hardware controller input, mapping output,
elevated applications, and SmartScreen behavior need manual Windows testing.
