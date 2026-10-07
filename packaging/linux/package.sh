#!/usr/bin/env bash
set -euo pipefail
build=$(realpath "${1:?build directory}")
output=$(realpath -m "${2:?output directory}")
revision=${3:?full source revision}
[[ $revision =~ ^[0-9a-f]{40}$ ]] || { echo 'Invalid source revision' >&2; exit 1; }
mkdir -p "$output"
# Configure CPack with /usr install paths and DEB generator before invoking this script.
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cpack --config "$build/CPackConfig.cmake" -G DEB -D "CPACK_DEBIAN_PACKAGE_VERSION=0~git.${revision:0:8}" -B "$work"
packages=("$work"/*.deb)
[[ ${#packages[@]} == 1 && -f ${packages[0]} ]] || { echo 'Expected one deb' >&2; exit 1; }
name="Pad-${revision:0:8}-Ubuntu-22.04-amd64.deb"
mv "${packages[0]}" "$output/$name"
(cd "$output" && sha256sum "$name" > "$name.sha256")
python3 - "$output" "$revision" "$name" <<'PY'
import json,pathlib,sys
output,revision,name=sys.argv[1:]
pathlib.Path(output,'build-info.json').write_text(json.dumps(dict(product='Pad',revision=revision,artifact=name,architecture='amd64',platform='Ubuntu 22.04',qt='5.15',configuration='Release',updates=False),indent=2)+'\n')
PY
dpkg-deb --info "$output/$name"
echo "Package: $output/$name"
