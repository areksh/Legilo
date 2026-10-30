#!/usr/bin/env python3
"""Downloads the documents in tests/corpus/sources.json into
third_party/corpus-sources/, checking each against its SHA-256.

A source without a "sha256" is downloaded and its hash printed, to be
pinned in sources.json. Run with: python3 -I tests/corpus/tools/fetch_sources.py
"""
import hashlib
import json
import pathlib
import sys
import time
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parents[3]
MANIFEST = ROOT / "tests/corpus/sources.json"
TARGET = ROOT / "third_party/corpus-sources"


def main():
    TARGET.mkdir(parents=True, exist_ok=True)
    sources = json.loads(MANIFEST.read_text())["sources"]
    failed = False
    for source in sources:
        suffix = ".pdf" if source["kind"] == "pdf" else ".txt"
        path = TARGET / (source["name"] + suffix)
        expected = source.get("sha256")
        if path.exists() and expected and hashlib.sha256(path.read_bytes()).hexdigest() == expected:
            print(f"have {path.name}")
            continue
        request = urllib.request.Request(source["url"], headers={"User-Agent": "legilo-corpus/1"})
        data = urllib.request.urlopen(request, timeout=60).read()
        digest = hashlib.sha256(data).hexdigest()
        if expected and digest != expected:
            print(f"MISMATCH {path.name}: got {digest}", file=sys.stderr)
            failed = True
            continue
        path.write_bytes(data)
        print(f"fetched {path.name} {digest}" + ("" if expected else "  (pin this)"))
        time.sleep(3)  # arXiv asks for a pause between requests
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
