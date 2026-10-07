#!/usr/bin/env python3
"""Reuse the normal CMake target's flags/objects, without upstream WITH_TESTS."""
import pathlib, shlex, subprocess, sys
repo = pathlib.Path(__file__).resolve().parents[2]
build = pathlib.Path(sys.argv[1]).resolve()
out = pathlib.Path(sys.argv[2]).resolve(); out.mkdir(parents=True, exist_ok=True)
flags = {}
for line in (build/'CMakeFiles/antimicrox.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        key, value = line.split(' = ', 1); flags[key] = shlex.split(value)
link = shlex.split((build/'CMakeFiles/antimicrox.dir/link.txt').read_text())
compiler = link[0]
main = 'CMakeFiles/antimicrox.dir/src/main.cpp.o'
assert main in link
subprocess.run([compiler, *flags['CXX_DEFINES'], *flags['CXX_INCLUDES'], *flags['CXX_FLAGS'], '-c',
                str(repo/'tests/pad/sensor_orientation.cpp'), '-o', str(out/'sensor.o')], cwd=build, check=True)
link[link.index(main)] = str(out/'sensor.o')
link[link.index('-o')+1] = str(out/'sensor-orientation')
subprocess.run(link, cwd=build, check=True)
