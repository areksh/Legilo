"""Paragraphs from the Project Gutenberg texts in third_party/corpus-sources/."""
import pathlib
import re

SOURCES = pathlib.Path(__file__).resolve().parents[3] / "third_party/corpus-sources"


def paragraphs(name):
    """The book's paragraphs, unwrapped, without Gutenberg's header and licence."""
    text = (SOURCES / f"{name}.txt").read_text(encoding="utf-8-sig").replace("\r\n", "\n")
    start = text.find("*** START OF")
    end = text.find("*** END OF")
    body = text[text.find("\n", start) + 1:end]
    result = []
    for block in re.split(r"\n\s*\n", body):
        block = " ".join(line.strip() for line in block.splitlines()).strip()
        block = re.sub(r"_([^_]+)_", r"\1", block)  # Gutenberg marks italics with underscores
        if block:
            result.append(block)
    return result


def prose(name, first, count, minimum=200):
    """count paragraphs of at least minimum characters, from the first'th on."""
    long_ones = [p for p in paragraphs(name) if len(p) >= minimum and not p.isupper()]
    return long_ones[first:first + count]
