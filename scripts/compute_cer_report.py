#!/usr/bin/env python3
"""Merge ASR results with corpus, compute CER, output content_report.tsv."""
import csv, re, os, sys

CORPUS = [
    "打开投影仪","关闭电灯","把音量调大","空调调到二十六度","播放下一首","暂停播放",
    "开始上课","下课","打开窗帘","调亮灯光","静音","提高亮度到百分之八十",
    "把温度调到二十二度","切换到 HDMI 输入","降低屏幕亮度","今天星期几",
    "帮我查一下明天的天气","开始录制","停止录音","打开麦克风",
]

def cer(ref, hyp):
    n, m = len(ref), len(hyp)
    dp = [[0]*(m+1) for _ in range(n+1)]
    for i in range(n+1): dp[i][0] = i
    for j in range(m+1): dp[0][j] = j
    for i in range(1, n+1):
        for j in range(1, m+1):
            c = 0 if ref[i-1] == hyp[j-1] else 1
            dp[i][j] = min(dp[i-1][j]+1, dp[i][j-1]+1, dp[i-1][j-1]+c)
    return dp[n][m] / max(n, 1)

def normalize(s):
    """Remove punctuation and spaces for comparison."""
    s = re.sub(r"[^\w\u4e00-\u9fff]", "", s)
    return s.lower()

base = os.path.join(os.path.dirname(__file__) or ".", "..", ".cache", "models", "speaker_eval")
results_path = os.path.join(base, "asr_results.tsv")
report_path = os.path.join(base, "content_report.tsv")

rows = []
with open(results_path, encoding="utf-8") as f:
    reader = csv.DictReader(f, delimiter="\t")
    for r in reader:
        m = re.search(r"_t(\d+)", r["wav"])
        idx = int(m.group(1)) if m else 0
        text_in = CORPUS[idx] if idx < len(CORPUS) else ""
        text_out = r.get("text_out", "")
        c = cer(normalize(text_in), normalize(text_out))
        passed = c < 0.3
        rows.append({"wav": r["wav"], "speaker": r["speaker"],
                      "text_in": text_in, "text_out": text_out,
                      "cer": c, "pass": passed})

with open(report_path, "w", newline="", encoding="utf-8") as f:
    w = csv.DictWriter(f, fieldnames=["wav","speaker","cer","pass","text_in","text_out"], delimiter="\t")
    w.writeheader(); w.writerows(rows)

passed = sum(1 for r in rows if r["pass"])
print(f"Total: {len(rows)}, PASS: {passed} ({passed/len(rows)*100:.1f}%), FAIL: {len(rows)-passed}")
if passed < len(rows):
    print("FAIL samples:")
    for r in rows:
        if not r["pass"]:
            print(f"  {r['wav']} cer={r['cer']:.3f} ref=<<{r['text_in']}>> hyp=<<{r['text_out']}>>")