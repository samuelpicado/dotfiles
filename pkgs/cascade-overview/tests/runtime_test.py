#!/usr/bin/env python3
"""Integration checks for the isolated /tmp/cascade-qa compositor only."""
import json
import os
from pathlib import Path
import subprocess
import shutil
import sys
import time
import unittest

ROOT = Path(os.environ.get('CASCADE_QA_ROOT', '/tmp/cascade-qa')).resolve()
if not ROOT.is_relative_to('/tmp'):
    raise RuntimeError('Runtime tests require an isolated compositor under /tmp')
HC = shutil.which('hyprctl')
WTYPE = os.environ.get('CASCADE_QA_WTYPE') or shutil.which('wtype')
FOOT = shutil.which('foot')
MONITOR = os.environ.get('CASCADE_QA_MONITOR', 'CASCADE-QA')


class CascadeRuntime(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not WTYPE or not FOOT or not HC:
            raise RuntimeError('Runtime tests require hyprctl, foot and wtype (or CASCADE_QA_WTYPE)')
        cls.env = os.environ.copy()
        cls.env['CASCADE_QA_ROOT'] = str(ROOT)
        runtime = Path(os.environ.get('CASCADE_QA_RUNTIME', ROOT / 'runtime')).resolve()
        if not runtime.is_relative_to(ROOT):
            raise RuntimeError('QA runtime must be inside the isolated QA directory')
        cls.env.update(XDG_RUNTIME_DIR=str(runtime), WAYLAND_DISPLAY='wayland-1',
                       HYPRLAND_INSTANCE_SIGNATURE=(ROOT / 'instance').read_text().strip(),
                       XDG_STATE_HOME=str(ROOT / 'state'), XDG_CONFIG_HOME=str(ROOT / 'config'),
                       XDG_CACHE_HOME=str(ROOT / 'cache'))
        cls.eval('hl.config({plugin={cascade={animate=false}}})')

    @classmethod
    def hc(cls, *arguments):
        result = subprocess.run([HC, *arguments], env=cls.env, text=True, capture_output=True, timeout=6)
        if result.returncode or result.stdout.startswith(("Couldn't", 'error:', 'Err:')):
            raise AssertionError(result.stdout + result.stderr)
        return result.stdout.strip()

    @classmethod
    def eval(cls, expression):
        return cls.hc('eval', expression)

    def state(self):
        return json.loads(self.hc('cascade'))

    def clients(self):
        return json.loads(self.hc('clients', '-j'))

    def api(self, action, argument=''):
        self.eval(f'assert(hl.plugin.cascade.{action}({json.dumps(argument)}))')
        time.sleep(.08)

    def command(self, argument):
        self.api('command', argument)

    def keys(self, *arguments):
        subprocess.run([WTYPE, *arguments], env=self.env, check=True, timeout=6)
        time.sleep(.12)

    def wait(self, predicate):
        deadline = time.monotonic() + 4
        while time.monotonic() < deadline:
            result = predicate()
            if result:
                return result
            time.sleep(.05)
        self.fail('Runtime condition did not become true')

    def fresh(self):
        self.api('close')
        self.command('group-new ' + self.id().split('.')[-1])
        return self.state()['selected']

    def foot(self, command='printf Cascade-QA; exec sleep 3600'):
        tag = 'cascade-qa-' + str(time.monotonic_ns())
        log = (ROOT / 'foot-tests.log').open('a')
        process = subprocess.Popen([FOOT, '--config=/dev/null', '--app-id=' + tag,
                                    '--title=' + tag, 'sh', '-c', command],
                                   env=self.env, stdout=log, stderr=log, start_new_session=True)
        log.close()
        def cleanup():
            if process.poll() is None:
                process.terminate()
            try:
                process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=2)
        self.addCleanup(cleanup)
        return self.wait(lambda: next((client for client in self.clients() if client['class'] == tag), None))

    def client(self, address):
        return next(client for client in self.clients() if client['address'] == address)

    def group(self, group_id=None):
        state = self.state()
        return next(group for group in state['groups'] if group['id'] == (state['activeGroup'] if group_id is None else group_id))

    def close_window(self, window):
        self.hc('dispatch', f'hl.dsp.window.close({{window={json.dumps("address:" + window["address"])}}})')
        self.wait(lambda: not any(client['address'] == window['address'] for client in self.clients()))

    def hit(self, kind, **attributes):
        return self.wait(lambda: next((hit for hit in self.state()['hits']
                                      if hit['kind'] == kind and all(hit.get(key) == value for key, value in attributes.items())), None))

    def center(self, hit):
        width, height = self.dimensions()
        left, right = max(2, hit['x']), min(width - 2, hit['x'] + hit['w'])
        top, bottom = max(2, hit['y']), min(height - 2, hit['y'] + hit['h'])
        self.assertGreater(right, left)
        self.assertGreater(bottom, top)
        return round((left + right) / 2), round((top + bottom) / 2)

    def drag(self, source, destination):
        width, height = self.dimensions()
        subprocess.run([sys.executable, str(Path(__file__).with_name('virtual_pointer.py')), 'drag',
                        *map(str, self.center(source) + self.center(destination)), '--width', str(width), '--height', str(height)], check=True, timeout=6)
        time.sleep(.15)

    def click(self, hit):
        width, height = self.dimensions()
        subprocess.run([sys.executable, str(Path(__file__).with_name('virtual_pointer.py')), 'click',
                        *map(str, self.center(hit)), '--width', str(width), '--height', str(height)],
                       env=self.env, check=True, timeout=6)

    def dimensions(self):
        monitor = next(m for m in json.loads(self.hc('monitors', '-j')) if m['id'] == self.state()['monitor'])
        return round(monitor['width'] / monitor['scale']), round(monitor['height'] / monitor['scale'])

    def test_01_inspect_with_real_window(self):
        self.fresh()
        self.foot()
        self.api('open')
        self.assertTrue(any(hit['kind'] == 1 and hit.get('window') for hit in self.state()['hits']))
        self.assertTrue(self.state()['active'])

    def test_02_initial_state_is_saved(self):
        path = Path(self.state()['statePath'])
        self.assertEqual(path.parent, ROOT / 'state/cascade-overview')
        self.assertTrue(path.is_file())
        self.assertGreater(path.stat().st_size, 0)

    def test_03_delete_native_active_empty_and_reopen(self):
        original = self.fresh()
        self.foot()
        self.command('desktop-new')
        removed = self.state()['selected']
        self.assertNotEqual(removed, original)
        self.command('desktop-left')
        self.api('open')
        self.command('desktop-delete')
        self.api('close')
        self.api('open')
        self.assertNotIn(removed, [desktop for group in self.state()['groups'] for desktop in group['desktops']])
        self.assertNotEqual(json.loads(self.hc('activeworkspace', '-j'))['id'], removed)

    def test_04_tab_new_enter_does_not_focus_stale_window(self):
        original = self.fresh()
        window = self.foot()
        self.api('open')
        self.keys('-k', 'Tab', '-k', 'n')
        selected = self.state()['selected']
        self.assertNotEqual(selected, original)
        self.keys('-k', 'Return')
        self.assertFalse(self.state()['active'])
        self.assertEqual(json.loads(self.hc('activeworkspace', '-j'))['id'], selected)
        active = json.loads(self.hc('activewindow', '-j'))
        self.assertNotEqual(active.get('address'), window['address'])

    def test_05_minimal_overview_keeps_legacy_names_and_disables_rename(self):
        desktop = self.fresh()
        self.foot()
        names = {group['id']: group['name'] for group in self.state()['groups']}
        self.api('open')
        self.keys('-k', 'F2', '-k', 'BackSpace')
        state = self.state()
        self.assertTrue(state['active'])
        self.assertFalse(state.get('renaming', False))
        self.assertEqual(state['selected'], desktop)
        self.assertEqual({group['id']: group['name'] for group in state['groups']}, names)
        self.assertFalse(any(hit['kind'] in (2, 4, 5, 9) for hit in state['hits']), 'Overview must omit desktop creation, group drag handles, rename and individual desktop deletion controls')
        self.hit(8, group=state['activeGroup'])
        subprocess.run(['grim', str(ROOT / 'artifacts/minimal-overview.png')], env=self.env, check=True, timeout=6)
        self.eval('local ok=hl.plugin.cascade.command("rename QA ÑΩ"); assert(not ok, "Group renaming must be unavailable")')
        self.assertEqual({group['id']: group['name'] for group in self.state()['groups']}, names, 'Disabling rename must preserve existing group names in metadata')

    def test_06_arrow_selection_and_external_workspace(self):
        self.fresh()
        self.foot()
        self.command('desktop-new')
        second = self.state()['selected']
        self.command('desktop-select 1')
        self.api('open')
        self.keys('-k', 'Right')
        self.assertEqual(self.state()['selected'], second)
        self.hc('dispatch', 'hl.dsp.focus({workspace=9001})')
        self.wait(lambda: self.state()['selected'] == 9001)

    def test_07_drag_window_to_existing_desktop(self):
        self.fresh()
        window = self.foot()
        self.command('desktop-select 2')
        destination = self.state()['selected']
        self.command('desktop-select 1')
        self.api('open')
        self.drag(self.hit(1, window=window['address']), self.hit(0, desktop=destination))
        self.assertEqual(self.client(window['address'])['workspace']['id'], destination)

    def test_08_drag_window_to_reserve_replenishes_it(self):
        original = self.fresh()
        window = self.foot()
        self.command('desktop-select 2')
        destination = self.state()['selected']
        self.api('open')
        group_id = self.state()['activeGroup']
        self.assertFalse(any(hit['kind'] == 2 for hit in self.state()['hits']), 'The redundant + Escritorio control must be absent')
        self.drag(self.hit(1, window=window['address']), self.hit(0, desktop=destination))
        self.wait(lambda: len(self.group(group_id)['desktops']) == 3)
        self.assertEqual(self.client(window['address'])['workspace']['id'], destination)
        self.assertNotEqual(destination, original)
        reserve = self.group(group_id)['desktops'][-1]
        self.assertNotEqual(reserve, destination)
        self.assertFalse(any(client['workspace']['id'] == reserve for client in self.clients()))
        self.assertFalse(any(hit['kind'] == 2 for hit in self.state()['hits']))

    def test_09_drag_window_to_another_group(self):
        self.fresh()
        window = self.foot()
        self.command('group-new Drag Destination')
        state = self.state()
        target_group, destination = state['activeGroup'], state['selected']
        self.api('navigate', 'up')
        self.api('open')
        self.drag(self.hit(1, window=window['address']), self.hit(0, group=target_group, desktop=destination))
        self.assertEqual(self.client(window['address'])['workspace']['id'], destination)

    def test_10_occupied_delete_is_rejected(self):
        desktop = self.fresh()
        window = self.foot()
        self.eval('local ok=hl.plugin.cascade.command("desktop-delete"); assert(not ok)')
        self.eval('local ok=hl.plugin.cascade.command("group-delete"); assert(not ok)')
        self.assertEqual(self.state()['selected'], desktop)
        self.assertEqual(self.client(window['address'])['workspace']['id'], desktop)

    def test_11_fullscreen_survives_overview(self):
        self.fresh()
        window = self.foot()
        self.hc('dispatch', 'hl.dsp.window.fullscreen({action="toggle"})')
        before = self.client(window['address'])['fullscreen']
        self.assertNotEqual(before, 0)
        self.api('open')
        self.assertEqual(self.client(window['address'])['fullscreen'], before)
        self.api('close')
        self.assertEqual(self.client(window['address'])['fullscreen'], before)

    def test_12_order_and_drag_desktop_between_groups(self):
        self.fresh()
        self.foot()
        source_group = self.state()['activeGroup']
        self.command('desktop-new')
        desktop = self.state()['selected']
        window = self.foot()
        self.command('desktop-left')
        group = next(g for g in self.state()['groups'] if g['id'] == source_group)
        self.assertEqual(group['desktops'][0], desktop)
        self.command('group-new Desktop Destination')
        target_group = self.state()['activeGroup']
        destination = self.state()['selected']
        self.command('group-up')
        self.command('group-down')
        self.api('navigate', 'up')
        self.api('open')
        source = dict(self.hit(0, desktop=desktop))
        source['h'] = min(28, source['h'])
        self.drag(source, self.hit(0, group=target_group, desktop=destination))
        groups = self.state()['groups']
        self.assertIn(desktop, next(g['desktops'] for g in groups if g['id'] == target_group))
        self.assertNotIn(desktop, next(g['desktops'] for g in groups if g['id'] == source_group))
        self.assertEqual(self.client(window['address'])['workspace']['id'], desktop)

    def test_13_groups_remember_their_selected_desktop(self):
        self.fresh()
        self.foot()
        self.command('desktop-new')
        remembered = self.state()['selected']
        self.command('group-new Recall Destination')
        other = self.state()['selected']
        self.api('navigate', 'up')
        self.assertEqual(self.state()['selected'], remembered)
        self.api('navigate', 'down')
        self.assertEqual(self.state()['selected'], other)

    def test_14_native_gesture_events(self):
        driver = os.environ.get('CASCADE_QA_GESTURE_DRIVER')
        self.assertTrue(driver, 'Set CASCADE_QA_GESTURE_DRIVER to the package gesture fixture')
        self.hc('plugin', 'load', driver)
        self.addCleanup(lambda: self.hc('plugin', 'unload', driver))
        self.fresh()
        self.foot()
        self.command('desktop-new')
        second = self.state()['selected']
        self.command('desktop-select 1')
        self.eval('assert(hl.plugin.cascade_test.swipe(3,-120,0,false))')
        self.assertEqual(self.state()['selected'], second)
        self.command('group-new Gesture Destination')
        destination = self.state()['activeGroup']
        self.api('navigate', 'up')
        self.eval('assert(hl.plugin.cascade_test.swipe(3,0,-120,false))')
        self.assertEqual(self.state()['activeGroup'], destination)
        selected = self.state()['selected']
        self.eval('assert(not hl.plugin.cascade_test.swipe(2,120,0,false))')
        self.assertEqual(self.state()['selected'], selected)
        self.eval('assert(hl.plugin.cascade_test.swipe(3,0,120,true))')
        self.assertEqual(self.state()['activeGroup'], destination)
        self.eval('assert(hl.plugin.cascade_test.swipe(4,0,-120,false))')
        self.assertTrue(self.state()['active'])
        self.eval('assert(hl.plugin.cascade_test.swipe(4,0,120,false))')
        self.assertFalse(self.state()['active'])

    def test_15_monitors_allocate_distinct_desktops(self):
        self.api('close', 'all')
        self.hc('dispatch', f'hl.dsp.focus({{monitor={json.dumps(MONITOR)}}})')
        first_monitor = self.state()['monitor']
        self.hc('output', 'create', 'headless', 'CASCADE-QA-B')
        self.addCleanup(lambda: self.hc('output', 'remove', 'CASCADE-QA-B'))
        self.eval('hl.monitor({output="CASCADE-QA-B",mode="1280x800@60",position="1280x0",scale=1})')
        self.hc('dispatch', 'hl.dsp.focus({monitor="CASCADE-QA-B"})')
        self.wait(lambda: self.state().get('monitor') != first_monitor)
        self.command('desktop-new')
        second_ids = {d for g in self.state()['groups'] for d in g['desktops']}
        self.hc('dispatch', f'hl.dsp.focus({{monitor={json.dumps(MONITOR)}}})')
        first_ids = {d for g in self.state()['groups'] for d in g['desktops']}
        self.assertFalse(first_ids & second_ids)
        self.hc('dispatch', 'hl.dsp.focus({monitor="CASCADE-QA-B"})')
        self.api('open')
        self.assertTrue(self.state()['active'])
        self.api('close')

    def test_16_plugin_reload_preserves_groups(self):
        plugin = os.environ.get('CASCADE_QA_PLUGIN')
        self.assertTrue(plugin, 'Set CASCADE_QA_PLUGIN to the loaded plugin path')
        self.api('close', 'all')
        self.command('desktop-select 1')
        before = self.state()['groups']
        self.hc('plugin', 'unload', plugin)
        self.hc('plugin', 'load', plugin)
        self.assertEqual(self.state()['groups'], before)

    def test_17_fractional_scale_hit_testing(self):
        self.fresh()
        window = self.foot()
        self.eval(f'hl.monitor({{output={json.dumps(MONITOR)},mode="1280x800@60",position="0x0",scale=1.25}})')
        self.addCleanup(lambda: self.eval(f'hl.monitor({{output={json.dumps(MONITOR)},mode="1280x800@60",position="0x0",scale=1}})'))
        self.api('open')
        hit = self.hit(1, window=window['address'])
        self.assertEqual(self.dimensions(), (1024, 640))
        subprocess.run([sys.executable, str(Path(__file__).with_name('virtual_pointer.py')), 'click',
                        *map(str, self.center(hit)), '--width', '1024', '--height', '640'], check=True, timeout=6)
        self.wait(lambda: not self.state()['active'])
        self.assertEqual(json.loads(self.hc('activewindow', '-j'))['address'], window['address'])

    def test_18_inactive_window_preview_updates(self):
        self.assertTrue(shutil.which('grim'), 'Live-preview QA requires grim')
        self.fresh()
        self.foot("while :; do printf '\\033[48;2;0;200;0m\\033[2J'; sleep .5; printf '\\033[48;2;0;0;200m\\033[2J'; sleep .5; done")
        self.command('group-new Live preview destination')
        self.api('open')
        self.eval('hl.config({plugin={cascade={animate=false}}})')
        saw_green = saw_blue = False
        for _ in range(8):
            data = subprocess.check_output(['grim', '-t', 'ppm', '-'], env=self.env, timeout=6).split(b'\n', 3)[3]
            green = blue = 0
            for r, g, b in zip(data[0::3], data[1::3], data[2::3]):
                green += g > 120 and g > r * 2 and g > b * 2
                blue += b > 120 and b > r * 2 and b > g * 2
            saw_green |= green > 1000
            saw_blue |= blue > 1000
            if saw_green and saw_blue:
                break
            time.sleep(.3)
        self.assertTrue(saw_green and saw_blue, 'The inactive window preview must display both live background colors')

    def test_19_group_arrow_buttons_reorder_rows_and_preserve_selection(self):
        desktop = self.fresh()
        source = self.state()['activeGroup']
        self.command('group-new Row Destination')
        target = self.state()['activeGroup']
        self.api('navigate', 'up')
        self.api('open')
        original = [group['id'] for group in self.state()['groups']]
        expected = original.copy()
        index = expected.index(source)
        self.assertEqual(expected[index + 1], target)
        expected[index], expected[index + 1] = expected[index + 1], expected[index]
        self.click(self.hit(7, group=source))
        self.wait(lambda: [group['id'] for group in self.state()['groups']] == expected)
        self.assertTrue(self.state()['active'])
        self.assertEqual(self.state()['activeGroup'], source)
        self.assertEqual(self.state()['selected'], desktop)
        self.click(self.hit(6, group=source))
        self.wait(lambda: [group['id'] for group in self.state()['groups']] == original)
        self.assertEqual(self.state()['activeGroup'], source)
        self.assertEqual(self.state()['selected'], desktop)

    def test_20_delete_native_active_group_and_reopen(self):
        desktop = self.fresh()
        removed = self.state()['activeGroup']
        self.api('open')
        self.command('group-delete')
        self.api('close')
        self.api('open')
        self.assertNotIn(removed, [g['id'] for g in self.state()['groups']])
        self.assertNotIn(desktop, [d for g in self.state()['groups'] for d in g['desktops']])
        self.assertNotEqual(json.loads(self.hc('activeworkspace', '-j'))['id'], desktop)

    def test_21_overview_forces_composition_and_releases_it(self):
        self.fresh()
        self.foot()
        self.hc('dispatch', 'hl.dsp.window.fullscreen({action="toggle"})')
        # Exclude the headless backend's hardware-cursor fallback from lock accounting.
        self.eval('hl.config({cursor={invisible=true},render={direct_scanout=true},plugin={cascade={animate=false}}})')
        self.hc('dispatch', 'hl.dsp.cursor.move({x=100,y=400})')
        self.addCleanup(lambda: self.eval('hl.config({cursor={invisible=false},render={direct_scanout=false}})'))
        def blocked():
            monitor = next(m for m in json.loads(self.hc('monitors', '-j')) if m['name'] == MONITOR)
            return 'SW' in monitor['directScanoutBlockedBy']
        self.wait(lambda: not blocked())
        self.api('open')
        self.api('open')
        self.assertTrue(blocked(), 'A fullscreen client must not bypass the overview render pass')
        self.api('close')
        self.wait(lambda: not blocked())

    def test_22_software_cursor_is_visible_above_overview(self):
        self.fresh()
        self.eval('hl.config({cursor={invisible=false,inactive_timeout=-1}})')
        self.hc('setcursor', 'Adwaita', '32')
        width, height = self.dimensions()
        x, y = min(100, width // 4), min(400, height - 60)
        pointer = [sys.executable, str(Path(__file__).with_name('virtual_pointer.py')), 'move']
        extent = ['--width', str(width), '--height', str(height)]
        self.api('open')
        subprocess.run(pointer + [str(x), str(y)] + extent, env=self.env, check=True, timeout=6)
        time.sleep(.2)
        ppm = subprocess.check_output(['grim', '-t', 'ppm', '-'], env=self.env, timeout=6)
        _, dimensions, _, pixels = ppm.split(b'\n', 3)
        pixel_width, pixel_height = map(int, dimensions.split())
        scale_x, scale_y = pixel_width / width, pixel_height / height
        pixel_x, pixel_y = round(x * scale_x), round(y * scale_y)
        bright = 0
        for sample_y in range(max(0, pixel_y - 5), min(pixel_height, pixel_y + round(45 * scale_y))):
            for sample_x in range(max(0, pixel_x - 5), min(pixel_width, pixel_x + round(45 * scale_x))):
                offset = (sample_y * pixel_width + sample_x) * 3
                bright += max(pixels[offset:offset + 3]) > 100
        self.assertGreater(bright, 8, 'The software cursor must be rendered above the overview background')

    def test_23_occupied_delete_displays_feedback(self):
        desktop = self.fresh()
        self.foot()
        self.command('desktop-new')
        self.command('desktop-select 1')
        self.api('open')
        self.keys('-k', 'Delete')
        self.assertIn('vacíos', self.state().get('status', ''))
        self.assertIn(desktop, [d for g in self.state()['groups'] for d in g['desktops']])
        card = self.hit(0, desktop=desktop)
        state = self.state()
        monitor = next(m for m in json.loads(self.hc('monitors', '-j')) if m['id'] == state['monitor'])
        logical_width, logical_height = self.dimensions()
        ppm = subprocess.check_output(['grim', '-t', 'ppm', '-'], env=self.env, timeout=6)
        _, dimensions, _, pixels = ppm.split(b'\n', 3)
        width, height = map(int, dimensions.split())
        scale_x, scale_y = width / logical_width, height / logical_height
        left = max(0, round((card['x'] - monitor['x']) * scale_x) + 2)
        right = min(width, round((card['x'] - monitor['x'] + card['w']) * scale_x) - 2)
        top = max(0, round((card['y'] - monitor['y']) * scale_y) + 2)
        bottom = min(height, round((card['y'] - monitor['y'] + card['h']) * scale_y) - 2)
        interior = b''.join(pixels[(row * width + left) * 3:(row * width + right) * 3] for row in range(top, bottom))
        warning = sum(r > 180 and g > 100 and b < 140 for r, g, b in zip(interior[0::3], interior[1::3], interior[2::3]))
        self.assertGreater(warning, 20, 'Deletion feedback must be visible inside the selected desktop panel')

    def test_24_move_without_window_does_not_create_desktops(self):
        self.fresh()
        before = self.state()['groups']
        self.eval('local ok=hl.plugin.cascade.command("window-to 3"); assert(not ok)')
        self.assertEqual(self.state()['groups'], before)

    def test_25_animated_close_restores_hardware_cursor(self):
        self.fresh()
        self.foot()
        self.eval('hl.config({cursor={invisible=true,no_hardware_cursors=false,use_cpu_buffer=0,inactive_timeout=-1},render={direct_scanout=true}})')
        self.addCleanup(lambda: self.eval('hl.config({cursor={invisible=false},render={direct_scanout=false},plugin={cascade={animate=false}}})'))
        def blocked():
            monitor = next(m for m in json.loads(self.hc('monitors', '-j')) if m['name'] == MONITOR)
            return 'SW' in monitor['directScanoutBlockedBy']
        self.wait(lambda: not blocked())
        self.hc('setcursor', 'Adwaita', '32')
        self.eval('hl.config({cursor={invisible=false},plugin={cascade={animate=true}}})')
        width, height = self.dimensions()
        subprocess.run([sys.executable, str(Path(__file__).with_name('virtual_pointer.py')), 'move',
                        str(width // 2), str(height // 2), '--width', str(width), '--height', str(height)],
                       env=self.env, check=True, timeout=6)
        # Wayland outputs advertise hardware cursors; headless still checks lock release.
        for _ in range(3):
            self.api('open')
            time.sleep(.8)
            self.assertTrue(self.state()['active'])
            self.assertTrue(blocked())
            self.api('close')
            time.sleep(1.2)
            self.assertFalse(self.state()['active'], 'The compositor must survive the animated close')
            self.eval('hl.config({cursor={invisible=true}})')
            self.wait(lambda: not blocked())
            self.eval('hl.config({cursor={invisible=false}})')

    def test_26_disabled_monitor_releases_its_overview_lock(self):
        self.api('close', 'all')
        self.hc('dispatch', f'hl.dsp.focus({{monitor={json.dumps(MONITOR)}}})')
        self.fresh()
        self.eval('hl.config({cursor={invisible=true},render={direct_scanout=true},plugin={cascade={animate=false}}})')
        self.addCleanup(lambda: self.eval('hl.config({cursor={invisible=false},render={direct_scanout=false}})'))
        original = next(m for m in json.loads(self.hc('monitors', '-j')) if m['name'] == MONITOR)
        self.hc('output', 'create', 'headless', 'CASCADE-QA-HOTPLUG')
        self.addCleanup(lambda: self.hc('output', 'remove', 'CASCADE-QA-HOTPLUG'))
        self.eval('hl.monitor({output="CASCADE-QA-HOTPLUG",mode="1280x800@60",position="1280x0",scale=1})')
        restore = f'hl.monitor({{output={json.dumps(MONITOR)},disabled=false,mode="{original["width"]}x{original["height"]}@60",position="0x0",scale={original["scale"]}}})'
        self.addCleanup(lambda: self.eval(restore))
        self.hc('dispatch', f'hl.dsp.focus({{monitor={json.dumps(MONITOR)}}})')
        self.api('open')
        def monitor():
            return next((m for m in json.loads(self.hc('monitors', '-j')) if m['name'] == MONITOR), None)
        self.assertIn('SW', monitor()['directScanoutBlockedBy'])
        self.eval(f'hl.monitor({{output={json.dumps(MONITOR)},disabled=true}})')
        self.wait(lambda: monitor() is None)
        self.eval(restore)
        self.wait(monitor)
        self.hc('dispatch', f'hl.dsp.focus({{monitor={json.dumps(MONITOR)}}})')
        self.assertEqual(monitor()['id'], original['id'])
        self.assertFalse(self.state()['active'])
        self.wait(lambda: 'SW' not in monitor()['directScanoutBlockedBy'])
        self.api('open')
        self.assertTrue(self.state()['active'])
        self.api('close')
        self.assertNotIn('SW', monitor()['directScanoutBlockedBy'])

    def test_27_overview_restores_cursor_hidden_by_client(self):
        driver = os.environ.get('CASCADE_QA_GESTURE_DRIVER')
        self.assertTrue(driver)
        self.hc('plugin', 'load', driver)
        self.addCleanup(lambda: self.hc('plugin', 'unload', driver))
        self.addCleanup(lambda: self.api('close', 'all'))
        self.fresh()
        self.foot()
        self.eval('hl.config({cursor={invisible=false,inactive_timeout=-1},plugin={cascade={animate=false}}})')
        self.hc('setcursor', 'Adwaita', '32')
        self.eval('hl.plugin.cascade_test.hide_cursor(); assert(not hl.plugin.cascade_test.cursor_visible())')
        self.api('open')
        self.eval('assert(hl.plugin.cascade_test.cursor_visible(), "Overview must provide a cursor when the client hid it")')
        self.eval('hl.plugin.cascade_test.hide_cursor(); assert(hl.plugin.cascade_test.cursor_visible(), "Client cursor changes must not hide the overview cursor")')
        self.api('close')
        self.eval('assert(not hl.plugin.cascade_test.cursor_visible(), "Closing must restore the client cursor preference")')

    def test_28_multiple_overviews_share_cursor_and_unload_restores_it(self):
        driver = os.environ.get('CASCADE_QA_GESTURE_DRIVER')
        plugin = os.environ.get('CASCADE_QA_PLUGIN')
        self.assertTrue(driver and plugin)
        self.hc('plugin', 'load', driver)
        self.addCleanup(lambda: self.hc('plugin', 'unload', driver))
        self.api('close', 'all')
        self.eval('hl.config({cursor={invisible=false,inactive_timeout=-1},plugin={cascade={animate=false}}})')
        self.hc('output', 'create', 'headless', 'CASCADE-QA-CURSOR')
        self.addCleanup(lambda: self.hc('output', 'remove', 'CASCADE-QA-CURSOR'))
        self.addCleanup(lambda: self.api('close', 'all'))
        self.eval('hl.monitor({output="CASCADE-QA-CURSOR",mode="1280x800@60",position="1280x0",scale=1})')
        self.hc('dispatch', f'hl.dsp.focus({{monitor={json.dumps(MONITOR)}}})')
        self.hc('setcursor', 'Adwaita', '32')
        self.eval('hl.plugin.cascade_test.hide_cursor(); assert(not hl.plugin.cascade_test.cursor_visible())')
        self.api('open', 'all')
        self.api('close')
        self.assertFalse(self.state()['active'])
        self.eval('assert(hl.plugin.cascade_test.cursor_visible(), "Another open overview must retain the shared cursor")')
        self.api('close', 'all')
        self.eval('assert(not hl.plugin.cascade_test.cursor_visible(), "The last closed overview must restore the client cursor")')
        self.api('open')
        self.eval('assert(hl.plugin.cascade_test.cursor_visible())')
        self.hc('plugin', 'unload', plugin)
        try:
            self.eval('assert(not hl.plugin.cascade_test.cursor_visible(), "Unloading an open overview must restore the client cursor")')
        finally:
            self.hc('plugin', 'load', plugin)


    def test_29_reordered_desktops_animate_in_visual_order(self):
        driver = os.environ['CASCADE_QA_GESTURE_DRIVER']
        self.hc('plugin', 'load', driver)
        self.addCleanup(lambda: self.hc('plugin', 'unload', driver))
        first = self.fresh()
        self.foot()
        self.command('desktop-select 2')
        second = self.state()['selected']
        self.foot()
        self.command('desktop-left')
        self.assertGreater(second, first)
        self.eval('hl.config({plugin={cascade={animate=true}}})')
        self.addCleanup(lambda: self.eval('hl.config({plugin={cascade={animate=false}}})'))
        # The visual right neighbour has a lower native workspace ID.
        self.eval('assert(hl.plugin.cascade.navigate("right"))')
        self.eval(f'assert(hl.plugin.cascade_test.workspace({second}).goalX < 0, "Outgoing desktop must move left in visual order")')
        time.sleep(.7)
        self.eval('assert(hl.plugin.cascade.navigate("left"))')
        self.eval(f'assert(hl.plugin.cascade_test.workspace({first}).goalX > 0, "Outgoing desktop must move right in visual order")')

    def test_30_horizontal_swipe_moves_before_fingers_lift(self):
        driver = os.environ['CASCADE_QA_GESTURE_DRIVER']
        self.hc('plugin', 'load', driver)
        self.addCleanup(lambda: self.hc('plugin', 'unload', driver))
        first = self.fresh()
        self.foot("while :; do printf '\\033[48;2;200;0;0m\\033[2J'; sleep .05; done")
        self.command('desktop-select 2')
        second = self.state()['selected']
        self.foot("while :; do printf '\\033[48;2;0;0;200m\\033[2J'; sleep .05; done")
        time.sleep(.2)
        self.command('desktop-select 1')
        self.eval('hl.config({plugin={cascade={animate=true}}})')
        self.addCleanup(lambda: self.eval('hl.config({plugin={cascade={animate=false}}})'))
        self.addCleanup(lambda: self.eval('hl.plugin.cascade_test.finish(true)'))
        self.eval('assert(hl.plugin.cascade_test.begin(3)); hl.plugin.cascade_test.update(3,-30,0)')
        self.assertEqual(json.loads(self.hc('activeworkspace', '-j'))['id'], first)
        self.eval(f'local a=hl.plugin.cascade_test.workspace({first}); local b=hl.plugin.cascade_test.workspace({second}); assert(a.x < -1 and b.x > 0 and b.forced, "Both desktops must follow the held gesture")')
        self.eval('hl.plugin.cascade_test.update(3,-30,0)')
        time.sleep(.08)
        self.eval(f'assert(hl.plugin.cascade_test.workspace({first}).x < -100, "The next update must move immediately")')
        frame = subprocess.check_output(['grim', '-t', 'ppm', '-'], env=self.env, timeout=6)
        subprocess.run(['grim', str(ROOT / 'artifacts/held-swipe.png')], env=self.env, check=True, timeout=6)
        pixels = frame.split(b'\n', 3)[3]
        blue = sum(b > 80 and b > r * 1.5 and b > g * 1.5 for r, g, b in zip(pixels[0::3], pixels[1::3], pixels[2::3]))
        (ROOT / 'artifacts/held-swipe.ppm').write_bytes(frame)
        self.assertGreater(blue, 1000, 'The incoming blue desktop must be visible before the gesture ends')
        self.eval('hl.plugin.cascade_test.finish(true)')
        time.sleep(.7)
        self.assertEqual(json.loads(self.hc('activeworkspace', '-j'))['id'], first)
        self.eval(f'local a=hl.plugin.cascade_test.workspace({first}); local b=hl.plugin.cascade_test.workspace({second}); assert(math.abs(a.x)<1 and not a.forced and not b.forced, "Cancel must restore the source and release rendering")')

    def test_31_swipe_commit_reverse_and_edges_follow_reordered_desktops(self):
        driver = os.environ['CASCADE_QA_GESTURE_DRIVER']
        self.hc('plugin', 'load', driver)
        self.addCleanup(lambda: self.hc('plugin', 'unload', driver))
        first = self.fresh()
        self.foot()
        self.command('desktop-select 2')
        second = self.state()['selected']
        self.foot()
        self.command('desktop-left')
        state = self.state()
        last = next(group['desktops'][-1] for group in state['groups'] if group['id'] == state['activeGroup'])
        self.eval('hl.config({plugin={cascade={animate=true}}})')
        self.addCleanup(lambda: self.eval('hl.config({plugin={cascade={animate=false,reverseSwipe=false}}})'))
        self.addCleanup(lambda: self.eval('hl.plugin.cascade_test.finish(true)'))
        self.eval(f'''assert(hl.plugin.cascade_test.begin(3)); hl.plugin.cascade_test.update(3,30,0)
assert(hl.plugin.cascade_test.workspace({second}).x>0 and hl.plugin.cascade_test.workspace({last}).x<0, "The first desktop must wrap to the last desktop in its own group")
hl.plugin.cascade_test.update(3,-100,0)
local before=hl.plugin.cascade_test.workspace({first}).x
assert(before>0, "Reversing the held gesture must bring in the right neighbour")
hl.plugin.cascade_test.finish(false)
local after=hl.plugin.cascade_test.workspace({first})
assert(after.x>=0 and after.x<=before+1 and after.goalX==0, "Release must continue from the dragged position")
assert(hl.plugin.cascade_test.workspace({second}).goalX<0, "Commit must keep moving left despite inverted IDs")''')
        self.assertEqual(self.state()['selected'], first)
        self.assertEqual(json.loads(self.hc('activeworkspace', '-j'))['id'], first)
        time.sleep(.7)
        self.eval(f'''assert(hl.plugin.cascade_test.begin(3)); hl.plugin.cascade_test.update(3,70,0)
assert(hl.plugin.cascade_test.workspace({first}).x>0 and hl.plugin.cascade_test.workspace({second}).x<0)
hl.plugin.cascade_test.finish(false)
assert(hl.plugin.cascade_test.workspace({first}).goalX>0)''')
        self.assertEqual(self.state()['selected'], second)
        time.sleep(.7)
        self.command('desktop-select 2')
        time.sleep(.7)
        self.eval('hl.config({plugin={cascade={reverseSwipe=true}}})')
        self.eval('assert(hl.plugin.cascade_test.begin(3)); hl.plugin.cascade_test.update(3,-70,0); hl.plugin.cascade_test.finish(false)')
        self.assertEqual(self.state()['selected'], second)

    def test_32_unloading_during_swipe_restores_native_rendering(self):
        driver, plugin = os.environ['CASCADE_QA_GESTURE_DRIVER'], os.environ['CASCADE_QA_PLUGIN']
        self.hc('plugin', 'load', driver)
        self.addCleanup(lambda: self.hc('plugin', 'unload', driver))
        first = self.fresh()
        self.foot()
        self.command('desktop-select 2')
        second = self.state()['selected']
        self.foot()
        self.command('desktop-select 1')
        self.eval('assert(hl.plugin.cascade_test.begin(3)); hl.plugin.cascade_test.update(3,-60,0)')
        self.hc('plugin', 'unload', plugin)
        try:
            self.eval(f'local a=hl.plugin.cascade_test.workspace({first}); local b=hl.plugin.cascade_test.workspace({second}); assert(a.x==0 and a.alpha==1 and not a.forced and b.x==0 and not b.forced, "Unloading a held gesture must restore the native desktop")')
        finally:
            self.hc('plugin', 'load', plugin)

    def test_33_fullscreen_swipe_at_fractional_scale_blocks_scanout(self):
        driver = os.environ['CASCADE_QA_GESTURE_DRIVER']
        self.hc('plugin', 'load', driver)
        self.addCleanup(lambda: self.hc('plugin', 'unload', driver))
        first = self.fresh()
        window = self.foot()
        self.hc('dispatch', 'hl.dsp.window.fullscreen({action="toggle"})')
        self.command('desktop-select 2')
        second = self.state()['selected']
        self.foot()
        self.command('desktop-select 1')
        self.eval(f'hl.monitor({{output={json.dumps(MONITOR)},mode="1280x800@60",position="0x0",scale=1.25}})')
        self.addCleanup(lambda: self.eval(f'hl.monitor({{output={json.dumps(MONITOR)},mode="1280x800@60",position="0x0",scale=1}})'))
        self.addCleanup(lambda: self.eval('hl.plugin.cascade_test.finish(true)'))
        self.eval('assert(hl.plugin.cascade_test.begin(3)); hl.plugin.cascade_test.update(3,-60,0)')
        monitor = next(m for m in json.loads(self.hc('monitors', '-j')) if m['name'] == MONITOR)
        self.assertIn('OFFSET', monitor['solitaryBlockedBy'], 'The moving fullscreen desktop must not use direct scanout')
        self.eval('hl.plugin.cascade_test.finish(false)')
        self.assertEqual(json.loads(self.hc('activeworkspace', '-j'))['id'], second)
        self.assertEqual(self.client(window['address'])['fullscreen'], 2)
        self.assertNotEqual(first, second)


    def test_34_last_empty_desktop_is_automatic_and_swipe_accessible(self):
        driver = os.environ['CASCADE_QA_GESTURE_DRIVER']
        self.hc('plugin', 'load', driver)
        self.addCleanup(lambda: self.hc('plugin', 'unload', driver))
        first = self.fresh()
        group_id = self.state()['activeGroup']
        def desktops():
            return next(g['desktops'] for g in self.state()['groups'] if g['id'] == group_id)
        self.foot()
        self.wait(lambda: len(desktops()) == 2)
        spare = desktops()[-1]
        self.assertNotEqual(spare, first)
        self.assertEqual(self.state()['selected'], first)
        self.assertFalse(self.state()['active'])
        self.eval('assert(hl.plugin.cascade_test.swipe(3,-120,0,false))')
        self.assertEqual(self.state()['selected'], spare)
        self.assertEqual(json.loads(self.hc('activeworkspace', '-j'))['id'], spare)
        self.assertEqual(desktops(), [first, spare])
        self.eval('assert(hl.plugin.cascade_test.swipe(3,-120,0,false))')
        self.assertEqual(self.state()['selected'], first, 'The unused reserve must wrap back to the first desktop')
        self.assertEqual(desktops(), [first, spare], 'Visiting an unused reserve must not grow the group')
        self.eval('assert(hl.plugin.cascade_test.swipe(3,-120,0,false))')
        self.assertEqual(self.state()['selected'], spare)
        window = self.foot()
        self.wait(lambda: len(desktops()) == 3)
        self.assertEqual(self.state()['selected'], spare, 'Creating a reserve must not switch the current desktop')
        reserve = desktops()[-1]
        self.hc('dispatch', 'hl.dsp.window.close()')
        self.wait(lambda: not any(c['address'] == window['address'] for c in self.clients()))
        self.wait(lambda: desktops() == [first, reserve])
        self.assertEqual(self.state()['selected'], reserve, 'Closing the last window before the reserve must select the surviving reserve')
        self.assertEqual(json.loads(self.hc('activeworkspace', '-j'))['id'], reserve)

    def test_35_moving_into_reserve_preserves_overview_selection_and_reload(self):
        first = self.fresh()
        window = self.foot()
        group_id = self.state()['activeGroup']
        def group():
            return next(g for g in self.state()['groups'] if g['id'] == group_id)
        self.wait(lambda: len(group()['desktops']) == 2)
        spare = group()['desktops'][-1]
        self.api('open')
        self.keys('-k', 'Right')
        self.assertEqual(self.state()['selected'], spare)
        self.hc('dispatch', f'hl.dsp.window.move({{workspace={spare}}})')
        self.wait(lambda: self.client(window['address'])['workspace']['id'] == spare)
        self.wait(lambda: len(group()['desktops']) == 3)
        self.assertEqual(self.state()['selected'], spare, 'Window events must preserve the overview selection')
        self.assertEqual(group()['desktops'][:2], [first, spare])
        self.assertFalse(any(c['workspace']['id'] == group()['desktops'][-1] for c in self.clients()))
        self.api('close')
        self.command('desktop-select 2')
        before = self.state()['groups']
        plugin = os.environ['CASCADE_QA_PLUGIN']
        self.hc('plugin', 'unload', plugin)
        self.hc('plugin', 'load', plugin)
        self.assertEqual(self.state()['groups'], before)


    def prepare_horizontal_swipe(self):
        driver = os.environ['CASCADE_QA_GESTURE_DRIVER']
        self.hc('plugin', 'load', driver)
        self.addCleanup(lambda: self.hc('plugin', 'unload', driver))
        first = self.fresh()
        self.foot()
        self.command('desktop-select 2')
        second = self.state()['selected']
        self.foot()
        self.command('desktop-select 1')
        self.eval('hl.config({plugin={cascade={animate=true}},gestures={workspace_swipe_distance=200},general={gaps_workspaces=0}})')
        self.addCleanup(lambda: self.eval('hl.config({plugin={cascade={animate=false}},animations={enabled=false}}); hl.animation({leaf="workspaces",enabled=false,speed=5,bezier="default",style="slide"})'))
        self.addCleanup(lambda: self.eval('hl.plugin.cascade_test.finish(true)'))
        return first, second

    def native_workspace_x(self, desktop):
        return float(self.hc('repl', f'hl.plugin.cascade_test.workspace({desktop}).x'))

    def test_36_swipe_reversal_responds_after_multiple_desktops(self):
        first, second = self.prepare_horizontal_swipe()
        state = self.state()
        third = next(group['desktops'][-1] for group in state['groups'] if group['id'] == state['activeGroup'])
        width, _ = self.dimensions()
        self.eval('assert(hl.plugin.cascade_test.begin(3)); hl.plugin.cascade_test.update(3,-400,0)')
        self.wait(lambda: self.state()['selected'] == third)
        self.assertEqual(json.loads(self.hc('activeworkspace', '-j'))['id'], third)
        self.eval('hl.plugin.cascade_test.update(3,20,0)')
        self.wait(lambda: abs(self.native_workspace_x(third) - width * 20 / 200) < 1)
        self.eval(f'assert(hl.plugin.cascade_test.workspace({second}).x<0, "The previous desktop must enter from the left")')
        self.eval('hl.plugin.cascade_test.update(3,-40,0)')
        self.wait(lambda: abs(self.native_workspace_x(third) + width * 20 / 200) < 1)
        self.eval(f'assert(hl.plugin.cascade_test.workspace({first}).x>0, "Reversing across zero must wrap in from the right")')
        self.eval('hl.plugin.cascade_test.update(3,40,0); hl.plugin.cascade_test.update(3,-40,0)')
        self.wait(lambda: abs(self.native_workspace_x(third) + width * 20 / 200) < 1)
        self.eval('hl.plugin.cascade_test.finish(true)')
        self.wait(lambda: abs(self.native_workspace_x(third)) < 1)
        self.assertEqual(self.state()['selected'], third, 'Cancel must preserve completed desktop segments')

    def test_37_new_swipe_continues_an_unfinished_transition(self):
        first, second = self.prepare_horizontal_swipe()
        width, _ = self.dimensions()
        self.eval('hl.config({animations={enabled=true}}); hl.curve("qaSlowSwipe",{type="bezier",points={{0,0},{1,1}}}); hl.animation({leaf="workspaces",enabled=true,speed=20,bezier="qaSlowSwipe",style="slide"})')
        self.eval(f'''assert(hl.plugin.cascade_test.begin(3)); hl.plugin.cascade_test.update(3,-80,0); hl.plugin.cascade_test.finish(false)
assert(hl.plugin.cascade_test.workspace({second}).x>100)
assert(hl.plugin.cascade_test.begin(3))''')
        time.sleep(.04)
        self.eval(f'''
local before=hl.plugin.cascade_test.workspace({second}).x
assert(before>100, "The previous transition must still be in progress")
hl.plugin.cascade_test.update(3,-12,0)
local after=hl.plugin.cascade_test.workspace({second}).x
assert(math.abs(after-(before-{width}*12/200))<1, "A new gesture must start at the visible position, not at zero")
hl.plugin.cascade_test.finish(false)''')
        self.assertEqual(json.loads(self.hc('activeworkspace', '-j'))['id'], second)

    def test_38_swipe_respects_the_configured_completion_curve(self):
        _, second = self.prepare_horizontal_swipe()
        self.eval('hl.curve("qaCustomSwipe",{type="bezier",points={{0.23,1},{0.32,1}}}); hl.animation({leaf="workspaces",enabled=true,speed=6,bezier="qaCustomSwipe",style="slide"})')
        self.eval('assert(hl.plugin.cascade_test.swipe(3,-100,0,false))')
        self.eval(f'assert(hl.plugin.cascade_test.workspace({second}).bezier=="qaCustomSwipe", "A workspace change must preserve the configured easing")')

    def test_39_swipe_input_bursts_are_applied_once_per_frame(self):
        first, _ = self.prepare_horizontal_swipe()
        width, _ = self.dimensions()
        self.eval(f'''assert(hl.plugin.cascade_test.begin(3)); hl.plugin.cascade_test.update(3,-20,0)
local presented=hl.plugin.cascade_test.workspace({first}).x
for i=1,20 do hl.plugin.cascade_test.update(3,-1,0) end
assert(hl.plugin.cascade_test.workspace({first}).x==presented, "Input bursts must queue the latest position instead of rebuilding geometry for every sample")''')
        self.wait(lambda: abs(self.native_workspace_x(first) + width * 40 / 200) < 1)

    def test_40_overview_reversal_has_no_saturation_dead_zone(self):
        first, _ = self.prepare_horizontal_swipe()
        self.eval('hl.config({plugin={cascade={animate=false}}})')
        self.api('open')
        self.eval('assert(hl.plugin.cascade_test.begin(3)); hl.plugin.cascade_test.update(3,1000,0)')
        time.sleep(.08)
        before = self.hit(0, desktop=first)['x']
        self.eval('hl.plugin.cascade_test.update(3,-30,0)')
        self.wait(lambda: self.hit(0, desktop=first)['x'] < before - 10)

    def test_41_held_swipe_crosses_multiple_desktops_and_reverses(self):
        first, second = self.prepare_horizontal_swipe()
        state = self.state()
        group_id = state['activeGroup']
        desktops = next(group['desktops'] for group in state['groups'] if group['id'] == group_id)
        self.assertEqual(desktops[:2], [first, second])
        self.assertEqual(len(desktops), 3)
        third = desktops[2]
        width, _ = self.dimensions()
        self.eval('assert(hl.plugin.cascade_test.begin(3)); hl.plugin.cascade_test.update(3,-450,0)')
        self.wait(lambda: self.state()['selected'] == third)
        self.wait(lambda: abs(self.native_workspace_x(third) + width * 50 / 200) < 1)
        self.eval(f'''local incoming=hl.plugin.cascade_test.workspace({first})
assert(incoming.x>0 and incoming.forced, "The remainder must keep dragging the next desktop after two complete segments")
assert(not hl.plugin.cascade_test.workspace({second}).forced, "A completed segment must release the old desktop")''')
        self.eval('hl.plugin.cascade_test.update(3,100,0)')
        self.wait(lambda: abs(self.native_workspace_x(third) - width * 50 / 200) < 1)
        self.eval(f'''assert(hl.plugin.cascade_test.workspace({second}).x<0, "A reversal must immediately bring back the previous desktop")
hl.plugin.cascade_test.finish(false)
local outgoing=hl.plugin.cascade_test.workspace({third})
assert(not outgoing or outgoing.goalX>0, "Release must keep moving right; an empty outgoing workspace may already be destroyed")''')
        self.assertEqual(self.state()['selected'], second)
        self.assertEqual(self.state()['activeGroup'], group_id)
        self.assertEqual(json.loads(self.hc('activeworkspace', '-j'))['id'], second)
        self.eval(f'''for _,id in ipairs({{{','.join(map(str, desktops))}}}) do
local ws=hl.plugin.cascade_test.workspace(id)
assert(not ws or not ws.forced, "Finishing a multi-desktop gesture must release every desktop")
end''')

    def test_42_large_samples_preserve_overflow_across_circular_reordered_desktops(self):
        first, second = self.prepare_horizontal_swipe()
        self.command('desktop-select 2')
        self.command('desktop-left')
        state = self.state()
        group_id = state['activeGroup']
        desktops = next(group['desktops'] for group in state['groups'] if group['id'] == group_id)
        self.assertEqual(desktops[:2], [second, first])
        count = len(desktops)
        steps = count * 2 + 2
        distance = steps * 200 + 30
        width, _ = self.dimensions()
        self.wait(lambda: abs(self.native_workspace_x(second)) < 1)
        forward = desktops[steps % count]
        self.eval(f'assert(hl.plugin.cascade_test.begin(3)); hl.plugin.cascade_test.update(3,{-distance},0)')
        self.wait(lambda: self.state()['selected'] == forward)
        self.wait(lambda: abs(self.native_workspace_x(forward) + width * 30 / 200) < 1)
        self.eval('hl.plugin.cascade_test.finish(true)')
        self.wait(lambda: abs(self.native_workspace_x(forward)) < 1)
        self.assertEqual(self.state()['selected'], forward, 'Cancel keeps every complete turn and discards only the remainder')
        self.eval(f'assert(hl.plugin.cascade_test.begin(3)); hl.plugin.cascade_test.update(3,{distance},0)')
        self.wait(lambda: self.state()['selected'] == second)
        self.wait(lambda: abs(self.native_workspace_x(second) - width * 30 / 200) < 1)
        self.eval('hl.plugin.cascade_test.finish(false)')
        self.assertEqual(self.state()['selected'], second, 'A remainder below 45 must not commit an additional desktop')
        state = self.state()
        self.assertEqual(state['activeGroup'], group_id)
        self.assertEqual(next(group['desktops'] for group in state['groups'] if group['id'] == group_id), desktops)

    def test_43_continuous_swipe_handles_one_two_desktops_and_reverse_setting(self):
        driver = os.environ['CASCADE_QA_GESTURE_DRIVER']
        self.hc('plugin', 'load', driver)
        self.addCleanup(lambda: self.hc('plugin', 'unload', driver))
        first = self.fresh()
        self.eval('hl.config({plugin={cascade={animate=true,reverseSwipe=false}},gestures={workspace_swipe_distance=200},general={gaps_workspaces=0}})')
        self.addCleanup(lambda: self.eval('hl.config({plugin={cascade={animate=false,reverseSwipe=false}}})'))
        self.addCleanup(lambda: self.eval('hl.plugin.cascade_test.finish(true)'))
        state = self.state()
        group_id = state['activeGroup']
        self.assertEqual(next(group['desktops'] for group in state['groups'] if group['id'] == group_id), [first])
        self.eval(f'''assert(hl.plugin.cascade_test.begin(3))
hl.plugin.cascade_test.update(3,-1500,0); hl.plugin.cascade_test.update(3,3000,0)
hl.plugin.cascade_test.finish(false)
local ws=hl.plugin.cascade_test.workspace({first})
assert(ws.x==0 and not ws.forced, "A single desktop must not slide against itself")''')
        self.assertEqual(self.state()['selected'], first)
        self.foot()
        self.wait(lambda: len(self.group(group_id)['desktops']) == 2)
        second = self.group(group_id)['desktops'][-1]
        self.command('desktop-select 1')
        width, _ = self.dimensions()
        self.eval('assert(hl.plugin.cascade_test.begin(3)); hl.plugin.cascade_test.update(3,-220,0)')
        self.wait(lambda: self.state()['selected'] == second)
        self.wait(lambda: abs(self.native_workspace_x(second) + width * 20 / 200) < 1)
        self.eval('hl.plugin.cascade_test.update(3,40,0)')
        self.wait(lambda: abs(self.native_workspace_x(second) - width * 20 / 200) < 1)
        self.eval(f'''assert(hl.plugin.cascade_test.workspace({first}).x<0, "With two desktops the same neighbour must move to the other side on reversal")
hl.plugin.cascade_test.finish(true)''')
        self.wait(lambda: abs(self.native_workspace_x(second)) < 1)
        self.eval('hl.config({plugin={cascade={reverseSwipe=true}}}); assert(hl.plugin.cascade_test.begin(3)); hl.plugin.cascade_test.update(3,-670,0)')
        self.wait(lambda: self.state()['selected'] == first)
        self.wait(lambda: abs(self.native_workspace_x(first) - width * 70 / 200) < 1)
        self.eval('hl.plugin.cascade_test.finish(true)')
        self.assertEqual(self.state()['selected'], first)
        state = self.state()
        self.assertEqual(next(group['desktops'] for group in state['groups'] if group['id'] == group_id), [first, second])

    def test_44_continuous_swipe_preserves_fullscreen_and_cleans_cancel_and_unload(self):
        first, second = self.prepare_horizontal_swipe()
        self.command('desktop-select 2')
        window = next(client for client in self.clients() if client['workspace']['id'] == second)
        self.hc('dispatch', 'hl.dsp.window.fullscreen({action="toggle"})')
        self.assertEqual(self.client(window['address'])['fullscreen'], 2)
        self.command('desktop-select 1')
        self.eval(f'hl.monitor({{output={json.dumps(MONITOR)},mode="1280x800@60",position="0x0",scale=1.25}})')
        self.addCleanup(lambda: self.eval(f'hl.monitor({{output={json.dumps(MONITOR)},mode="1280x800@60",position="0x0",scale=1}})'))
        state = self.state()
        desktops = next(group['desktops'] for group in state['groups'] if group['id'] == state['activeGroup'])
        third = desktops[2]
        width, _ = self.dimensions()
        self.eval('assert(hl.plugin.cascade_test.begin(3)); hl.plugin.cascade_test.update(3,-240,0)')
        self.wait(lambda: self.state()['selected'] == second)
        self.wait(lambda: abs(self.native_workspace_x(second) + width * 40 / 200) < 1)
        monitor = next(m for m in json.loads(self.hc('monitors', '-j')) if m['name'] == MONITOR)
        self.assertIn('OFFSET', monitor['solitaryBlockedBy'], 'A fullscreen desktop must stay composed after crossing a segment boundary')
        self.eval('hl.plugin.cascade_test.finish(true)')
        self.wait(lambda: abs(self.native_workspace_x(second)) < 1)
        self.assertEqual(self.state()['selected'], second, 'Cancel must preserve the completed first segment')
        self.assertEqual(self.client(window['address'])['fullscreen'], 2)
        self.eval(f'''for _,id in ipairs({{{','.join(map(str, desktops))}}}) do
local ws=hl.plugin.cascade_test.workspace(id)
assert(not ws or not ws.forced, "Cancel must clear every rendering override")
end''')
        self.eval('assert(hl.plugin.cascade_test.begin(3)); hl.plugin.cascade_test.update(3,-240,0)')
        self.wait(lambda: self.state()['selected'] == third)
        self.wait(lambda: abs(self.native_workspace_x(third) + width * 40 / 200) < 1)
        plugin = os.environ['CASCADE_QA_PLUGIN']
        self.hc('plugin', 'unload', plugin)
        try:
            self.assertEqual(json.loads(self.hc('activeworkspace', '-j'))['id'], third)
            self.eval(f'''local current=hl.plugin.cascade_test.workspace({third})
assert(current.x==0 and current.alpha==1, "Unload must restore the last completed desktop")
for _,id in ipairs({{{','.join(map(str, desktops))}}}) do
local ws=hl.plugin.cascade_test.workspace(id)
assert(not ws or not ws.forced, "Unload must clear all old and current rendering overrides")
end''')
            self.assertEqual(self.client(window['address'])['fullscreen'], 2)
        finally:
            self.hc('plugin', 'load', plugin)
        self.assertEqual(self.state()['selected'], third)

    def test_45_repeated_manual_creation_and_n_keep_one_trailing_empty(self):
        self.fresh()
        group_id = self.state()['activeGroup']
        for _ in range(4):
            previous = self.state()['selected']
            self.command('desktop-new')
            selected = self.state()['selected']
            self.assertNotEqual(selected, previous)
            self.wait(lambda: self.group(group_id)['desktops'] == [selected])
            self.assertEqual(json.loads(self.hc('activeworkspace', '-j'))['id'], selected)
        occupied = self.state()['selected']
        window = self.foot()
        self.wait(lambda: len(self.group(group_id)['desktops']) == 2)
        self.api('open')
        for _ in range(4):
            previous = self.group(group_id)['desktops'][-1]
            self.keys('-k', 'n')
            selected = self.state()['selected']
            self.assertNotEqual(selected, previous)
            self.wait(lambda: self.group(group_id)['desktops'] == [occupied, selected])
            self.assertEqual(json.loads(self.hc('activeworkspace', '-j'))['id'], occupied)
            self.assertEqual(self.client(window['address'])['workspace']['id'], occupied)
        self.keys('-k', 'Return')
        self.assertFalse(self.state()['active'])
        self.assertEqual(json.loads(self.hc('activeworkspace', '-j'))['id'], selected)
        self.assertEqual(self.group(group_id)['desktops'], [occupied, selected])

    def test_46_last_window_close_prunes_active_desktop_and_cancels_held_swipe(self):
        first, second = self.prepare_horizontal_swipe()
        group_id = self.state()['activeGroup']
        reserve = self.group(group_id)['desktops'][-1]
        window = next(client for client in self.clients() if client['workspace']['id'] == second)
        self.command('desktop-select 2')
        other = self.foot()
        self.eval('assert(hl.plugin.cascade_test.begin(3)); hl.plugin.cascade_test.update(3,-20,0)')
        self.close_window(window)
        self.assertEqual(self.group(group_id)['desktops'], [first, second, reserve], 'A desktop with another mapped window must remain')
        self.assertEqual(self.state()['selected'], second)
        self.close_window(other)
        self.wait(lambda: self.group(group_id)['desktops'] == [first, reserve])
        self.wait(lambda: json.loads(self.hc('activeworkspace', '-j'))['id'] == reserve)
        self.wait(lambda: abs(self.native_workspace_x(reserve)) < 1)
        self.assertEqual(self.state()['selected'], reserve)
        self.eval(f'''hl.plugin.cascade_test.finish(false)
local current=hl.plugin.cascade_test.workspace({reserve})
assert(current.x==0 and current.alpha==1 and not current.forced, "Closing a dragged empty desktop must leave the surviving reserve centered")
for _,id in ipairs({{{first},{second},{reserve}}}) do
local ws=hl.plugin.cascade_test.workspace(id)
assert(not ws or not ws.forced, "Pruning a swipe source must clear all rendering overrides")
end''')
        self.assertEqual(json.loads(self.hc('activeworkspace', '-j'))['id'], reserve)
        self.assertTrue(any(client['workspace']['id'] == first for client in self.clients()))

    def test_47_moving_the_last_window_prunes_the_source_and_preserves_windows(self):
        first, second = self.prepare_horizontal_swipe()
        group_id = self.state()['activeGroup']
        reserve = self.group(group_id)['desktops'][-1]
        original = next(client for client in self.clients() if client['workspace']['id'] == first)
        moved = next(client for client in self.clients() if client['workspace']['id'] == second)
        self.command('desktop-select 2')
        selector = json.dumps('address:' + moved['address'])
        self.hc('dispatch', f'hl.dsp.window.move({{workspace={first},window={selector},follow=false}})')
        self.wait(lambda: self.client(moved['address'])['workspace']['id'] == first)
        self.wait(lambda: self.group(group_id)['desktops'] == [first, reserve])
        self.wait(lambda: json.loads(self.hc('activeworkspace', '-j'))['id'] == reserve)
        self.assertEqual(self.state()['selected'], reserve)
        self.assertEqual(self.client(original['address'])['workspace']['id'], first)
        self.assertEqual(self.client(moved['address'])['workspace']['id'], first)

    def test_48_interior_empty_survives_reload_until_the_trailing_run_reaches_it(self):
        first, second = self.prepare_horizontal_swipe()
        group_id = self.state()['activeGroup']
        middle_window = next(client for client in self.clients() if client['workspace']['id'] == second)
        self.command('desktop-select 3')
        third = self.state()['selected']
        last_window = self.foot()
        self.wait(lambda: len(self.group(group_id)['desktops']) == 4)
        reserve = self.group(group_id)['desktops'][-1]
        self.close_window(middle_window)
        self.assertEqual(self.group(group_id)['desktops'], [first, second, third, reserve], 'An occupied desktop must separate the interior empty desktop from the reserve')
        self.assertEqual(self.state()['selected'], third)
        plugin = os.environ['CASCADE_QA_PLUGIN']
        self.hc('plugin', 'unload', plugin)
        self.hc('plugin', 'load', plugin)
        self.assertEqual(self.group(group_id)['desktops'], [first, second, third, reserve])
        self.assertEqual(self.state()['selected'], third)
        self.close_window(last_window)
        self.wait(lambda: self.group(group_id)['desktops'] == [first, reserve])
        self.wait(lambda: json.loads(self.hc('activeworkspace', '-j'))['id'] == reserve)
        self.assertEqual(self.state()['selected'], reserve)
        self.hc('plugin', 'unload', plugin)
        self.hc('plugin', 'load', plugin)
        self.assertEqual(self.group(group_id)['desktops'], [first, reserve], 'Reload must not resurrect any pruned empty desktop')
        self.assertEqual(self.state()['selected'], reserve)

    def test_49_pruning_native_desktop_preserves_another_group_overview_selection(self):
        first, second = self.prepare_horizontal_swipe()
        source_group = self.state()['activeGroup']
        reserve = self.group(source_group)['desktops'][-1]
        closing = next(client for client in self.clients() if client['workspace']['id'] == second)
        self.command('desktop-select 2')
        self.command('group-new Keep preview selection')
        preview_group = self.state()['activeGroup']
        preview_window = self.foot()
        self.command('desktop-select 2')
        preview = self.state()['selected']
        self.api('navigate', 'up')
        self.assertEqual(json.loads(self.hc('activeworkspace', '-j'))['id'], second)
        self.api('open')
        self.addCleanup(lambda: self.api('close', 'all'))
        self.api('navigate', 'down')
        self.assertEqual(self.state()['activeGroup'], preview_group)
        self.assertEqual(self.state()['selected'], preview)
        self.close_window(closing)
        self.wait(lambda: self.group(source_group)['desktops'] == [first, reserve])
        self.wait(lambda: json.loads(self.hc('activeworkspace', '-j'))['id'] == reserve)
        state = self.state()
        self.assertTrue(state['active'], 'Pruning must keep the overview open')
        self.assertEqual(state['activeGroup'], preview_group, 'Native focus repair must stay inside the source group without switching the preview group')
        self.assertEqual(state['selected'], preview)
        self.assertEqual(self.group(source_group)['selected'], reserve)
        self.assertEqual(self.client(preview_window['address'])['workspace']['id'], self.group(preview_group)['desktops'][0])
        self.wait(lambda: not any(hit['desktop'] == second for hit in self.state()['hits']))

    def test_50_background_pruning_preserves_other_monitor_focus_and_overview(self):
        first, second = self.prepare_horizontal_swipe()
        source_group = self.state()['activeGroup']
        reserve = self.group(source_group)['desktops'][-1]
        closing = next(client for client in self.clients() if client['workspace']['id'] == second)
        self.command('desktop-select 2')
        other_monitor = 'CASCADE-QA-PRUNE-B'
        self.hc('output', 'create', 'headless', other_monitor)
        self.addCleanup(lambda: self.hc('output', 'remove', other_monitor))
        self.addCleanup(lambda: self.hc('dispatch', f'hl.dsp.focus({{monitor={json.dumps(MONITOR)}}})'))
        self.eval(f'hl.monitor({{output={json.dumps(other_monitor)},mode="1280x800@60",position="1280x0",scale=1}})')
        self.hc('dispatch', f'hl.dsp.focus({{monitor={json.dumps(other_monitor)}}})')
        self.wait(lambda: self.state()['monitor'] != next(m['id'] for m in json.loads(self.hc('monitors', '-j')) if m['name'] == MONITOR))
        native_other = self.fresh()
        focused_window = self.foot()
        self.command('group-new Background prune preview')
        preview_group = self.state()['activeGroup']
        self.foot()
        self.command('desktop-select 2')
        preview = self.state()['selected']
        self.api('navigate', 'up')
        self.assertEqual(json.loads(self.hc('activeworkspace', '-j'))['id'], native_other)
        self.api('open')
        self.addCleanup(lambda: self.api('close', 'all'))
        self.api('navigate', 'down')
        before = self.state()
        self.assertEqual(before['activeGroup'], preview_group)
        self.assertEqual(before['selected'], preview)
        self.assertEqual(json.loads(self.hc('activewindow', '-j'))['address'], focused_window['address'])
        self.close_window(closing)
        def source_repaired():
            monitor = next(m for m in json.loads(self.hc('monitors', '-j')) if m['name'] == MONITOR)
            return monitor['activeWorkspace']['id'] == reserve
        self.wait(source_repaired)
        monitors = json.loads(self.hc('monitors', '-j'))
        self.assertEqual(next(m['name'] for m in monitors if m['focused']), other_monitor, 'Repairing an inactive monitor must preserve the focused monitor')
        self.assertEqual(next(m['activeWorkspace']['id'] for m in monitors if m['name'] == other_monitor), native_other)
        self.assertEqual(json.loads(self.hc('activewindow', '-j')).get('address'), focused_window['address'], 'Repairing an inactive monitor must not clear or steal the active window')
        self.hc('dispatch', f'hl.dsp.focus({{monitor={json.dumps(other_monitor)}}})')
        after = self.state()
        self.assertEqual(after['monitor'], before['monitor'])
        self.assertTrue(after['active'], 'Repairing a background monitor must keep the other overview open')
        self.assertEqual(after['activeGroup'], preview_group, 'A workspace change on another monitor must preserve the preview group')
        self.assertEqual(after['selected'], preview, 'A workspace change on another monitor must preserve the preview desktop')
        self.assertEqual(after['groups'], before['groups'])
        self.assertTrue(any(client['workspace']['id'] == first for client in self.clients()))


if __name__ == '__main__':
    unittest.main(verbosity=2)
