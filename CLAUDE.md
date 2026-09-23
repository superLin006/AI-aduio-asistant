# Repository guidance

This repository contains two parallel products:

- `apps/`: runnable applications - `apps/android/` (Kotlin/Compose, Android JNI)
  and the per-platform demos (`apps/android-kws-demo/`, `apps/rk3588-speaker-demo/`).
- `sdk/`: C++17 SDK (`include/`, `src/`, `tests/`) plus its CLI programs
  (`sdk/cli/`) built on the sherpa-onnx C++ API.
- `tools/`: build/run/eval/data scripts; `assets/`: configs + model index;
  build output goes to `build/`, hand-off packages to `delivery/` (both ignored).

Large model artifacts are external. See `assets/models/README.md`; do not commit model
binaries or credentials. The Linux BM1684X backend uses Qwen3-ASR-0.6B with
`provider="sophon"`. Its tokenizer directory must contain `vocab.json` and
`merges.txt`.

## Validation

```sh
sh tools/build/build_linux_sophon.sh

cd apps/android
./gradlew testDebugUnitTest assembleDebug
```

Cross-compiled aarch64 binaries must be executed on the Ubuntu 20.04 BM1684X
board. Keep compatibility at GLIBC 2.30 or below and disable LTO for board
builds. Android secrets belong in user-level Gradle properties; Linux secrets
belong in environment variables or untracked configuration.

## Architecture

The shared behavioral pipeline is KWS -> VAD -> offline ASR -> intent parsing ->
device command. Platform I/O and inference adapters must stay outside the core
state-machine logic. Public C++ interfaces live under
`sdk/include/ai_audio_assistant/` and use RAII/PImpl to keep vendor headers out
of consumers.
