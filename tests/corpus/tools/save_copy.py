#!/usr/bin/env python3
"""Saves text selected in a PDF reader as corpus samples. pdftotext stands
in for Okular, but Firefox, Chromium and Adobe Reader copy differently, so
real selections keep the detector honest.

Usage: python3 -I tests/corpus/tools/save_copy.py VIEWER NAME [--negative] [--local] [--clipboard]

It asks you to select a passage in the reader, then press Enter here, and
saves what is selected (the primary selection, which is what Legilo's
speak-selection reads) as copy-VIEWER-NAME-1.txt, then -2 and so on,
until you type q. --clipboard reads the Ctrl+C clipboard instead.

VIEWER is the program you select in (okular, firefox, ...). --negative
marks text whose line breaks must stay (a menu, a poem). --local stores
samples under tests/corpus/local/, which git ignores, for documents that
must not be committed. The text itself is never printed.

Selecting anything in the terminal replaces the selection, so start this
first and do not select its command or output.
"""
import argparse
import pathlib
import re
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[3]


def selected(clipboard):
    args = ["wl-paste", "--no-newline"] + ([] if clipboard else ["--primary"])
    result = subprocess.run(args, capture_output=True, text=True)
    return result.stdout if result.returncode == 0 else ""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("viewer")
    parser.add_argument("name")
    parser.add_argument("--negative", action="store_true")
    parser.add_argument("--local", action="store_true")
    parser.add_argument("--clipboard", action="store_true")
    args = parser.parse_args()

    base = ROOT / ("tests/corpus/local" if args.local else "tests/corpus/detection")
    folder = base / ("negative" if args.negative else "positive")
    folder.mkdir(parents=True, exist_ok=True)
    slug = re.sub(r"[^a-z0-9]+", "-", f"copy-{args.viewer}-{args.name}".lower()).strip("-")
    number = 1 + max((int(p.stem.rsplit("-", 1)[1]) for p in folder.glob(f"{slug}-*.txt")
                      if p.stem.rsplit("-", 1)[1].isdigit()), default=0)
    source = "clipboard" if args.clipboard else "selection"
    saved = 0
    while True:
        answer = input(f"Select a passage in {args.viewer}, then press Enter here (q to finish): ")
        if answer.strip().lower() == "q":
            break
        text = selected(args.clipboard)
        if not text.strip():
            print(f"The {source} is empty; nothing saved.")
            continue
        if "save_copy.py" in text:
            print(f"The {source} holds this command, not a passage; nothing saved.")
            continue
        path = folder / f"{slug}-{number}.txt"
        path.write_text(text.rstrip("\n") + "\n", encoding="utf-8")
        print(f"Saved {len(text.splitlines())} lines as {path.relative_to(ROOT)}")
        number += 1
        saved += 1
    if saved and not args.local:
        print("Run python3 -I tests/corpus/tools/split.py to give them a set.")


if __name__ == "__main__":
    main()
