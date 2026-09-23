#!/bin/sh
set -eu

# Downloads a sherpa-onnx speaker embedding model and the reference test
# utterances (fangjun/leijun/liudehua) used by the speaker demo.
#
# Usage: sh tools/data/download_speaker_model.sh
# Env:   SPEAKER_MODEL overrides the default eres2netv2 zh-cn model.

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
OUT_DIR=${OUT_DIR:-$ROOT_DIR/.cache/models/speaker}
BASE_URL=${BASE_URL:-https://github.com/k2-fsa/sherpa-onnx/releases/download/speaker-recongition-models}
MODEL=${SPEAKER_MODEL:-3dspeaker_speech_eres2netv2_sv_zh-cn_16k-common.onnx}

WAVS="
fangjun-sr-1.wav fangjun-sr-2.wav fangjun-test-sr-1.wav fangjun-test-sr-2.wav
leijun-sr-1.wav leijun-sr-2.wav leijun-test-sr-1.wav leijun-test-sr-2.wav leijun-test-sr-3.wav
liudehua-sr-1.wav liudehua-sr-2.wav liudehua-test-sr-1.wav liudehua-test-sr-2.wav
"

mkdir -p "$OUT_DIR"

download() {
  name=$1
  if [ -s "$OUT_DIR/$name" ]; then
    echo "already present: $name"
    return
  fi
  echo "downloading $name"
  wget -q --show-progress -O "$OUT_DIR/$name" "$BASE_URL/$name"
}

download "$MODEL"
for wav in $WAVS; do
  download "$wav"
done

echo "speaker assets ready in $OUT_DIR"
