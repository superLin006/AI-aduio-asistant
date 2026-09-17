#!/bin/sh
cd /root/speaker_test
export LD_LIBRARY_PATH=/root/speaker_test:/data/deliver_dispatch_sdk_v9.5d_candidate_w8bf16/lib:/opt/sophon/libsophon-current/lib

# Test single registration with verbose output
./ai_audio_speaker_demo \
  --speaker-model /root/speaker_test/3dspeaker_speech_eres2netv2_sv_zh-cn_16k-common.onnx \
  --register test=speaker_eval/wav16k/Aiden_t00.wav 2>&1

echo "---"
# Check wav file
file speaker_eval/wav16k/Aiden_t00.wav
ls -la speaker_eval/wav16k/Aiden_t00.wav