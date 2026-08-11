#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BIN_DIR=${BIN_DIR:-$ROOT_DIR/build/sophon/linux}
PACKAGE_DIR=${PACKAGE_DIR:-/data/deliver_dispatch_sdk_v9.5d_candidate_w8bf16}
SHERPA_LIB=${SHERPA_LIB:-/home/xh/itc_project/sherpa-onnx-2025-1217/build-sophon-ort1244-verified/install/lib}
KWS_DIR=${KWS_DIR:-$ROOT_DIR/.cache/models/kws-wenetspeech}
VAD_MODEL=${VAD_MODEL:-$ROOT_DIR/android/app/src/main/assets/silero_vad.onnx}
ASR_MODEL_DIR=${ASR_MODEL_DIR:-/home/xh/itc_project/Sophon_model_zoo/Qwen3-ASR/models}
ASR_HOTWORDS=${ASR_HOTWORDS:-}
BACKEND=${1:-local}

: "${WAKE_AUDIO:?Set WAKE_AUDIO to a 16 kHz wake-word WAV}"
: "${COMMAND_AUDIO:?Set COMMAND_AUDIO to a 16 kHz command WAV}"

export LD_LIBRARY_PATH="$ROOT_DIR/lib:$BIN_DIR:$SHERPA_LIB:$PACKAGE_DIR/lib:/opt/sophon/libsophon-current/lib:${LD_LIBRARY_PATH:-}"

set -- \
  --intent-backend "$BACKEND" \
  --tools "$PACKAGE_DIR/tools_dispatch.json" \
  --embedding "$PACKAGE_DIR/models/bge-small-zh-onnx" \
  --kws-dir "$KWS_DIR" \
  --vad "$VAD_MODEL" \
  --asr-bmodel "$ASR_MODEL_DIR/BM1684X/qwen3_asr_merged_w4g64.bmodel" \
  --asr-tokenizer "$ASR_MODEL_DIR" \
  --wake-audio "$WAKE_AUDIO" \
  --command-audio "$COMMAND_AUDIO"

# Qwen3-ASR prompt hotwords are opt-in. A broad dispatch vocabulary can bias
# unrelated commands, so production defaults to the model without a prompt.
if [ -n "$ASR_HOTWORDS" ]; then
  set -- "$@" --asr-hotwords "$(cat "$ASR_HOTWORDS")"
fi

if [ "$BACKEND" = local ]; then
  exec "$BIN_DIR/ai_audio_voice_intent_demo" "$@" \
    --llm-model "$PACKAGE_DIR/models/model_0.6b_dispatch"
fi
if [ "$BACKEND" = deepseek ]; then
  : "${DEEPSEEK_API_KEY:?Set DEEPSEEK_API_KEY for the cloud intent backend}"
  exec "$BIN_DIR/ai_audio_voice_intent_demo" "$@" \
    --llm-tokenizer "$PACKAGE_DIR/models/model_0.6b_dispatch/config/tokenizer.json"
fi

echo "Unknown backend: $BACKEND (expected local or deepseek)" >&2
exit 2
