#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Build two public downloads and matching source from explicit local inputs."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import stat
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'public'))
from package_sd import build_sd, digest, engine_digest, stage_autorun
from setup_full_game import INSTALLER_NAME, INSTALLER_SHA256, WAD_SHA256
from export_doom_source import export
from build_support import REVISIONS, verify_upstream

VERSION = (ROOT / 'public/VERSION').read_text().strip()
ENGINE_SHA256 = 'fce8a541caad68b886943d39d878dd9633a0eac9a14418b8ac99afeb42be8c7e'
EXTRACTOR_ZIP_SHA256 = '6989342c9b026a00a72a38f23b62a8e6a22cc5de69805cf47d68ac2fec993065'
EXTRACTOR_SHA256 = '946bd06d8b3722a791fea4d84f496139eef6faef61640924cb72f5f1998a68e1'
EXTRACTOR_DIR = 'windows/innoextract'
EXTRACTOR_EXE = EXTRACTOR_DIR + '/innoextract.exe'
DOCS = ('README.md', 'RELEASE-NOTES.md', 'VERIFICATION.md', 'VALIDATION.json', 'VERSION')
NOTICES = ('ENGINE-LICENSE.txt', 'MUSIC-NOTICES.txt', 'NOTICES.txt',
           'SHAREWARE-LICENSE.txt', 'RUNTIME-NOTICES.txt')


def validate_release_engine(engine):
    """Bind the release binary to the build record and current matching inputs."""
    info = json.loads((engine.parent / 'build-info.json').read_text())
    if info.get('sha256') != digest(engine):
        raise ValueError('Release engine does not match its build record')
    expected_sources = {str(p.relative_to(ROOT)): digest(p) for p in (ROOT / 'doom').glob('*.[ch]')}
    expected_tools = {name: digest(ROOT / 'tools' / name) for name in
                      ('build_doom.py', 'doom_music_sources.py', 'doom_help_sources.py', 'doom_opening_sources.py', 'build_support.py')}
    if info.get('local_sources') != expected_sources or info.get('build_tools_sha256') != expected_tools:
        raise ValueError('Release source does not match its build record')
    if info.get('target') != 'mpc' or info.get('engine_revision') != REVISIONS['doomgeneric'] or info.get('music_revision') != REVISIONS['chocolate-music']:
        raise ValueError('Release build record has incorrect target or upstream revision')
    if info.get('help_content_sha256') != digest(ROOT / 'doom/help_pages.json'):
        raise ValueError('Release help source does not match its build record')
    generated = info.get('generated_sources_sha256')
    if not isinstance(generated, dict) or 'mpc_opening_pixels.inc' not in generated:
        raise ValueError('Missing generated source build record')
    for name, sha in generated.items():
        if Path(name).name != name or digest(engine.parent / name) != sha:
            raise ValueError('Generated source does not match its build record')
    if info.get('music_driver_sha256') != digest(engine.parent / 'mpc_oplmusic.inc'):
        raise ValueError('Generated music source does not match its build record')
    if info['sha256'] != ENGINE_SHA256:
        raise ValueError('Release engine does not match the tested MP DOOM 1.0.1 candidate')
    verify_upstream(ROOT)
    return engine_digest(engine)


def copy(src, dest):
    if src.is_symlink() or not src.is_file():
        raise ValueError(f'Expected regular input: {src}')
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(src, dest)


def archive(folder, dest):
    """Stable order, time, modes and compression; no links or machine metadata."""
    with zipfile.ZipFile(dest, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for p in sorted(folder.rglob('*')):
            if p.is_symlink():
                raise ValueError(f'Symlinks cannot be released: {p}')
            if not p.is_file():
                continue
            info = zipfile.ZipInfo(str(p.relative_to(folder)), (2026, 11, 4, 0, 0, 0))
            info.create_system = 3
            info.external_attr = (stat.S_IFREG | 0o644) << 16
            info.compress_type = zipfile.ZIP_DEFLATED
            z.writestr(info, p.read_bytes(), compress_type=zipfile.ZIP_DEFLATED, compresslevel=9)


def verify_archive(path, profile=None, windows_setup=False):
    """Read every archived byte and reject private or unexpected payload types."""
    inventory = {}
    with zipfile.ZipFile(path) as z:
        if z.testzip() is not None:
            raise ValueError('Archive CRC failure')
        for info in z.infolist():
            p = Path(info.filename)
            if (p.is_absolute() or '..' in p.parts or '\\' in info.filename or ':' in info.filename
                    or stat.S_ISLNK(info.external_attr >> 16)):
                raise ValueError('Unsafe archive entry')
            bad = {'.local', '.git', '.tools', '__pycache__', 'demo', 'preview', 'handoff.md', 'authorized_keys'}
            if any(part in bad or part.startswith('._') or part.startswith('ssh_') for part in p.parts):
                raise ValueError(f'Private or legacy entry: {p}')
            data = z.read(info)
            approved_exe = (windows_setup and info.filename == EXTRACTOR_EXE
                            and hashlib.sha256(data).hexdigest() == EXTRACTOR_SHA256)
            if (p.suffix.lower() in ('.exe', '.dll', '.pem', '.key', '.dsg')
                    or data[:2] == b'MZ') and not approved_exe:
                raise ValueError(f'Private data or key entry: {p}')
            if re.search(rb'(?m)^-----BEGIN (?:OPENSSH|RSA|EC|DSA) PRIVATE KEY-----\r?$', data):
                raise ValueError(f'Private key content: {p}')
            if re.search(rb'/(?:Users|home)/[^/\x00\s]+/', data) or re.search(rb'/private/t[m]p/mpc-', data):
                raise ValueError(f'Machine path leaked: {p}')
            if p.suffix.lower() == '.wad':
                if profile != 'shareware' or info.filename != 'SD/autorun/doom.wad' or hashlib.sha256(data).hexdigest() != '1d7d43be501e67d927e415e0b8f3e29c3bf33075e859721816f652a526cac771':
                    raise ValueError(f'Unapproved game data: {p}')
            if data[:4] == b'IWAD' and p.suffix.lower() != '.wad':
                raise ValueError(f'Hidden game data: {p}')
            inventory[info.filename] = {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
    return inventory


def stage_windows(full, engine, extractor_zip):
    if digest(extractor_zip) != EXTRACTOR_ZIP_SHA256:
        raise ValueError('Expected the pinned innoextract 1.9 Windows ZIP; see SOURCE-README.md')
    extractor_dir = full / EXTRACTOR_DIR
    extractor_dir.mkdir(parents=True)
    # The entire archive is pinned, including the dependency license notices.
    with zipfile.ZipFile(extractor_zip) as z:
        z.extractall(extractor_dir)
    if digest(full / EXTRACTOR_EXE) != EXTRACTOR_SHA256:
        raise ValueError('The bundled Windows extractor did not pass its file check')
    windows = full / 'windows'
    payload = windows / 'autorun'
    stage_autorun(engine, 'ultimate-doom', payload, ROOT / 'public/assets')
    (windows / 'payload.json').write_text(json.dumps({
        'installer_name': INSTALLER_NAME,
        'installer_bytes': 19165952,
        'installer_sha256': INSTALLER_SHA256,
        'wad_sha256': WAD_SHA256,
        'extractor_sha256': EXTRACTOR_SHA256,
        'autorun': {p.name: digest(p) for p in sorted(payload.iterdir())},
    }, indent=2) + '\n', encoding='ascii')
    copy(ROOT / 'public/windows/setup.ps1', windows / 'setup.ps1')
    name = 'Start MP DOOM Setup.cmd'
    launcher = (ROOT / 'public/windows' / name).read_text(encoding='ascii')
    (full / name).write_bytes(launcher.replace('\n', '\r\n').encode('ascii'))


def build(engine, shareware, zig_root, destination, extractor_zip):
    if destination.exists() or destination.is_symlink():
        raise ValueError('Release output must not exist; choose a fresh directory')
    engine_hash = validate_release_engine(engine)
    with tempfile.TemporaryDirectory(prefix='mpc-public-release-') as temporary:
        stage = Path(temporary)
        starter, full, source = [stage / name for name in ('starter', 'full-setup', 'source')]
        starter.mkdir(); full.mkdir()
        build_sd(engine, shareware, 'shareware', starter / 'SD', ROOT / 'public/assets')
        for target in (starter, full):
            for name in DOCS:
                copy(ROOT / 'public' / name, target / name)
            for name in NOTICES:
                copy(ROOT / 'public/assets' / name, target / name)
            copy(ROOT / 'THIRD-PARTY-NOTICES.txt', target / 'THIRD-PARTY-NOTICES.txt')
            copy(engine.parent / 'build-info.json', target / 'ENGINE-BUILD.json')
            (target / 'RELEASE.json').write_text(json.dumps({'version': VERSION, 'engine_sha256': engine_hash,
                'target': 'aarch64-linux-musl', 'firmware': '1.3.0.12'}, indent=2) + '\n')
        for name in ('setup_full_game.py', 'package_sd.py'):
            copy(ROOT / 'public' / name, full / name)
        copy(engine, full / 'engine/mpc-doom')
        for name in ('autorun.sh.in', 'launch-doom.sh.in', *NOTICES):
            copy(ROOT / 'public/assets' / name, full / 'assets' / name)
        stage_windows(full, engine, extractor_zip)
        extras = ['public/' + name for name in (*DOCS, 'setup_full_game.py', 'package_sd.py', 'assets', 'windows')]
        export(source, ROOT, zig_root, extras)
        copy(ROOT / 'public/VERIFICATION.md', source / 'VERIFICATION.md')
        copy(engine.parent / 'build-info.json', source / 'ENGINE-BUILD.json')
        destination.mkdir(parents=True)
        try:
            audit = {'version': VERSION, 'engine_sha256': engine_hash, 'archives': {}}
            for name, folder, profile in (('starter', starter, 'shareware'), ('full-setup', full, None), ('source', source, None)):
                path = destination / f'mp-doom-{VERSION}-{name}.zip'
                archive(folder, path)
                files = verify_archive(path, profile, windows_setup=(name == 'full-setup'))
                audit['archives'][path.name] = {'bytes': path.stat().st_size, 'sha256': digest(path), 'files': files}
            (destination / 'SHA256SUMS.txt').write_text(''.join(f"{v['sha256']}  {k}\n" for k, v in audit['archives'].items()))
            (destination / 'ARCHIVE-AUDIT.json').write_text(json.dumps(audit, indent=2, sort_keys=True) + '\n')
        except BaseException:
            shutil.rmtree(destination)
            raise
    return destination


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--engine', required=True, type=Path)
    p.add_argument('--shareware', required=True, type=Path)
    p.add_argument('--zig-root', required=True, type=Path)
    p.add_argument('--output', required=True, type=Path)
    p.add_argument('--windows-extractor-zip', required=True, type=Path)
    args = p.parse_args()
    print(build(args.engine.resolve(), args.shareware.resolve(), args.zig_root.resolve(),
                args.output.absolute(), args.windows_extractor_zip.resolve()))


if __name__ == '__main__':
    main()
