#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Make a local MPC SD folder from your supported GOG Ultimate DOOM installer.

Requires Python 3.9+ and innoextract 1.9+. The Windows installer is read as an
archive and is never executed. No game data is downloaded or redistributed.
"""
import argparse
import hashlib
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile


INSTALLER_NAME = 'setup_the_ultimate_doom_1.9_(28044).exe'
INSTALLER_SHA256 = 'd82f77b9dafc2b20fe71054473d32b5466722b9cb9b7db4a004cd61d903c2ba9'
WAD_SHA256 = '6fdf361847b46228cfebd9f3af09cd844282ac75f3edbb61ca4cb27103ce2e7f'
WAD_BYTES = 12408292
WAD_LUMPS = 2306
ROOT = Path(__file__).resolve().parent


def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def validate_wad(path):
    if path.is_symlink() or not path.is_file():
        raise ValueError('The installer did not extract the expected DOOM.WAD file.')
    if path.stat().st_size != WAD_BYTES or sha256(path) != WAD_SHA256:
        raise ValueError('The extracted DOOM.WAD does not match supported Ultimate DOOM 1.9.')
    data = path.read_bytes()
    magic, count, directory = struct.unpack_from('<4sii', data)
    if magic != b'IWAD' or count != WAD_LUMPS or directory < 12 or directory + count * 16 > len(data):
        raise ValueError('The extracted WAD directory is invalid.')
    names = set()
    for index in range(count):
        offset, length, name = struct.unpack_from('<ii8s', data, directory + index * 16)
        if offset < 0 or length < 0 or offset + length > len(data):
            raise ValueError('The extracted WAD contains an invalid data entry.')
        names.add(name.rstrip(b'\0'))
    expected = {f'E{episode}M{level}'.encode() for episode in range(1, 5) for level in range(1, 10)}
    if not expected.issubset(names):
        raise ValueError('The extracted WAD is missing required Ultimate DOOM maps.')


def setup(installer, output, extractor=None):
    installer = Path(installer).absolute()
    output = Path(output).absolute()
    if output.exists() or output.is_symlink():
        raise ValueError(f'Output already exists; choose a new folder: {output}')
    if installer.is_symlink() or not installer.is_file():
        raise ValueError('Choose your downloaded GOG Windows offline installer file.')
    if sha256(installer) != INSTALLER_SHA256:
        raise ValueError(f'This is not the supported GOG installer: {INSTALLER_NAME}. '
                         'No files were extracted. See README for the supported download.')
    candidate = str(extractor) if extractor is not None else shutil.which('innoextract')
    if not candidate:
        raise ValueError('innoextract is required. On macOS install it with: brew install innoextract')
    tool = shutil.which(candidate)
    if not tool:
        raise ValueError('Cannot run innoextract. Install it with: brew install innoextract; '
                         'or provide its executable using --extractor PATH.')
    with tempfile.TemporaryDirectory(prefix='mpc-doom-import-') as temporary:
        folder = Path(temporary)
        result = subprocess.run([tool, '--extract', '--silent', '--include', 'DOOM.WAD',
                                 '--output-dir', str(folder), '--', str(installer)],
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        if result.returncode:
            detail = result.stderr.strip()[-600:]
            raise ValueError('innoextract could not read the supported installer.' +
                             (f' {detail}' if detail else ''))
        wad = folder / 'DOOM.WAD'
        validate_wad(wad)
        from package_sd import build_sd
        return build_sd(engine=ROOT / 'engine/mpc-doom', wad=wad,
                        profile='ultimate-doom', output=output, assets=ROOT / 'assets')


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--installer', required=True, type=Path,
                        help=f'Your GOG offline installer ({INSTALLER_NAME})')
    parser.add_argument('--output', required=True, type=Path,
                        help='New local SD preparation folder; an existing folder is never replaced')
    parser.add_argument('--extractor', type=Path,
                        help='Optional path to innoextract; otherwise find it on PATH')
    args = parser.parse_args(argv)
    try:
        result = setup(args.installer, args.output, args.extractor)
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        print(f'Setup stopped: {error}', file=sys.stderr)
        return 1
    print(f'Full-game SD folder prepared: {result.parent}')
    print('Follow README to copy autorun to your SD card while the MPC is powered off.')
    print('Your installer was read as an archive and was never executed.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
