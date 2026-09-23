#!/usr/bin/env python3
"""
Qwen3-ASR 内容校验脚本。
用 PyTorch Qwen3-ASR 逐条转写合成 wav，与输入文本归一化对比，输出 quality report。
"""

import argparse, csv, os, re, sys, time


def normalize(s: str) -> str:
    """去标点、全半角归一化、小写"""
    s = s.strip()
    # Full-width → half-width
    s = s.replace("，", ",").replace("。", ".").replace("？", "?").replace("！", "!")
    s = s.replace("：", ":").replace("；", ";").replace("（", "(").replace("）", ")")
    s = s.replace("“", "\"").replace("”", "\"").replace("‘", "'").replace("’", "'")
    s = s.replace(" ", "").replace("  ", "")
    # Remove punctuation (keep Chinese chars + digits + letters)
    s = re.sub(r"[^\w\u4e00-\u9fff]", "", s)
    return s.lower()


def cer(ref: str, hyp: str) -> float:
    """Character error rate via Levenshtein distance."""
    n, m = len(ref), len(hyp)
    dp = [[0] * (m + 1) for _ in range(n + 1)]
    for i in range(n + 1):
        dp[i][0] = i
    for j in range(m + 1):
        dp[0][j] = j
    for i in range(1, n + 1):
        for j in range(1, m + 1):
            cost = 0 if ref[i - 1] == hyp[j - 1] else 1
            dp[i][j] = min(dp[i - 1][j] + 1, dp[i][j - 1] + 1, dp[i - 1][j - 1] + cost)
    return dp[n][m] / max(n, 1)


def main():
    parser = argparse.ArgumentParser(description="ASR 内容校验")
    parser.add_argument("--manifest",
                        default=os.path.join(
                            os.path.dirname(__file__) or ".",
                            "..", "..", ".cache", "models", "speaker_eval", "manifest.tsv"))
    parser.add_argument("--wav_dir", default=None)
    parser.add_argument("--model_dir",
                        default="/home/xh/itc_project/Sophon_model_zoo/Qwen3-ASR/models/qwen3-asr-0.6b")
    parser.add_argument("--output", default=None)  # replaces manifest path with _report
    parser.add_argument("--cer_threshold", type=float, default=0.3,
                        help="CER 上限，超过此值标记 FAIL")
    args = parser.parse_args()

    # Read manifest
    csv.field_size_limit(2**20)
    rows = []
    wav_dir = args.wav_dir or os.path.join(os.path.dirname(args.manifest), "wav16k")
    with open(args.manifest, "r", encoding="utf-8") as f:
        reader = csv.DictReader(f, delimiter="\t")
        for row in reader:
            row["wav_path"] = os.path.join(wav_dir, row["wav"])
            rows.append(row)

    print(f"Loaded {len(rows)} utterances from {args.manifest}", flush=True)

    # Load ASR model
    import torch
    device = "cuda" if torch.cuda.is_available() else "cpu"
    print(f"Device: {device}", flush=True)
    print(f"Loading ASR model from {args.model_dir} ...", flush=True)
    t0 = time.time()
    from transformers import AutoModelForCausalLM, AutoProcessor
    model = AutoModelForCausalLM.from_pretrained(
        args.model_dir, device_map=device, torch_dtype=torch.bfloat16,
        trust_remote_code=True)
    processor = AutoProcessor.from_pretrained(args.model_dir)
    print(f"ASR model loaded in {time.time()-t0:.1f}s", flush=True)

    # Transcribe & evaluate
    import soundfile as sf
    results = []
    t_start = time.time()
    for i, row in enumerate(rows):
        text_in = row["text"]
        wav_path = row["wav_path"]
        try:
            audio, sr = sf.read(wav_path)
            if audio.ndim > 1:
                audio = audio.mean(-1)
            inputs = processor(audio, sampling_rate=sr, return_tensors="pt").to(device)
            with torch.no_grad():
                output = model.generate(**inputs, max_new_tokens=64)
            text_out = processor.decode(output[0], skip_special_tokens=True)
        except Exception as e:
            results.append({
                "wav": row["wav"], "speaker": row["speaker"],
                "text_in": text_in, "text_out": "",
                "cer": 1.0, "pass": False, "error": str(e),
            })
            print(f"[ERR] {row['wav']}: {e}", flush=True)
            continue

        ref = normalize(text_in)
        hyp = normalize(text_out)
        c = cer(ref, hyp)
        passed = c <= args.cer_threshold
        results.append({
            "wav": row["wav"], "speaker": row["speaker"],
            "text_in": text_in, "text_out": text_out,
            "cer": c, "pass": passed, "error": "",
        })
        if (i + 1) % 20 == 0:
            elapsed = time.time() - t_start
            rate = (i + 1) / elapsed
            print(f"  {i+1}/{len(rows)} ({rate:.1f}/s)", flush=True)

    elapsed = time.time() - t_start
    passed = sum(1 for r in results if r["pass"])
    print(f"\nResults: {passed}/{len(results)} PASS "
          f"({passed/len(results)*100:.1f}%) in {elapsed:.0f}s "
          f"({len(results)/elapsed:.1f}/s)", flush=True)

    # Write report
    out_path = args.output or (args.manifest.rsplit(".", 1)[0] + "_report.tsv")
    fieldnames = ["wav", "speaker", "cer", "pass", "text_in", "text_out", "error"]
    with open(out_path, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=fieldnames, delimiter="\t")
        w.writeheader()
        for r in results:
            w.writerow(r)
    print(f"Report written to {out_path}", flush=True)

    # Print FAIL summary
    fails = [r for r in results if not r["pass"]]
    if fails:
        print(f"\nFAILED ({len(fails)}):")
        for r in fails:
            print(f"  {r['wav']} cer={r['cer']:.3f}  in=<<{r['text_in'][:40]}>>  "
                  f"out=<<{r['text_out'][:40]}>>")


if __name__ == "__main__":
    main()