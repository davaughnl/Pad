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
                str(repo/'tests/pad/profile_roundtrip.cpp'), '-o', str(out/'profile.o')], cwd=build, check=True)
link[link.index(main)] = str(out/'profile.o')
link[link.index('-o')+1] = str(out/'profile-roundtrip')
subprocess.run(link, cwd=build, check=True)
includes = [x for x in flags['CXX_INCLUDES'] if x.startswith('-I')]
subprocess.run(['cc', '-shared', '-fPIC', *includes, *[x for x in flags['CXX_FLAGS'] if x.startswith('-I')],
                str(repo/'tests/pad/virtual_controller.c'), '-o', str(out/'controller.so'), '-ldl', *[x for x in link if pathlib.Path(x).name.startswith('libSDL2.so')],
                *[x for x in link if x.startswith('-L')]], cwd=build, check=True)
subprocess.run(['cc', *includes, str(repo/'tests/pad/xevents.c'), '-o', str(out/'xevents'), *[x for x in link if pathlib.Path(x).name.startswith('libX11.so')]], check=True)
