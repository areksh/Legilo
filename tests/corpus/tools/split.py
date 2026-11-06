#!/usr/bin/env python3
"""Writes tests/corpus/detection/split.txt, which fixes each sample in the
tuning set or the held-out set (design 9.2). Detection is tuned on the
tuning set only and scored on the held-out set.

Excerpts of one PDF share a set, and so does every copy made by hand
from that PDF (detection/copies.tsv, from tools/identify_copies.py), so
the held-out set never scores a document the detector was tuned on. The
documents held out are those in detection/heldout-documents.txt: they
were added after the detector's features had been chosen, which the
first held-out documents had been looked at for, so those now tune.

Every other sample's set follows from a hash of its name, so it never
changes and adding samples leaves the others where they are: about one
in three is held out.
Run after adding samples:
python3 -I tests/corpus/tools/split.py
"""
import hashlib
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parents[3]
DETECTION = ROOT / "tests/corpus/detection"


# Prefixes of samples cut from a PDF, named <document>-<n>.
FROM_DOCUMENTS = ("arxiv-", "plos-", "nist-", "made-", "scotus-", "usgs-", "irs-", "uscourts-")
HELD_OUT_DOCUMENTS = {line for line in (DETECTION / "heldout-documents.txt").read_text().splitlines()
                      if line and not line.startswith("#")}


def copy_sources():
    path = DETECTION / "copies.tsv"
    if not path.exists():
        return {}
    pairs = [line.split("\t") for line in path.read_text().splitlines() if line and not line.startswith("#")]
    return {pathlib.Path(copy).name: source for copy, source in pairs}


COPIES = copy_sources()


def group(name):
    if name.startswith("copy-"):
        if name not in COPIES:
            raise SystemExit(f"{name} has no source in copies.tsv; run tools/identify_copies.py")
        return COPIES[name]
    stem = name.removesuffix(".txt")
    return re.sub(r"-\d+$", "", stem) if stem.startswith(FROM_DOCUMENTS) else stem


def held_out(name):
    source = group(name)
    if source.startswith(FROM_DOCUMENTS):
        return source in HELD_OUT_DOCUMENTS
    return int(hashlib.sha256(source.encode()).hexdigest(), 16) % 3 == 0


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
