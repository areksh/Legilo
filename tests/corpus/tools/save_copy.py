#!/usr/bin/env python3
"""Saves text selected in a PDF reader as corpus samples. pdftotext stands
in for Okular, but Firefox, Chromium and Adobe Reader copy differently, so
real selections keep the detector honest.

Usage: python3 -I tests/corpus/tools/save_copy.py VIEWER NAME [--negative] [--local] [--clipboard]

Start it, then select passages in the reader; there is no need to come
back to the terminal. Each selection that stays unchanged for a moment is
saved as copy-VIEWER-NAME-1.txt, then -2 and so on. A selection that
extends or trims the one just saved replaces it: Chromium updates the
selection while it is still being dragged, so a pause mid-drag would
otherwise save part of it. Press Ctrl+C here when done. It reads the primary selection, which is what Legilo's
speak-selection reads; --clipboard watches the Ctrl+C clipboard instead.

Focusing the terminal replaces the primary selection with the
terminal's own, so the helper never waits for input. Selections shorter
than a few words (a stray click), repeats, and text containing this
command are ignored.

VIEWER is the program you select in (okular, firefox, ...). --negative
marks text whose line breaks must stay (a menu, a poem). --local stores
samples under tests/corpus/local/, which git ignores, for documents that
must not be committed. The text itself is never printed.
"""
import argparse
import pathlib
import re
import subprocess
import time

ROOT = pathlib.Path(__file__).resolve().parents[3]
POLL_SECONDS = 0.5
STEADY_SECONDS = 2.5  # a selection still being dragged keeps changing
MIN_CHARACTERS = 40


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

    # Whatever is selected already is not a new passage.
    last_saved = selected(args.clipboard)
    candidate, since = None, 0.0
    saved = 0
    last_path = None
    print(f"Watching the {'clipboard' if args.clipboard else 'selection'}. "
          f"Select passages in {args.viewer}; press Ctrl+C here when done.")
    try:
        while True:
            time.sleep(POLL_SECONDS)
            text = selected(args.clipboard)
            if text != candidate:
                candidate, since = text, time.monotonic()
                continue
            if (time.monotonic() - since < STEADY_SECONDS or text == last_saved
                    or len(text.strip()) < MIN_CHARACTERS or "save_copy.py" in text):
                continue
            # The same selection, dragged further or pulled back.
            revised = last_path is not None and (last_saved.strip() in text or text.strip() in last_saved)
            path = last_path if revised else folder / f"{slug}-{number}.txt"
            path.write_text(text.rstrip("\n") + "\n", encoding="utf-8")
            print(f"{'Updated' if revised else 'Saved'} {len(text.splitlines())} lines as {path.relative_to(ROOT)}")
            last_saved, last_path = text, path
            if not revised:
                number += 1
                saved += 1
    except KeyboardInterrupt:
        print()
    print(f"{saved} saved.")
    if saved and not args.local:
        print("Run python3 -I tests/corpus/tools/split.py to give them a set.")


if __name__ == "__main__":
    main()
