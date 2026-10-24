#!/usr/bin/env python3
import subprocess
import tempfile
from pathlib import Path

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="savenx-tests-") as temp:
    for source in sorted((root / "tests").glob("*_test.cpp")):
        binary = Path(temp) / source.stem
        extra = [str(root / "source/remote/URL.cpp"), str(root / "source/remote/Form.cpp")] if source.stem == "protocol_encoding_test" else []
        subprocess.run(["g++", "-std=c++23", "-Wall", "-Wextra", "-Werror", "-I", str(root / "include"), str(source), *extra, "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
        print(f"PASS: {source.name}", flush=True)
