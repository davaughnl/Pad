#!/usr/bin/env bash
set -euo pipefail
build=$(realpath "${1:?build directory}")
output=$(realpath -m "${2:?output directory}")
revision=${3:?full source revision}
version=${4:-}
[[ $revision =~ ^[0-9a-f]{40}$ ]] || { echo 'Invalid source revision' >&2; exit 1; }
if [[ -n $version ]]; then
    [[ $version =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo 'Invalid release version' >&2; exit 1; }
    grep -Fxq "PAD_VERSION:STRING=$version" "$build/CMakeCache.txt" || { echo 'Release version differs from compiled app' >&2; exit 1; }
    package_version=$version
    name="Pad-$version-Ubuntu-22.04-amd64.deb"
else
    package_version="0~git.${revision:0:8}"
    name="Pad-${revision:0:8}-Ubuntu-22.04-amd64.deb"
    version=$(sed -n 's/^PAD_VERSION:STRING=//p' "$build/CMakeCache.txt")
fi
mkdir -p "$output"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cpack --config "$build/CPackConfig.cmake" -G DEB -D "CPACK_DEBIAN_PACKAGE_VERSION=$package_version" -B "$work"
packages=("$work"/*.deb)
[[ ${#packages[@]} == 1 && -f ${packages[0]} ]] || { echo 'Expected one deb' >&2; exit 1; }
[[ $(dpkg-deb -f "${packages[0]}" Version) == "$package_version" ]] || { echo 'Wrong package version' >&2; exit 1; }
[[ $(dpkg-deb -f "${packages[0]}" Architecture) == amd64 ]] || { echo 'Wrong package architecture' >&2; exit 1; }
mv "${packages[0]}" "$output/$name"
(cd "$output" && sha256sum "$name" > "$name.sha256")
python3 - "$output" "$revision" "$name" "$version" <<'PY'
import json,pathlib,sys
output,revision,name,version=sys.argv[1:]
pathlib.Path(output,'build-info.json').write_text(json.dumps(dict(product='Pad',revision=revision,artifact=name,version=version or 'development',architecture='amd64',platform='Ubuntu 22.04',qt='5.15',configuration='Release',updates=False),indent=2)+'\n')
PY
dpkg-deb --info "$output/$name"
echo "Package: $output/$name"
