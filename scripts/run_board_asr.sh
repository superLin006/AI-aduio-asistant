#!/bin/bash
set -eu
: "${BOARD_PASS:?export BOARD_PASS first (see 0_Note/board_credentials.md)}"
export SSHPASS="$BOARD_PASS"
SSH="sshpass -e ssh -o StrictHostKeyChecking=no root@172.16.25.248"
# Deploy offline_demo and run batch ASR on board
$SSH bash -s <<'REMOTE'
set -eu
cd /root/speaker_test
export LD_LIBRARY_PATH=/root/speaker_test:/data/deliver_dispatch_sdk_v9.5d_candidate_w8bf16/lib:/opt/sophon/libsophon-current/lib

echo "speaker	wav	text_in	text_out	cer" > /data/Qwen3-TTS/asr_results.tsv
OK=0; FAIL=0; TOTAL=0
for f in /data/Qwen3-TTS/speaker_eval_out/*.wav; do
  TOTAL=$((TOTAL+1))
  # Extract speaker + text index from filename
  base=$(basename "$f" .wav)
  spk=${base%_t*}
  idx=${base##*_t}
  # Speech-to-text
  text_out=$(./ai_audio_offline_demo \
    --bmodel /data/qwen3_asr/qwen3_asr_merged_w8bf16.bmodel \
    --tokenizer /data/models/qwen3-asr/config \
    --audio "$f" 2>/dev/null | grep -v '^/workspace\|^open usercpu\|^\[Timing\]')
  # Collapse to single line
  text_out=$(echo "$text_out" | tr -d '\n')
  echo "$spk	$base	$idx	$text_out" >> /data/Qwen3-TTS/asr_results.tsv
  if [ $((TOTAL % 30)) -eq 0 ]; then echo "  $TOTAL/180 done"; fi
done
echo "DONE: $TOTAL files"
REMOTE