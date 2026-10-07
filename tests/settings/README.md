# Pad settings migration tests

These Qt5 Core tests build independently of the legacy GUI test suite:

```sh
cmake -S tests/settings -B build-settings
cmake --build build-settings
ctest --test-dir build-settings --output-on-failure
```

They check first-run copying, existing Pad settings, a missing upstream file,
source bytes/timestamps/permissions and adjacent profiles staying unchanged,
source priority and legacy fallback, copy failure, QSettings compatibility, and
actual source/destination paths in both dialog messages. All fixtures live in a
QTemporaryDir; the Linux path test sets XDG_CONFIG_HOME to that directory.

Linux stores Pad settings at $XDG_CONFIG_HOME/pad/pad_settings.ini, or
~/.config/pad/pad_settings.ini when XDG_CONFIG_HOME is unset. Non-portable
Windows builds use Qt's user-scope INI directory (normally %APPDATA%/Pad) and
pad_settings.ini. Explicit portable Windows builds keep pad_settings.ini next
to the executable.

Import order is current upstream settings, then older compatible settings.
An existing destination, even an empty file or a symlink, always prevents an
import. The copy operation never overwrites or changes the source. No profile
files are converted or moved. Import failure leaves the source intact and
shows the real source and destination file paths for manual copying.
