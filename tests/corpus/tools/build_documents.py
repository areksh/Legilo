#!/usr/bin/env python3
"""Typesets the corpus's own documents as PDFs with LibreOffice.

Each document is described once, below, as blocks of content plus a
layout. From that this writes a flat ODT, which LibreOffice converts to
PDF in third_party/corpus-sources/made/, and the clean text that reflow
should recover, as tests/corpus/cleanup/<name>/expected.txt.

Body text comes from public-domain books (tools/gutenberg.py); the menu,
recipe, letter, terms and table are written for the corpus.

Run with: python3 -I tests/corpus/tools/build_documents.py
"""
import html
import pathlib
import subprocess
import sys
import tempfile

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import gutenberg  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parents[3]
MADE = ROOT / "third_party/corpus-sources/made"
CLEANUP = ROOT / "tests/corpus/cleanup"

POUND = chr(0xA3)
BULLET = chr(0x2022)
EM_DASH = chr(0x2014)

# --------------------------------------------------------------- documents
# Blocks: ("title", text), ("subtitle", text), ("h", text), ("p", text),
# ("li", text) for a bullet item, ("ol", n, text) for a numbered one,
# ("menu", dish, price), ("lines", [text, ...]) for lines kept as they are,
# ("break", master) to start a page with another page style, and
# ("table", [[cell, ...], ...]).


def paper():
    body = gutenberg.prose("gutenberg-1228", 160, 9)
    return [("title", "Notes on Variation under Domestication"),
            ("subtitle", "A. N. Observer, Department of Natural History"),
            ("h", "Abstract"), ("p", body[0]),
            ("h", "1. Introduction"), ("p", body[1]), ("p", body[2]), ("p", body[3]),
            ("h", "2. Methods of Selection"), ("p", body[4]), ("p", body[5]),
            ("h", "3. Discussion"), ("p", body[6]), ("p", body[7]), ("p", body[8])]


def report():
    body = gutenberg.prose("gutenberg-1228", 240, 8)
    return [("title", "Annual Report of the Riverside Natural History Society"),
            ("subtitle", "For the year ending 31 March 2026"),
            ("h", "Foreword"),
            ("p", "This report records a year of steady work by the Society's members and volunteers. "
                  "The collections have grown, the reading room has opened on two further evenings each "
                  "week, and the long-planned survey of the river meadows has at last begun. We are "
                  "grateful to everyone who gave their time, and to the trustees who kept the finances "
                  "sound through a difficult winter."),
            ("p", "The pages that follow set out the year's activities, the state of the collections, "
                  "and the accounts. As in previous years, we include an extended essay drawn from the "
                  "Society's lecture series, chosen by the committee as the most widely discussed."),
            ("break", "Body"),
            ("h", "The Year's Lecture"), ("p", body[0]), ("p", body[1]), ("p", body[2]),
            ("p", body[3]), ("p", body[4]), ("h", "Further Remarks"), ("p", body[5]),
            ("p", body[6]), ("p", body[7])]


def novel():
    paragraphs = gutenberg.paragraphs("gutenberg-1342")
    start = next(i for i, p in enumerate(paragraphs) if p.startswith("It is a truth universally"))
    chapter = [p for p in paragraphs[start:start + 40] if not p.startswith("[")][:16]
    return [("h", "Chapter I")] + [("p", p) for p in chapter]


def newsletter():
    body = gutenberg.prose("gutenberg-2701", 400, 10)
    return [("title", "The Harbour Gazette"), ("subtitle", "News and stories from the old port"),
            ("h", "From the Archive")] + [("p", p) for p in body[:5]] + \
           [("h", "Letters to the Editor")] + [("p", p) for p in body[5:]]


def menu():
    return [("title", "The Old Mill Kitchen"), ("subtitle", "Dinner served from six until ten"),
            ("h", "Starters"),
            ("menu", "Roasted tomato soup with basil oil", f"{POUND}6.50"),
            ("menu", "Smoked mackerel pate, pickled cucumber, sourdough toast", f"{POUND}8.00"),
            ("menu", "Crispy squid with lemon and aioli", f"{POUND}8.50"),
            ("menu", "Beetroot, goat's cheese and walnut salad (v)", f"{POUND}7.50"),
            ("h", "Mains"),
            ("menu", "Slow-cooked beef and ale pie, mash, greens", f"{POUND}17.50"),
            ("menu", "Pan-fried hake, crushed potatoes, samphire, brown shrimp butter", f"{POUND}19.00"),
            ("menu", "Wild mushroom and spinach risotto (v)", f"{POUND}15.00"),
            ("menu", "Chicken, leek and tarragon casserole", f"{POUND}16.50"),
            ("menu", "Old Mill burger, cheddar, bacon jam, chips", f"{POUND}16.00"),
            ("h", "Sides"),
            ("menu", "Hand-cut chips", f"{POUND}4.00"), ("menu", "Buttered greens", f"{POUND}4.00"),
            ("menu", "House salad", f"{POUND}3.50"),
            ("h", "Desserts"),
            ("menu", "Sticky toffee pudding, clotted cream", f"{POUND}7.50"),
            ("menu", "Dark chocolate tart, raspberry sorbet", f"{POUND}8.00"),
            ("menu", "Selection of local cheeses, oatcakes, chutney", f"{POUND}9.50"),
            ("lines", ["Please tell your server about any allergies.",
                       "A discretionary service charge of 12.5% is added to tables of six or more."])]


def recipe():
    return [("title", "Lemon and Thyme Roast Chicken"),
            ("subtitle", "Serves four. Preparation 20 minutes, cooking 1 hour 30 minutes."),
            ("p", "A simple roast that rewards a little patience. The lemon keeps the meat moist while the "
                  "thyme perfumes the skin, and the vegetables cook in the juices underneath, so the whole "
                  "meal comes out of one tin. Leave the chicken to rest before carving; it makes all the "
                  "difference to how tender it is."),
            ("h", "Ingredients"),
            ("li", "1 whole chicken, about 1.6 kg"), ("li", "2 unwaxed lemons"),
            ("li", "A small bunch of fresh thyme"), ("li", "4 cloves of garlic, unpeeled"),
            ("li", "800 g floury potatoes, cut into chunks"), ("li", "3 carrots, halved lengthways"),
            ("li", "2 tablespoons of olive oil"), ("li", "Salt and black pepper"),
            ("h", "Method"),
            ("ol", 1, "Heat the oven to 200 degrees, or 180 for a fan oven. Take the chicken out of the fridge "
                      "half an hour before you start, so that it cooks evenly."),
            ("ol", 2, "Halve one lemon and push it into the cavity with half the thyme and the garlic. Slice "
                      "the other lemon and lay the slices in the bottom of a large roasting tin."),
            ("ol", 3, "Toss the potatoes and carrots in the oil with a good pinch of salt, and spread them "
                      "around the lemon slices. Sit the chicken on top and season the skin generously."),
            ("ol", 4, "Roast for an hour and a half, basting twice, until the juices run clear when the thigh "
                      "is pierced. Rest the chicken under foil for fifteen minutes while the vegetables "
                      "finish crisping in the oven.")]


def letter():
    return [("lines", ["14 Orchard Lane", "Little Hampden", "Oxfordshire", "OX7 4QT"]),
            ("lines", ["3 September 2026"]),
            ("lines", ["Ms Helen Carter", "Customer Services", "Northern Rail Holidays", "PO Box 812",
                       "Leeds LS1 4AB"]),
            ("p", "Dear Ms Carter,"),
            ("p", "I am writing about the tour of the Scottish Highlands that my wife and I took between the "
                  "fourth and the eleventh of August, booking reference NRH-20931. I want to begin by saying "
                  "how much we enjoyed most of the holiday, and how helpful your guide was throughout."),
            ("p", "However, on the third day the coach broke down outside Fort William, and we waited nearly "
                  "five hours for a replacement. As a result we missed the boat trip on Loch Ness that was a "
                  "large part of our reason for choosing this tour, and we were not offered any alternative or "
                  "refund for it."),
            ("p", "I would be grateful if you could look into this and let me know what compensation you are "
                  "able to offer. I have enclosed copies of our booking confirmation and the itinerary."),
            ("p", "Yours sincerely,"), ("lines", ["Peter Morgan"])]


def terms():
    return [("title", "Terms and Conditions of Membership"),
            ("h", "1. Definitions"),
            ("p", "1.1 In these terms, the Club means the Riverside Rowing Club, a company limited by guarantee, "
                  "and the Member means any person whose application for membership has been accepted by the "
                  "committee and whose subscription is fully paid."),
            ("p", "1.2 The Premises means the boathouse, the landing stage and any other land or buildings "
                  "that the Club owns, leases or is permitted to use from time to time."),
            ("h", "2. Subscriptions"),
            ("p", "2.1 Annual subscriptions fall due on the first day of April each year. A Member who has not "
                  "paid within sixty days of that date shall be treated as having resigned, unless the "
                  "committee agrees otherwise in writing."),
            ("p", "2.2 The committee may vary the rates of subscription at the annual general meeting, and "
                  "shall give Members at least twenty-eight days' notice of any proposed change."),
            ("h", "3. Use of Equipment"),
            ("p", "3.1 Members may use the Club's boats and equipment only after completing the safety course "
                  "and only during the hours set out in the boathouse rules, as amended from time to time."),
            ("p", "3.2 A Member who damages any boat or equipment through negligence shall be liable for the "
                  "reasonable cost of its repair or replacement, as determined by the committee.")]


def table():
    rows = [["Station", "Jan", "Feb", "Mar", "Apr", "May", "Jun"],
            ["Northgate", "41.2", "38.9", "35.1", "28.4", "22.0", "19.7"],
            ["Riverside", "44.8", "40.3", "37.6", "30.2", "24.9", "21.1"],
            ["Hill Road", "36.5", "34.0", "31.8", "26.7", "20.3", "18.2"],
            ["Market Square", "52.1", "49.6", "44.0", "36.9", "29.4", "25.8"],
            ["Airport", "33.9", "31.2", "29.5", "24.1", "19.8", "17.0"]]
    return [("title", "Monthly Mean Nitrogen Dioxide (micrograms per cubic metre)"), ("table", rows),
            ("lines", ["Source: council monitoring network. Provisional figures."])]


def poems():
    raw = (gutenberg.SOURCES / "gutenberg-12242.txt").read_text(encoding="utf-8-sig").replace("\r\n", "\n")
    begin = raw.find("Success is counted sweetest")
    stanzas = [s for s in raw[begin:begin + 2500].split("\n\n") if s.strip()][:10]
    blocks = []
    for stanza in stanzas:
        lines = [line.strip() for line in stanza.splitlines() if line.strip()]
        blocks.append(("lines", lines) if len(lines) > 1 else ("h", lines[0]))
    return blocks


# name: (blocks, layout, reflows). reflows is the corpus label: whether the
# line breaks are layout, which reflow should undo.
DOCUMENTS = {
    "made-paper": (paper, {"columns": 2, "header": "Journal of Natural Variation, Vol. 12, No. 3",
                           "footer": "page"}, True),
    "made-report": (report, {"columns": 1, "front": True, "footer": "Page {page} of {count}"}, True),
    "made-novel": (novel, {"columns": 1, "header": "PRIDE AND PREJUDICE", "footer": "dashed",
                           "font": 12}, True),
    "made-newsletter": (newsletter, {"columns": 3, "footer": "Page {page} of {count}", "font": 9.5},
                        True),
    "made-recipe": (recipe, {"columns": 1, "ragged": True}, True),
    "made-letter": (letter, {"columns": 1, "ragged": True}, True),
    "made-terms": (terms, {"columns": 1, "footer": "page"}, True),
    "made-menu": (menu, {"columns": 1}, False),
    "made-table": (table, {"columns": 1}, False),
    "made-poems": (poems, {"columns": 1, "footer": "page"}, False),
}

# --------------------------------------------------------------- ODT


def esc(text):
    return html.escape(text, quote=False)


def footer_xml(footer):
    if footer is None:
        return ""
    if footer == "page":
        inner = '<text:page-number text:select-page="current"/>'
    elif footer == "dashed":
        inner = f'{EM_DASH} <text:page-number text:select-page="current"/> {EM_DASH}'
    else:
        inner = esc(footer).replace("{page}", '<text:page-number text:select-page="current"/>') \
                           .replace("{count}", "<text:page-count/>")
    return f'<style:footer><text:p text:style-name="Footer">{inner}</text:p></style:footer>'


def odt(blocks, layout):
    columns = layout.get("columns", 1)
    align = "start" if layout.get("ragged") else "justify"
    size = layout.get("font", 11)
    header = layout.get("header")
    header_xml = (f'<style:header><text:p text:style-name="Header">{esc(header)}</text:p></style:header>'
                  if header else "")
    footer = footer_xml(layout.get("footer"))
    columns_xml = f'<style:columns fo:column-count="{columns}" fo:column-gap="0.6cm"/>'
    front = layout.get("front")
    body = []
    first = True
    for block in blocks:
        kind = block[0]
        if kind == "title" and front and first:
            body.append(f'<text:p text:style-name="FrontTitle">{esc(block[1])}</text:p>')
        elif kind == "title":
            body.append(f'<text:p text:style-name="Title">{esc(block[1])}</text:p>')
        elif kind == "subtitle":
            body.append(f'<text:p text:style-name="Subtitle">{esc(block[1])}</text:p>')
        elif kind == "h":
            body.append(f'<text:p text:style-name="Heading">{esc(block[1])}</text:p>')
        elif kind == "p":
            body.append(f'<text:p text:style-name="Body">{esc(block[1])}</text:p>')
        elif kind == "li":
            body.append(f'<text:p text:style-name="Item">{BULLET}<text:tab/>{esc(block[1])}</text:p>')
        elif kind == "ol":
            body.append(f'<text:p text:style-name="Item">{block[1]}.<text:tab/>{esc(block[2])}</text:p>')
        elif kind == "menu":
            body.append(f'<text:p text:style-name="Menu">{esc(block[1])}<text:tab/>{esc(block[2])}</text:p>')
        elif kind == "lines":
            inner = "<text:line-break/>".join(esc(line) for line in block[1])
            body.append(f'<text:p text:style-name="Lines">{inner}</text:p>')
        elif kind == "break":
            body.append(f'<text:p text:style-name="BodyStart">{""}</text:p>')
        elif kind == "table":
            rows = "".join("<table:table-row>" + "".join(
                f'<table:table-cell office:value-type="string"><text:p text:style-name="Cell">{esc(c)}'
                f"</text:p></table:table-cell>" for c in row) + "</table:table-row>" for row in block[1])
            body.append(f'<table:table table:name="Data"><table:table-column table:number-columns-repeated='
                        f'"{len(block[1][0])}"/>{rows}</table:table>')
        first = False
    front_master = ""
    if front:
        front_master = (f'<style:master-page style:name="Front" style:page-layout-name="Front" '
                        f'style:next-style-name="Front">{footer_xml("page")}</style:master-page>')
    return f"""<?xml version="1.0" encoding="UTF-8"?>
<office:document xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0"
 xmlns:style="urn:oasis:names:tc:opendocument:xmlns:style:1.0"
 xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0"
 xmlns:table="urn:oasis:names:tc:opendocument:xmlns:table:1.0"
 xmlns:fo="urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0"
 office:version="1.3" office:mimetype="application/vnd.oasis.opendocument.text">
<office:font-face-decls><style:font-face style:name="Liberation Serif" svg:font-family="'Liberation Serif'"
 xmlns:svg="urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0"/></office:font-face-decls>
<office:styles>
<style:default-style style:family="paragraph">
 <style:paragraph-properties fo:hyphenation-ladder-count="no-limit"/>
 <style:text-properties style:font-name="Liberation Serif" fo:font-size="{size}pt" fo:language="en"
  fo:country="US" fo:hyphenate="true" fo:hyphenation-remain-char-count="2"
  fo:hyphenation-push-char-count="2"/>
</style:default-style>
<style:style style:name="Body" style:family="paragraph">
 <style:paragraph-properties fo:text-align="{align}" fo:margin-bottom="0.2cm"/></style:style>
<style:style style:name="BodyStart" style:family="paragraph" style:parent-style-name="Body"
 style:master-page-name="Body"><style:paragraph-properties style:page-number="1"/></style:style>
<style:style style:name="Title" style:family="paragraph">
 <style:paragraph-properties fo:text-align="center" fo:margin-bottom="0.3cm"/>
 <style:text-properties fo:font-size="18pt" fo:font-weight="bold" fo:hyphenate="false"/></style:style>
<style:style style:name="FrontTitle" style:family="paragraph" style:parent-style-name="Title"
 style:master-page-name="Front"/>
<style:style style:name="Subtitle" style:family="paragraph">
 <style:paragraph-properties fo:text-align="center" fo:margin-bottom="0.4cm"/>
 <style:text-properties fo:font-style="italic" fo:hyphenate="false"/></style:style>
<style:style style:name="Heading" style:family="paragraph">
 <style:paragraph-properties fo:margin-top="0.3cm" fo:margin-bottom="0.15cm" fo:keep-with-next="always"/>
 <style:text-properties fo:font-weight="bold" fo:font-size="{size + 2}pt" fo:hyphenate="false"/></style:style>
<style:style style:name="Item" style:family="paragraph">
 <style:paragraph-properties fo:margin-left="0.8cm" fo:text-indent="-0.5cm" fo:text-align="start">
  <style:tab-stops><style:tab-stop style:position="0cm"/></style:tab-stops></style:paragraph-properties></style:style>
<style:style style:name="Menu" style:family="paragraph">
 <style:paragraph-properties fo:margin-bottom="0.15cm"><style:tab-stops>
  <style:tab-stop style:position="16cm" style:type="right" style:leader-style="dotted" style:leader-text="."/>
 </style:tab-stops></style:paragraph-properties></style:style>
<style:style style:name="Lines" style:family="paragraph">
 <style:paragraph-properties fo:text-align="start" fo:margin-bottom="0.4cm"/>
 <style:text-properties fo:hyphenate="false"/></style:style>
<style:style style:name="Header" style:family="paragraph">
 <style:paragraph-properties fo:text-align="center"/><style:text-properties fo:font-size="9pt"/></style:style>
<style:style style:name="Footer" style:family="paragraph">
 <style:paragraph-properties fo:text-align="center"/><style:text-properties fo:font-size="9pt"/></style:style>
<style:style style:name="Cell" style:family="paragraph"><style:text-properties fo:font-size="10pt"/></style:style>
</office:styles>
<office:automatic-styles>
<style:page-layout style:name="Page"><style:page-layout-properties fo:page-width="21cm" fo:page-height="29.7cm"
 fo:margin-top="1.5cm" fo:margin-bottom="1.5cm" fo:margin-left="2cm" fo:margin-right="2cm"
 style:num-format="1">{columns_xml}</style:page-layout-properties>
 <style:header-style/><style:footer-style/></style:page-layout>
<style:page-layout style:name="Front"><style:page-layout-properties fo:page-width="21cm" fo:page-height="29.7cm"
 fo:margin-top="1.5cm" fo:margin-bottom="1.5cm" fo:margin-left="2cm" fo:margin-right="2cm"
 style:num-format="i"/><style:header-style/><style:footer-style/></style:page-layout>
</office:automatic-styles>
<office:master-styles>
<style:master-page style:name="Standard" style:page-layout-name="Page">{header_xml}{footer}</style:master-page>
<style:master-page style:name="Body" style:page-layout-name="Page">{header_xml}{footer}</style:master-page>
{front_master}
</office:master-styles>
<office:body><office:text>{"".join(body)}</office:text></office:body>
</office:document>
"""

# --------------------------------------------------------------- expected


def expected_text(blocks):
    """The clean text reflow should recover: one paragraph per block."""
    out = []
    for block in blocks:
        kind = block[0]
        if kind in ("title", "subtitle", "h", "p"):
            out.append(block[1])
        elif kind == "li":
            out.append(f"{BULLET} {block[1]}")
        elif kind == "ol":
            out.append(f"{block[1]}. {block[2]}")
    return "\n\n".join(out) + "\n"


def main():
    MADE.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as work:
        sources = []
        for name, (make, layout, reflows) in DOCUMENTS.items():
            blocks = make()
            path = pathlib.Path(work) / f"{name}.fodt"
            path.write_text(odt(blocks, layout), encoding="utf-8")
            sources.append(str(path))
            if reflows and all(b[0] in ("title", "subtitle", "h", "p", "li", "ol", "break") for b in blocks):
                target = CLEANUP / name
                target.mkdir(parents=True, exist_ok=True)
                (target / "expected.txt").write_text(expected_text(blocks), encoding="utf-8")
        subprocess.run(["soffice", f"-env:UserInstallation=file://{work}/profile", "--headless",
                        "--convert-to", "pdf", "--outdir", str(MADE), *sources],
                       check=True, stdout=subprocess.DEVNULL)
    for name in DOCUMENTS:
        print(f"{name}.pdf", (MADE / f"{name}.pdf").stat().st_size, "bytes")


if __name__ == "__main__":
    main()
