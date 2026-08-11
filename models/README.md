# Model assets

Model binaries are runtime assets and are not committed to this repository.

The Linux Sophon build expects the local verified assets from:

- BModel: `/home/xh/itc_project/Sophon_model_zoo/Qwen3-ASR/models/BM1684X/qwen3_asr_merged_w4g64.bmodel`
- Tokenizer input: Qwen3 `vocab.json` and `merges.txt`. The integrated
  sherpa-onnx tokenizer currently requires these two extracted files rather
  than the Hugging Face `tokenizer.json` alone.
- Test audio: `/home/xh/itc_project/Sophon_model_zoo/Qwen3-ASR/test_data`

The merged model is Qwen3-ASR-0.6B, W4BF16 group-size 64, with a 30-second
offline encoder limit and a 512-token sequence budget.
