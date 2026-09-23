#!/bin/bash
# 组装 RK3588 声纹交付包（可从零重建）到 delivery/rk3588-speaker-verification。
#
# 依赖（本地资产，均不进 Git）：
#   apps/rk3588-speaker-demo/build/bin/*      两个 demo 程序（缺失时自动调用同目录 build.sh）
#   apps/rk3588-speaker-demo/models/*.rknn|*.onnx  声纹/VAD 模型（见 assets/models/README.md）
#   SHERPA_REPO/build-rknn/install            RKNN 版 sherpa-onnx install（默认 GitLab 工作树）
#   RKNN_LIB/lib/librknnrt.so                 RKNN 运行库
# 覆盖项：SHERPA_REPO / RKNN_LIB / MODELS / OUT
set -eu

DIR=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
DEMO=$DIR/apps/rk3588-speaker-demo
PACKAGING=$DEMO/packaging

SHERPA_REPO=${SHERPA_REPO:-/home/xh/itc_project/Claude_code/gitlab_sherpa/itc_wt}
RKNN_LIB=${RKNN_LIB:-/home/xh/itc_project/RK_model_zoo/rknn2/eres2netv2/cpp/3rdparty/rknn}
MODELS=${MODELS:-$DEMO/models}
OUT=${OUT:-$DIR/delivery}
INSTALL=$SHERPA_REPO/build-rknn/install
PKG=$OUT/rk3588-speaker-verification

echo "[1/6] 检查依赖"
[ -d "$INSTALL/lib" ] || { echo "缺少 $INSTALL（先构建 sherpa RKNN 版）" >&2; exit 1; }
[ -f "$RKNN_LIB/lib/librknnrt.so" ] || { echo "缺少 $RKNN_LIB/lib/librknnrt.so" >&2; exit 1; }
for m in eres2netv2_T300_fp.rknn campplus_T300_fp.rknn silero_vad.onnx silero_vad.rknn; do
  [ -f "$MODELS/$m" ] || { echo "缺少模型 $MODELS/$m（见 assets/models/README.md）" >&2; exit 1; }
done

if [ ! -x "$DEMO/build/bin/ai_audio_speaker_demo" ] || [ ! -x "$DEMO/build/bin/online_speaker_demo" ]; then
  echo "  demo 二进制缺失，先运行 $DEMO/build.sh"
  (cd "$DEMO" && SHERPA_REPO="$SHERPA_REPO" sh build.sh)
fi

echo "[2/6] 组装目录 $PKG"
rm -rf "$PKG"
mkdir -p "$PKG/bin" "$PKG/lib" "$PKG/models" "$PKG/wavs" "$PKG/sdk/include" "$PKG/sdk/lib"
cp "$DEMO/build/bin/ai_audio_speaker_demo" "$DEMO/build/bin/online_speaker_demo" "$PKG/bin/"
cp "$INSTALL/lib/libsherpa-onnx-c-api.so" "$INSTALL/lib/libsherpa-onnx-cxx-api.so" \
   "$INSTALL/lib/libonnxruntime.so" "$PKG/lib/"
cp "$RKNN_LIB/lib/librknnrt.so" "$PKG/lib/"
cp "$MODELS"/*.rknn "$MODELS"/*.onnx "$PKG/models/"
cp "$DEMO"/wavs/*.wav "$PKG/wavs/"
cp "$DEMO/demo.sh" "$DEMO/online_demo.sh" "$PKG/"
cp "$PACKAGING/README.md" "$PKG/README.md"
cp "$PACKAGING/speaker-verifier-cxx-api.cc" "$PKG/sdk/"

echo "[3/6] SDK 头文件 + sdk/lib 相对链接"
cp -r "$INSTALL/include/sherpa-onnx" "$PKG/sdk/include/"
( cd "$PKG/sdk/lib" && ln -sf ../../lib/libsherpa-onnx-c-api.so . && ln -sf ../../lib/libsherpa-onnx-cxx-api.so . )

echo "[4/6] 交叉编译 SDK 示例"
docker run --rm --user "$(id -u):$(id -g)" -e HOME=/tmp -v "$PKG":/pkg -w /pkg \
  sophon-cross-build:latest sh -c '
    aarch64-linux-gnu-g++ -std=c++17 -I sdk/include sdk/speaker-verifier-cxx-api.cc \
      -L sdk/lib -lsherpa-onnx-cxx-api -L lib -lonnxruntime -lrknnrt \
      -o sdk/speaker-verifier'

echo "[5/6] 生成 SHA256SUMS"
( cd "$PKG" && find . \( -type f -o -type l \) ! -name SHA256SUMS -print0 | sort -z | \
    xargs -0 sha256sum > SHA256SUMS && \
    sha256sum -c SHA256SUMS >/dev/null && \
    echo "  $(grep -c . SHA256SUMS) 项校验通过" )

echo "[6/6] 打 tar.gz"
( cd "$OUT" && rm -f rk3588-speaker-verification.tar.gz rk3588-speaker-verification.tar.gz.sha256 && \
    tar -czf rk3588-speaker-verification.tar.gz rk3588-speaker-verification && \
    sha256sum rk3588-speaker-verification.tar.gz > rk3588-speaker-verification.tar.gz.sha256 )
ls -la "$OUT"/rk3588-speaker-verification.tar.gz*
echo PACKAGE_DONE
