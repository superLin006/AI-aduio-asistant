# Linux Sophon integration

The Linux SDK wraps the sherpa-onnx C++ API and deliberately does not copy its
BMRuntime inference implementation. This keeps Qwen3-ASR preprocessing,
prompting, tokenization, and decoding in one maintained backend.

## Runtime contract

- Platform: BM1684X SoC, Ubuntu 20.04, aarch64
- Provider: `sophon`
- Model: merged Qwen3-ASR-0.6B encoder + Qwen3 LLM BModel
- Input: mono audio; sherpa-onnx resamples to 16 kHz
- Offline segment limit: 30 seconds
- Tokenizer: one directory containing `vocab.json` and `merges.txt`
- ONNX Runtime: 1.24.4, shared with the dispatch LLM SDK

## Verified result

`test_zh.wav` (5.6115 seconds) was run on the target board. The wrapper returned
the expected Chinese transcript in 0.515 seconds, RTF 0.092. The timing excludes
model construction and includes offline decode.

## API

Include `ai_audio_assistant/offline_asr.hpp`, construct `OfflineAsrConfig`, then
reuse one `OfflineAsr` instance for multiple calls. Construction loads the
model, so it must not occur once per utterance.

The next pipeline layer should place KWS and VAD ahead of this API and guarantee
that each finalized VAD segment is at most 30 seconds.

## Assistant pipeline

`AssistantPipeline` now implements the first complete inference path:

```text
LISTENING --KWS--> ACTIVATED --post-wake guard--> VAD --segment--> Qwen3-ASR
```

It emits typed events for wake detection, speech start, final transcript,
activation timeout, and errors. A 500 ms post-wake guard prevents the tail of
the wake phrase from being recognized as the command. The BM1684X integration
test detected `文森特卡索`, then transcribed the separate Chinese command audio
after the ONNX Runtime 1.24.4 upgrade. A second board test loaded Qwen3-ASR,
BGE, and the local dispatch Qwen model in one process successfully.

Use `scripts/download_kws_model.sh` to retrieve the KWS assets from ModelScope.
The KWS decoder is intentionally FP32 while encoder and joiner are INT8; this
matches the upstream sherpa-onnx reference configuration.

The canonical Linux wake-word file is `configs/kws/keywords_xiaohui.txt` and
contains only “小慧”. The board pipeline has been verified with a separately
synthesized 16 kHz “小慧，小慧” wake sample.

## Speaker recognition

`SpeakerVerifier` (`linux/include/ai_audio_assistant/speaker_verifier.hpp`)
wraps the sherpa-onnx speaker embedding C API: enrollment
(`Register`/`RegisterFile`, multiple utterances per speaker are averaged),
1:1 `Verify`, 1:N `Identify` (cosine similarity above a configurable
threshold), and JSON persistence (`Save`/`Load`). Set
`SpeakerVerifierConfig::model_path` to a sherpa-onnx speaker embedding ONNX
model (wespeaker / 3d-speaker / nemo); `speakers_path` optionally
auto-loads an enrollment database at construction.

`AssistantPipeline` accepts an optional `AssistantConfig::speaker`. When
enabled, the transcript event carries `speaker_name`/`speaker_score`
identified from the VAD segment; failures never drop the transcript.
`ai_audio_speaker_demo` exercises registration/verification/identification
against WAV files; `ai_audio_pipeline_demo` and `ai_audio_voice_intent_demo`
accept `--speaker-model` (+ `--speaker-speakers` on the pipeline demo).

Reference assets: `scripts/download_speaker_model.sh` pulls the pre-exported
`3dspeaker_speech_eres2netv2_sv_zh-cn_16k-common.onnx` model and the
fangjun/leijun/liudehua enrollment and test utterances from the
`sherpa-onnx` “speaker-recongition-models” release; `scripts/run_speaker_demo.sh`
registers three speakers and identifies the test set. On BM1684X the board
test identified 7/7 test utterances correctly (scores 0.78–0.90) and the
full KWS → VAD → ASR → voiceprint pipeline reported the enrolled speaker
(`fangjun`, score 0.73) for a 1-second command segment.

## Voice regression

`ai_audio_batch_regression_demo` loads Qwen3-ASR and the local dispatch model
once, then compares the semantic plan produced from each recognized utterance
with the plan produced from its source sentence. Use
`scripts/analyze_batch_regression.py` to summarize its JSONL output.

The current ChatTTS corpus contains all 110 v9.5d live cases. On BM1684X the
baseline completed 110/110 without runtime errors and matched 66 semantic plans
(60.0%). A broad dispatch hotword prompt matched 68/109 successful cases but
introduced one empty transcript and unrelated prompt bias. Hotwords therefore
remain opt-in through `ASR_HOTWORDS`; the production default is no prompt.

ChatTTS input is maintained separately from the protocol-oriented device test
text. Full signal filenames are represented by unique spoken aliases such as
“打架识别”, while commands containing ASCII protocol terms (`LED`, `IP`) are
not synthesized. The retained `configs/regression/dispatch_speech_stable_v95d_59.txt`
set contains the 59 utterances whose semantic plans passed the first BM1684X
speech regression. Excluded cases remain text tests or require human recordings;
they must not be reported as ASR failures from an artificial pronunciation.

Do not feed the entire synthesized corpus to the live VMC until the semantic
plan gate passes. The text-only v9.5d release smoke test has separately passed
8/8 against the real controller, while the synthesized-audio form currently
matches only 4/8 plans and is intentionally blocked from device execution.
