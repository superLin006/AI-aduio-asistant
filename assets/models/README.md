# Model assets

Model binaries are runtime assets and are not committed to this repository
(except the small VAD models bundled with the RK3588 demo).

## Linux / Sophon (BM1684X) build

The Linux Sophon build expects the local verified assets from:

- BModel: `/home/xh/itc_project/Sophon_model_zoo/Qwen3-ASR/models/BM1684X/qwen3_asr_merged_w4g64.bmodel`
- Tokenizer input: Qwen3 `vocab.json` and `merges.txt`. The integrated
  sherpa-onnx tokenizer currently requires these two extracted files rather
  than the Hugging Face `tokenizer.json` alone.
- Test audio: `/home/xh/itc_project/Sophon_model_zoo/Qwen3-ASR/test_data`

The merged model is Qwen3-ASR-0.6B, W4BF16 group-size 64, with a 30-second
offline encoder limit and a 512-token sequence budget.

## RK3588 voiceprint demo (`apps/rk3588-speaker-demo`)

| Asset | Location | Notes |
|---|---|---|
| Speaker model (RKNN) | `/home/xh/itc_project/RK_model_zoo/rknn2/eres2netv2/models/eres2netv2_T300_fp.rknn` | ERes2NetV2, fixed 300-frame window; sherpa metadata in `custom_string`. Conversion / validation records: that project's `.context/` |
| VAD model (ONNX, CPU, default) | `apps/rk3588-speaker-demo/models/silero_vad.onnx` | 643 KB, committed with the demo; used by default in the online demo |
| VAD model (RKNN, optional) | `apps/rk3588-speaker-demo/models/silero_vad.rknn` | silero-vad-v4, official pre-converted build (2.2 MB). **Experimental**: in our long-stream test it missed an 8.2 s speech clip (see the demo README); CPU VAD is the default |
| Acceptance audio | `apps/rk3588-speaker-demo/wavs/` | 13 real-speaker clips + `conversation.wav` (concatenated) |

Conversion toolchain: `rknn-toolkit2 2.3.2` (conda env `rknn232`).
The VAD RKNN model can also be regenerated with
`sherpa-onnx/scripts/silero_vad/v4/export-rknn.py` or downloaded from the
`sherpa-onnx` release tag `asr-models` (`silero-vad-v4-rk3588.rknn`).

## Asset index (all locations)

| Asset | Location | Committed |
|---|---|---|
| Android speaker model (CAM++) | `apps/android/app/src/main/assets/3dspeaker_speech_campplus_sv_zh-cn_16k-common.onnx` | yes (27 MB) |
| Android VAD | `apps/android/app/src/main/assets/silero_vad.onnx` | yes |
| Android runtime libs | `apps/android/app/src/main/jniLibs/arm64-v8a/{libonnxruntime,librknnrt,libsherpa-onnx-jni}.so` | yes (~26 MB) |
| KWS demo assets | `apps/android-kws-demo/app/src/main/assets/kws-wenetspeech/` | yes |
| RK3588 demo VAD | `apps/rk3588-speaker-demo/models/silero_vad.{onnx,rknn}` | yes (small) |
| RK3588 speaker models | `apps/rk3588-speaker-demo/models/{eres2netv2,campplus}_T300_fp.rknn` | no (built/downloaded) |
| Speaker eval audio | `.cache/models/speaker_eval/wav16k` (TTS), `apps/rk3588-speaker-demo/wavs/` (real) | no / yes |
| ERes2NetV2 source + RKNN project | `RK_model_zoo/rknn2/eres2netv2/` (sha256 `4d7bd622…`) | separate repo |
| CAM++ source + RKNN project | `RK_model_zoo/rknn2/campplus/` (sha256 `f682b514…` source, `c3f87cd9…` rknn) | separate repo |
| Qwen3-ASR BModel | `Sophon_model_zoo/Qwen3-ASR/models/BM1684X/` | separate repo |
| sherpa-onnx source trees | `sherpa-onnx-2025-1217/` (Sophon+RKNN installs), GitLab `itc` work tree | separate repos |
| Hand-off package | `delivery/rk3588-speaker-verification.tar.gz` | no (generated) |

Regenerate the RK3588 speaker models with `RK_model_zoo/rknn2/{eres2netv2,campplus}/python/convert_rknn.py`;
assemble the hand-off package with `tools/build/package_speaker_delivery.sh`.
