#!/usr/bin/env python3
"""Records which PDF each hand-made copy (copy-*.txt) came from, in
tests/corpus/detection/copies.tsv, so that tools/split.py can keep a copy
in the same set as its document's excerpts.

A copy's source is the corpus PDF that shares the most runs of five
words with it. Run after adding copies:
python3 -I tests/corpus/tools/identify_copies.py
"""
import pathlib
import re
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[3]
DETECTION = ROOT / "tests/corpus/detection"
SOURCES = ROOT / "third_party/corpus-sources"
RUN = 5


def runs(text):
    words = re.findall(r"[a-z]+", text.lower())
    return {tuple(words[i:i + RUN]) for i in range(len(words) - RUN + 1)}


def main():
    documents = {}
    for pdf in sorted(SOURCES.glob("*.pdf")) + sorted((SOURCES / "made").glob("*.pdf")):
        text = subprocess.run(["pdftotext", str(pdf), "-"], capture_output=True, text=True,
                              check=True).stdout
        documents[pdf.stem] = runs(text)
    lines = ["# copy<TAB>source document. Written by tools/identify_copies.py."]
    for path in sorted(DETECTION.glob("*/copy-*.txt")):
        copy = runs(path.read_text(encoding="utf-8"))
        overlap = {name: len(copy & shingles) for name, shingles in documents.items()}
        source, shared = max(overlap.items(), key=lambda item: item[1])
        share = shared / max(len(copy), 1)
        if share < 0.3:
            raise SystemExit(f"{path.name}: no corpus PDF matches it well ({share:.0%})")
        lines.append(f"{path.parent.name}/{path.name}\t{source}")
        print(f"{path.parent.name}/{path.name:32} {source:22} {share:.0%} of its word runs")
    (DETECTION / "copies.tsv").write_text("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
