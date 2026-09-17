#!/bin/bash
set -e
: "${BOARD_PASS:?export BOARD_PASS first (see 0_Note/board_credentials.md)}"
export SSHPASS="$BOARD_PASS"
SSH="sshpass -e ssh -o StrictHostKeyChecking=no root@172.16.25.248"
$SSH bash -s <<'REMOTE'
set -e
cd /root/speaker_test
export LD_LIBRARY_PATH=/root/speaker_test:/data/deliver_dispatch_sdk_v9.5d_candidate_w8bf16/lib:/opt/sophon/libsophon-current/lib

echo "===== TEST 1: enroll 2 utterances per speaker + save ====="
./ai_audio_speaker_demo \
  --speaker-model 3dspeaker_speech_eres2netv2_sv_zh-cn_16k-common.onnx \
  --threshold 0.5 \
  --register fangjun=fangjun-sr-1.wav --register fangjun=fangjun-sr-2.wav \
  --register leijun=leijun-sr-1.wav --register leijun=leijun-sr-2.wav \
  --register liudehua=liudehua-sr-1.wav --register liudehua=liudehua-sr-2.wav \
  --identify fangjun-test-sr-1.wav --identify fangjun-test-sr-2.wav \
  --identify leijun-test-sr-1.wav --identify leijun-test-sr-2.wav --identify leijun-test-sr-3.wav \
  --identify liudehua-test-sr-1.wav --identify liudehua-test-sr-2.wav \
  --save speakers.json

echo
echo "===== TEST 2: persistence (load speakers.json + identify) ====="
./ai_audio_speaker_demo \
  --speaker-model 3dspeaker_speech_eres2netv2_sv_zh-cn_16k-common.onnx \
  --threshold 0.5 \
  --load speakers.json \
  --identify fangjun-test-sr-1.wav --identify leijun-test-sr-1.wav --identify liudehua-test-sr-1.wav

echo
echo "===== TEST 3: pipeline E2E (wake + fangjun command + voiceprint) ====="
./ai_audio_pipeline_demo \
  --kws-dir kws \
  --vad /data/qwen3_asr/silero_vad.onnx \
  --bmodel /data/qwen3_asr/qwen3_asr_merged_w8bf16.bmodel \
  --tokenizer /data/models/qwen3-asr/config \
  --wake-audio kws/3.wav \
  --command-audio fangjun-test-sr-1.wav \
  --speaker-model 3dspeaker_speech_eres2netv2_sv_zh-cn_16k-common.onnx \
  --speaker-speakers speakers.json

echo
echo "===== TEST 4: pipeline without voiceprint (regression) ====="
./ai_audio_pipeline_demo \
  --kws-dir kws \
  --vad /data/qwen3_asr/silero_vad.onnx \
  --bmodel /data/qwen3_asr/qwen3_asr_merged_w8bf16.bmodel \
  --tokenizer /data/models/qwen3-asr/config \
  --wake-audio kws/3.wav \
  --command-audio fangjun-test-sr-1.wav

echo "ALL_TESTS_DONE"
REMOTE
