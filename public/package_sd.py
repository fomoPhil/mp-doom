#!/usr/bin/env python3
"""Create a credential-free MPC DOOM SD folder from verified local game data."""
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess

SALT = b'f7bed1c9-7e90-4731-9194-c996666b4331'
PROFILES = {
    'shareware': (4196020, '1d7d43be501e67d927e415e0b8f3e29c3bf33075e859721816f652a526cac771'),
    'ultimate-doom': (12408292, '6fdf361847b46228cfebd9f3af09cd844282ac75f3edbb61ca4cb27103ce2e7f'),
}


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda: f.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def regular(path):
    if path.is_symlink() or not path.is_file():
        raise ValueError(f'Expected a regular, non-symlink file: {path}')


def engine_digest(path):
    regular(path)
    data = path.read_bytes()
    try:
        if data[:6] != b'\x7fELF\x02\x01' or struct.unpack_from('<H', data, 18)[0] != 183:
            raise ValueError('Engine must be a 64-bit little-endian ARM executable')
        offset = struct.unpack_from('<Q', data, 32)[0]
        size, count = struct.unpack_from('<HH', data, 54)
        if size < 56 or not count or offset + size * count > len(data):
            raise ValueError('Engine ELF headers are invalid')
        if any(struct.unpack_from('<I', data, offset + i * size)[0] in (2, 3) for i in range(count)):
            raise ValueError('Engine must be statically linked')
    except struct.error as exc:
        raise ValueError('Engine ELF headers are invalid') from exc
    return hashlib.sha256(data).hexdigest()


def stage_autorun(engine: Path, profile: str, folder: Path, assets: Path) -> None:
    """Stage the exact MPC payload, excluding game data, in a new folder."""
    if profile not in PROFILES:
        raise ValueError('Unsupported game profile')
    engine_hash = engine_digest(engine)
    _, wad_hash = PROFILES[profile]
    names = ('autorun.sh.in', 'launch-doom.sh.in', 'ENGINE-LICENSE.txt', 'MUSIC-NOTICES.txt')
    for name in names:
        regular(assets / name)
    launcher = (assets / 'launch-doom.sh.in').read_text()
    if launcher.count('@PROFILE@') != 1:
        raise ValueError('Invalid launcher template')
    launcher = launcher.replace('@PROFILE@', profile).encode()
    template = (assets / 'autorun.sh.in').read_text()
    for token, value in (('@ENGINE_SHA256@', engine_hash), ('@WAD_SHA256@', wad_hash),
                         ('@LAUNCH_SHA256@', hashlib.sha256(launcher).hexdigest())):
        if template.count(token) != 1:
            raise ValueError('Invalid autorun template')
        template = template.replace(token, value)
    script = template.encode()
    manifest = {'value0': {'scripts': [{'path': 'autorun.sh', 'askQuestion': False,
        'message': 'Start temporary DOOM?', 'yesButtonText': 'Play', 'noButtonText': 'Cancel'}], 'files': []}}
    raw = (json.dumps(manifest, indent=2) + '\n').encode()
    folder.mkdir()
    for src, name in ((engine, 'mpc-doom'),
                      (assets / 'ENGINE-LICENSE.txt', 'ENGINE-LICENSE.txt'),
                      (assets / 'MUSIC-NOTICES.txt', 'MUSIC-NOTICES.txt')):
        shutil.copyfile(src, folder / name)
    if digest(folder / 'mpc-doom') != engine_hash:
        raise ValueError('Package readback failed')
    for name, contents in (('launch-doom.sh', launcher), ('autorun.sh', script), ('autorun.json', raw),
                           ('autorun.check', hashlib.sha256(raw + script + SALT).hexdigest().encode())):
        (folder / name).write_bytes(contents)
    for name in ('launch-doom.sh', 'autorun.sh'):
        subprocess.run(['/bin/sh', '-n', str(folder / name)], check=True)


def build_sd(engine: Path, wad: Path, profile: str, output: Path, assets: Path) -> Path:
    engine, wad, output, assets = map(Path, (engine, wad, output, assets))
    if profile not in PROFILES:
        raise ValueError('Unsupported game profile')
    if output.exists() or output.is_symlink():
        raise ValueError(f'Output already exists; choose a new folder: {output}')
    regular(wad)
    size, wad_hash = PROFILES[profile]
    if wad.stat().st_size != size or digest(wad) != wad_hash:
        raise ValueError(f'Game data does not match the supported {profile} release')
    output.mkdir(mode=0o700)
    try:
        folder = output / 'autorun'
        stage_autorun(engine, profile, folder, assets)
        shutil.copyfile(wad, folder / 'doom.wad')
        if digest(folder / 'doom.wad') != wad_hash:
            raise ValueError('Package readback failed')
        return folder
    except BaseException:
        shutil.rmtree(output)
        raise


def main():
    import argparse
    import sys
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--engine', required=True, type=Path)
    parser.add_argument('--wad', required=True, type=Path)
    parser.add_argument('--profile', required=True, choices=sorted(PROFILES))
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--assets', type=Path, default=Path(__file__).resolve().parent / 'assets')
    args = parser.parse_args()
    try:
        result = build_sd(args.engine, args.wad, args.profile, args.output, args.assets)
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        print(f'Package stopped: {error}', file=sys.stderr)
        return 1
    print(f'SD folder prepared: {result.parent}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
