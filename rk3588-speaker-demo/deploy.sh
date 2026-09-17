#!/bin/sh
# 一键部署 RK3588 声纹 demo 到板卡并运行。
#
# Usage: BOARD_PASS=... sh deploy.sh
#   BOARD_PASS 见 0_Note/board_credentials.md；板卡 scp 子系统不可用，文件走 tar 管道。
#
# 可覆盖的环境变量:
#   BOARD_IP / BOARD_PORT / BOARD_USER / REMOTE_DIR / MODEL
set -eu

DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_DIR=$(dirname "$DIR")
BOARD_IP=${BOARD_IP:-172.16.58.55}
BOARD_PORT=${BOARD_PORT:-26666}
BOARD_USER=${BOARD_USER:-root}
: "${BOARD_PASS:?export BOARD_PASS first (see 0_Note/board_credentials.md)}"
REMOTE_DIR=${REMOTE_DIR:-/userdata/ai_audio_speaker_rk3588_test}

SHERPA_REPO=${SHERPA_REPO:-/home/xh/itc_project/sherpa-onnx-2025-1217}
SHERPA_INSTALL=$SHERPA_REPO/build-rknn/install
RKNN_LIB=${RKNN_LIB:-/home/xh/itc_project/RK_model_zoo/rknn2/eres2netv2/cpp/3rdparty/rknn}
MODEL=${MODEL:-/home/xh/itc_project/RK_model_zoo/rknn2/eres2netv2/models/eres2netv2_T300_fp.rknn}
OUT=$DIR/build

[ -f "$OUT/bin/ai_audio_speaker_demo" ] || { echo "缺少 build/bin/ai_audio_speaker_demo，先执行 sh build.sh" >&2; exit 1; }
[ -f "$MODEL" ] || { echo "模型不存在: $MODEL（可用 MODEL=... 覆盖）" >&2; exit 1; }

export SSHPASS="$BOARD_PASS"
SSH="sshpass -e ssh -o StrictHostKeyChecking=yes -o UserKnownHostsFile=$HOME/.ssh/known_hosts -o ConnectTimeout=10 -p $BOARD_PORT $BOARD_USER@$BOARD_IP"

echo "== 打包 =="
BUNDLE=$OUT/bundle
rm -rf "$BUNDLE"
mkdir -p "$BUNDLE/bin" "$BUNDLE/lib" "$BUNDLE/models" "$BUNDLE/wavs"
cp "$OUT/bin/ai_audio_speaker_demo" "$BUNDLE/bin/"
cp "$SHERPA_INSTALL"/lib/*.so* "$BUNDLE/lib/"
cp "$RKNN_LIB/lib/librknnrt.so" "$BUNDLE/lib/"
cp "$MODEL" "$BUNDLE/models/"
cp "$DIR"/wavs/*.wav "$BUNDLE/wavs/"
ls "$BUNDLE/bin" "$BUNDLE/models"

echo "== 部署 =="
$SSH "command -v tar >/dev/null || { echo NO_TAR; exit 1; }; mkdir -p $REMOTE_DIR && echo REMOTE_OK"
tar -C "$BUNDLE" -cf - . | $SSH "tar -C $REMOTE_DIR -xf -"
cat "$DIR/demo.sh" | $SSH "cat > $REMOTE_DIR/demo.sh"

echo "== 板端一键 demo（provider=rknn）=="
$SSH "cd $REMOTE_DIR && sh demo.sh" | tee "$OUT/board_run.log"

echo DEPLOY_DONE
