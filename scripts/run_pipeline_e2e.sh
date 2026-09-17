#!/bin/bash
set -eu
: "${BOARD_PASS:?export BOARD_PASS first (see 0_Note/board_credentials.md)}"
export SSHPASS="$BOARD_PASS"
# Pipeline E2E: TTS 合成唤醒词(小慧,Vivian) + 命令(打开投影仪,Uncle_Fu)
# → 板卡 pipeline_demo 验证 speaker 识别
cd /home/xh/itc_project/superlin/AI-aduio-asistant
mkdir -p .cache/models/speaker_eval/wav24k

# 1. Download synthesized wavs from board
sshpass -e scp root@172.16.25.248:/data/Qwen3-TTS/wake_xiaohui.wav .cache/models/speaker_eval/wav24k/ 2>/dev/null
sshpass -e scp root@172.16.25.248:/data/Qwen3-TTS/cmd_proj.wav .cache/models/speaker_eval/wav24k/ 2>/dev/null
echo "Downloaded TTS wavs"

# 2. Resample 24k→16k (use conda env python which has soundfile)
/home/xh/miniconda3/envs/sophon-qwen3-tts/bin/python <<'PYEOF'
import numpy as np, soundfile as sf, os
base = ".cache/models/speaker_eval"
os.makedirs(f"{base}/wav16k", exist_ok=True)
for name in ["wake_xiaohui", "cmd_proj"]:
    audio, sr = sf.read(f"{base}/wav24k/{name}.wav")
    if sr == 24000:
        n = int(len(audio) * 16000 / sr)
        audio = np.interp(np.linspace(0, 1, n), np.linspace(0, 1, len(audio)), audio).astype(audio.dtype)
    sf.write(f"{base}/wav16k/{name}.wav", audio, 16000)
    print(f"Resampled {name}.wav -> 16k ({n} samples)")
PYEOF

# 3. Deploy to board
sshpass -e scp .cache/models/speaker_eval/wav16k/wake_xiaohui.wav .cache/models/speaker_eval/wav16k/cmd_proj.wav root@172.16.25.248:/root/speaker_test/speaker_eval/wav16k/ 2>/dev/null

# 4. Update KWS keywords to 小慧
sshpass -e ssh root@172.16.25.248 "printf 'x iǎo h uì @小慧\n' > /root/speaker_test/kws/keywords.txt && echo KWS_KEYWORDS_UPDATED"

# 5. Run pipeline E2E on board
sshpass -e ssh root@172.16.25.248 bash -s <<'REMOTE'
set -eu
cd /root/speaker_test
export LD_LIBRARY_PATH=/root/speaker_test:/data/deliver_dispatch_sdk_v9.5d_candidate_w8bf16/lib:/opt/sophon/libsophon-current/lib
echo "=== PIPELINE E2E: wake(小慧,Vivian) + cmd(打开投影仪,Uncle_Fu) ==="
./ai_audio_pipeline_demo \
  --kws-dir kws \
  --vad /data/qwen3_asr/silero_vad.onnx \
  --bmodel /data/qwen3_asr/qwen3_asr_merged_w8bf16.bmodel \
  --tokenizer /data/models/qwen3-asr/config \
  --wake-audio speaker_eval/wav16k/wake_xiaohui.wav \
  --command-audio speaker_eval/wav16k/cmd_proj.wav \
  --speaker-model /root/speaker_test/3dspeaker_speech_eres2netv2_sv_zh-cn_16k-common.onnx \
  --speaker-speakers speaker_eval/speakers.json
REMOTE
echo "E2E_DONE"