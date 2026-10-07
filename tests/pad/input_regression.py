#!/usr/bin/env python3
"""Black-box real Pad + real SDL virtual-device + real XTest receiver."""
import os, pathlib, select, socket, subprocess, sys, tempfile, time
repo = pathlib.Path(__file__).resolve().parents[2]
binary = pathlib.Path(sys.argv[1]).resolve()
harness = pathlib.Path(sys.argv[2]).resolve()
fixture = pathlib.Path(sys.argv[3]).resolve() if len(sys.argv)>3 else repo/'tests/pad/mapping.amgp'
with tempfile.TemporaryDirectory(prefix='pad-input-') as tmp:
    tmp = pathlib.Path(tmp)
    env = dict(os.environ, XDG_CONFIG_HOME=str(tmp/'config'), XDG_DATA_HOME=str(tmp/'data'),
               PAD_TEST_SOCKET=str(tmp/'sdl.sock'), SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS='1')
    log = (tmp/'pad.log').open('w+')
    pad = subprocess.Popen([str(binary), '--hidden', '--no-tray', '--eventgen', 'xtest',
                            '--profile', str(fixture), '--log-level', 'debug'],
                           env=dict(env, LD_PRELOAD=str(harness/'controller.so')), stdout=log, stderr=log)
    receiver = None
    client = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
    client.bind(str(tmp/'client.sock')); client.settimeout(3)
    def command(text):
        client.sendto(text.encode(), env['PAD_TEST_SOCKET'])
        assert client.recv(64) == b'0', 'virtual SDL command failed: '+text
    pending = bytearray()
    def events(duration=0.15):
        end = time.monotonic()+duration; result=[]
        while time.monotonic()<end:
            ready, _, _ = select.select([receiver.stdout],[],[],max(0,end-time.monotonic()))
            if ready:
                chunk = os.read(receiver.stdout.fileno(),4096)
                assert chunk, 'X11 receiver exited'
                pending.extend(chunk)
                while b'\n' in pending:
                    line, _, rest = pending.partition(b'\n'); pending[:] = rest
                    result.append(line.decode().strip())
        return result
    def expect(text, wanted):
        command(text); got=events(0.30)
        assert got==wanted, f'{text}: expected {wanted}, got {got}'
        assert pad.poll() is None, 'Pad exited unexpectedly'
    try:
        end=time.monotonic()+10
        while not pathlib.Path(env['PAD_TEST_SOCKET']).exists():
            assert pad.poll() is None, 'Pad exited during startup'
            assert time.monotonic()<end, 'virtual controller socket never created'
            time.sleep(.05)
        command('ping')
        # A receiver created after Pad startup keeps focus away from the hidden Qt window.
        time.sleep(.7)
        receiver=subprocess.Popen([str(harness/'xevents')],env=env,stdout=subprocess.PIPE)
        assert select.select([receiver.stdout],[],[],3)[0], 'X11 receiver startup timeout'
        assert receiver.stdout.readline().strip()==b'ready', 'X11 receiver failed'
        assert events()==[], 'unexpected startup input'
        expect('button 0 1',['key 97 1']); expect('button 0 0',['key 97 0'])
        expect('button 1 1',['mouse 1 1']); expect('button 1 0',['mouse 1 0'])
        print('PASS button -> XTest key and mouse press/release')
        expect('axis 0 1000',[])
        expect('axis 0 -20000',['key 98 1']); expect('axis 0 0',['key 98 0'])
        expect('axis 0 20000',['key 99 1']); expect('axis 0 0',['key 99 0'])
        print('PASS axis dead zone, negative/positive key mapping, center release')
        # Disconnect while a mapped button is held must release its output.
        expect('button 0 1',['key 97 1']); expect('detach',['key 97 0'])
        expect('attach',[])
        # Reload via the application's existing single-instance command route.
        reload=subprocess.run([str(binary),'--profile',str(fixture)],
                              env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=5)
        assert reload.returncode==0, 'profile reload after reconnect failed: '+reload.stdout
        time.sleep(.4)
        expect('button 0 1',['key 97 1']); expect('button 0 0',['key 97 0'])
        expect('axis 0 -20000',['key 98 1']); expect('axis 0 0',['key 98 0'])
        expect('button 1 1',['mouse 1 1']); expect('detach',['mouse 1 0'])
        print('PASS disconnect releases held key/mouse, reconnect, profile reload, input resumes')
    except BaseException:
        log.flush(); log.seek(0); sys.stderr.write(log.read()[-12000:]); raise
    finally:
        client.close()
        if receiver: receiver.terminate(); receiver.wait(timeout=5)
        pad.terminate()
        try: pad.wait(timeout=5)
        except subprocess.TimeoutExpired: pad.kill(); pad.wait()
        log.close()
