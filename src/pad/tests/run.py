#!/usr/bin/env python3
"""Compile the presentation fixture from the real application's objects.
Usage: python3 src/pad/tests/run.py build
Requires the app's Qt5/SDL/X11 environment and xvfb-run.
"""
import pathlib
import shlex
import subprocess
import sys
import tempfile

build = pathlib.Path(sys.argv[1]).resolve()
source = pathlib.Path(__file__).with_name('presentation.cpp').resolve()
flags = {}
for line in (build / 'CMakeFiles/antimicrox.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        key, value = line.split(' = ', 1)
        flags[key] = shlex.split(value)
link = shlex.split((build / 'CMakeFiles/antimicrox.dir/link.txt').read_text())
with tempfile.TemporaryDirectory(prefix='pad-presentation-') as folder:
    obj = str(pathlib.Path(folder) / 'presentation.o')
    executable = str(pathlib.Path(folder) / 'presentation')
    subprocess.run([link[0], *flags['CXX_DEFINES'], *flags['CXX_INCLUDES'],
                    *flags['CXX_FLAGS'], '-c', str(source), '-o', obj], cwd=build, check=True)
    link[link.index('CMakeFiles/antimicrox.dir/src/main.cpp.o')] = obj
    link[link.index('-o') + 1] = executable
    subprocess.run(link, cwd=build, check=True)
    subprocess.run(['xvfb-run', '-a', executable], check=True)
