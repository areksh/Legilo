#!/usr/bin/env python3
"""Fits PDF detection's weights and threshold on the corpus's tuning set.

Reads the features tst_detectioncorpus writes, fits a logistic regression
on the tuning set (and any local samples), and picks the threshold with
the highest recall whose tuning precision is at least 0.99, a margin
over the 0.98 target. The held-out set plays no part. Prints the weights
to copy into DetectorWeights (src/core/text/PdfDetector.h).

    LEGILO_CORPUS_FEATURES=features.tsv build/linux-debug/tests/unit/tst_detectioncorpus
    python3 -I tests/corpus/tools/tune_detector.py features.tsv

Weights are printed divided by the threshold, so the threshold is always
1: a fit's scale is arbitrary, and this keeps one fit comparable with the
next. Features absent from the corpus (owner, html: no sample has the
hint) keep their design weights (7.2.3), scaled the same way from the
design's threshold of 0.45.
"""
import csv
import math
import sys

FITTED = ["wrap", "midline", "hyphen", "ligature", "pagenum", "list", "symbols"]
KEPT = {"owner": 0.10, "html": -0.05}
DESIGN_THRESHOLD = 0.45
PRECISION = 0.99


def fit(rows, steps=20000, rate=0.5, l2=0.001):
    weights = [0.0] * len(FITTED)
    bias = 0.0
    n = len(rows)
    for _ in range(steps):
        grad = [0.0] * len(FITTED)
        grad_bias = 0.0
        for features, label in rows:
            z = bias + sum(w * x for w, x in zip(weights, features))
            error = 1 / (1 + math.exp(-z)) - label
            for i, x in enumerate(features):
                grad[i] += error * x
            grad_bias += error
        for i in range(len(weights)):
            weights[i] -= rate * (grad[i] / n + l2 * weights[i])
        bias -= rate * grad_bias / n
    return weights, bias


def scores(rows, weights):
    return [(sum(w * x for w, x in zip(weights, features)), label) for features, label in rows]


def evaluate(scored, threshold):
    tp = sum(1 for s, l in scored if s >= threshold and l)
    fp = sum(1 for s, l in scored if s >= threshold and not l)
    fn = sum(1 for s, l in scored if s < threshold and l)
    precision = tp / (tp + fp) if tp + fp else 1.0
    recall = tp / (tp + fn) if tp + fn else 1.0
    return precision, recall


def main():
    data = list(csv.DictReader(open(sys.argv[1]), delimiter="\t"))
    rows = [([float(r[f]) for f in FITTED], int(r["positive"])) for r in data
            if r["set"] in ("tuning", "local")]
    weights, bias = fit(rows)
    scored = scores(rows, weights)
    best = None
    for threshold in sorted({s for s, _ in scored}):
        precision, recall = evaluate(scored, threshold)
        if precision >= PRECISION and (best is None or recall > best[2]):
            best = (threshold, precision, recall)
    threshold, precision, recall = best
    # Leave room below the lowest positive kept: the cut sits halfway to
    # the next lower score.
    lower = max((s for s, _ in scored if s < threshold), default=threshold - 0.1)
    threshold = (threshold + lower) / 2
    print(f"tuning set: precision {precision:.3f}, recall {recall:.3f} ({len(rows)} samples)")
    for name, weight in zip(FITTED, weights):
        print(f"    double {name} = {weight / threshold:.2f};")
    for name, weight in KEPT.items():
        print(f"    double {name} = {weight / DESIGN_THRESHOLD:.2f}; (not in the corpus)")
    print("    double threshold = 1.0;")


if __name__ == "__main__":
    main()
