# Synthetic sensor, calibration, slot and menu coverage

Baseline: c3c28941f9309412b7281270f7bfc760e8aa36f1.
Build/run uses the existing build_dialog_harness.py and run_dialog_tests.py.
The expanded suite has 33 independent test groups. Model objects run on the
input worker QThread; profile and settings writes stay in temporary directories.

New assertions:

- Accelerometer and gyro edit: zone spin/slider, diagonal range, delay, name;
  every available explicit preset's actual assignments, None clears slots;
  mouse-settings launch/close reenables launcher; all sensor-direction buttons
  receive X/Y and wheel speeds.
- Sensor default-preset regression: a fresh unmapped sensor must display None.
- Gyro and accelerometer calibration: Start, Continue, feed real moved signals
  from the worker model until sampling completes, verify uncalibrated until
  Save, Save applies sampled coefficients across all eight sets, calibrated
  data appears in a written profile XML, Save clears dirty state.
- Stick calibration: timed rate samples, repeated near-center oscillation,
  repeated positive/negative extrema through both phases; Save enabled only
  after completion; Save applies finite offsets and valid gains across all
  sets; written profile contains calibration. No estimator/private-state bypass.
- Advanced slots: Cycle, Delay, Hold, Pause, PressTime, Release, Distance,
  MouseSpeedMod, SetChange insert actual model slots; seconds/tenths/hundredths
  convert to milliseconds; editing an existing delay updates its code. Existing
  join/split regression still checks key ordering. Text, Execute with args and
  LoadProfile slots retain contents; profile XML retains loadprofile mode.
  Script fixture is never executed and target profile slot is never activated.
- Set menus: every Set1-8 action changes active set, Copy No leaves state alone,
  Copy Yes copies mappings, Settings opens Set Names, names button toggles back.
- Main menu actions: Key Checker, Settings and Calibration launch their actual
  dialog classes and close through their controls.
- Invalid accelerometer vector regression: constructor/update show Unavailable;
  zero raw coordinates and magnitude remain visible; math returns NaN as an
  explicit invalid sentinel; zero/non-finite model input stays SENSOR_CENTERED.

Results on baseline: 31/33 pass. The two failing product regressions are
sensor-default-preset and sensor-zero-vector. The latter encodes the agreed
invalidity/Unavailable contract; baseline exposes nan in the UI. No product
fixes are included. Initial fixture mistakes (preset ordering, selected blank
slot, too-fast gain waveform) were corrected before classifying product bugs.

Limits: synthetic signals are not real controller/sensor accuracy evidence.
Stick waveform completion depends on elapsed sampling cadence; its case has a
45-second timeout, other groups 15 seconds. No full event-output assertion for
sensor direction mapping or advanced sequences; tests assert resulting model,
settings and XML state. No Windows, Wayland, uinput, haptics, browser-link menus,
tray Quit/update routes, every contextual menu, all mixed-slot combinations,
all timing edges, or real script execution. This is not exhaustive certification
that every control everywhere works, and makes no visual/layout quality claim.

Additional reopen assertions: after applying each available preset for each
sensor type, reopen the real dialog and assert selected preset data matches.
Worker-thread snapshots compare each direction's ordered slot mode, code,
alias, text and extra-data before/after reopen. Constructor must not mutate
mappings. This absorbs the UI lane's behavioral assertions without its
pixel-review fixture or relying on summary labels alone.
