"""Build validation shared by standalone DOOM tools."""
import hashlib
import json
from pathlib import Path
import struct
import subprocess

REVISIONS = {
    'doomgeneric': 'e257a09527062bfb986b6e5e0780839b6cbd5b71',
    'chocolate-music': '35fb1372d10756ca27eca05665bd8a7cebc71c05',
}


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify_upstream(root: Path):
    """Check Git pins in development, or all upstream files in an export."""
    manifest = root / 'UPSTREAM-SOURCES.json'
    if manifest.exists():
        record = json.loads(manifest.read_text())
        if record.get('revisions') != REVISIONS or not isinstance(record.get('files'), dict):
            raise ValueError('Invalid upstream source manifest')
        expected = record['files']
        if not expected or any(not isinstance(k, str) or not k.startswith('upstream/') or '..' in Path(k).parts or not isinstance(v, str) for k, v in expected.items()):
            raise ValueError('Invalid upstream source inventory')
        upstream = root / 'upstream'
        if not upstream.is_dir() or upstream.is_symlink():
            raise ValueError('Missing or invalid upstream directory')
        entries = list(upstream.rglob('*'))
        if any(p.is_symlink() or not (p.is_dir() or p.is_file()) for p in entries):
            raise ValueError('Unsupported upstream entry')
        actual = {str(p.relative_to(root)): sha256(p) for p in entries if p.is_file()}
        if actual != expected:
            missing = sorted(expected.keys() - actual.keys())
            extra = sorted(actual.keys() - expected.keys())
            modified = sorted(k for k in expected.keys() & actual.keys() if expected[k] != actual[k])
            raise ValueError(f'Upstream source mismatch: missing={missing}, extra={extra}, modified={modified}')
        return
    for name, revision in REVISIONS.items():
        upstream = root / 'upstream' / name
        actual = subprocess.check_output(['git', '-C', str(upstream), 'rev-parse', 'HEAD'], text=True).strip()
        dirty = subprocess.check_output(['git', '-C', str(upstream), 'status', '--porcelain'], text=True).strip()
        if actual != revision or dirty:
            raise ValueError(f'{name} must match the unmodified pinned revision')


def check_static_arm64(path):
    data = path.read_bytes()
    if data[:6] != b'\x7fELF\x02\x01' or struct.unpack_from('<H', data, 18)[0] != 183:
        raise ValueError('Expected 64-bit little-endian AArch64 ELF')
    offset = struct.unpack_from('<Q', data, 32)[0]
    size, count = struct.unpack_from('<HH', data, 54)
    types = [struct.unpack_from('<I', data, offset + i * size)[0] for i in range(count)]
    if 2 in types or 3 in types:
        raise ValueError('Binary requires a dynamic loader or dynamic linking')
    return hashlib.sha256(data).hexdigest()
