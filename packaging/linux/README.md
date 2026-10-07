# Pad Linux package

This package targets Ubuntu 22.04 amd64 desktops with an X11 session. Other
Linux distributions and Wayland are not covered by this release gate.

## Install and run

Download the `.deb` and matching `.sha256` from the same verified build.
Run in the download folder:

```sh
sha256sum -c Pad-*-Ubuntu-22.04-amd64.deb.sha256
sudo apt install ./Pad-*-Ubuntu-22.04-amd64.deb
pad
```

Or launch Pad from the applications menu. Connect a controller, choose
Refresh controllers, open a button or axis, choose its keyboard or mouse
mapping, then save a profile. The `.amgp` profile format remains compatible
with the upstream mapper. Settings are kept in the user's configuration
folder; installing a package does not delete profiles.

To remove the app: `sudo apt remove pad`. Close Pad before upgrading.

The package does not include system Qt/SDL libraries; apt installs declared
runtime dependencies. Automatic app updates are disabled in this build.

## Reproduce the package

See `.github/workflows/pad-linux.yml` for the exact build dependencies,
Release configuration and tests. Use an Ubuntu 22.04 amd64 environment.

```sh
cmake -S . -B build -G 'Unix Makefiles' -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr -DUSE_QT6_BY_DEFAULT=OFF \
  -DCHECK_FOR_UPDATES=OFF -DCPACK_GENERATOR=DEB
cmake --build build --parallel 4
bash packaging/linux/package.sh build dist "$(git rev-parse HEAD)"
```

The output includes source revision metadata and an artifact SHA-256.
Packaging is not verification: require the matching functional, installed
input and extracted-package smoke results. Inspect the smoke screenshots.
