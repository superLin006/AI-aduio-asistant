#!/bin/sh
# Deploy the RK3588 speaker demo (provider=rknn) to the board and run the
# register/identify regression on board 172.16.58.55.
#
# Usage: BOARD_PASS=... sh scripts/deploy_rk3588_speaker_test.sh
# (BOARD_PASS: see 0_Note/board_credentials.md; scp is unavailable on the board,
#  files are transferred with a tar pipe)
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BOARD_IP=${BOARD_IP:-172.16.58.55}
BOARD_PORT=${BOARD_PORT:-26666}
BOARD_USER=${BOARD_USER:-root}
: "${BOARD_PASS:?export BOARD_PASS first}"
REMOTE_DIR=${REMOTE_DIR:-/userdata/ai_audio_speaker_rk3588_test}

SHERPA_REPO=${SHERPA_REPO:-/home/xh/itc_project/sherpa-onnx-2025-1217}
SHERPA_INSTALL=$SHERPA_REPO/build-rknn/install
RKNN_LIB=${RKNN_LIB:-/home/xh/itc_project/RK_model_zoo/rknn2/eres2netv2/cpp/3rdparty/rknn}
MODEL=${MODEL:-/home/xh/itc_project/RK_model_zoo/rknn2/eres2netv2/models/eres2netv2_T300_fp.rknn}
SPKR=${SPKR:-$ROOT_DIR/.cache/models/speaker}
OUT=$ROOT_DIR/build/rk3588_speaker

export SSHPASS="$BOARD_PASS"
SSH="sshpass -e ssh -o StrictHostKeyChecking=yes -o UserKnownHostsFile=$HOME/.ssh/known_hosts -o ConnectTimeout=10 -p $BOARD_PORT $BOARD_USER@$BOARD_IP"

echo "== stage bundle =="
BUNDLE=$OUT/bundle
rm -rf "$BUNDLE"
mkdir -p "$BUNDLE/bin" "$BUNDLE/lib" "$BUNDLE/models" "$BUNDLE/wavs"
cp "$OUT/bin/ai_audio_speaker_demo" "$BUNDLE/bin/"
cp "$SHERPA_INSTALL"/lib/*.so* "$BUNDLE/lib/"
cp "$RKNN_LIB/lib/librknnrt.so" "$BUNDLE/lib/"
cp "$MODEL" "$BUNDLE/models/"
cp "$SPKR"/*.wav "$BUNDLE/wavs/"
ls "$BUNDLE/bin" "$BUNDLE/models"

echo "== deploy =="
$SSH "command -v tar >/dev/null || { echo NO_TAR; exit 1; }; mkdir -p $REMOTE_DIR && echo REMOTE_OK"
tar -C "$BUNDLE" -cf - . | $SSH "tar -C $REMOTE_DIR -xf -"
cat "$ROOT_DIR/scripts/run_rk3588_speaker_demo.sh" | $SSH "cat > $REMOTE_DIR/run_rk3588_speaker_demo.sh"

echo "== run one-click demo on board (provider=rknn) =="
$SSH "cd $REMOTE_DIR && sh run_rk3588_speaker_demo.sh" | tee "$OUT/board_run.log"

echo DEPLOY_RK3588_SPEAKER_DONE
