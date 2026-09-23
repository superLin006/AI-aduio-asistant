#!/usr/bin/env python3
"""Summarize ai_audio_batch_regression_demo JSONL output."""

import argparse
import json
import statistics
from difflib import SequenceMatcher
from pathlib import Path


def percentile(values, fraction):
    if not values:
        return 0.0
    ordered = sorted(values)
    return ordered[round((len(ordered) - 1) * fraction)]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("jsonl", type=Path)
    parser.add_argument("--matched-cases", type=Path)
    parser.add_argument("--matched-source-cases", type=Path)
    args = parser.parse_args()

    rows = []
    for raw in args.jsonl.read_text(encoding="utf-8").splitlines():
        if raw.startswith("JSONL: "):
            rows.append(json.loads(raw[7:]))
    successful = [row for row in rows if row.get("ok")]
    exact = [row for row in successful if row["asr_text"] == row["source_text"]]
    plan_matches = [row for row in successful if row.get("plan_match")]
    similarities = [
        SequenceMatcher(None, row["source_text"], row["asr_text"]).ratio()
        for row in successful
    ]
    asr_ms = [row["asr_ms"] for row in successful]
    intent_ms = [row["intent_ms"] for row in successful]
    report = {
        "total": len(rows),
        "successful": len(successful),
        "errors": len(rows) - len(successful),
        "asr_exact": len(exact),
        "asr_similarity_mean": statistics.fmean(similarities) if similarities else 0.0,
        "semantic_plan_match": len(plan_matches),
        "semantic_plan_match_rate": len(plan_matches) / len(successful) if successful else 0.0,
        "asr_ms_p50": percentile(asr_ms, 0.50),
        "asr_ms_p95": percentile(asr_ms, 0.95),
        "intent_ms_p50": percentile(intent_ms, 0.50),
        "intent_ms_p95": percentile(intent_ms, 0.95),
    }
    print(json.dumps(report, ensure_ascii=False, indent=2))

    if args.matched_cases:
        args.matched_cases.write_text(
            "\n".join(row["asr_text"] for row in plan_matches) + "\n",
            encoding="utf-8",
        )
    if args.matched_source_cases:
        args.matched_source_cases.write_text(
            "# ChatTTS 实测语义计划稳定的口语用例；仅用于离线语音回归。\n"
            + "\n".join(row["source_text"] for row in plan_matches)
            + "\n",
            encoding="utf-8",
        )


if __name__ == "__main__":
    main()
