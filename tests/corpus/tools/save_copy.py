#!/usr/bin/env python3
"""Saves what is on the clipboard as a corpus sample, for text copied by
hand from a PDF reader. pdftotext stands in for Okular, but Firefox,
Chromium and Adobe Reader copy differently, so real copies keep the
detector honest.

Usage: python3 -I tests/corpus/tools/save_copy.py VIEWER NAME [--negative] [--local]

VIEWER is the program copied from (okular, firefox, ...). --negative
marks text whose line breaks must stay (a menu, a poem). --local stores
it under tests/corpus/local/, which git ignores, for documents that must
not be committed. The text itself is never printed.
"""
import argparse
import pathlib
import re
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[3]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("viewer")
    parser.add_argument("name")
    parser.add_argument("--negative", action="store_true")
    parser.add_argument("--local", action="store_true")
    args = parser.parse_args()

    text = subprocess.run(["wl-paste", "--no-newline"], capture_output=True, text=True, check=True).stdout
    if not text.strip():
        raise SystemExit("The clipboard is empty.")
    base = ROOT / ("tests/corpus/local" if args.local else "tests/corpus/detection")
    folder = base / ("negative" if args.negative else "positive")
    folder.mkdir(parents=True, exist_ok=True)
    slug = re.sub(r"[^a-z0-9]+", "-", f"copy-{args.viewer}-{args.name}".lower()).strip("-")
    path = folder / f"{slug}.txt"
    if path.exists():
        raise SystemExit(f"{path.relative_to(ROOT)} exists already; choose another name.")
    path.write_text(text.rstrip("\n") + "\n", encoding="utf-8")
    print(f"Saved {len(text.splitlines())} lines as {path.relative_to(ROOT)}")
    if not args.local:
        print("Run tools/split.py to give it a set.")


if __name__ == "__main__":
    main()
