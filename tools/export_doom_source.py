#!/usr/bin/env python3
"""Stage a standalone DOOM source tree; no private data or generated binaries."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
from build_support import REVISIONS, sha256, verify_upstream

ROOT = Path(__file__).resolve().parents[1]


def export(dest, upstream_root, zig_root, extras):
    verify_upstream(upstream_root)
    if subprocess.check_output([str(zig_root / 'zig'), 'version'], text=True).strip() != '0.15.2':
        raise ValueError('Zig 0.15.2 is required')
    if dest.exists() and any(dest.iterdir()):
        raise ValueError('Destination must be empty')
    dest.mkdir(parents=True, exist_ok=True)

    def copy(src, target):
        if src.is_symlink() or not src.is_file():
            raise ValueError('Expected regular file: ' + str(src))
        dst = dest / target
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(src, dst)

    for name in ('build_doom.py', 'build_support.py', 'doom_music_sources.py', 'doom_help_sources.py', 'doom_opening_sources.py', 'export_doom_source.py'):
        copy(ROOT / 'tools' / name, 'tools/' + name)
    public_builder = ROOT / 'tools/build_public_release.py'
    if public_builder.is_file():
        copy(public_builder, 'tools/build_public_release.py')
    for path in sorted((ROOT / 'doom').iterdir()):
        if path.suffix in ('.c', '.h') or path.name in ('help_pages.json', 'MUSIC-NOTICES.txt', 'test_opening.py'):
            copy(path, path.relative_to(ROOT))
    for name in ('TOOLCHAIN.json', 'SOURCE-README.md', 'LICENSE', 'THIRD-PARTY-NOTICES.txt'):
        copy(ROOT / name, name)
    for extra in extras:
        path = Path(extra)
        if path.is_absolute() or '..' in path.parts or not path.parts or path.parts[0] != 'public':
            raise ValueError('Extras must be paths inside public/')
        source = ROOT / path
        if source.is_symlink():
            raise ValueError('Symlinks are not allowed')
        paths = sorted(source.rglob('*')) if source.is_dir() else [source]
        for item in paths:
            if item.is_symlink():
                raise ValueError('Symlinks are not allowed')
            if item.is_file():
                copy(item, item.relative_to(ROOT))

    upstream = upstream_root / 'upstream'
    for name in ('LICENSE', 'README.md', 'README.TXT'):
        copy(upstream / 'doomgeneric' / name, 'upstream/doomgeneric/' + name)
    for path in sorted((upstream / 'doomgeneric/doomgeneric').iterdir()):
        if path.suffix in ('.c', '.h') or path.name == 'Makefile':
            copy(path, path.relative_to(upstream_root))
    copy(upstream / 'chocolate-music/COPYING.md', 'upstream/chocolate-music/COPYING.md')
    for subdir in ('src', 'opl'):
        for path in sorted((upstream / 'chocolate-music' / subdir).rglob('*')):
            if path.is_file() and path.suffix in ('.c', '.h'):
                copy(path, path.relative_to(upstream_root))
    # Zig's linked runtime source and licenses, as distributed with the pinned compiler.
    copy(zig_root / 'LICENSE', 'upstream/zig-runtime/LICENSE')
    copy(zig_root / 'lib/compiler_rt.zig', 'upstream/zig-runtime/lib/compiler_rt.zig')
    for subdir in ('lib/libc/musl', 'lib/compiler_rt'):
        for path in sorted((zig_root / subdir).rglob('*')):
            if path.is_file():
                copy(path, Path('upstream/zig-runtime') / path.relative_to(zig_root))
    files = {str(p.relative_to(dest)): sha256(p) for p in sorted((dest / 'upstream').rglob('*')) if p.is_file()}
    (dest / 'UPSTREAM-SOURCES.json').write_text(json.dumps({'revisions': REVISIONS, 'files': files}, indent=2, sort_keys=True) + '\n')
    verify_upstream(dest)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('destination', type=Path)
    p.add_argument('--upstream-root', type=Path, default=ROOT, help='Checkout holding pinned upstream repos')
    p.add_argument('--zig-root', type=Path, required=True, help='Unpacked pinned Zig 0.15.2 distribution')
    p.add_argument('--include-public', action='append', default=[], metavar='PATH', help='Include an allowlisted public/ file or directory')
    args = p.parse_args()
    export(args.destination, args.upstream_root, args.zig_root, args.include_public)


if __name__ == '__main__':
    main()
