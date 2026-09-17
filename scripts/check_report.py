#!/usr/bin/env python3
import csv
with open("/tmp/board_report.tsv") as f:
    rows = list(csv.DictReader(f, delimiter="\t"))
passes = sum(1 for r in rows if str(r.get("pass","")).lower() == "true")
fails = sum(1 for r in rows if str(r.get("pass","")).lower() != "true")
print(f"Total: {len(rows)}, PASS: {passes}, FAIL: {fails}")
if rows:
    print(f"pass column samples: {[(r['wav'], r['pass']) for r in rows[:5]]}")