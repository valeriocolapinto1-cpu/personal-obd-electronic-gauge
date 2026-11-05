#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check (or fix) clang-format on all C/C++ sources. Works on Windows and Linux.

Usage:
  python scripts/check_format.py            # check, exit 1 if anything is unformatted
  python scripts/check_format.py --fix      # rewrite files in place
"""
import argparse
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SOURCE_DIRS = ["firmware", "sim", "tests", "tools"]
EXTENSIONS = {".c", ".cc", ".cpp", ".h", ".hpp"}
EXCLUDE_PARTS = {"build", "managed_components", "third_party"}


def sources():
    for d in SOURCE_DIRS:
        base = ROOT / d
        if not base.is_dir():
            continue
        for p in sorted(base.rglob("*")):
            if p.suffix in EXTENSIONS and p.is_file() and not (EXCLUDE_PARTS & set(p.relative_to(ROOT).parts)):
                yield p


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--fix", action="store_true", help="format files in place")
    ap.add_argument("--clang-format", default="clang-format", help="clang-format executable")
    args = ap.parse_args()

    files = list(sources())
    bad = []
    for f in files:
        if args.fix:
            subprocess.run([args.clang_format, "-i", "--style=file", str(f)], check=True)
            continue
        r = subprocess.run([args.clang_format, "--dry-run", "--Werror", "--style=file", str(f)],
                           capture_output=True, text=True)
        if r.returncode != 0:
            bad.append(f.relative_to(ROOT))
            sys.stderr.write(r.stderr)
    if bad:
        print(f"\n{len(bad)} file(s) need formatting. Run: python scripts/check_format.py --fix")
        for b in bad:
            print(f"  {b}")
        return 1
    print(f"{len(files)} file(s) checked" + (" and formatted" if args.fix else ", all formatted"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
