#!/usr/bin/env python3
"""
声纹评测分析脚本。
解析 eval_speaker_matrix.sh 输出的 best_results.txt，生成：
- 已注册音色的混淆矩阵、逐音色/总体准确率、同/跨音色分布、阈值扫描
- 未注册探针音色（--probe-speakers，如从注册库剔除的 Dylan）的
  预测分布、得分统计、以及在各阈值下的"正确拒绝率"
用法: analyze_speaker_eval.py <best_results.txt> [--out-prefix X] [--probe-speakers "Dylan"]
"""

import argparse, re, csv, json, os
from collections import defaultdict


def parse_best_results(path: str):
    """Parse best_results.txt: lines like 'best=<wav> -> name=<n> score=<s>'"""
    results = []
    speakers = set()
    with open(path) as f:
        for line in f:
            m = re.search(r"best=(.+) -> name=(\S+) score=([\d.eE+-]+)", line)
            if m:
                wav = m.group(1)
                name = m.group(2)
                score = float(m.group(3))
                wav_base = os.path.basename(wav)
                gt = wav_base.split("_t")[0] if "_t" in wav_base else "unknown"
                speakers.add(gt)
                results.append({"wav": wav_base, "gt": gt, "pred": name, "score": score})
    return results, sorted(speakers)


def confusion_matrix(results, speakers):
    cm = {s: {t: 0 for t in speakers} for s in speakers}
    for r in results:
        cm[r["gt"]][r["pred"]] += 1
    return cm


def print_confusion(cm, speakers):
    print("\n=== Confusion Matrix (rows=ground truth, cols=predicted) ===")
    header = "{:>14}".format("")
    for s in speakers:
        header += f"{s:>10}"
    print(header)
    for gt in speakers:
        row = f"{gt:>14}"
        for pred in speakers:
            row += f"{cm[gt][pred]:>10}"
        print(row)


def threshold_scan(results, thresholds):
    """For each threshold: TPR/FRR over same-speaker, FAR over cross-speaker."""
    rows = []
    for t in thresholds:
        correct = false_accept = false_reject = 0
        total_same = total_diff = 0
        for r in results:
            if r["gt"] == r["pred"]:
                total_same += 1
                if r["score"] >= t:
                    correct += 1
                else:
                    false_reject += 1
            else:
                total_diff += 1
                if r["score"] >= t:
                    false_accept += 1
        rows.append({"threshold": t,
                     "tpr": correct / max(total_same, 1),
                     "far": false_accept / max(total_diff, 1),
                     "frr": false_reject / max(total_same, 1),
                     "correct": correct, "false_accept": false_accept,
                     "false_reject": false_reject})
    return rows


def print_distribution(name, scores):
    if not scores:
        print(f"  {name}: no samples")
        return
    import statistics
    print(f"  {name}: n={len(scores)} mean={statistics.mean(scores):.4f} "
          f"median={statistics.median(scores):.4f} "
          f"min={min(scores):.4f} max={max(scores):.4f}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("results_path")
    ap.add_argument("--out-prefix", default=None)
    ap.add_argument("--probe-speakers", default="",
                    help="未注册音色（空格/逗号分隔），其音频作为开放集探针单独分析")
    args = ap.parse_args()
    probes = {s for s in re.split(r"[\s,]+", args.probe_speakers) if s}
    out_prefix = args.out_prefix or os.path.splitext(args.results_path)[0]

    results, all_speakers = parse_best_results(args.results_path)
    registered = [s for s in all_speakers if s not in probes]
    reg_results = [r for r in results if r["gt"] not in probes]
    probe_results = [r for r in results if r["gt"] in probes]

    print(f"Parsed {len(results)} results, {len(all_speakers)} GT speakers: {all_speakers}")
    print(f"Registered: {registered}  |  Unregistered probes: {sorted(probes)}")

    cm = confusion_matrix(reg_results, registered)
    print_confusion(cm, registered)

    print("\n=== Per-speaker Accuracy (registered only) ===")
    for s in registered:
        total = sum(cm[s].values())
        correct = cm[s][s]
        print(f"  {s:>10}: {correct}/{total} = {correct/max(total,1)*100:.1f}%")

    overall = sum(cm[s][s] for s in registered)
    total = sum(sum(cm[s].values()) for s in registered)
    print(f"\n  Overall: {overall}/{total} = {overall/max(total,1)*100:.1f}%")

    same, cross = [], []
    for r in reg_results:
        (same if r["gt"] == r["pred"] else cross).append(r["score"])
    print("\n=== Score Distribution (registered only) ===")
    print_distribution("Same speaker ", same)
    print_distribution("Cross speaker", cross)

    thresholds = [round(t * 0.05, 2) for t in range(6, 17)]  # 0.30 .. 0.80
    scan = threshold_scan(reg_results, thresholds)
    print("\n=== Threshold Scan (registered only) ===")
    print(f"{'thresh':>7}  {'TPR':>6}  {'FAR':>6}  {'FRR':>6}  "
          f"{'correct':>7}  {'fa':>7}  {'fr':>7}")
    for row in scan:
        print(f"{row['threshold']:>7.2f}  {row['tpr']:>6.3f}  {row['far']:>6.3f}  "
              f"{row['frr']:>6.3f}  {row['correct']:>7}  "
              f"{row['false_accept']:>7}  {row['false_reject']:>7}")
    best = min(scan, key=lambda r: abs(r["far"] - r["frr"]))
    print(f"\nRecommended threshold: {best['threshold']:.2f} "
          f"(TPR={best['tpr']:.3f} FAR={best['far']:.3f} FRR={best['frr']:.3f})")

    # Probe section: unregistered speakers should be rejected at the threshold
    probe_info = {}
    if probe_results:
        print("\n=== Unregistered Probes (should be rejected) ===")
        for spk in sorted(probes):
            rows = [r for r in probe_results if r["gt"] == spk]
            preds = defaultdict(int)
            for r in rows:
                preds[r["pred"]] += 1
            scores = [r["score"] for r in rows]
            print(f"\n  {spk}: n={len(rows)}  mean_score={sum(scores)/max(len(scores),1):.4f}  "
                  f"min={min(scores):.4f}  max={max(scores):.4f}")
            for p, c in sorted(preds.items(), key=lambda kv: -kv[1]):
                print(f"      -> {p}: {c}")
            rej = {}
            for row in scan:
                t = row["threshold"]
                rej[t] = sum(1 for s in scores if s < t)
            print("      rejected at threshold: " +
                  "  ".join(f"{t:.2f}:{rej[t]}/{len(scores)}" for t in thresholds))
            probe_info[spk] = {
                "n": len(rows),
                "mean_score": sum(scores) / max(len(scores), 1),
                "pred_distribution": dict(preds),
                "rejected_at_threshold": {f"{t:.2f}": rej[t] for t in thresholds},
            }

    cm_json = {s: cm[s] for s in registered}
    report = {
        "registered_speakers": registered,
        "probe_speakers": sorted(probes),
        "overall_accuracy": overall / max(total, 1),
        "per_speaker_accuracy": {s: cm[s][s] / max(sum(cm[s].values()), 1) for s in registered},
        "confusion_matrix": cm_json,
        "threshold_scan": scan,
        "recommended_threshold": best["threshold"],
        "score_distribution": {
            "same_speaker": {"n": len(same), "mean": sum(same) / max(len(same), 1)},
            "cross_speaker": {"n": len(cross), "mean": sum(cross) / max(len(cross), 1)},
        },
        "probe_speakers_detail": probe_info,
    }
    with open(out_prefix + "_report.json", "w") as f:
        json.dump(report, f, indent=2, default=str)
    with open(out_prefix + "_scan.csv", "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=["threshold", "tpr", "far", "frr",
                                          "correct", "false_accept", "false_reject"])
        w.writeheader()
        w.writerows(scan)

    print(f"\nReports written: {out_prefix}_report.json, {out_prefix}_scan.csv")


if __name__ == "__main__":
    main()
