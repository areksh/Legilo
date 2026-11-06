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

A sample is labelled by what it holds, not by where it came from. An
excerpt takes its document's label, but an excerpt of a paper can hold
nothing but a table or a chart's axis labels; `detection/relabelled.tsv`
lists those excerpts, with the reason for each, and makes them negative.
An excerpt that mixes the two, such as a letter's address block followed
by its wrapped body, keeps the positive label: reflow helps it more than
it harms it.

## Layout

| Path | Contents |
|---|---|
| `detection/positive/`, `detection/negative/` | One sample per file |
| `detection/split.txt` | Which samples are for tuning and which are held out |
| `detection/relabelled.tsv` | Excerpts that hold no prose, so are negative |
| `detection/heldout-documents.txt` | The documents whose samples are held out |
| `detection/copies.tsv` | The document each hand-made copy came from |
| `cleanup/<name>/input.txt`, `expected.txt` | Whole documents and the text reflow should recover |
| `sources.json` | The downloaded documents, with URLs, hashes and licences |
| `tools/` | The scripts that make all of the above |
| `local/` | Samples that must not be committed; git ignores it |

Samples are named after where they came from:

- `arxiv-*`, `plos-*`, `nist-*`, `scotus-*`, `usgs-*`, `irs-*`, `uscourts-*`:
  excerpts of downloaded PDFs (`sources.json`): papers, a thesis, a
  security guide, a Supreme Court opinion, a fact sheet, a tax guide, the
  Federal Rules of Evidence and two letters to Congress.
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
Ctrl+C there, or until it has saved `--count` of them. It reads the
primary selection, which Legilo's speak-selection reads:

```
python3 -I tests/corpus/tools/save_copy.py firefox papers --count 4
python3 -I tests/corpus/tools/save_copy.py okular menu --negative
python3 -I tests/corpus/tools/save_copy.py firefox policy --local --from ~/Documents/policies
```

The selection belongs to the whole desktop, so the helper saves a
selection only when most of it comes from one of the PDFs in `--from`
(the corpus's sources by default) and ignores anything else, which it
never prints. The name of a copy records the viewer the helper was told
about; check that it matches the viewer that made it, whose habits the
design document lists.

Focusing the terminal replaces the primary selection with the terminal's
own, which is why the helper watches instead of waiting for Enter.
`--local` keeps the samples out of git, for documents that must not be
committed; `--clipboard` watches the Ctrl+C clipboard instead.
Then run `tools/identify_copies.py`, which records the PDF each copy came
from in `detection/copies.tsv`, and `tools/split.py`.

## The split

`tools/split.py` assigns each sample to the tuning or held-out set.
Excerpts of one PDF share a set, and so do the copies made by hand from
it, so the held-out score never includes documents the detector was tuned
on. The documents held out are listed in `detection/heldout-documents.txt`.
They were added after the detector's features were chosen: the first
held-out documents had been looked at while choosing them, so they moved
to the tuning set. Samples not cut from a document, such as the emails
and poems, are split by a hash of their name, which keeps the sets stable
as samples are added; about a third is held out.

Tune on the tuning set only, and score the held-out set once per change
of features, never to choose between them.

## Licences

- `arxiv-*` and `plos-*` excerpts: CC BY 4.0, from the papers listed with
  their authors' attribution in `sources.json`.
- `nist-*` excerpts: NIST SP 800-12 Rev. 1, a work of the United States
  government, in the public domain in the United States.
- Text from Project Gutenberg books (in `made-*`, `poem-*`, `web-*`): in
  the public domain.
- Everything else: written for Legilo, under its licence (GPL-3.0-or-later).
