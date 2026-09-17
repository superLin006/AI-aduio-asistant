#!/bin/sh
set -eu

# Registers three reference speakers (fangjun/leijun/liudehua) from two
# enrollment utterances each, then identifies every test utterance and saves
# the enrollment database.
#
# Usage: sh scripts/run_speaker_demo.sh
# Env:   SPEAKER_MODEL overrides the default model; THRESHOLD sets the match
#        threshold; SPEAKERS_JSON overrides the persistence file.

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BIN_DIR=${BIN_DIR:-$ROOT_DIR/build/sophon/linux}
MODEL_DIR=${MODEL_DIR:-$ROOT_DIR/.cache/models/speaker}
SHERPA_LIB=${SHERPA_LIB:-/home/xh/itc_project/sherpa-onnx-2025-1217/build-sophon-ort1244-verified/install/lib}
PACKAGE_DIR=${PACKAGE_DIR:-/data/deliver_dispatch_sdk_v9.5d_candidate_w8bf16}
MODEL=${SPEAKER_MODEL:-3dspeaker_speech_eres2netv2_sv_zh-cn_16k-common.onnx}
THRESHOLD=${THRESHOLD:-0.5}
SPEAKERS_JSON=${SPEAKERS_JSON:-$ROOT_DIR/.cache/models/speaker/speakers.json}

export LD_LIBRARY_PATH="$ROOT_DIR/lib:$BIN_DIR:$SHERPA_LIB:$PACKAGE_DIR/lib:/opt/sophon/libsophon-current/lib:${LD_LIBRARY_PATH:-}"

exec "$BIN_DIR/ai_audio_speaker_demo" \
  --speaker-model "$MODEL_DIR/$MODEL" \
  --threshold "$THRESHOLD" \
  --register "fangjun=$MODEL_DIR/fangjun-sr-1.wav" \
  --register "fangjun=$MODEL_DIR/fangjun-sr-2.wav" \
  --register "leijun=$MODEL_DIR/leijun-sr-1.wav" \
  --register "leijun=$MODEL_DIR/leijun-sr-2.wav" \
  --register "liudehua=$MODEL_DIR/liudehua-sr-1.wav" \
  --register "liudehua=$MODEL_DIR/liudehua-sr-2.wav" \
  --verify "fangjun=$MODEL_DIR/fangjun-sr-1.wav" \
  --identify "$MODEL_DIR/fangjun-test-sr-1.wav" \
  --identify "$MODEL_DIR/fangjun-test-sr-2.wav" \
  --identify "$MODEL_DIR/leijun-test-sr-1.wav" \
  --identify "$MODEL_DIR/leijun-test-sr-2.wav" \
  --identify "$MODEL_DIR/leijun-test-sr-3.wav" \
  --identify "$MODEL_DIR/liudehua-test-sr-1.wav" \
  --identify "$MODEL_DIR/liudehua-test-sr-2.wav" \
  --save "$SPEAKERS_JSON"
