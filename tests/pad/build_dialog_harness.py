#!/usr/bin/env python3
"""Link native Qt control tests with the ordinary application object files."""
import pathlib,shlex,subprocess,sys
repo=pathlib.Path(__file__).resolve().parents[2]
build=pathlib.Path(sys.argv[1]).resolve()
out=build/'pad-tests'; out.mkdir(exist_ok=True)
flags={}
for line in (build/'CMakeFiles/antimicrox.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        k,v=line.split(' = ',1); flags[k]=shlex.split(v)
link=shlex.split((build/'CMakeFiles/antimicrox.dir/link.txt').read_text())
qtcore=next(pathlib.Path(x) for x in link if pathlib.Path(x).name.startswith('libQt5Core.so'))
qtinclude=next(pathlib.Path(flags['CXX_INCLUDES'][n+1]).parent for n,x in enumerate(flags['CXX_INCLUDES']) if x=='-isystem' and flags['CXX_INCLUDES'][n+1].endswith('/QtCore'))
subprocess.run([link[0],*flags['CXX_DEFINES'],*flags['CXX_INCLUDES'],*flags['CXX_FLAGS'],
               '-DQT_TESTLIB_LIB','-isystem',str(qtinclude/'QtTest'),'-c',str(repo/'tests/pad/dialog_controls.cpp'),'-o',str(out/'dialogs.o')],cwd=build,check=True)
link[link.index('CMakeFiles/antimicrox.dir/src/main.cpp.o')]=str(out/'dialogs.o')
link[link.index('-o')+1]=str(out/'dialog-controls')
link.append(str(qtcore.parent/'libQt5Test.so'))
subprocess.run(link,cwd=build,check=True)
