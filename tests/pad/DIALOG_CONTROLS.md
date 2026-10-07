# Native dialog/control regressions

Build normal Pad (Qt5, Unix Makefiles, X11/XTest, WITH_TESTS=OFF), then:

```sh
python3 tests/pad/build_dialog_harness.py build
xvfb-run -a python3 tests/pad/run_dialog_tests.py build/pad-tests/dialog-controls
```

The harness reuses the application's compiled objects, replaces main.cpp, and
links Qt5Test. It uses real dialog classes, named controls, actual click/value
signals, and asserts model/settings/file outcomes. Models live on a worker
QThread like production; helpers using BlockingQueuedConnection therefore
run on the right thread. Temporary profiles/settings isolate destructive
operations. Each group runs in a separate process with a 15-second timeout.
A single group can be run as `dialog-controls stick-assignment` under Xvfb.

## Coverage

- Button edit: toggle on/off, turbo on/off, button/action names, keypad on/off.
- Virtual keyboard/mouse: A assignment, None clear, left mouse assignment,
  None clear.
- Advanced button: toggle, turbo, interval, cycle reset/interval, clear,
  add/delete; join/split preserves key order.
- Axis: dead-zone spin/slider both ways, max zone, preset assign/None clear.
- Dpad: preset assign/None clear, delay, mode.
- Stick: dead zone, diagonal, max/modifier zone, inversion, preset/None.
- Mouse button/axis/dpad/stick: X/Y speed, spring/cursor mode, spring dimensions,
  horizontal/vertical wheel speeds, asserting every affected model button.
- Profile properties: name and keypress duration; set names Save/Cancel.
- Settings: Save/Cancel, recent count and keypad persistence, mapping table
  add/remove, key-repeat dependent controls enabled/disabled.
- Calibration: available stick, unsampled Save disabled, Start enters sampling,
  Cancel closes. Full measured calibration and Save are NOT covered.
- About: tab switching, nonempty info, Close.
- Auto-profile and default profile: Save/Cancel, class/title/partial values,
  invalid default-profile path rejected, valid path saved.
- Throttle confirmation: No doesn't emit change, Yes emits exactly once.
- Key display: synthetic native key release shows expected Qt/native values.
- Advanced stick assignment: disable, reenable, choose Axis1/Axis2; all eight
  sets acquire the right stick axes. This failed before 1ec7f192's fix.
- Quick Set: controller button selection assigns key, closes and restores
  normal mapped output handling.
- Controller mapping: raw button binds A, generated mapping contains a:b0,
  dead-zone selection changes model, Save persists, Discard removes.
- Real profile operations: Save As creates/selects file with mapping; Save
  updates mapping; Save As Cancel creates nothing; Remove removes the recent
  entry without deleting its file; Load restores the saved mapping.

## Results and limits

Validated on main `1ec7f192` (Qt5 5.15.3, SDL2 2.0.20, Xvfb/XTest): 22/22
functional groups passed in two complete repeated runs.
The Stick 1 regression failed repeatedly on b2ea1b9c and passed after the
build lead's one-line fix, now merged as 1ec7f192. No product edits are in
this test patch.

This is NOT exhaustive every-control-everywhere certification. Uncovered:
sensor edit/mouse dialogs, window capture dialogs, full calibration samples
and Save, all advanced slot types/timing/mixing combinations, all settings,
all presets/modes, every menu/button route, hardware haptics, real controllers,
Windows, Wayland, uinput. No visual/layout quality claim is made here.
