#!/usr/bin/env python3
"""Adds Diablo Splitscreen to Steam as a non-Steam game (Steam Deck Game Mode).

Steam keeps its shortcuts in userdata/<user>/config/shortcuts.vdf, a binary key/value
file, and rewrites that file when it exits. An edit made while Steam runs is therefore
lost or, worse, merged with a stale copy: this tool refuses to write unless Steam is
closed, checks again right before the atomic replace, and keeps the first original as
shortcuts.vdf.diablo-splitscreen-backup. A file it cannot parse and write back byte for
byte is never touched.

usage: steam-shortcut.py add <name> <exe> <start dir> <icon>
       steam-shortcut.py status <exe>        prints missing / present / no-steam
"""
import glob
import os
import struct
import subprocess
import sys
import tempfile
import zlib

MAP, STRING, INT32, FLOAT32, UINT64, END = 0, 1, 2, 3, 7, 8
FIXED = {INT32: 4, FLOAT32: 4, UINT64: 8}
BACKUP_SUFFIX = '.diablo-splitscreen-backup'


def read_cstring(data, pos):
    end = data.index(b'\0', pos)
    return data[pos:end], end + 1


def parse_map(data, pos):
    items = []
    while True:
        if pos >= len(data):
            raise ValueError('unterminated map')
        kind = data[pos]
        pos += 1
        if kind == END:
            return items, pos
        key, pos = read_cstring(data, pos)
        if kind == MAP:
            value, pos = parse_map(data, pos)
        elif kind == STRING:
            value, pos = read_cstring(data, pos)
        elif kind in FIXED:
            size = FIXED[kind]
            if pos + size > len(data):
                raise ValueError('truncated number')
            value, pos = data[pos:pos + size], pos + size
        else:
            raise ValueError('unknown field type %d' % kind)
        items.append([kind, key, value])


def write_map(items):
    out = bytearray()
    for kind, key, value in items:
        out += bytes([kind]) + key + b'\0'
        if kind == MAP:
            out += write_map(value)
        elif kind == STRING:
            out += value + b'\0'
        else:
            out += value
    out.append(END)
    return bytes(out)


def load(data):
    items, end = parse_map(data, 0)
    if end != len(data) or len(items) != 1 or items[0][:2] != [MAP, b'shortcuts']:
        raise ValueError('not a shortcuts.vdf')
    if write_map(items) != data:
        raise ValueError('shortcuts.vdf does not round-trip; not editing it')
    return items


def field(entry, name):
    for item in entry:
        if item[1].lower() == name.lower():
            return item
    return None


def find(items, exe):
    found = []
    for entry in items[0][2]:
        if entry[0] != MAP:
            raise ValueError('unexpected shortcut entry')
        target = field(entry[2], b'exe')
        if target and target[0] == STRING and target[2].strip(b'"') == exe.encode():
            found.append(entry)
    return found


def new_entry(index, name, exe, start_dir, icon):
    appid = (zlib.crc32((exe + name).encode()) & 0xFFFFFFFF) | 0x80000000
    string = lambda key, value: [STRING, key, value.encode()]
    return [MAP, str(index).encode(), [
        [INT32, b'appid', struct.pack('<I', appid)],
        string(b'AppName', name),
        string(b'Exe', '"%s"' % exe),
        string(b'StartDir', '"%s"' % start_dir),
        string(b'icon', icon),
        string(b'LaunchOptions', ''),
        [INT32, b'AllowOverlay', struct.pack('<I', 1)],
        [MAP, b'tags', []],
    ]]


def steam_closed():
    # pgrep exits 1 for "no such process"; anything else (running, pgrep missing) is
    # not proof that Steam will leave the file alone.
    try:
        code = subprocess.run(['pgrep', '-x', 'steam'], stdout=subprocess.DEVNULL, check=False).returncode
    except OSError:
        code = 2
    if code != 1:
        raise RuntimeError('Steam is running (or could not be checked); close it first')


def config_dirs():
    roots = [os.path.expanduser('~/.steam/steam'), os.path.expanduser('~/.local/share/Steam')]
    seen, dirs = set(), []
    for root in roots:
        for config in sorted(glob.glob(os.path.join(root, 'userdata', '*', 'config'))):
            real = os.path.realpath(config)
            if real not in seen:
                seen.add(real)
                dirs.append(config)
    return dirs


def replace(path, data):
    backup = path + BACKUP_SUFFIX
    if os.path.exists(path) and not os.path.exists(backup):
        with open(path, 'rb') as src, open(backup, 'xb') as dst:
            dst.write(src.read())
    fd, temporary = tempfile.mkstemp(dir=os.path.dirname(path), prefix='.shortcuts-')
    try:
        with os.fdopen(fd, 'wb') as out:
            out.write(data)
            out.flush()
            os.fsync(out.fileno())
        if os.path.exists(path):
            os.chmod(temporary, os.stat(path).st_mode & 0o777)
        steam_closed()
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def plan(config, name, exe, start_dir, icon):
    path = os.path.join(config, 'shortcuts.vdf')
    data = b'\0shortcuts\0\x08\x08'
    if os.path.exists(path):
        with open(path, 'rb') as src:
            data = src.read()
    items = load(data)
    found = find(items, exe)
    if len(found) > 1:
        raise ValueError('several shortcuts already point at %s' % exe)
    if found:
        return path, None
    indices = [int(entry[1]) for entry in items[0][2] if entry[1].isdigit()]
    items[0][2].append(new_entry(max(indices, default=-1) + 1, name, exe, start_dir, icon))
    return path, write_map(items)


def main(args):
    if len(args) == 2 and args[0] == 'status':
        configs = config_dirs()
        if not configs:
            print('no-steam')
            return 0
        present = all(plan(c, '', args[1], '', '')[1] is None for c in configs)
        print('present' if present else 'missing')
        return 0
    if len(args) != 5 or args[0] != 'add':
        print(__doc__, file=sys.stderr)
        return 2
    _, name, exe, start_dir, icon = args
    configs = config_dirs()
    if not configs:
        raise RuntimeError('no Steam user found')
    # Parse every user's file before writing any, so one bad file changes nothing.
    plans = [plan(c, name, exe, start_dir, icon) for c in configs]
    steam_closed()
    for path, data in plans:
        if data is None:
            print('already present', path)
        else:
            replace(path, data)
            print('added', path)
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main(sys.argv[1:]))
    except (OSError, ValueError, RuntimeError) as error:
        print('steam-shortcut: %s' % error, file=sys.stderr)
        sys.exit(1)
