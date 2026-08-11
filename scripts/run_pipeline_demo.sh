#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD_DIR=${BUILD_DIR:-$ROOT_DIR/build/sophon/linux}
KWS_DIR=${KWS_DIR:-$ROOT_DIR/.cache/models/kws-wenetspeech}
VAD_MODEL=${VAD_MODEL:-$ROOT_DIR/android/app/src/main/assets/silero_vad.onnx}
QWEN_ZOO=${QWEN_ZOO:-/home/xh/itc_project/Sophon_model_zoo/Qwen3-ASR}
SHERPA_LIB=${SHERPA_LIB:-/home/xh/itc_project/sherpa-onnx-2025-1217/build-sophon-ort1244-verified/install/lib}
RUNTIME_PACKAGE=${RUNTIME_PACKAGE:-/home/xh/itc_project/deliver_dispatch_sdk_v9.5d_candidate_w8bf16}

export LD_LIBRARY_PATH="$BUILD_DIR:$SHERPA_LIB:$RUNTIME_PACKAGE/lib:/opt/sophon/libsophon-current/lib:${LD_LIBRARY_PATH:-}"
exec "$BUILD_DIR/ai_audio_pipeline_demo" \
  --kws-dir "$KWS_DIR" --vad "$VAD_MODEL" \
  --bmodel "$QWEN_ZOO/models/BM1684X/qwen3_asr_merged_w4g64.bmodel" \
  --tokenizer "${TOKENIZER_DIR:?Set TOKENIZER_DIR to a directory containing vocab.json and merges.txt}" \
  --wake-audio "${WAKE_AUDIO:?Set WAKE_AUDIO to a 16 kHz wake-word WAV}" \
  --command-audio "${COMMAND_AUDIO:?Set COMMAND_AUDIO to a 16 kHz command WAV}"
