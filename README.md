# Pad

Map your controller to keyboard and mouse controls. Keep Pad running while you use a game or desktop app, including in the background.

Pad is a redesigned fork of [AntiMicroX](https://github.com/AntiMicroX/antimicrox). The controller mapping engine, profiles, macros and per-application profiles come from that project. This is controller input to keyboard/mouse output, not a virtual gamepad driver.

## Status

In development. Linux builds are checked locally. Windows is an upstream-supported platform; Pad-specific Windows validation is pending. macOS is deferred until the Linux/Windows version is finished. No signed installers are available yet.

Input injection is subject to OS permissions, elevated applications, anti-cheat and application input restrictions. Pad cannot promise compatibility with every game.

## Build

See [BUILDING.md](BUILDING.md) for Qt, SDL2 and compiler dependencies.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/bin/pad
```

## Compatibility

Existing AntiMicroX `.amgp` profiles are supported. Settings and translation resource paths remain compatible with upstream for this first milestone.

## License and credits

GPL-3.0-or-later. Original copyright notices, contributor credits, licenses and the upstream change history are retained. See [LICENSE](LICENSE) and [README.upstream.md](README.upstream.md).
