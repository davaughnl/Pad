# Pad

Pad maps your game controller to keyboard and mouse controls. Plug in a pad, pick a game, assign a profile, and keep Pad running in the background while you play or work.

Pad is a mapper. It reads your controller and sends keyboard and mouse input. It is not a virtual gamepad driver.

Pad runs on Windows (x64) and Linux (Ubuntu 22.04 amd64, X11). macOS is not supported yet.

## Contents

- [Get started](#get-started)
- [Features and how to use them](#features-and-how-to-use-them)
- [Profiles and compatibility](#profiles-and-compatibility)
- [Optional driver mode](#optional-driver-mode)
- [Status and limits](#status-and-limits)
- [Build from source](#build)
- [Credits](#credits)
- [License](#license)

## Get started

1. Download Pad from the [Releases page](https://github.com/davaughnl/Pad/releases). No release has been published yet, so check back there for the first one.
   - **Windows x64:** run `Pad-<version>-Windows-x64-Setup.exe`, or unzip `Pad-<version>-Windows-x64.zip` for the portable build.
   - **Ubuntu 22.04 amd64 (X11):** run `sudo apt install ./Pad-<version>-Ubuntu-22.04-amd64.deb`, then start `pad`. A `.sha256` file is published next to each download so you can verify it.
2. Plug in your controller. It appears under **Controllers** in the sidebar, with a picture of that pad.
3. On first run Pad shows a short three-step guide: plug in your pad, pick a game, assign a profile. You can skip it.
4. Map inputs on the main screen, then choose **Save** to keep the profile.

## Features and how to use them

### Controller pictures for 7 families

Pad shows a render of the pad that is actually connected. Xbox One and Series, Xbox 360, PlayStation 4, PlayStation 5, Nintendo Switch Pro, Stadia and 8BitDo each have their own. Pads Pad does not recognise show the Xbox render. When you map a button, a ring marks that control on the picture. Nothing is shown for a pad that is not plugged in.

### Mapping

Select an input in the table, then choose what it does: a key, a mouse button, mouse movement, or a combination. Use the search box above the table to find an input or an action. **Sets** (1 to 8) give you up to eight layouts per profile that you can switch between. **Quick Set** assigns inputs fast, **Names** gives your inputs your own labels, and **Controller Mapping** lets you correct how Pad reads a pad it does not know. **Reset** clears the profile.

### Profiles

Use **Save**, **Save As**, **Load** and **Remove** above the table. Recent profiles are listed in the sidebar, and a click switches to one. Existing `.amgp` profiles work as they are.

### Auto-profiles

Pad can switch profile when a game starts.

1. Open **Settings** and choose **Auto Profile**.
2. Choose **Add game**, then pick the game's program file and the profile to use. You can also pick which controller it applies to.
3. Make sure **Switch profiles automatically** is on. Adding a game turns it on for you.

If you need to match a game by window title instead, open **Match by window title** in the same dialog. When a profile switches, a short note in the top right corner of the window says which one loaded.

### Speed presets

Open **Mouse Speed Settings** for a stick or a sensor and use the **Preset** menu: Precision, Slow, Medium, Fast and Rapid, plus Desktop, Call of Duty and Valorant. The game names are starting points, not exact matches to those games. Adjust the speed values afterwards to taste.

### Gyro aim

On controllers with a motion sensor (PlayStation 4 and PlayStation 5 pads), a **Gyro aim** button appears in the bar above the mapping table. Turn it on and tilting the controller moves the mouse. Turn it off to clear it. The sliders button next to it opens the gyro's mouse settings for sensitivity and dead zone. The button does not appear for pads without a motion sensor.

### Battery indicator

When a wireless pad reports its charge, the bar above the table shows Battery full, medium, low or empty. Pads that are wired, or that do not report a charge level, show nothing.

### Test sticks

Choose **Test sticks** to open a live view of both sticks. Each stick has a plot with its position as a dot and your profile's dead zone drawn as a dashed circle. The dot brightens once it leaves the dead zone. Use it to set dead zones and to spot a drifting stick.

### First-run guide

A three-step screen on first launch walks through plugging in a pad, picking a game and assigning a profile. It shows your pad's picture once one is connected.

### Updates

Choose **Check for updates** in the sidebar. Pad checks the latest release, downloads it, verifies it and offers to install and restart. Nothing installs without your click.

## Profiles and compatibility

Pad works with controllers that SDL2 supports, including Xbox 360, Xbox One and Series, PlayStation 4 (DualShock 4), PlayStation 5 (DualSense), Nintendo Switch Pro, 8BitDo, Logitech and Stadia pads. Unusual devices such as wheels and flight sticks can be mapped by hand in **Controller Mapping**.

Existing `.amgp` profiles work as they are. Pad keeps its own settings file and, on first run, copies compatible settings from earlier versions without changing the originals.

## Optional driver mode

Driver mode is experimental and off by default. It uses the Interception keyboard and mouse driver so input reaches apps that ignore normal input. It affects Windows as a whole, needs administrator approval and a restart, and is never installed unless you choose it. If a keyboard or mouse stops working after installing it, follow the recovery steps in the `driver` folder of your install (`RECOVERY.md`) and read them before you install the driver.

## Status and limits

Pad's builds are tested on Windows x64 and on Ubuntu 22.04 amd64 with X11. Automated tests cover the mapping engine and the screens. Testing with physical controllers, including gyro, battery readings and the stick tester, is still limited, so report anything that misbehaves. Windows installers are not code-signed yet, so Windows may show a warning. macOS is not supported yet. Input injection depends on OS permissions, elevated apps, anti-cheat software and per-app input rules, so Pad cannot promise to work in every game.

## Build

See [BUILDING.md](BUILDING.md) for Qt, SDL2 and compiler dependencies.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/bin/pad
```

## Credits

Pad began as a fork of [AntiMicroX](https://github.com/AntiMicroX/antimicrox), a GPL-licensed gamepad mapper by the AntiMicroX developers and contributors, itself a continuation of Antimicro. Pad's interface, features and packaging have since been redesigned and extended. Pad uses Qt and SDL2, and bundles Geist fonts and Lucide icons. Required copyright and license notices are kept in the source and package legal files.

## License

Pad is licensed under GPL-3.0-or-later, inherited from AntiMicroX. Required copyright and license notices are preserved in the source and package legal files. See the [license](LICENSE) and the legal notices included with the source and packages.
