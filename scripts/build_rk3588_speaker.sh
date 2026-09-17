#!/bin/sh
# Cross-compile the AI-audio-assistant speaker demo (with RKNN provider support)
# for the RK3588 board. Uses the sophon-cross-build image (Ubuntu 20.04 + GCC 9.4,
# target glibc 2.31 <= board glibc 2.34).
#
# Env overrides:
#   SHERPA_REPO  sherpa-onnx tree with build-rknn/install (RKNN backend)
#   RKNN_LIB     dir containing include/rknn_api.h and lib/librknnrt.so
#   LLM_SDK      llm-sdk tree (nlohmann json headers)
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SHERPA_REPO=${SHERPA_REPO:-/home/xh/itc_project/sherpa-onnx-2025-1217}
RKNN_LIB=${RKNN_LIB:-/home/xh/itc_project/RK_model_zoo/rknn2/eres2netv2/cpp/3rdparty/rknn}
LLM_SDK=${LLM_SDK:-/home/xh/itc_project/superlin/llm-sdk}
OUT=build/rk3588_speaker

if [ ! -f "$SHERPA_REPO/build-rknn/install/lib/libsherpa-onnx-c-api.so" ]; then
  echo "missing $SHERPA_REPO/build-rknn/install; build sherpa RKNN first (build-rknn-docker.sh)" >&2
  exit 1
fi

docker run --rm --user "$(id -u):$(id -g)" -e HOME=/tmp \
  -v "$ROOT_DIR":/workspace \
  -v "$SHERPA_REPO":/sherpa-repo:ro \
  -v "$RKNN_LIB":/rknn:ro \
  -v "$LLM_SDK":/llm-sdk:ro \
  -w /workspace \
  sophon-cross-build:latest sh -c '
    set -eu
    mkdir -p '"$OUT"'/bin
    aarch64-linux-gnu-g++ -std=c++17 -O2 -Wall -fPIC \
      -Ilinux/include \
      -I/sherpa-repo/build-rknn/install/include \
      -I/llm-sdk/3rdparty/common/nlohmann \
      linux/src/speaker_verifier.cpp linux/apps/speaker_demo.cpp \
      -o '"$OUT"'/bin/ai_audio_speaker_demo \
      -L/sherpa-repo/build-rknn/install/lib -lsherpa-onnx-cxx-api -lsherpa-onnx-c-api \
      -L/rknn/lib -lrknnrt -lpthread -ldl -lm \
      -Wl,--allow-shlib-undefined -Wl,-rpath,\$ORIGIN/../lib
    file '"$OUT"'/bin/ai_audio_speaker_demo
  '
echo BUILD_RK3588_SPEAKER_DONE
