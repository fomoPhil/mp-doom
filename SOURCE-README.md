# MPC DOOM source

The local DOOM port in `doom/` is offered under the GNU General Public License,
version 2 or (at your option) any later version. See LICENSE and the notices in
THIRD-PARTY-NOTICES.txt and doom/MUSIC-NOTICES.txt for upstream components.
No game WAD is included.

## Build the MPC executable

On Apple Silicon macOS, install the official Zig 0.15.2 distribution identified
in TOOLCHAIN.json and verify its SHA-256 against that file. From this directory:

    ZIG=/absolute/path/to/zig python3 tools/build_doom.py

The static ARM64 Linux executable and build record are written to
`build/doom/mpc/`. The MPC executable is stripped of debug symbols for
reproducible release builds. Python 3, macOS `file`, and the Zig distribution are required.
The build checks UPSTREAM-SOURCES.json against every file under upstream before
compilation. The manifest describes this export; it is not a cryptographic
signature. Exported Zig runtime source accompanies the binary for license
compliance; the build uses the installed official Zig distribution.

To recreate the source export from a development checkout, run
`python3 tools/export_doom_source.py DEST --zig-root /path/to/zig-distribution`.
Optional `--include-public public/PATH` arguments include explicitly selected
public packaging assets. The destination must be empty.

## Rebuild the public ZIPs

Run `python3 tools/build_public_release.py --help` for the release inputs. The
Windows setup requires the unmodified [innoextract 1.9 Windows ZIP](https://constexpr.org/innoextract/files/innoextract-1.9-windows.zip).
Its SHA-256 is `6989342c9b026a00a72a38f23b62a8e6a22cc5de69805cf47d68ac2fec993065`.
The builder checks this hash and includes the upstream documentation and
licenses. Upstream [innoextract source](https://constexpr.org/innoextract/files/innoextract-1.9.tar.gz)
is available separately. Building the public ZIPs does not require the GOG installer.
