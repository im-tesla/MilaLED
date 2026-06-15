#!/usr/bin/env python3
"""
Build pipeline: npm run build → gzip all dist/ assets → copy to data/

Run from project root:
    python scripts/build_web.py

Or hooked into PlatformIO via extra_scripts:
    extra_scripts = pre:scripts/build_web.py
"""

import gzip
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).parent.parent
WEB_DIR = ROOT / "web"
DIST_DIR = WEB_DIR / "dist"
DATA_DIR = ROOT / "data"

SKIP_GZIP = {".gz", ".woff", ".woff2", ".png", ".jpg", ".jpeg", ".webp", ".ico"}


def run(cmd: list[str], cwd: Path) -> None:
    result = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    if result.stdout:
        print(result.stdout, end="")
    if result.returncode != 0:
        print(result.stderr, file=sys.stderr)
        sys.exit(result.returncode)


def gzip_file(src: Path, dst: Path) -> None:
    dst.parent.mkdir(parents=True, exist_ok=True)
    with open(src, "rb") as f_in:
        with gzip.open(dst, "wb", compresslevel=9) as f_out:
            shutil.copyfileobj(f_in, f_out)


def main() -> None:
    # 1. npm run build
    print("==> Building React app...")
    npm = "npm.cmd" if sys.platform == "win32" else "npm"
    run([npm, "run", "build"], cwd=WEB_DIR)

    # 2. Clean data/ (keep presets/ if it exists)
    if DATA_DIR.exists():
        for item in DATA_DIR.iterdir():
            if item.name == "presets":
                continue
            if item.is_dir():
                shutil.rmtree(item)
            else:
                item.unlink()
    DATA_DIR.mkdir(exist_ok=True)

    # 3. Copy and gzip
    print("==> Compressing to data/...")
    total = 0
    for src in DIST_DIR.rglob("*"):
        if src.is_dir():
            continue
        rel = src.relative_to(DIST_DIR)
        if src.suffix.lower() in SKIP_GZIP:
            dst = DATA_DIR / rel
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dst)
            size = dst.stat().st_size
            print(f"  copy  {rel}  ({size:,} B)")
        else:
            dst = DATA_DIR / (str(rel) + ".gz")
            gzip_file(src, dst)
            size = dst.stat().st_size
            orig = src.stat().st_size
            pct = round((1 - size / orig) * 100) if orig else 0
            print(f"  gzip  {rel}.gz  ({orig:,} -> {size:,} B, -{pct}%)")
        total += 1

    print(f"==> Done. {total} files written to data/")
    print("==> Run: pio run --target uploadfs")


if __name__ == "__main__":
    main()
