#!/usr/bin/env python3
"""Cuts the corpus's PDFs into excerpts, as a reader would select them.

For each PDF, picks pages at random (with a fixed seed, so the excerpts
can be made again) and takes a run of lines from them, extracted with
pdftotext, which shares poppler with Okular. Excerpts alternate between
pdftotext's reading order and its content-stream order (-raw), and page
breaks become line breaks, as a clipboard copy gives. Writes:

- tests/corpus/detection/{positive,negative}/<source>-<n>.txt, labelled
  by whether the line breaks are layout that reflow should undo;
- tests/corpus/cleanup/<name>/input.txt for each document the corpus
  typeset itself and has expected text for (tools/build_documents.py);
- with --local DIR, excerpts of the PDFs in DIR under tests/corpus/local/,
  which git ignores, for documents that must not be committed.

Run with: python3 -I tests/corpus/tools/make_excerpts.py [--local DIR]
"""
import argparse
import pathlib
import random
import subprocess
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import build_documents  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parents[3]
SOURCES = ROOT / "third_party/corpus-sources"
DETECTION = ROOT / "tests/corpus/detection"
CLEANUP = ROOT / "tests/corpus/cleanup"
LOCAL = ROOT / "tests/corpus/local"

EXCERPTS = {"arxiv": 6, "plos": 5, "nist": 8, "made": 4}
BODY_FRACTION = 0.6  # research papers end in references and appendices


def pages(pdf):
    for line in subprocess.run(["pdfinfo", str(pdf)], capture_output=True, text=True,
                               check=True).stdout.splitlines():
        if line.startswith("Pages:"):
            return int(line.split()[1])
    return 1


def extract(pdf, first, last, raw):
    args = ["pdftotext", "-f", str(first), "-l", str(last)] + (["-raw"] if raw else []) + [str(pdf), "-"]
    text = subprocess.run(args, capture_output=True, text=True, check=True).stdout
    return text.replace("\f", "\n")


def excerpt(pdf, rng, raw, body_only):
    count = pages(pdf)
    last_page = max(1, int(count * BODY_FRACTION)) if body_only else count
    first = rng.randint(1, last_page)
    lines = extract(pdf, first, min(first + 1, count), raw).split("\n")
    for _ in range(20):
        length = rng.randint(6, 50)
        start = rng.randint(0, max(0, len(lines) - length))
        chosen = lines[start:start + length]
        while chosen and not chosen[0].strip():
            chosen.pop(0)
        while chosen and not chosen[-1].strip():
            chosen.pop()
        if sum(1 for line in chosen if line.strip()) >= 3:
            return "\n".join(chosen) + "\n"
    return None


def write_excerpts(pdf, label_dir, name, number, rng, body_only):
    label_dir.mkdir(parents=True, exist_ok=True)
    written = 0
    for i in range(number):
        text = excerpt(pdf, rng, raw=i % 2 == 1, body_only=body_only)
        if text:
            (label_dir / f"{name}-{i + 1}.txt").write_text(text, encoding="utf-8")
            written += 1
    return written


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--local", type=pathlib.Path, help="PDFs to excerpt into tests/corpus/local/")
    args = parser.parse_args()

    total = 0
    for pdf in sorted(SOURCES.glob("*.pdf")):
        kind = pdf.stem.split("-")[0]
        rng = random.Random(pdf.stem)
        total += write_excerpts(pdf, DETECTION / "positive", pdf.stem, EXCERPTS[kind], rng, True)
    for name, (_, _, reflows) in build_documents.DOCUMENTS.items():
        pdf = SOURCES / "made" / f"{name}.pdf"
        label = DETECTION / ("positive" if reflows else "negative")
        total += write_excerpts(pdf, label, name, EXCERPTS["made"], random.Random(name), False)
        if (CLEANUP / name / "expected.txt").exists():
            (CLEANUP / name / "input.txt").write_text(extract(pdf, 1, pages(pdf), raw=True), encoding="utf-8")
    print(f"{total} excerpts")

    if args.local:
        local = 0
        for pdf in sorted(args.local.glob("*.pdf")):
            name = "local-" + "-".join(pdf.stem.lower().split())
            local += write_excerpts(pdf, LOCAL / "positive", name, 6, random.Random(name), False)
        print(f"{local} local excerpts in {LOCAL}")


if __name__ == "__main__":
    main()
