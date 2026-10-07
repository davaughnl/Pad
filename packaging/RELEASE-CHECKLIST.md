# Pad release checklist

Scope: controller-to-keyboard/mouse mapper for Windows x64 and Ubuntu 22.04
amd64 on X11. macOS is deferred. A green build is a release candidate, not
physical-device certification.

## Candidate gate

- [ ] Record the full source revision. Require Windows and Linux jobs for that
  exact revision. Do not carry green results forward across code changes.
- [ ] Verify distribution SHA-256 against the matching artifact manifest.
- [ ] Inspect the actual Windows editor and extracted-package screenshots,
  including accelerometer/gyro, main window and Settings.
- [ ] Inspect Linux extracted-package main/Settings and sensor screenshots.
- [ ] Verify legal notices, upstream attribution, Geist and Lucide licenses.
- [ ] Keep a matching source archive available with the binaries (GPL).
- [ ] Decide release name/version and publish final downloads. Build artifacts
  expire; a temporary CI artifact is not a permanent release page.

## Automated evidence

Linux gate covers 34 real Qt dialog/model groups, including fresh sensor
storage patterns, saved-preset reopen selection and mapping preservation,
calibration, settings, profile operations, slot controls and menu routes.
It also covers invalid orientation math and safe preview rendering, 10,000
unchanged valid-vector comparisons, and invalid-sample direction/shock
history guards.

Linux black-box tests deliver real XTest keyboard/mouse press and release
from an SDL virtual controller. Axis dead zone/centering, held-output release
on disconnect, reconnect/profile reload and stable native XML roundtrips
are checked. Keyboard viewport tests scroll every visible key fully into view
at 800x520 with/without keypad, check hit targets and click the rightmost key
to verify its assigned alias. These run in Linux and native Windows fixtures.
The Linux package gate repeats input tests against the installed
binary and checks extracted-package startup/Settings and dependencies.

Windows gate builds Qt5/MSVC Release, checks profile association behavior,
runs eight native editor/model cases (including both sensor dialogs), stages
runtime DLLs, and smoke-tests the extracted portable zip without development
dependencies in PATH. Native screenshots still need human pixel inspection.
These model tests are not a Windows SendInput delivery test.

Both gates check Pad branding/resources. The UI is visually reviewed, but
neither visual review nor these tests certify every possible control path.

## Explicitly unverified

- Physical controller models, sensors, drift and real calibration accuracy.
- Windows SendInput key/mouse delivery into another process or game, elevated
  targets and anti-cheat compatibility.
- Wayland, uinput device permissions, haptics and force feedback.
- Every mixed-slot/timing/script route and all contextual/tray actions.
- Other Linux distributions/architectures and macOS.

Before calling this a broadly hardware-tested stable release, run a physical
controller matrix and Windows mapped-output check. A release candidate can
ship with these limits stated plainly; it must not imply they passed.

## Install/run handoff

Windows: extract the entire verified zip, keep its folders together, and run
`bin/pad.exe`. No separate Qt/SDL installation is required. Close Pad before
replacing an existing extracted copy. See the portable README in the zip.

Ubuntu 22.04 amd64: download the matching `.deb` and `.sha256`, check the
hash, then `sudo apt install ./Pad-*-Ubuntu-22.04-amd64.deb`. Launch `pad`
or the Pad applications entry. Use an X11 desktop session for the verified
mapping path. See `packaging/linux/README.md` for details.

Connect a controller, choose Refresh controllers, assign keyboard/mouse
controls and save a profile. Automatic app updating is disabled in these
packages. Upgrade from verified downloads; retain a backup of profiles.
