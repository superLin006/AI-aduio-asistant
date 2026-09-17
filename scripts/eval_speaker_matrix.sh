#!/bin/sh
set -eu
# 声纹评测矩阵（板卡执行）：注册 K 条/音色 → --best-match 全部测试音频
# EXCLUDE_SPEAKERS="Dylan Sohee" 时：被排除音色不注册，其全部音频作为"未注册探针"进入 best-match

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
EVAL_DIR=${EVAL_DIR:-/root/speaker_test/speaker_eval}
SPEAKER_MODEL=${SPEAKER_MODEL:-$EVAL_DIR/../3dspeaker_speech_eres2netv2_sv_zh-cn_16k-common.onnx}
THRESHOLD=${THRESHOLD:-0.5}
EXCLUDE_SPEAKERS=${EXCLUDE_SPEAKERS:-}

export LD_LIBRARY_PATH=/root/speaker_test:/data/deliver_dispatch_sdk_v9.5d_candidate_w8bf16/lib:/opt/sophon/libsophon-current/lib:${LD_LIBRARY_PATH:-}
mkdir -p "$EVAL_DIR" && cd "$EVAL_DIR"

echo "Building register/best-match lists ..."
EXCLUDE_SPEAKERS="$EXCLUDE_SPEAKERS" python3 <<'PYEOF'
import csv, re, os
excluded = {s for s in re.split(r"[\s,]+", os.environ.get("EXCLUDE_SPEAKERS", "")) if s}
rows = list(csv.DictReader(open("manifest.tsv"), delimiter="\t"))
def sk(r):
    m = re.search(r'_t(\d+)', r['wav'])
    return (r['speaker'], int(m.group(1)) if m else 0)
rows.sort(key=sk)
reg, best, seen = [], [], {}
for r in rows:
    spk = r['speaker']
    if spk in excluded:
        best.append('wav16k/' + r['wav'])  # 未注册探针：全部进入 best-match
        continue
    seen.setdefault(spk, 0)
    if seen[spk] < 3:
        reg.append('--register ' + spk + '=wav16k/' + r['wav'])
    else:
        best.append('wav16k/' + r['wav'])
    seen[spk] += 1
print(f'{len(reg)} register files, {len(best)} best-match files (excluded={sorted(excluded)})')
open("register_cmds.txt","w").write(' '.join(reg) + '\n')
open("best_cmds.txt","w").write('\n'.join(best) + '\n')
PYEOF

REG_ARGS=$(cat register_cmds.txt)
echo "=== REGISTRATION ==="
/root/speaker_test/ai_audio_speaker_demo \
  --speaker-model "$SPEAKER_MODEL" --threshold "$THRESHOLD" \
  $REG_ARGS --save "$EVAL_DIR/speakers.json"

echo "=== BEST-MATCH ==="
/root/speaker_test/ai_audio_speaker_demo \
  --speaker-model "$SPEAKER_MODEL" \
  --load "$EVAL_DIR/speakers.json" \
  $(cat best_cmds.txt | while read w; do echo --best-match "$w"; done) \
  2>&1 | tee "$EVAL_DIR/best_results.txt"