#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
PACKAGE_DIR=${PACKAGE_DIR:-/data/deliver_dispatch_sdk_v9.5d_candidate_w8bf16}
BIN_DIR=${BIN_DIR:-$ROOT_DIR/build/sophon/linux}
BACKEND=${1:-local}
TEXT=${2:-把信号源一在主屏上开一个窗口}
SHERPA_LIB=${SHERPA_LIB:-/home/xh/itc_project/sherpa-onnx-2025-1217/build-sophon-ort1244-verified/install/lib}

export LD_LIBRARY_PATH="$ROOT_DIR/lib:$BIN_DIR:$SHERPA_LIB:$PACKAGE_DIR/lib:${LD_LIBRARY_PATH:-}"

if [ "$BACKEND" = local ]; then
  exec "$BIN_DIR/ai_audio_intent_demo" local \
    "$PACKAGE_DIR/models/model_0.6b_dispatch" \
    "$PACKAGE_DIR/tools_dispatch.json" \
    "$PACKAGE_DIR/models/bge-small-zh-onnx" \
    "$TEXT"
fi

if [ "$BACKEND" = deepseek ]; then
  : "${DEEPSEEK_API_KEY:?Set DEEPSEEK_API_KEY for the cloud intent backend}"
  exec "$BIN_DIR/ai_audio_intent_demo" deepseek \
    "$PACKAGE_DIR/models/model_0.6b_dispatch/config/tokenizer.json" \
    "$PACKAGE_DIR/tools_dispatch.json" \
    "$PACKAGE_DIR/models/bge-small-zh-onnx" \
    "$TEXT"
fi

echo "Unknown backend: $BACKEND (expected local or deepseek)" >&2
exit 2
