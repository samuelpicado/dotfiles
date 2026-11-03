#!/usr/bin/env python3
"""Send virtual-pointer events only to the isolated Cascade QA socket."""
import argparse
import os
import socket
import struct
import time
from pathlib import Path

ROOT = Path(os.environ.get('CASCADE_QA_ROOT', '/tmp/cascade-qa')).resolve()
if not ROOT.is_relative_to('/tmp'):
    raise RuntimeError('Virtual input requires an isolated compositor under /tmp')
RUNTIME = Path(os.environ.get('CASCADE_QA_RUNTIME', ROOT / 'runtime')).resolve()
if not RUNTIME.is_relative_to(ROOT):
    raise RuntimeError('Virtual input must use the isolated QA runtime')
DISPLAY = 'wayland-1'


def u32(*values):
    return struct.pack('=' + 'I' * len(values), *values)


def string(value):
    data = value.encode() + b'\0'
    return u32(len(data)) + data + b'\0' * (-len(data) % 4)


class Pointer:
    def __init__(self):
        self.connection = socket.socket(socket.AF_UNIX)
        self.connection.settimeout(3)
        self.connection.connect(str(RUNTIME / DISPLAY))
        self.buffer = b''
        self.global_id = None
        self.send(1, 1, u32(2))  # wl_display.get_registry
        self.send(1, 0, u32(3))  # wl_display.sync
        self.roundtrip(3)
        if self.global_id is None:
            raise RuntimeError('QA compositor does not expose a virtual pointer')
        self.send(2, 0, u32(self.global_id) + string('zwlr_virtual_pointer_manager_v1') + u32(1, 4))
        self.send(4, 0, u32(0, 5))  # manager.create_virtual_pointer(seat=null, id=5)
        self.send(1, 0, u32(6))
        self.roundtrip(6)

    def send(self, resource, opcode, data=b''):
        self.connection.sendall(u32(resource, ((len(data) + 8) << 16) | opcode) + data)

    def roundtrip(self, callback):
        while True:
            while len(self.buffer) < 8:
                chunk = self.connection.recv(65536)
                if not chunk:
                    raise RuntimeError('QA compositor closed the Wayland connection')
                self.buffer += chunk
            resource, info = struct.unpack('=II', self.buffer[:8])
            size, opcode = info >> 16, info & 65535
            while len(self.buffer) < size:
                chunk = self.connection.recv(65536)
                if not chunk:
                    raise RuntimeError('QA compositor closed the Wayland connection')
                self.buffer += chunk
            data, self.buffer = self.buffer[8:size], self.buffer[size:]
            if resource == 1 and opcode == 0:
                raise RuntimeError('Wayland protocol error: ' + repr(data))
            if resource == 2 and opcode == 0:
                global_id, length = struct.unpack('=II', data[:8])
                interface = data[8:8 + length - 1].decode()
                if interface == 'zwlr_virtual_pointer_manager_v1':
                    self.global_id = global_id
            if resource == callback and opcode == 0:
                return

    def frame(self):
        self.send(5, 4)

    def move(self, x, y, width, height):
        self.send(5, 1, u32(int(time.monotonic() * 1000) & 0xffffffff, x, y, width, height))
        self.frame()

    def button(self, pressed):
        self.send(5, 2, u32(int(time.monotonic() * 1000) & 0xffffffff, 272, int(pressed)))
        self.frame()

    def finish(self):
        self.send(1, 0, u32(7))
        self.roundtrip(7)
        self.connection.close()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('action', choices=('move', 'click', 'drag'))
    parser.add_argument('coordinates', type=int, nargs='+')
    parser.add_argument('--width', type=int, default=1280)
    parser.add_argument('--height', type=int, default=800)
    args = parser.parse_args()
    expected = 4 if args.action == 'drag' else 2
    if len(args.coordinates) != expected:
        parser.error(f'{args.action} requires {expected} coordinates')
    pointer = Pointer()
    x, y = args.coordinates[:2]
    pointer.move(x, y, args.width, args.height)
    time.sleep(.08)
    if args.action == 'click':
        pointer.button(True)
        time.sleep(.08)
        pointer.button(False)
    elif args.action == 'drag':
        pointer.button(True)
        time.sleep(.1)
        end_x, end_y = args.coordinates[2:]
        for step in range(1, 16):
            pointer.move(round(x + (end_x - x) * step / 15), round(y + (end_y - y) * step / 15), args.width, args.height)
            time.sleep(.02)
        pointer.button(False)
    pointer.finish()


if __name__ == '__main__':
    main()
