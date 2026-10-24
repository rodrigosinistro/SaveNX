#!/usr/bin/env python3
import subprocess
import tempfile
from pathlib import Path

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="savenx-tests-") as temp:
    for source in sorted((root / "tests").glob("*_test.cpp")):
        binary = Path(temp) / source.stem
        subprocess.run(["g++", "-std=c++23", "-Wall", "-Wextra", "-Werror", "-I", str(root / "include"), str(source), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
        print(f"PASS: {source.name}", flush=True)
