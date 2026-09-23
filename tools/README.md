# tools/

构建 / 运行 / 评测 / 数据脚本。所有脚本假定从仓库任意位置调用（内部按 `$0` 定位仓库根），
板卡相关脚本通过环境变量传参（例如 `BOARD_PASS`，见 `0_Note/board_credentials.md`）。

```text
tools/
├── build/   构建与打包
├── run/     运行（宿主 demo、板卡部署与 E2E）
├── eval/    评测与报告
└── data/    资产下载与数据准备
```

## build/

| 脚本 | 用途 | 平台 |
|---|---|---|
| `build_linux_sophon.sh` | 交叉编译 SDK + CLI（docker `sophon-cross-build`），产物 `build/sophon/sdk/` | Sophon BM1684X |
| `build_sherpa_sophon_ort1244.sh` | 构建 sherpa-onnx Sophon 版（ONNX Runtime 1.24.4） | Sophon BM1684X |
| `package_speaker_delivery.sh` | 组装 RK3588 声纹交付包到 `delivery/`（demo + 库 + 模型 + SDK 示例 + 校验和） | RK3588 |

## run/

| 脚本 | 用途 | 平台 |
|---|---|---|
| `run_offline_demo.sh` | 板卡离线 ASR demo（`build/sophon/sdk/ai_audio_offline_demo`） | Sophon |
| `run_intent_demo.sh` / `run_voice_intent_demo.sh` | 意图识别 demo（local Qwen3-0.6B / DeepSeek） | Sophon |
| `run_pipeline_demo.sh` | KWS→VAD→ASR→意图 全链路 demo | Sophon |
| `run_speaker_demo.sh` | 声纹注册/识别 demo（宿主或板端二进制） | Sophon |
| `deploy_speaker_test.sh` / `run_board_speaker_tests.sh` / `debug_register.sh` | 板端声纹测试部署与调试 | Sophon（旧链路，新工作请用 `apps/rk3588-speaker-demo/`） |
| `run_board_asr.sh` / `run_pipeline_e2e.sh` | 板端批量 ASR / 端到端（TTS→板端） | Sophon |
| `run_tts_synth.sh` | 触发 TTS 合成（调用 `tools/data/tts_synth.py`） | 宿主（GPU） |

## eval/

| 脚本 | 用途 |
|---|---|
| `eval_speaker_matrix.sh` | 声纹评测矩阵（板端执行：注册 K 条/音色 → best-match 全部测试音频） |
| `analyze_speaker_eval.py` | 汇总声纹评测结果（阈值扫描、同/跨说话人分布） |
| `analyze_batch_regression.py` | 汇总板端批量 ASR 回归 JSONL（配合 `run_board_asr.sh`） |
| `compute_cer_report.py` | 把 ASR 结果与语料合并算 CER，输出 `content_report.tsv` |
| `check_report.py` | 快速检查报告字段完整性 |
| `verify_tts_content.py` | 用 Qwen3-ASR 复听合成音频，核对内容质量 |

## data/

| 脚本 | 用途 |
|---|---|
| `download_kws_model.sh` | 下载 KWS 资产（ModelScope）并放入 `.cache/models/kws-wenetspeech` |
| `download_speaker_model.sh` | 下载声纹模型与参考测试音频（sherpa-onnx release） |
| `tts_synth.py` | Qwen3-TTS 多音色批量合成（9 音色，与板卡 speaker_id 映射一致） |
| `resample_and_manifest.py` | 24k→16k 重采样并生成 manifest |

## 与 `apps/` 的关系

- `apps/rk3588-speaker-demo/`：RK3588 声纹的**独立演示工程**（自带 `build.sh` / `deploy.sh` /
  `demo.sh` / `online_demo.sh`），板卡链路用它的脚本，不用 `tools/run/` 里的旧 Sophon 链路。
- `tools/run/` 中的 Sophon 声纹脚本（`deploy_speaker_test.sh` 等）为 BM1684X 时期的旧链路，
  保留备查；如确认不再使用可在评审后删除。

## 资产与产物位置

- 模型与配置：`assets/`（索引见 `assets/models/README.md`）
- 本地缓存：`.cache/models/`
- 构建产物：`build/`（忽略）
- 交付产物：`delivery/`（忽略；由 `tools/build/package_speaker_delivery.sh` 生成）
