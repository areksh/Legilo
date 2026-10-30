# Regression corpus

Text samples for the clean-up pipeline (design 7.2, 9.2): the detection
corpus that PDF detection is tuned and scored on, and clean-up pairs
whose output must match exactly.

## Labels

A **positive** sample is text whose line breaks are layout that reflow
should undo: prose wrapped to a page width, with hyphenated breaks, page
numbers and running headers. A **negative** sample is text whose line
breaks mean something and must stay: emails, chat, code, poems, lists,
addresses, tables, subtitles. A menu, a table or a poem copied from a PDF
is negative, because reflowing it would damage it. That is the decision
the detector makes, and what its precision target (98%) protects.

## Layout

| Path | Contents |
|---|---|
| `detection/positive/`, `detection/negative/` | One sample per file |
| `detection/split.txt` | Which samples are for tuning and which are held out |
| `cleanup/<name>/input.txt`, `expected.txt` | Whole documents and the text reflow should recover |
| `sources.json` | The downloaded documents, with URLs, hashes and licences |
| `tools/` | The scripts that make all of the above |
| `local/` | Samples that must not be committed; git ignores it |

Samples are named after where they came from:

- `arxiv-*`, `plos-*`, `nist-*`: excerpts of downloaded PDFs (`sources.json`).
- `made-*`: excerpts of documents typeset for the corpus with LibreOffice
  (`tools/build_documents.py`): a two-column paper, a report with Roman
  front matter, a novel chapter, a three-column newsletter, a recipe, a
  letter, terms and conditions, a menu, a table and poems.
- `copy-<viewer>-*`: text copied by hand from a PDF reader.
- Everything else in `negative/`: written for the corpus
  (`tools/make_negatives.py`), apart from `poem-*` and `web-*`, which
  come from public-domain books.

## Making it again

The tools are run by hand and need Python 3, pdftotext (poppler-utils)
and LibreOffice; the build never runs them. From the repository root:

```
python3 -I tests/corpus/tools/fetch_sources.py   # downloads into third_party/corpus-sources/
python3 -I tests/corpus/tools/build_documents.py # typesets the made-* PDFs
python3 -I tests/corpus/tools/make_excerpts.py   # cuts the excerpts and cleanup inputs
python3 -I tests/corpus/tools/make_negatives.py
python3 -I tests/corpus/tools/split.py
```

Excerpts are cut with a fixed seed, so they come out the same. The
downloaded PDFs are not committed; `sources.json` pins each by SHA-256.

## Adding real copies

pdftotext shares poppler with Okular, so the excerpts stand in for an
Okular copy. Firefox, Chromium and Adobe Reader copy text differently, so
the corpus needs real samples from them too. Start the helper, then
select passages in the reader; each selection that stays unchanged for a
moment is saved, without going back to the terminal, until you press
Ctrl+C there. It reads the primary selection, which Legilo's
speak-selection reads:

```
python3 -I tests/corpus/tools/save_copy.py firefox papers
python3 -I tests/corpus/tools/save_copy.py okular menu --negative
python3 -I tests/corpus/tools/save_copy.py firefox policy --local
```

Focusing the terminal replaces the primary selection with the terminal's
own, which is why the helper watches instead of waiting for Enter.
`--local` keeps the samples out of git, for documents that must not be
committed; `--clipboard` watches the Ctrl+C clipboard instead.
Then run `tools/split.py`.

## The split

`tools/split.py` assigns each sample to the tuning or held-out set from a
hash of its name, so the sets are stable as samples are added; about a
third is held out. Excerpts of one PDF share a set, so the held-out score
never includes documents the detector was tuned on. Tune on the tuning
set only.

## Licences

- `arxiv-*` and `plos-*` excerpts: CC BY 4.0, from the papers listed with
  their authors' attribution in `sources.json`.
- `nist-*` excerpts: NIST SP 800-12 Rev. 1, a work of the United States
  government, in the public domain in the United States.
- Text from Project Gutenberg books (in `made-*`, `poem-*`, `web-*`): in
  the public domain.
- Everything else: written for Legilo, under its licence (GPL-3.0-or-later).
