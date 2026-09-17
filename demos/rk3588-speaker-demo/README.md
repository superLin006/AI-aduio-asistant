# RK3588 声纹识别 Demo（独立工程）

独立的一键演示目录（与 `demos/android-kws-demo/` 同一种组织方式），只做一件事：

```text
16 kHz WAV -> fbank 特征 -> ERes2NetV2（RK3588 内置 NPU, provider=rknn）-> 声纹注册 / 识别
```

演示流程：① 注册 3 位说话人（各 2 条）并保存声纹库 → ② 加载声纹库识别 7 条测试语音 → ③ 陌生人拒绝（未注册者被拒绝）。
不包含 KWS / ASR / 意图识别（完整语音管线见 `../../docs/linux-sophon.md`）。

## 目录

```text
demos/rk3588-speaker-demo/
├── README.md   本文件
├── build.sh    宿主机交叉编译（docker sophon-cross-build，glibc 2.31）
├── deploy.sh   一键：打包 -> 部署到板卡 -> 执行 demo（板卡 scp 不可用，走 tar 管道）
├── demo.sh     板端一键 demo 脚本（由 deploy.sh 上传并执行）
└── wavs/       13 条 16 kHz 真人验收音频（fangjun / leijun / liudehua）
```

demo 程序复用应用代码，不复制源码：`../../linux/apps/speaker_demo.cpp` + `../../linux/src/speaker_verifier.cpp`。

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

# 1) 交叉编译（产物 build/bin/ai_audio_speaker_demo，aarch64 / glibc 2.31）
sh build.sh

# 2) 一键部署 + 板端 demo（MODEL=... 可覆盖模型路径）
BOARD_PASS=... sh deploy.sh
# 板端目录: /userdata/ai_audio_speaker_rk3588_test
# 运行日志: build/board_run.log
```

## 板端实测（2026-09-16）

| 阶段 | 结果 |
|---|---|
| 注册 3 人 × 2 条 | 全部 ok，`speakers=3` |
| 识别 7 条测试语音 | **7/7 全对**（0.776–0.892） |
| 陌生人拒绝（库中剔除李德华） | identify 全部 `matched=0`；最近邻 0.29 / 0.40 < 阈值 0.5 |
| 单窗（3 s）NPU 推理 | ~112 ms；端到端 112–345 ms/条 |

## 说明

- 调用方式：`ai_audio_speaker_demo --speaker-model <model.rknn> --speaker-provider rknn --threshold 0.5 ...`
- CPU 对照：改 `--speaker-provider cpu` 并换 ONNX 模型即可（`--load/--save` 声纹库通用）
- `num_threads`：RKNN 后端下映射为 NPU core mask（普通线程数自动映射为 AUTO）
- 模型契约：固定 300 帧窗口（3.0 s，10 ms/帧），末窗零填充，逐窗推理后 embedding 取均值；
  元数据（sample_rate / feature_normalize_type / output_dim / window_frames）由模型 custom_string 携带
