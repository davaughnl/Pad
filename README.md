# Pad

Map your controller to keyboard and mouse controls. Keep Pad running while you use a game or desktop app, including in the background.

Map buttons, sticks and sensors to keyboard/mouse output, with saved profiles, macros and per-application profiles. Pad is not a virtual gamepad driver.

## Status

Development builds are gated on Windows x64 and Ubuntu 22.04 amd64/X11, including native editor/model tests, package smoke tests and Linux mapped-output tests. Physical controller coverage, Windows mapped-output delivery and Wayland/uinput remain unverified. macOS is deferred. No signed installers are available yet.

Input injection is subject to OS permissions, elevated applications, anti-cheat and application input restrictions. Pad cannot promise compatibility with every game.

## Build

See [BUILDING.md](BUILDING.md) for Qt, SDL2 and compiler dependencies.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/bin/pad
```

## Compatibility

Existing `.amgp` profiles are supported. Pad uses its own settings file and copies compatible settings on first run without changing the original.

## License

GPL-3.0-or-later. Required copyright and license notices are preserved in the source and package legal files. See the [license](LICENSE) and the legal notices included with the source and packages.
