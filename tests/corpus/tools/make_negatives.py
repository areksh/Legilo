#!/usr/bin/env python3
"""Writes the corpus's negative samples: text whose line breaks mean
something, which reflow must leave alone. Most are written for the
corpus; the poems and the web-style prose come from public-domain books.

Run with: python3 -I tests/corpus/tools/make_negatives.py
"""
import pathlib
import random
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import gutenberg  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parents[3]
NEGATIVE = ROOT / "tests/corpus/detection/negative"

SAMPLES = {
    # ------------------------------------------------------------ email
    "email-1": """Hi Sam,

Thanks for sending the draft over. I've had a quick look and it reads well, but I think the second section could be shorter. Could we talk it through on Thursday?

Also, can you check whether the figures in table 2 are the final ones? Finance sent a revised set yesterday.

Cheers,
Jo""",
    "email-2": """Dear all,

Just a reminder that the office will be closed on Monday for the bank holiday. The heating engineer is coming on Tuesday morning, so please clear the area around the radiators in meeting room B before you leave on Friday.

Have a lovely long weekend.

Best wishes,
Margaret
Office Manager""",
    "email-3": """Hello,

My order (number 448120) arrived today but the blue mug was broken. I've attached two photos of the packaging and the damage.

Could you send a replacement, or refund the cost if it's out of stock?

Thank you,
Daniel Price""",
    "email-4": """On Tue, 14 Oct 2026 at 09:12, Priya Shah wrote:
> Are we still on for lunch tomorrow?

Yes! 12:30 at the usual place? I might be five minutes late, I've got a call at noon.

Rob""",
    "email-5": """Hi team,

Quick update on the release:

- The build is green on all platforms
- Two bugs left in the installer, both assigned
- Docs are being reviewed by Anna

If nothing else comes up, we ship on Wednesday.

Thanks,
Lee""",
    "email-6": """Good morning Mr Patel,

Following our telephone conversation, I confirm your appointment with Dr Hughes on Friday 7 November at 10.40am at the Elm Street surgery.

Please arrive ten minutes early and bring a list of any medication you are taking.

Kind regards,
Reception
Elm Street Medical Practice
Tel: 01632 960 123""",
    "email-7": """Hey,

Can you pick up some milk on the way home? And bread if they have the seeded one.

Thanks x""",
    "email-8": """Dear Professor Green,

I am a second-year student on the MSc in Environmental Science, and I am writing to ask whether you would be willing to supervise my dissertation next year. My proposed topic is the effect of hedgerow management on farmland bird populations in the Cotswolds.

I have attached a short proposal and my transcript. I would be very grateful for the chance to discuss it with you.

Yours sincerely,
Amelia Hart""",
    "email-9": """Subject: Server maintenance tonight

All,

The file server will be offline from 22:00 to 23:30 tonight for updates. Please save your work and log off before then.

IT Support""",
    "email-10": """Hi Mum,

We landed safely, the flight was fine and the hotel is lovely. It's much warmer than we expected!

Will call tomorrow.

Love,
Kate""",
    # ------------------------------------------------------------ chat
    "chat-1": """[09:14] alex: morning! anyone seen the new build?
[09:15] sam: yeah, the login page is broken on firefox
[09:15] alex: ugh. on it
[09:21] sam: thanks
[09:40] alex: fixed in 1.4.2, can you check?
[09:41] sam: works now :)""",
    "chat-2": """Tom: are you coming tonight?
Ella: yes but I'll be late
Tom: ok no worries
Tom: we're at the corner table
Ella: see you there""",
    "chat-3": """Mark Jenkins, 10:02
Has anyone got the slides from yesterday?

Sophie Turner, 10:05
I'll send them now

Mark Jenkins, 10:06
Thanks Sophie!""",
    "chat-4": """me: did you book the train
Dan: not yet, the 8:15 or the 9:40?
me: 8:15 is cheaper
Dan: booked
me: legend""",
    "chat-5": """<jim> the deploy failed again
<carla> which step?
<jim> migrations
<carla> try running them by hand first
<jim> ok that worked
<carla> nice, it was the lock timeout then""",
    "chat-6": """Grandma: Happy birthday darling!!! Hope you have a wonderful day
Lucy: Thank you Grandma! Love you
Grandma: Love you too. Is the parcel there yet?
Lucy: Yes it came this morning, I love the scarf""",
    # ------------------------------------------------------------ code
    "code-cpp": """#include <iostream>
#include <vector>

int main()
{
    std::vector<int> values{3, 1, 4, 1, 5};
    int total = 0;
    for (int value : values) {
        total += value;
    }
    std::cout << "Total: " << total << '\\n';
    return 0;
}""",
    "code-python": """def read_config(path):
    \"\"\"Read a key=value file into a dict.\"\"\"
    config = {}
    with open(path) as file:
        for line in file:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            key, value = line.split("=", 1)
            config[key.strip()] = value.strip()
    return config""",
    "code-json": """{
  "name": "weather-station",
  "version": "2.1.0",
  "sensors": [
    {"id": "t1", "type": "temperature", "unit": "C"},
    {"id": "h1", "type": "humidity", "unit": "%"}
  ],
  "interval": 60
}""",
    "code-yaml": """services:
  web:
    image: nginx:1.27
    ports:
      - "8080:80"
    volumes:
      - ./site:/usr/share/nginx/html:ro
  db:
    image: postgres:17
    environment:
      POSTGRES_PASSWORD: example""",
    "code-shell": """#!/bin/sh
set -eu
for file in *.jpg; do
    convert "$file" -resize 50% "small/$file"
done
echo "Done: $(ls small | wc -l) images\"""",
    "code-sql": """SELECT c.name, COUNT(o.id) AS orders
FROM customers c
LEFT JOIN orders o ON o.customer_id = c.id
WHERE o.created_at >= '2026-01-01'
GROUP BY c.name
ORDER BY orders DESC
LIMIT 10;""",
    "code-html": """<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>Opening hours</title>
</head>
<body>
  <h1>Opening hours</h1>
  <p>Monday to Friday, 9am to 5pm.</p>
</body>
</html>""",
    "code-css": """.card {
  display: flex;
  gap: 1rem;
  padding: 1.5rem;
  border-radius: 8px;
}

.card h2 {
  margin: 0;
  font-size: 1.25rem;
}""",
    "code-diff": """--- a/src/main.c
+++ b/src/main.c
@@ -10,7 +10,7 @@ int main(void)
 {
-    int count = 0;
+    size_t count = 0;
     while (read_line(buffer)) {
         count++;
     }""",
    "code-js": """export function debounce(fn, delay) {
  let timer;
  return (...args) => {
    clearTimeout(timer);
    timer = setTimeout(() => fn(...args), delay);
  };
}""",
    # ------------------------------------------------------------ addresses
    "address-1": """Mr John Smith
42 Station Road
Ashford
Kent
TN23 1PP""",
    "address-2": """The Manager
Harbour View Hotel
3 Quay Street
Falmouth
Cornwall TR11 3HH
United Kingdom""",
    "address-3": """Dr A. Kowalski
Department of Chemistry
University of Northfield
Northfield NF4 7XY""",
    "address-4": """Delivery address:
Flat 5, Rosebank House
17 Hillside Avenue
Bristol
BS6 7JQ
Phone: 07700 900461""",
    "address-5": """Sender:
M. Dubois
12 rue des Lilas
75011 Paris
France""",
    # ------------------------------------------------------------ lists
    "list-shopping": """Shopping
- eggs
- milk
- 2 lemons
- bread (seeded)
- washing-up liquid
- birthday card for Sue""",
    "list-todo": """TODO
[x] renew passport
[ ] book dentist
[ ] fix bike puncture
[ ] send invoice to Hartley & Co
[x] call the bank""",
    "list-numbered": """Before you start:
1. Switch off the power at the fuse box.
2. Remove the old fitting.
3. Note which wire goes where.
4. Connect the new fitting in the same way.
5. Switch the power back on and test.""",
    "list-packing": """Packing list
* passport
* chargers
* walking boots
* waterproof jacket
* sun cream
* travel adapter""",
    "list-agenda": """Agenda
1. Apologies
2. Minutes of the last meeting
3. Treasurer's report
4. Summer fete
5. Any other business""",
    "list-ingredients": """Pancakes (makes 8)
100g plain flour
2 large eggs
300ml milk
1 tbsp sunflower oil
pinch of salt""",
    "list-books": """Books to read this year
The Remains of the Day
Middlemarch
A Month in the Country
The Old Man and the Sea
Persuasion""",
    "list-contacts": """Plumber - Steve - 07700 900812
Electrician - Bright Sparks - 01632 960455
Vet - Willow Lane Vets - 01632 960877
School office - 01632 960300""",
    # ------------------------------------------------------------ markdown
    "markdown-1": """# Project Setup

## Requirements

- Python 3.12
- PostgreSQL 17

## Installation

Clone the repository and install the dependencies:

    pip install -r requirements.txt

Then run `make migrate` to set up the database.""",
    "markdown-2": """## Changelog

### 1.3.0
- Added dark mode
- Fixed crash when opening empty files

### 1.2.1
- Fixed typo in the settings dialog""",
    "markdown-3": """**Meeting notes, 3 Oct**

Attendees: Jo, Sam, Priya

* Budget approved for Q4
* Sam to draft the hiring plan
* Next meeting: 17 Oct

> Remember to update the shared calendar.""",
    "markdown-4": """| Name  | Role      | Start date |
|-------|-----------|------------|
| Anna  | Designer  | 2026-02-01 |
| Ben   | Developer | 2026-03-15 |
| Chloe | Tester    | 2026-04-02 |""",
    "markdown-5": """1. Open **Settings**
2. Choose *Accounts*
3. Click **Add account** and follow the steps

See the [help page](https://example.com/help) for details.""",
    # ------------------------------------------------------------ subtitles
    "subtitles-1": """1
00:00:01,200 --> 00:00:03,800
Where were you last night?

2
00:00:04,100 --> 00:00:06,500
I told you, I was at work.

3
00:00:07,000 --> 00:00:09,200
Until two in the morning?""",
    "subtitles-2": """WEBVTT

00:00:00.000 --> 00:00:02.500
Welcome back to the channel.

00:00:02.800 --> 00:00:06.000
Today we're going to build
a bird feeder from scrap wood.""",
    "subtitles-3": """45
00:03:12,400 --> 00:03:14,900
- Is it ready?
- Almost.

46
00:03:15,300 --> 00:03:17,000
Then let's go.""",
    "subtitles-4": """[MUSIC PLAYING]

NARRATOR: The river has shaped this valley
for ten thousand years.

[BIRDSONG]""",
    # ------------------------------------------------------------ tables
    "table-csv": """date,station,temperature,rainfall
2026-10-01,Northgate,12.4,0.0
2026-10-02,Northgate,11.8,3.2
2026-10-03,Northgate,10.9,7.5
2026-10-04,Northgate,13.1,0.4""",
    "table-tsv": """Item\tQty\tPrice
Notebook\t3\t2.50
Pens (box)\t1\t4.99
Stapler\t1\t7.25""",
    "table-aligned": """Team          P   W   D   L  Pts
Riverside    10   7   2   1   23
Hill Rovers  10   6   3   1   21
Northgate    10   5   2   3   17
Old Town     10   2   1   7    7""",
    "table-timetable": """Mon   07:15  07:45  08:15  08:45
Tue   07:15  07:45  08:15  08:45
Sat   08:30  09:30  10:30  11:30
Sun   no service""",
    "table-results": """Sample  Mass (g)  Volume (ml)  Density
A       12.41     10.0         1.241
B       11.98     10.0         1.198
C       13.07     10.0         1.307""",
    # ------------------------------------------------------------ misc
    "form-1": """Name: Sarah Collins
Date of birth: 14/03/1988
Membership number: 55120
Phone: 07700 900233
Emergency contact: Mark Collins""",
    "log-1": """2026-10-30 09:12:01 INFO  Starting service on port 8080
2026-10-30 09:12:02 INFO  Connected to database
2026-10-30 09:14:37 WARN  Slow query took 2.4s
2026-10-30 09:15:10 ERROR Connection reset by peer""",
    "terminal-1": """$ ls -l
total 12
-rw-r--r-- 1 jo jo  220 Oct 30 09:00 notes.txt
drwxr-xr-x 2 jo jo 4096 Oct 29 18:22 photos
-rwxr-xr-x 1 jo jo  512 Oct 28 11:05 backup.sh""",
    "lyrics-1": """Down by the water where the old boats lie
We counted the stars in a borrowed sky
And you said the morning would never come
But the gulls were singing and the tide was turning""",
    "signage-1": """CAR PARK
Pay and display
Maximum stay 3 hours
No return within 2 hours
Disabled badge holders free""",
    "receipt-1": """THE CORNER CAFE
Flat white            3.20
Toasted teacake       2.80
Orange juice          2.10
TOTAL                 8.10
Card payment
Thank you for visiting!""",
    "quiz-1": """Q1. What is the capital of Australia?
A) Sydney
B) Canberra
C) Melbourne

Q2. How many sides has a hexagon?
A) 5
B) 6
C) 8""",
    "headings-1": """Chapter 3
The Long Road North

Part One
Leaving Home""",
}


def poems():
    """Dickinson poems as typed: one line of verse per line."""
    raw = (gutenberg.SOURCES / "gutenberg-12242.txt").read_text(encoding="utf-8-sig").replace("\r\n", "\n")
    begin = raw.find("Success is counted sweetest")
    stanzas = [s.strip("\n") for s in raw[begin:].split("\n\n") if s.strip()]
    # Stanzas of three lines or more; headings are single lines.
    verse = [s for s in stanzas if len(s.splitlines()) >= 3]
    rng = random.Random("poems")
    chosen = rng.sample(verse[:200], 8)
    return {f"poem-{i + 1}": "\n".join(line.strip() for line in stanza.splitlines())
            for i, stanza in enumerate(chosen)}


def web_prose():
    """Book paragraphs as copied from a web page: one line per paragraph."""
    out = {}
    for i, (book, first) in enumerate([("gutenberg-1342", 120), ("gutenberg-1228", 400),
                                       ("gutenberg-2701", 900), ("gutenberg-1342", 300),
                                       ("gutenberg-1228", 600), ("gutenberg-2701", 1500),
                                       ("gutenberg-1342", 500), ("gutenberg-1228", 800)]):
        count = 1 + i % 3
        out[f"web-{i + 1}"] = "\n\n".join(gutenberg.prose(book, first, count, minimum=150))
    return out


def main():
    NEGATIVE.mkdir(parents=True, exist_ok=True)
    samples = {**SAMPLES, **poems(), **web_prose()}
    for name, text in samples.items():
        (NEGATIVE / f"{name}.txt").write_text(text.rstrip("\n") + "\n", encoding="utf-8")
    print(f"{len(samples)} negatives")


if __name__ == "__main__":
    main()
