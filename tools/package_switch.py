#!/usr/bin/env python3
"""Package an SD layout from the newly compiled NRO only."""
import hashlib
import os
import shutil
import zipfile
from pathlib import Path

root = Path(__file__).resolve().parents[1]
nro = root / "SaveNX.nro"
data = nro.read_bytes()
if len(data) < 0x80 or data[0x10:0x14] != b"NRO0":
    raise SystemExit("The newly compiled file is not a valid NRO container")
version = (root / "include/version.hpp").read_text().split('VERSION = "')[1].split('"')[0]
dist = root / "dist" / "switch" / "SaveNX"
dist.mkdir(parents=True, exist_ok=True)
shutil.copyfile(nro, dist / "SaveNX.nro")
(dist / "SaveNX.nro.sha256").write_text(hashlib.sha256(data).hexdigest() + "  SaveNX.nro\n")
shutil.copyfile(root / "README.md", dist / "README.md")
for name in ("LICENSE", "NOTICE.md"):
    shutil.copyfile(root / name, dist / name)
commit = os.environ.get("GITHUB_SHA", "codex/savenx-v0.3.0")
(dist / "SOURCE.txt").write_text(f"SaveNX {version}\nhttps://github.com/rodrigosinistro/SaveNX/tree/{commit}\n")
shutil.copytree(root / "docs", dist / "docs", dirs_exist_ok=True)
(dist / "config").mkdir(exist_ok=True)
shutil.copyfile(root / "config/google-drive.example.json", dist / "config/google-drive.example.json")
archive = root / f"SaveNX-v{version}-switch.zip"
with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as z:
    for p in sorted((root / "dist").rglob("*")):
        if p.is_file():
            if p.name in {"google-drive.json", "webdav.json"}:
                raise SystemExit("A session or real credentials must not be packaged")
            z.write(p, p.relative_to(root / "dist").as_posix())
with zipfile.ZipFile(archive) as z:
    if z.testzip():
        raise SystemExit("ZIP CRC failed")
print(archive.name)
