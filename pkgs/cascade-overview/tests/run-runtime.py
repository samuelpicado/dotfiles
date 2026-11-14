#!/usr/bin/env python3
"""Run the real compositor tests in a disposable session; never use the user's socket."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import signal
import subprocess
import sys
import tempfile
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('package', type=Path, help='Built Cascade package in /nix/store')
    parser.add_argument('--nested', action='store_true', help='Keep the Wayland output to exercise hardware cursor rendering')
    parser.add_argument('--hyprglass', type=Path, help='Load HyprGlass with the real session configuration')
    parser.add_argument('--tests', nargs='+', help='Specific unittest method names')
    args = parser.parse_args()
    package = args.package.resolve()
    plugin = package / 'lib/libHyprspace.so'
    driver = package / 'share/cascade-overview/tests/gesture-driver.so'
    for path in (plugin, driver):
        if not path.is_file():
            parser.error(f'Missing artifact: {path}')
    if args.hyprglass and not args.hyprglass.is_file():
        parser.error(f'Missing HyprGlass plugin: {args.hyprglass}')
    for binary in ('Hyprland', 'hyprctl', 'foot', 'dbus-run-session'):
        if not shutil.which(binary):
            parser.error(f'Missing executable: {binary}')
    if not (os.environ.get('CASCADE_QA_WTYPE') or shutil.which('wtype')):
        parser.error('Install wtype for QA or set CASCADE_QA_WTYPE')
    parent_runtime = os.environ.get('XDG_RUNTIME_DIR', f'/run/user/{os.getuid()}')
    parent_display = Path(os.environ.get('WAYLAND_DISPLAY', 'wayland-1'))
    if not parent_display.is_absolute():
        parent_display = Path(parent_runtime) / parent_display
    # Unix-domain sockets include Hyprland's long instance signature.
    root = Path(tempfile.mkdtemp(prefix='cascade-'))
    for directory in ('r', 'state', 'config', 'cache', 'artifacts'):
        (root / directory).mkdir(mode=0o700)
    config = root / 'hyprland.lua'
    config.write_text('''hl.monitor({output="",mode="1280x800@60",position="auto",scale=1})
hl.config({input={kb_layout="us",follow_mouse=0},general={layout="dwindle",gaps_in=4,gaps_out=8},
decoration={rounding=4,blur={enabled=false}},animations={enabled=false},
misc={disable_hyprland_logo=true,disable_splash_rendering=true},ecosystem={enforce_permissions=false}})
''')
    if args.hyprglass:
        with config.open('a') as conf:
            conf.write('''if hl.plugin.hyprglass then
hl.plugin.hyprglass.config({enabled=false,default_theme="dark",default_preset="glass",
glass_opacity=0.85,blur_strength=1.5,tint_color=0x00000000,brightness=0.85,
refraction_strength=0.3,fresnel_strength=0.3,dark={brightness=0.78,contrast=0.92},
light={adaptive_boost=0.4},layers={enabled=true}})
hl.plugin.hyprglass.layer("fuzzel",{preset="glass",mask_threshold=0.05})
end
hl.window_rule({match={class="^cascade-qa-.*$"},opacity=0.77,tag="+hyprglass_enabled"})
hl.window_rule({match={class="^cascade-qa-.*$"},tag="+hyprglass_preset_glass"})
''')
    env = os.environ.copy()
    for key in ('HYPRLAND_INSTANCE_SIGNATURE', 'DISPLAY'):
        env.pop(key, None)
    env.update(PYTHONDONTWRITEBYTECODE='1', CASCADE_QA_ROOT=str(root), CASCADE_QA_RUNTIME=str(root / 'r'), XDG_RUNTIME_DIR=str(root / 'r'), XDG_STATE_HOME=str(root / 'state'),
               XDG_CONFIG_HOME=str(root / 'config'), XDG_CACHE_HOME=str(root / 'cache'), WAYLAND_DISPLAY=str(parent_display),
               HYPRLAND_NO_SD_VARS='1', HYPRLAND_NO_SD_NOTIFY='1', HYPRLAND_NO_RT='1', LIBSEAT_BACKEND='none', AQ_DRM_DEVICES='/dev/null')
    print(f'Isolated QA artifacts: {root}', flush=True)
    with (root / 'session.log').open('w') as log:
        process = subprocess.Popen(['dbus-run-session', '--', 'Hyprland', '--config', str(config)], env=env,
                                   stdout=log, stderr=log, start_new_session=True)
    try:
        deadline = time.monotonic() + 15
        while time.monotonic() < deadline:
            sockets = list((root / 'r/hypr').glob('*/.socket.sock'))
            if sockets:
                break
            if process.poll() is not None:
                raise RuntimeError(f'QA compositor exited; inspect {root / "session.log"}')
            time.sleep(.1)
        else:
            raise RuntimeError('QA compositor did not create its IPC socket')
        signature = sockets[0].parent.name
        (root / 'instance').write_text(signature)
        env.update(HYPRLAND_INSTANCE_SIGNATURE=signature, WAYLAND_DISPLAY='wayland-1',
                   CASCADE_QA_PLUGIN=str(plugin), CASCADE_QA_GESTURE_DRIVER=str(driver))
        def hc(*arguments):
            result = subprocess.run(['hyprctl', *arguments], env=env, text=True, capture_output=True, timeout=6, check=True)
            if result.stdout.startswith(('error:', "Couldn't")):
                raise RuntimeError(result.stdout)
            return result.stdout
        if args.nested:
            deadline = time.monotonic() + 15
            while not (monitors := json.loads(hc('monitors', '-j'))):
                if time.monotonic() >= deadline:
                    raise RuntimeError('QA compositor did not create its nested Wayland output')
                time.sleep(.1)
            monitor_name = monitors[0]['name']
        else:
            monitor_name = 'CASCADE-QA'
            hc('output', 'create', 'headless', monitor_name)
            hc('eval', 'hl.monitor({output="CASCADE-QA",mode="1280x800@60",position="0x0",scale=1})')
            for monitor in json.loads(hc('monitors', '-j')):
                if monitor['name'] != monitor_name:
                    hc('output', 'remove', monitor['name'])
        env.update(CASCADE_QA_MONITOR=monitor_name, CASCADE_QA_NESTED=str(int(args.nested)))
        if args.hyprglass:
            hc('plugin', 'load', str(args.hyprglass.resolve()))
        hc('plugin', 'load', str(plugin))
        # The warning for this deliberately isolated launch animates every refresh.
        hc('dismissnotify', '-1')
        (root / 'env.sh').write_text(''.join(f'export {key}={shlex.quote(value)}\n' for key, value in env.items()
                                           if key.startswith(('CASCADE_QA_', 'XDG_')) or key in ('WAYLAND_DISPLAY', 'HYPRLAND_INSTANCE_SIGNATURE')))
        command = [sys.executable, str(Path(__file__).with_name('runtime_test.py'))]
        if args.tests:
            command.extend(name if '.' in name else 'CascadeRuntime.' + name for name in args.tests)
        with (root / 'artifacts/runtime-tests.log').open('w') as log:
            result = subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=180)
        print((root / 'artifacts/runtime-tests.log').read_text())
        return result.returncode
    finally:
        if process.poll() is None:
            os.killpg(process.pid, signal.SIGTERM)
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGKILL)
            process.wait(timeout=5)


if __name__ == '__main__':
    sys.exit(main())
