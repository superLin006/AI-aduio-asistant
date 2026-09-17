#!/bin/sh
# 板端一键声纹 demo（在板端部署目录内运行，由 deploy.sh 上传）。
#
# 依赖（部署到当前目录）:
#   bin/ai_audio_speaker_demo、lib/*.so、models/eres2netv2_T300_fp.rknn、wavs/*.wav
#
# 演示内容:
#   Phase 1  注册 3 位说话人（每人 2 条语音）并保存声纹库
#   Phase 2  加载声纹库，识别 7 条测试语音
#   Phase 3  陌生人拒绝：声纹库只含 fangjun/leijun，李德华的语音应被拒绝
set -eu

BIN=./bin/ai_audio_speaker_demo
MODEL=./models/eres2netv2_T300_fp.rknn
W=./wavs
export LD_LIBRARY_PATH=lib:${LD_LIBRARY_PATH:-}

echo "##############################################"
echo "# Phase 1：注册说话人（fangjun / leijun / liudehua，各 2 条）"
echo "##############################################"
$BIN --speaker-model "$MODEL" --speaker-provider rknn --threshold 0.5 \
  --register "fangjun=$W/fangjun-sr-1.wav" --register "fangjun=$W/fangjun-sr-2.wav" \
  --register "leijun=$W/leijun-sr-1.wav" --register "leijun=$W/leijun-sr-2.wav" \
  --register "liudehua=$W/liudehua-sr-1.wav" --register "liudehua=$W/liudehua-sr-2.wav" \
  --save speakers_demo.json

echo
echo "##############################################"
echo "# Phase 2：识别测试语音（从上一步保存的声纹库加载）"
echo "##############################################"
$BIN --speaker-model "$MODEL" --speaker-provider rknn --threshold 0.5 \
  --load speakers_demo.json \
  --identify "$W/fangjun-test-sr-1.wav" --identify "$W/fangjun-test-sr-2.wav" \
  --identify "$W/leijun-test-sr-1.wav" --identify "$W/leijun-test-sr-2.wav" \
  --identify "$W/leijun-test-sr-3.wav" \
  --identify "$W/liudehua-test-sr-1.wav" --identify "$W/liudehua-test-sr-2.wav"

echo
echo "##############################################"
echo "# Phase 3：陌生人拒绝（声纹库仅含 fangjun/leijun，李德华未注册）"
echo "##############################################"
echo "# 3a) best-match：给出最近邻与分数（用于观察分数余量）"
echo "# 3b) identify  ：阈值 0.5 下应全部 matched=0（被拒绝）"
$BIN --speaker-model "$MODEL" --speaker-provider rknn --threshold 0.5 \
  --register "fangjun=$W/fangjun-sr-1.wav" --register "fangjun=$W/fangjun-sr-2.wav" \
  --register "leijun=$W/leijun-sr-1.wav" --register "leijun=$W/leijun-sr-2.wav" \
  --identify "$W/liudehua-test-sr-1.wav" --identify "$W/liudehua-test-sr-2.wav" \
  --best-match "$W/liudehua-test-sr-1.wav" --best-match "$W/liudehua-test-sr-2.wav"

echo
echo "RK3588 声纹 demo 结束（模型: $MODEL, provider=rknn）"
