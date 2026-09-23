# RK3588 声纹识别 Demo（独立工程）

独立的一键演示目录（与 `demos/android-kws-demo/` 同一种组织方式），包含两个 demo：

```text
离线：WAV 整段 -> fbank -> ERes2NetV2（RKNPU, provider=rknn）-> 注册 / 识别 / 陌生人拒绝
在线：流式进音 -> silero VAD（默认 RKNPU）分段 -> 每段结束即 RKNN 声纹识别 -> 实时输出说话人
```

## 目录

```text
demos/rk3588-speaker-demo/
├── README.md                    本文件
├── build.sh                     宿主机交叉编译（离线 + 在线两个程序）
├── deploy.sh                    一键：打包 -> 部署到板卡 -> 依次运行离线/在线 demo
├── demo.sh                      板端离线 demo（注册 → 识别 → 陌生人拒绝）
├── online_demo.sh               板端在线 demo（VAD 流式对话 + 麦克风实时）
├── src/online_speaker_demo.cpp  在线 demo 程序源码
├── models/eres2netv2_T300_fp.rknn 声纹模型（本地交付模型，不提交到源仓库）
├── models/silero_vad.onnx       VAD 模型（CPU，默认，643 KB）
├── models/silero_vad.rknn       VAD 模型（RKNPU 可选实验项，2.2 MB，官方预转换；长流有漏检倾向）
├── tools/make_conversation.py   生成拼接对话音频（供在线 demo 流式输入）
└── wavs/                        验收音频（13 条真人 + conversation.wav 拼接对话）
```

离线 demo 复用应用代码（不复制源码）：`../../linux/apps/speaker_demo.cpp` + `../../linux/src/speaker_verifier.cpp`。

## 前置资源

- sherpa-onnx（RKNN 后端）安装：`/home/xh/itc_project/sherpa-onnx-2025-1217/build-rknn/install`
  （构建方式见该仓库 `build-rknn-docker.sh`）
- 声纹模型（固定 300 帧窗口，含 sherpa 元数据）：放在 `models/eres2netv2_T300_fp.rknn`。
  当前本机源文件是 `/home/xh/itc_project/RK_model_zoo/rknn2/eres2netv2/models/eres2netv2_T300_fp.rknn`
  （转换与验收记录：`RK_model_zoo/rknn2/eres2netv2/README.md` 与 `.context/`）。
- 板卡：`172.16.58.55`（Buildroot, librknnrt 2.3.2, RKNPU 驱动 0.9.8）
- `BOARD_PASS`：从 `0_Note/board_credentials.md` 读取后导出（脚本不落盘密码）

## 使用

```sh
cd /home/xh/itc_project/superlin/AI-aduio-asistant/demos/rk3588-speaker-demo

# 1) 交叉编译（产物 build/bin/{ai_audio_speaker_demo, online_speaker_demo}）
sh build.sh

# 2) 一键部署 + 依次运行两个 demo（默认用本目录 models/ 下的模型）
BOARD_PASS=... sh deploy.sh
# 板端目录: /userdata/ai_audio_speaker_rk3588_test
# 日志: build/board_run.log（离线）、build/board_online_run.log（在线）
# 麦克风阶段: MIC_SECONDS=8 MIC_DEV=plughw:4,0 可调
```

## 开发/测试交付包

交付包包含 `build/bundle` 中的 AArch64 程序和运行库、`models/`、`wavs/`、两个板端脚本，以及从 GitLab RKNN 分支编译出的 `sdk/include/` 与 `sdk/lib/`。SDK C/C++ API 库已针对包内 ONNX Runtime 1.24.4 重新链接，与板端 demo 共用同一份 ONNX Runtime 和 RKNN Runtime。包内附有直接包含 C++ 头文件调用 `SpeakerVerifier` 的示例源码和 AArch64 可执行文件。交付包目录和压缩包位于：

```text
build/delivery/rk3588-speaker-verification/
build/delivery/rk3588-speaker-verification.tar.gz
```

把压缩包交给同事后，在 RK3588 板端解压并运行 `sh demo.sh` 验证离线注册、识别和陌生人拒绝；运行 `sh online_demo.sh` 验证 VAD 分段、声纹识别和可选麦克风输入。包内包含对应的 `libsherpa-onnx`、ONNX Runtime、RKNN Runtime、声纹/VAD 模型及验收 WAV。

此包同时支持**板端运行验证**和**SDK 头文件调用验证**，目标为 RK3588 AArch64。包内 sherpa-onnx C/C++ API 库来自 GitLab `itc` 分支（`36b80b9` 起含 RKNN 声纹后端移植与两轮审查修复，`d6f88c6` 起含 CAM++ 的 `window_align` 支持）；`sdk/lib/` 是指向 `lib/` 同一组库的相对符号链接。预编译 demo 与 SDK 示例共用同一组 Sherpa 库、ONNX Runtime 1.24.4 和 RKNN Runtime。

## 离线 demo 实测（2026-09-16）

| 阶段 | 结果 |
|---|---|
| 注册 3 人 × 2 条 | 全部 ok，`speakers=3` |
| 识别 7 条测试语音 | **7/7 全对**（0.776–0.892） |
| 陌生人拒绝（库中剔除李德华） | identify 全部 `matched=0`；最近邻 0.29 / 0.40 < 阈值 0.5 |
| 单窗（3 s）NPU 推理 | ~112 ms；端到端 112–345 ms/条 |

## 在线（流式）demo

- 管线：流式进音（100 ms/块）→ silero VAD 分段（min_speech 0.25 s / min_silence 0.6 s）→
  每段结束即在 NPU 上计算 embedding 并做 1:N 识别 → 实时打印，例如
  `[ 12.3s] speaker=leijun score=0.8300 matched=1 len=2.1s`
- **VAD 默认 CPU**（`models/silero_vad.onnx`，本对话流实测最稳）；阈值可调：`VAD_THRESHOLD`（默认 0.5）
- NPU VAD（`models/silero_vad.rknn`，官方预转换）为**可选实验项**：
  `VAD=./models/silero_vad.rknn VAD_PROVIDER=rknn sh online_demo.sh`
- 两种输入方式（同一程序）：
  - `--wav wavs/conversation.wav`：按实时节奏喂入（可复现，用于自动验证）
  - `--stdin`：读 16 kHz 单声道 s16le 裸 PCM，配麦克风：
    `arecord -D plughw:4,0 -f S16_LE -r 16000 -c 1 -t raw -d 8 | ./bin/online_speaker_demo --stdin ...`

### 板端实测（2026-09-20）

Phase A（拼接对话流，31.2 s / 6 段音频；时间轴：fangjun-sr-1 0–2.3s / leijun-sr-1 3.0–7.2s /
liudehua-sr-1 7.9–10.9s / fangjun-test-sr-1 11.6–17.2s / leijun-test-sr-1 17.9–26.1s / liudehua-test-sr-1 26.8–30.5s）：

| VAD | 分段 | 结果 |
|---|---|---|
| **CPU @0.5（默认）** | 8 段 | ✅ 全部识别正确（0.74–0.96） |
| NPU @0.5 | 6 段 | ❌ **漏检 17.9–26.1 s 的 8.2 s 语音**（复现两次） |
| NPU @0.35 | 8 段 | ⚠️ 补回漏检段，但把相邻两人的两段合并成一段（误标倾向） |

- 单条音频对照：NPU 与 CPU 分段一致（如 `fangjun-test-sr-1.wav` 两者逐段相同）
- 结论：NPU VAD（fp16 转换）概率在边界区翻转，长流上不可靠；**默认用 CPU VAD**，NPU VAD 待上游复查

Phase B（麦克风 `plughw:4,0`，8 s）：采音链路正常（成功采集 8.0 s）；
自动运行期间无人说话故无语音段，有人对着麦克风说话即可看到逐段实时识别输出。

## 板端复核（2026-09-23，板卡 172.16.40.115）

交付包验收时发现 `--speaker-provider rknn` 被忽略（报 `Protobuf parsing failed`）：原包内 sherpa 库由 GitLab `itc` 树构建，而该分支此前没有 RKNN 声纹后端。已把 RKNN 声纹后端移植进 GitLab（`itc` 分支提交 `d6f88c6`），并从该树重建 libs + 两个 demo + SDK 示例后复核（含推送前两轮代码审查的 JSON 加载加固）：

| 项 | 结果 |
|---|---|
| 离线：注册 3 人 × 2 条 | 6/6 ok，`speakers=3` |
| 离线：识别 7 条 | **7/7 全对**（0.776–0.892） |
| 离线：陌生人拒绝 | 2/2 `matched=0`（最近邻 0.29 / 0.40 < 0.5） |
| 在线：31.2 s 对话流（CPU VAD @0.5） | **8 段全部识别正确**（0.74–0.92） |
| SDK 示例（`SpeakerVerifier`，C++ 头文件调用） | `identified=fangjun score=0.891706`、`verified=yes`、`speakers.json` 正常生成 |
| 包内自校验 | `sha256sum -c SHA256SUMS` 33/33 OK |

- 板卡无 USB 麦克风时在线 Phase B 会提示失败并正常退出（`arecord -l` 可查实际采集卡，用 `MIC_DEV` 覆盖）
- 包内 ONNX Runtime 版本随 GitLab 树 cmake 默认（1.24.4）；BusyBox 板的 tar 不支持 `-z`，解压用 `gzip -dc xxx.tar.gz | tar -xf -`
- 推送前代码审查的两轮跟进（GitLab `1b4f57f`、`36b80b9`）：构造时 `speakers_path` 存在但无法解析会立即报错（不再静默空库）、加载失败回滚保持三处状态一致、SDK 示例演示了构造异常的处理；app 侧 `ai_audio_assistant::SpeakerVerifier` 改为对 sherpa `SpeakerVerifier` 的**薄适配**（289→138 行，两边共用一份实现）

## 说明

- 调用方式：`--speaker-provider rknn` 时 `--speaker-model` 指向 `.rknn` 模型（NPU）；改 `cpu` 则用 ONNX 模型
- 模型契约：固定 300 帧窗口（3.0 s，10 ms/帧），逐窗推理后 embedding 取均值；末窗默认零填充，
  元数据 `window_align=right` 时改为末窗对齐句尾（无零填充）；元数据（sample_rate /
  feature_normalize_type / output_dim / window_frames / window_align）由模型 custom_string 携带
- 第二个模型：`models/campplus_T300_fp.rknn`（3D-Speaker CAM++ zh-cn，Apache-2.0，带 `window_align=right`），
  用 `MODEL=./models/campplus_T300_fp.rknn sh demo.sh` 即可切换；单窗 32.7 ms（ERes2NetV2 111–120 ms），
  识别/拒识与 ERes2NetV2 同口径通过，逐窗保真度约 0.94（已知限制，见交付包 README）
- `num_threads`：RKNN 后端下映射为 NPU core mask（普通线程数自动映射为 AUTO）
