# Pad deterministic functional tests (Linux, Qt5, SDL >= 2.0.14, XTest)

These tests use the real application mapping pipeline and XTest output, not
source greps. They require no controller, root, or writable `/dev/uinput`.
They are fixture evidence, not physical controller compatibility evidence.

Build the ordinary application with Qt5, X11 and XTest enabled, using the
Unix Makefiles CMake generator. Leave upstream `WITH_TESTS=OFF`.
Then, from the repository root:

```sh
python3 tests/pad/build_harness.py build build/pad-tests
xvfb-run -a -s '-screen 0 1024x768x24' sh -c '
  build/pad-tests/profile-roundtrip tests/pad/mapping.amgp &&
  python3 tests/pad/input_regression.py build/bin/pad build/pad-tests
'
```

If your executable is named `antimicrox`, use that path instead. Local
extracted dependency prefixes need their ordinary `LD_LIBRARY_PATH` and Qt
platform plugin environment settings. The harness reuses the configured
application's object files and flags, replacing only its `main.cpp` object.
No product source or upstream test CMake files are changed.

## Assertions

- SDL virtual button press/release -> X11 `a` key press/release.
- SDL virtual button -> X11 left mouse press/release.
- Axis inside dead zone produces no event; negative/positive axis produces
  `b`/`c`, centering releases the mapped key.
- Mid-run detach releases a held mapped key; reattach plus explicit profile
  reload resumes mapped button/axis output; another detach releases a held
  mouse button. Pad must stay alive throughout.
- Native XMLConfigReader/Writer read/save/read retains the profile name,
  set name, keyboard and mouse slots, axis assignments and dead zone.
- Save/load/save XML remains byte-stable; loading clears stale mappings.

The controller preload receives acknowledged commands over a private
Unix datagram socket and applies them on SDL's polling thread. An independent
X11 window observes delivered events. No mocked XTest calls are accepted.
Each run isolates app configuration and cleans up its own child processes.

A third optional argument to `input_regression.py` selects a different profile
fixture. For a negative control, change the first slot's `0x41` to `0x44` in
a copy and pass that copy: the first event must fail (`key 100` vs `key 97`).
The fixture uses Qt key aliases (uppercase `0x41`/`0x42`/`0x43`), which map
to unshifted X11 `a`/`b`/`c` in the test display.

Validated on main commit `3ffa7137235f8acc1cd02964c2ea2e22ba37c779`,
Qt 5.15.3, SDL 2.0.20, Xvfb/XTest: three repeated full suite runs passed.
The wrong-key negative control failed at the first key assertion as expected.
Physical hardware, Wayland and uinput are outside this suite's scope.
