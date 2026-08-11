#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD_DIR=${BUILD_DIR:-$ROOT_DIR/build/sophon}
SHERPA_REPO=${SHERPA_REPO:-/home/xh/itc_project/sherpa-onnx-2025-1217}
SHERPA_INSTALL=${SHERPA_INSTALL:-$SHERPA_REPO/build-sophon-ort1244-verified/install}
SHERPA_BUILD=${SHERPA_BUILD:-$SHERPA_INSTALL}
SOPHON_SDK=${SOPHON_SDK:-/home/xh/itc_project/Sophon_model_zoo/0_Toolkits/soc-sdk-sp4}
LLM_SDK=${LLM_SDK:-/home/xh/itc_project/deliver_dispatch_sdk_v9.5d_candidate_w8bf16}

if ! readelf --version-info "$LLM_SDK/lib/libonnxruntime.so" 2>/dev/null | \
  grep -q 'VERS_1.24.4'; then
  echo "LLM_SDK must provide the unified ONNX Runtime 1.24.4 library." >&2
  exit 1
fi

if [ ! -f "$SHERPA_REPO/sherpa-onnx/c-api/cxx-api.h" ] || \
   [ ! -f "$SHERPA_INSTALL/lib/libsherpa-onnx-cxx-api.so" ]; then
  echo "Missing sherpa-onnx Sophon build linked to ONNX Runtime 1.24.4." >&2
  exit 1
fi

docker run --rm \
  -v "$ROOT_DIR:/workspace" \
  -v "$SHERPA_REPO:/sherpa-repo:ro" \
  -v "$SOPHON_SDK:/sophon-sdk:ro" \
  -v "$LLM_SDK:/llm-sdk:ro" \
  -w /workspace \
  sophon-cross-build:latest sh -c '
    set -eu
    rm -rf build/sophon
    cmake -S . -B build/sophon \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
      -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++ \
      -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF \
      -DSHERPA_ONNX_INSTALL_DIR=/sherpa-repo/build-sophon-ort1244-verified/install \
      -DSHERPA_ONNX_INCLUDE_ROOT=/sherpa-repo \
      -DSHERPA_ONNX_LIBRARY_ROOT=/sherpa-repo/build-sophon-ort1244-verified/install/lib \
      -DSOPHON_SDK_DIR=/sophon-sdk \
      -DLLM_SDK_DIR=/llm-sdk \
      -DONNXRUNTIME_ROOT=/llm-sdk
    cmake --build build/sophon --parallel "${BUILD_JOBS:-2}"
  '

if readelf --version-info "$BUILD_DIR/linux/libai_audio_assistant.so" 2>/dev/null | \
  grep -q 'VERS_1.17.1'; then
  echo "Unexpected ONNX Runtime 1.17.1 dependency in unified library." >&2
  exit 1
fi
