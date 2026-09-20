#!/bin/sh
# 板端在线（流式）声纹 demo：流式进音 -> silero VAD(CPU) 分段 -> 逐段 RKNN 声纹识别
# （由 deploy.sh 上传并执行；Phase B 麦克风部分可选）
set -eu

BIN=./bin/online_speaker_demo
MODEL=./models/eres2netv2_T300_fp.rknn
VAD=./models/silero_vad.onnx
W=./wavs
export LD_LIBRARY_PATH=lib:${LD_LIBRARY_PATH:-}

REG="--register fangjun=$W/fangjun-sr-1.wav --register fangjun=$W/fangjun-sr-2.wav \
--register leijun=$W/leijun-sr-1.wav --register leijun=$W/leijun-sr-2.wav \
--register liudehua=$W/liudehua-sr-1.wav --register liudehua=$W/liudehua-sr-2.wav"

echo "=================================================="
echo "# 在线 Phase A：拼接对话流（3 人轮流说话，实时喂入 + VAD 分段）"
echo "=================================================="
$BIN --speaker-model "$MODEL" --speaker-provider rknn --vad "$VAD" --threshold 0.5 \
  $REG --wav "$W/conversation.wav"

echo
echo "=================================================="
echo "# 在线 Phase B：麦克风实时（默认 8 秒，请对着麦克风说话）"
echo "=================================================="
MIC_DEV=${MIC_DEV:-plughw:4,0}
MIC_SECONDS=${MIC_SECONDS:-8}
if command -v arecord >/dev/null 2>&1; then
  echo "# 采音设备: $MIC_DEV（可用 MIC_DEV=plughw:3,0 覆盖）；请立刻说话…"
  arecord -D "$MIC_DEV" -f S16_LE -r 16000 -c 1 -t raw -d "$MIC_SECONDS" 2>/dev/null \
    | $BIN --speaker-model "$MODEL" --speaker-provider rknn --vad "$VAD" --threshold 0.5 $REG --stdin \
    || echo "# 麦克风阶段失败：检查 MIC_DEV / 麦克风接线"
else
  echo "# 板端无 arecord，跳过麦克风阶段"
fi

echo
echo "在线声纹 demo 结束（provider=rknn, VAD=CPU）"
