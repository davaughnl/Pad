#!/usr/bin/env python3
"""Smoke the extracted /usr/bin/pad and capture real pixels on an isolated X display."""
import os,pathlib,subprocess,sys,tempfile,time
archive=pathlib.Path(sys.argv[1]).resolve(); output=pathlib.Path(sys.argv[2]).resolve();output.mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory(prefix='pad-deb-smoke-') as tmp:
    root=pathlib.Path(tmp);subprocess.run(['dpkg-deb','--extract',str(archive),str(root)],check=True)
    binary=root/'usr/bin/pad';assert binary.exists(),'No /usr/bin/pad'
    for item in ['usr/share/applications/io.github.davaughnl.Pad.desktop','usr/share/licenses/Pad/LICENSE','usr/share/licenses/Pad/UPSTREAM-DEVELOPMENT.txt','usr/share/antimicrox/gamecontrollerdb.txt']:
        assert (root/item).exists(), 'Missing '+item
    deps=subprocess.run(['ldd',str(binary)],capture_output=True,text=True,check=True);assert 'not found' not in deps.stdout,deps.stdout
    (output/'dependencies.txt').write_text(deps.stdout)
    env=dict(os.environ,XDG_CONFIG_HOME=str(root/'config'),XDG_DATA_HOME=str(root/'data'),QT_QPA_PLATFORM='xcb')
    version=subprocess.run([str(binary),'--version'],env=env,capture_output=True,text=True,timeout=10,check=True)
    assert 'Pad Development build' in version.stdout and '3.6.1' not in version.stdout,version.stdout
    (output/'version.txt').write_text(version.stdout)
    with (output/'startup.log').open('w') as log:
        app=subprocess.Popen([str(binary),'--show','--no-tray','--eventgen','xtest'],env=env,stdout=log,stderr=log)
        try:
            def window(pattern):
                end=time.monotonic()+10
                while time.monotonic()<end:
                    assert app.poll() is None,'Pad exited before UI appeared'
                    result=subprocess.run(['xdotool','search','--onlyvisible','--name',pattern],capture_output=True,text=True)
                    if result.returncode==0:return result.stdout.splitlines()[-1]
                    time.sleep(.1)
                raise AssertionError('Window not found: '+pattern)
            main=window('^Pad');subprocess.run(['xdotool','windowsize',main,'1024','740'],check=True);time.sleep(.5)
            subprocess.run(['import','-window',main,str(output/'main.png')],check=True)
            subprocess.run(['xdotool','windowfocus',main,'key','ctrl+s'],check=True)
            settings=window('^Settings$');time.sleep(.5)
            subprocess.run(['import','-window',settings,str(output/'settings.png')],check=True)
            subprocess.run(['xdotool','windowfocus',settings,'key','Escape'],check=True)
            time.sleep(.2);assert app.poll() is None,'Pad exited after closing settings'
        finally:
            app.terminate()
            try:app.wait(timeout=5)
            except subprocess.TimeoutExpired:app.kill();app.wait()
    (output/'result.txt').write_text('PASS extracted deb dependency resolution, version, startup, Settings open/close. Screenshots require pixel inspection.\n')
