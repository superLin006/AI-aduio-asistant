#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
MODEL_ZOO=${MODEL_ZOO:-/home/xh/itc_project/Sophon_model_zoo/Qwen3-ASR}
BUILD_DIR=${BUILD_DIR:-$ROOT_DIR/build/sophon}
SHERPA_INSTALL=${SHERPA_INSTALL:-/home/xh/itc_project/sherpa-onnx-2025-1217/build-sophon-ort1244-verified/install}
RUNTIME_PACKAGE=${RUNTIME_PACKAGE:-/home/xh/itc_project/deliver_dispatch_sdk_v9.5d_candidate_w8bf16}

export LD_LIBRARY_PATH="$BUILD_DIR/linux:$SHERPA_INSTALL/lib:$RUNTIME_PACKAGE/lib:/opt/sophon/libsophon-current/lib:${LD_LIBRARY_PATH:-}"
exec "$BUILD_DIR/linux/ai_audio_offline_demo" \
  --bmodel "$MODEL_ZOO/models/BM1684X/qwen3_asr_merged_w4g64.bmodel" \
  --tokenizer "$MODEL_ZOO/models" \
  --audio "${1:-$MODEL_ZOO/test_data/test_zh.wav}"
