#!/usr/bin/env python3
"""Link tests/pad/update_tests.cpp against the normal Pad objects (no upstream WITH_TESTS)."""
import pathlib, shlex, subprocess, sys
repo = pathlib.Path(__file__).resolve().parents[2]
build = pathlib.Path(sys.argv[1]).resolve()
out = build / 'pad-tests'; out.mkdir(exist_ok=True)
flags = {}
for line in (build/'CMakeFiles/antimicrox.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        key, value = line.split(' = ', 1); flags[key] = shlex.split(value)
link = shlex.split((build/'CMakeFiles/antimicrox.dir/link.txt').read_text())
main = 'CMakeFiles/antimicrox.dir/src/main.cpp.o'
assert main in link
subprocess.run([link[0], *flags['CXX_DEFINES'], *flags['CXX_INCLUDES'], '-I' + str(repo/'src'), *flags['CXX_FLAGS'], '-c',
                str(repo/'tests/pad/update_tests.cpp'), '-o', str(out/'update_tests.o')], cwd=build, check=True)
link[link.index(main)] = str(out/'update_tests.o')
link[link.index('-o')+1] = str(out/'update-tests')
subprocess.run(link, cwd=build, check=True)
print(out/'update-tests')
