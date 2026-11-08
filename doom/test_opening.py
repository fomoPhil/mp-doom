#!/usr/bin/env python3
"""Run: python3 test_opening.py REPO_ROOT GENERATED_INCLUDE_DIR [RUNTIME_DIR]."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time

repo, image_dir = map(Path, sys.argv[1:3])
runtime = Path(sys.argv[3]) if len(sys.argv) > 3 else Path(__file__).resolve().parent
source = repo / "upstream/doomgeneric/doomgeneric"
flags = ["-D_DEFAULT_SOURCE", "-DDOOMGENERIC_RESX=320", "-DDOOMGENERIC_RESY=200",
         "-I" + str(image_dir), "-I" + str(source), "-I" + str(runtime),
         "-I" + str(repo / "doom")]
with tempfile.TemporaryDirectory(prefix="mpc-opening-test-") as temp:
    obj = Path(temp) / "port.o"
    binary = Path(temp) / "test-opening"
    subprocess.run(["cc", *flags, "-Dmain=mpc_port_main", "-c",
                    str(runtime / "mpc_port.c"), "-o", str(obj)], check=True)
    subprocess.run(["cc", *flags, str(Path(__file__).with_name("test_opening.c")),
                    str(obj), "-o", str(binary)], check=True)
    for name, variable, expected in [("normal", None, 0), ("power", "POWER", 0),
                                     ("signal", "SIGNAL", 0), ("flush failure", "FLUSH_FAIL", 1)]:
        env = {k: v for k, v in os.environ.items() if k not in ("POWER", "SIGNAL", "FLUSH_FAIL")}
        if variable:
            env[variable] = "1"
        start = time.monotonic()
        result = subprocess.run([str(binary)], env=env, capture_output=True, text=True, timeout=5)
        duration = time.monotonic() - start
        assert result.returncode == expected, (name, result.returncode, result.stdout, result.stderr)
        for line in ("FLUSHED_REAL_IMAGE", "CLOSE_INPUT", "CLOSE_DISPLAY", "MPC_DOOM_EXIT frames=0"):
            assert line in result.stdout, (name, line, result.stdout)
        if variable:
            assert "MPC_DOOM_INITIALIZED" not in result.stdout, (name, result.stdout)
            assert duration < 0.8, (name, duration)
        else:
            assert "MPC_DOOM_INITIALIZED" in result.stdout and "ELAPSED_MS " in result.stdout, result.stdout
        print(f"{name}: passed ({duration:.3f}s)")
