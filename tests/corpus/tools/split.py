#!/usr/bin/env python3
"""Writes tests/corpus/detection/split.txt, which fixes each sample in the
tuning set or the held-out set (design 9.2). Detection is tuned on the
tuning set only and scored on the held-out set.

A sample's set follows from a hash of its name, so it never changes and
adding samples leaves the others where they are: about one in three is
held out. Excerpts of one PDF share a set, hashed on the document's name,
so the held-out set never scores documents the detector was tuned on.
Run after adding samples:
python3 -I tests/corpus/tools/split.py
"""
import hashlib
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parents[3]
DETECTION = ROOT / "tests/corpus/detection"


# Prefixes of samples cut from a PDF, named <document>-<n>.
FROM_DOCUMENTS = ("arxiv-", "plos-", "nist-", "made-")


def group(name):
    stem = name.removesuffix(".txt")
    return re.sub(r"-\d+$", "", stem) if stem.startswith(FROM_DOCUMENTS) else stem


def held_out(name):
    return int(hashlib.sha256(group(name).encode()).hexdigest(), 16) % 3 == 0


def main():
    lines = ["# set<TAB>sample. Written by tools/split.py; edit by rerunning it."]
    counts = {}
    for label in ("positive", "negative"):
        for path in sorted((DETECTION / label).glob("*.txt")):
            which = "heldout" if held_out(path.name) else "tuning"
            counts[(label, which)] = counts.get((label, which), 0) + 1
            lines.append(f"{which}\t{label}/{path.name}")
    (DETECTION / "split.txt").write_text("\n".join(lines) + "\n")
    for (label, which), count in sorted(counts.items()):
        print(f"{label:9} {which:8} {count}")


if __name__ == "__main__":
    main()
