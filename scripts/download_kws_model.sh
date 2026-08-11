#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
DEST=${1:-$ROOT_DIR/.cache/models/kws-wenetspeech}
BASE=${MODELSCOPE_BASE:-https://www.modelscope.cn/models/pkufool/sherpa-onnx-kws-zipformer-wenetspeech-3.3M-2024-01-01/resolve/master}

mkdir -p "$DEST"
for file in \
  encoder-epoch-12-avg-2-chunk-16-left-64.int8.onnx \
  decoder-epoch-12-avg-2-chunk-16-left-64.onnx \
  joiner-epoch-12-avg-2-chunk-16-left-64.int8.onnx \
  tokens.txt keywords.txt; do
  curl -fL --retry 3 "$BASE/$file" -o "$DEST/$file"
done

# Product wake word. Keep this canonical file separate from downloaded model
# defaults so a model refresh cannot silently restore upstream sample phrases.
cp "$ROOT_DIR/configs/kws/keywords_xiaohui.txt" "$DEST/keywords.txt"

echo "KWS model ready: $DEST"
