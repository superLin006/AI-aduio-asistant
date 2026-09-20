# RK3588 声纹识别 Demo（独立工程）

独立的一键演示目录（与 `demos/android-kws-demo/` 同一种组织方式），包含两个 demo：

```text
离线：WAV 整段 -> fbank -> ERes2NetV2（RKNPU, provider=rknn）-> 注册 / 识别 / 陌生人拒绝
在线：流式进音 -> silero VAD(CPU) 分段 -> 每段结束即 RKNN 声纹识别 -> 实时输出说话人
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
├── models/silero_vad.onnx       VAD 模型（643 KB，随仓库提供）
├── tools/make_conversation.py   生成拼接对话音频（供在线 demo 流式输入）
└── wavs/                        验收音频（13 条真人 + conversation.wav 拼接对话）
```

离线 demo 复用应用代码（不复制源码）：`../../linux/apps/speaker_demo.cpp` + `../../linux/src/speaker_verifier.cpp`。

## 前置资源

- sherpa-onnx（RKNN 后端）安装：`/home/xh/itc_project/sherpa-onnx-2025-1217/build-rknn/install`
  （构建方式见该仓库 `build-rknn-docker.sh`）
- 声纹模型（固定 300 帧窗口，含 sherpa 元数据）：
  `/home/xh/itc_project/RK_model_zoo/rknn2/eres2netv2/models/eres2netv2_T300_fp.rknn`
  （转换与验收记录：`RK_model_zoo/rknn2/eres2netv2/README.md` 与 `.context/`）
- 板卡：`172.16.58.55`（Buildroot, librknnrt 2.3.2, RKNPU 驱动 0.9.8）
- `BOARD_PASS`：从 `0_Note/board_credentials.md` 读取后导出（脚本不落盘密码）

## 使用

```sh
cd /home/xh/itc_project/superlin/AI-aduio-asistant/demos/rk3588-speaker-demo

# 1) 交叉编译（产物 build/bin/{ai_audio_speaker_demo, online_speaker_demo}）
sh build.sh

# 2) 一键部署 + 依次运行两个 demo（MODEL=... 可覆盖模型路径）
BOARD_PASS=... sh deploy.sh
# 板端目录: /userdata/ai_audio_speaker_rk3588_test
# 日志: build/board_run.log（离线）、build/board_online_run.log（在线）
# 麦克风阶段: MIC_SECONDS=8 MIC_DEV=plughw:4,0 可调
```

## 离线 demo 实测（2026-09-16）

| 阶段 | 结果 |
|---|---|
| 注册 3 人 × 2 条 | 全部 ok，`speakers=3` |
| 识别 7 条测试语音 | **7/7 全对**（0.776–0.892） |
| 陌生人拒绝（库中剔除李德华） | identify 全部 `matched=0`；最近邻 0.29 / 0.40 < 阈值 0.5 |
| 单窗（3 s）NPU 推理 | ~112 ms；端到端 112–345 ms/条 |

## 在线（流式）demo

- 管线：流式进音（100 ms/块）→ silero VAD（CPU）分段（min_speech 0.25 s / min_silence 0.6 s）→
  每段结束即在 NPU 上计算 embedding 并做 1:N 识别 → 实时打印，例如
  `[ 12.3s] speaker=leijun score=0.8300 matched=1 len=2.1s`
- 两种输入方式（同一程序）：
  - `--wav wavs/conversation.wav`：按实时节奏喂入（可复现，用于自动验证）
  - `--stdin`：读 16 kHz 单声道 s16le 裸 PCM，配麦克风：
    `arecord -D plughw:4,0 -f S16_LE -r 16000 -c 1 -t raw -d 8 | ./bin/online_speaker_demo --stdin ...`
- 板端实测（2026-09-20）：
  - **Phase A（拼接对话流，31.2 s / 6 段音频）**：VAD 分出 8 个语音段，**8/8 全部识别正确**
    （分数 0.74–0.92，输出顺序与拼接顺序一致；同一说话人被 VAD 切分为多段时每段均识别正确）
  - **Phase B（麦克风 plughw:4,0，8 s）**：采音链路正常（成功采集 8.0 s）；
    自动运行期间无人说话故无语音段，有人对着麦克风说话即可看到逐段实时识别输出

## 说明

- 调用方式：`--speaker-provider rknn` 时 `--speaker-model` 指向 `.rknn` 模型（NPU）；改 `cpu` 则用 ONNX 模型
- 模型契约：固定 300 帧窗口（3.0 s，10 ms/帧），末窗零填充，逐窗推理后 embedding 取均值；
  元数据（sample_rate / feature_normalize_type / output_dim / window_frames）由模型 custom_string 携带
- `num_threads`：RKNN 后端下映射为 NPU core mask（普通线程数自动映射为 AUTO）
