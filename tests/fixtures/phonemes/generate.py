"""Generates the golden phoneme fixtures for the Phonemiser tests.

The Piper voices were trained on piper-phonemize's output, so its output
is the reference. Run it in a throwaway container, because piper-phonemize
only publishes wheels for Python 3.11 and older:

    podman run --rm -v "$PWD/tests/fixtures/phonemes:/f:Z" -w /f \
        docker.io/library/python:3.11-slim \
        sh -c 'pip install -q piper-phonemize==1.1.0 && python generate.py'

Each output file holds, for one espeak-ng voice, a list of cases: the input
text and the expected phonemes, one string per sentence.
"""

import json

from piper_phonemize import phonemize_espeak

CASES = [
    "The quick brown fox jumps over the lazy dog.",
    "Hello there. How are you? I'm fine!",
    "First, second; third: fourth.",
    "no full stop at the end",
    "",
    "   ",
    "It costs 42 pounds and 7 pence, in 1999.",
    "Pi is about 3.14, and the crowd was 2,500 strong.",
    "Dr. Smith lives on Baker St. near the U.K. border.",
    "NASA and the BBC both reported it.",
    "It's John's book, isn't it?",
    "The café served crème brûlée to a naïve guest.",
    "She said \"wait\" - then left.",
    "Well... maybe.",
    "Line one\nLine two.",
    "First paragraph.\n\nSecond paragraph.",
    "Visit example.com today.",
    "Colour, behaviour and organisation are spelt differently.",
    "Read the record, then record the read.",
    # Other scripts make espeak-ng switch language, which piper-phonemize
    # strips: "(el)...(en)".
    "The Greek letter \u03b1 is called alpha.",
    "Moscow is \u041c\u043e\u0441\u043a\u0432\u0430 in Russian.",
]

# Voice names as recorded in the Piper models' .onnx.json ("espeak": {"voice": ...}):
# "en" is British English, which the en_GB voices use.
for voice in ("en", "en-us"):
    cases = [
        {"text": text, "sentences": ["".join(s) for s in phonemize_espeak(text, voice)]}
        for text in CASES
    ]
    with open(f"{voice}.json", "w", encoding="utf-8") as out:
        json.dump({"voice": voice, "cases": cases}, out, ensure_ascii=False, indent=2)
        out.write("\n")
