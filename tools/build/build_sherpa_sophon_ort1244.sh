#!/usr/bin/env bash
set -euo pipefail

SHERPA_REPO=${SHERPA_REPO:-/home/xh/itc_project/sherpa-onnx-2025-1217}
SOPHON_SDK=${SOPHON_SDK:-/home/xh/itc_project/Sophon_model_zoo/0_Toolkits/soc-sdk-sp4}
LLM_SOURCE=${LLM_SOURCE:-/home/xh/itc_project/superlin/llm-sdk}
LLM_PACKAGE=${LLM_PACKAGE:-/home/xh/itc_project/deliver/deliver_dispatch_sdk_v10_w8bf16}
BUILD_NAME=${BUILD_NAME:-build-sophon-ort1244-verified}

test -f "$LLM_SOURCE/3rdparty/bm1684x/onnxruntime/include/onnxruntime_c_api.h"
test -f "$LLM_PACKAGE/lib/libonnxruntime.so"
test -f "$SOPHON_SDK/include/bmruntime_interface.h"

docker run --rm -i \
  -v "$SHERPA_REPO:/workspace" \
  -v "$SOPHON_SDK:/sdk:ro" \
  -v "$LLM_SOURCE:/llm-source:ro" \
  -v "$LLM_PACKAGE:/runtime:ro" \
  -w /workspace sophon-cross-build:latest bash -s -- "$BUILD_NAME" <<'CONTAINER'
set -euo pipefail
build_name=$1
deps=/workspace/build-sophon-linux-aarch64/_deps
source_args=()
for name in kaldi_native_fbank kaldi_decoder kaldifst openfst eigen json kissfft; do
  source_dir="$deps/${name}-src"
  if [[ -d "$source_dir" ]]; then
    variable=${name^^}
    source_args+=("-DFETCHCONTENT_SOURCE_DIR_${variable}=$source_dir")
  fi
done

export SHERPA_ONNXRUNTIME_INCLUDE_DIR=/llm-source/3rdparty/bm1684x/onnxruntime/include
export SHERPA_ONNXRUNTIME_LIB_DIR=/runtime/lib

cmake -S /workspace -B "/workspace/$build_name" \
  "${source_args[@]}" \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCMAKE_INSTALL_PREFIX="/workspace/$build_name/install" \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=ON \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DSHERPA_ONNX_USE_PRE_INSTALLED_ONNXRUNTIME_IF_AVAILABLE=ON \
  -DSHERPA_ONNX_ENABLE_SOPHON=ON \
  -DSHERPA_ONNX_SOPHON_SDK_DIR=/sdk \
  -DSHERPA_ONNX_ENABLE_GPU=OFF \
  -DSHERPA_ONNX_ENABLE_TESTS=OFF \
  -DSHERPA_ONNX_ENABLE_PYTHON=OFF \
  -DSHERPA_ONNX_ENABLE_CHECK=OFF \
  -DSHERPA_ONNX_ENABLE_PORTAUDIO=OFF \
  -DSHERPA_ONNX_ENABLE_JNI=OFF \
  -DSHERPA_ONNX_ENABLE_C_API=ON \
  -DSHERPA_ONNX_ENABLE_WEBSOCKET=OFF \
  -DSHERPA_ONNX_ENABLE_BINARY=OFF \
  -DSHERPA_ONNX_ENABLE_TTS=OFF \
  -DSHERPA_ONNX_ENABLE_SPEAKER_DIARIZATION=OFF \
  -DCMAKE_TOOLCHAIN_FILE=/workspace/toolchains/aarch64-ubuntu2004.toolchain.cmake
cmake --build "/workspace/$build_name" --parallel "${BUILD_JOBS:-4}"
cmake --install "/workspace/$build_name" --strip
# 容器内没有 readelf/binutils，用 grep 直接匹配 ELF 的 version symbol 字符串。
lib="/workspace/$build_name/install/lib/libsherpa-onnx-c-api.so"
if ! grep -aq 'VERS_1.24.4' "$lib"; then
  echo "libsherpa-onnx-c-api.so is not linked against ONNX Runtime 1.24.4" >&2
  exit 1
fi
if grep -aq 'VERS_1.17.1' "$lib"; then
  echo "Unexpected ONNX Runtime 1.17.1 dependency" >&2
  exit 1
fi
echo "GUARD_OK: linked against ONNX Runtime 1.24.4 only"
CONTAINER
