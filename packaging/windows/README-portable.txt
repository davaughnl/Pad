Pad for Windows x64

Extract the entire zip to a writable folder. Open bin\pad.exe. Do not run
Pad from inside the zip or move pad.exe away from its DLLs and share folder.
Qt5, SDL2, and the MSVC runtime are included. No Qt or Visual Studio install
is needed. This package is unsigned, so Windows may show a reputation warning.
Only use builds from https://github.com/davaughnl/Pad and verify the SHA256.

Profiles and settings are portable. Keep the bin folder writable; start in
that folder when using command-line profile paths. Updates are disabled in
this development package. Back up your profiles before replacing a build.

CI checks launch and runtime dependencies, not physical gamepad behavior.
Test a real controller's buttons/sticks and keyboard/mouse output on your
Windows PC before depending on a mapping. Pad cannot bypass Windows' input
restrictions for elevated applications unless run at matching privilege.

Source, changes, and license: https://github.com/davaughnl/Pad
The exact source revision is recorded in build-info.json.
Third-party notices are in licenses\.
