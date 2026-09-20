#!/bin/sh
# 交叉编译 RK3588 声纹 demo（离线 ai_audio_speaker_demo + 在线 online_speaker_demo）。
# 使用 docker sophon-cross-build（Ubuntu 20.04 + GCC 9.4，目标 glibc 2.31 <= 板端 2.34）。
#
# 可覆盖的环境变量:
#   SHERPA_REPO  sherpa-onnx 源码树（需含 build-rknn/install）
#   RKNN_LIB     含 include/rknn_api.h 与 lib/librknnrt.so 的目录
#   LLM_SDK      llm-sdk 树（提供 nlohmann json 头文件）
set -eu

DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_DIR=$(CDPATH= cd -- "$DIR/../.." && pwd)
SHERPA_REPO=${SHERPA_REPO:-/home/xh/itc_project/sherpa-onnx-2025-1217}
RKNN_LIB=${RKNN_LIB:-/home/xh/itc_project/RK_model_zoo/rknn2/eres2netv2/cpp/3rdparty/rknn}
LLM_SDK=${LLM_SDK:-/home/xh/itc_project/superlin/llm-sdk}

if [ ! -f "$SHERPA_REPO/build-rknn/install/lib/libsherpa-onnx-c-api.so" ]; then
  echo "缺少 $SHERPA_REPO/build-rknn/install（先构建 sherpa RKNN 版，见该仓库 build-rknn-docker.sh）" >&2
  exit 1
fi

docker run --rm --user "$(id -u):$(id -g)" -e HOME=/tmp \
  -v "$REPO_DIR":/repo \
  -v "$SHERPA_REPO":/sherpa-repo:ro \
  -v "$RKNN_LIB":/rknn:ro \
  -v "$LLM_SDK":/llm-sdk:ro \
  -w /repo/demos/rk3588-speaker-demo \
  sophon-cross-build:latest sh -c '
    set -eu
    mkdir -p build/bin
    COMMON="-std=c++17 -O2 -Wall -fPIC -I/repo/linux/include -I/sherpa-repo/build-rknn/install/include -I/llm-sdk/3rdparty/common/nlohmann"
    LIBS="-L/sherpa-repo/build-rknn/install/lib -lsherpa-onnx-cxx-api -lsherpa-onnx-c-api -L/rknn/lib -lrknnrt -lpthread -ldl -lm -Wl,--allow-shlib-undefined -Wl,-rpath,\$ORIGIN/../lib"
    aarch64-linux-gnu-g++ $COMMON /repo/linux/src/speaker_verifier.cpp /repo/linux/apps/speaker_demo.cpp \
      -o build/bin/ai_audio_speaker_demo $LIBS
    aarch64-linux-gnu-g++ $COMMON src/online_speaker_demo.cpp /repo/linux/src/speaker_verifier.cpp \
      -o build/bin/online_speaker_demo $LIBS
    file build/bin/ai_audio_speaker_demo build/bin/online_speaker_demo
  '
echo BUILD_DONE
