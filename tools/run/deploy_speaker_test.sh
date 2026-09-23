#!/bin/bash
set -e
: "${BOARD_PASS:?export BOARD_PASS first (see 0_Note/board_credentials.md)}"
export SSHPASS="$BOARD_PASS"
cd /home/xh/itc_project/superlin/AI-aduio-asistant
BIN=build/sophon/sdk
SPKR=.cache/models/speaker
KWS=.cache/models/kws-wenetspeech
KWS_TEST=.cache/models/kws-test
SHERPA_LIB=/home/xh/itc_project/sherpa-onnx-2025-1217/build-sophon-ort1244-verified/install/lib

SSH="sshpass -e ssh -o StrictHostKeyChecking=no root@172.16.25.248"

# Staging KWS dir whose keywords.txt triggers on the 0.wav test utterance.
rm -rf "$KWS_TEST"
mkdir -p "$KWS_TEST"
cp "$KWS"/encoder-epoch-12-avg-2-chunk-16-left-64.int8.onnx "$KWS_TEST"/
cp "$KWS"/decoder-epoch-12-avg-2-chunk-16-left-64.onnx "$KWS_TEST"/
cp "$KWS"/joiner-epoch-12-avg-2-chunk-16-left-64.int8.onnx "$KWS_TEST"/
cp "$KWS"/tokens.txt "$KWS_TEST"/
# Token sequence must match the reference test_keywords.txt exactly; note
# "uǒ" is a single token (no space between u and ǒ).
printf 'w \xc3\xa9n s \xc4\x93n t \xc3\xa8 k \xc7\x8e s u\xc7\x92  @\xe6\x96\x87\xe6\xa3\xae\xe7\x89\xb9\xe5\x8d\xa1\xe7\xb4\xa2\n' > "$KWS_TEST"/keywords.txt
cp "$KWS"/test_wavs/0.wav "$KWS_TEST"/wake_0.wav

# Drop stale flat-layout KWS files from the first deployment round.
$SSH "mkdir -p /root/speaker_test && rm -rf /root/speaker_test/kws && rm -f /root/speaker_test/encoder-*.onnx /root/speaker_test/decoder-*.onnx /root/speaker_test/joiner-*.onnx /root/speaker_test/tokens.txt /root/speaker_test/keywords.txt /root/speaker_test/wake_0.wav"

rm -f /tmp/deploy2.tar
tar -C "$BIN" -cf /tmp/deploy2.tar \
  ai_audio_speaker_demo ai_audio_speaker_config_test ai_audio_config_test \
  ai_audio_pipeline_demo libai_audio_assistant.so
tar -C "$SHERPA_LIB" -rf /tmp/deploy2.tar \
  libsherpa-onnx-c-api.so libsherpa-onnx-cxx-api.so libonnxruntime.so
tar -C "$KWS_TEST" -rf /tmp/deploy2.tar \
  --transform='s,^,kws/,' \
  encoder-epoch-12-avg-2-chunk-16-left-64.int8.onnx \
  decoder-epoch-12-avg-2-chunk-16-left-64.onnx \
  joiner-epoch-12-avg-2-chunk-16-left-64.int8.onnx \
  tokens.txt keywords.txt wake_0.wav
tar -C "$KWS/test_wavs" -rf /tmp/deploy2.tar \
  --transform='s,^,kws/,' \
  1.wav 2.wav 3.wav 4.wav 5.wav 6.wav test_keywords.txt
tar -C "$SPKR" -rf /tmp/deploy2.tar \
  fangjun-test-sr-1.wav fangjun-test-sr-2.wav

ls -la /tmp/deploy2.tar
cat /tmp/deploy2.tar | $SSH "cd /root/speaker_test && tar -xf - && echo UNTAR_OK"
