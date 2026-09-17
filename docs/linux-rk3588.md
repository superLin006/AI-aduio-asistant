# Linux RK3588 (RKNN) 声纹部署

`linux/` 的声纹模块支持在 RK3588 **内置 NPU** 上运行（sherpa-onnx RKNN 后端，`provider="rknn"`）。
目标板卡：`172.16.58.55`（Buildroot, RKNPU 驱动 0.9.8, librknnrt 2.3.2）。

## 前置资源

- sherpa-onnx（RKNN 后端）安装目录：
  `/home/xh/itc_project/sherpa-onnx-2025-1217/build-rknn/install`
  （构建方式：该仓库 `build-rknn-docker.sh`，docker `sophon-cross-build` / glibc 2.31）
- ERes2NetV2 RKNN 模型（固定 300 帧窗口，含 sherpa 元数据）：
  `/home/xh/itc_project/RK_model_zoo/rknn2/eres2netv2/models/eres2netv2_T300_fp.rknn`
  （转换/精度/性能验收记录见该工程 `.context/` 与 README）

## 构建

```sh
sh scripts/build_rk3588_speaker.sh
# 产物: build/rk3588_speaker/bin/ai_audio_speaker_demo (aarch64, glibc 2.31 <= 板端 2.34)
```

## 部署与一键 demo

板卡 scp 子系统不可用，脚本用 tar 管道传输；`BOARD_PASS` 从 `0_Note/board_credentials.md` 获取后导出：

```sh
BOARD_PASS=... sh scripts/deploy_rk3588_speaker_test.sh
# 板端目录: /userdata/ai_audio_speaker_rk3588_test
# 回归日志: build/rk3588_speaker/board_run.log
```

部署完成后自动在板端执行一键 demo（板端脚本 `scripts/run_rk3588_speaker_demo.sh`）：

1. **Phase 1**：注册 3 位说话人（fangjun/leijun/liudehua，各 2 条）并保存声纹库
2. **Phase 2**：加载声纹库，识别 7 条测试语音
3. **Phase 3**：陌生人拒绝——声纹库仅含 fangjun/leijun 时，李德华的语音在阈值 0.5 下被拒绝（最近邻分数 0.29 / 0.40，余量清晰）

实测（2026-09-16）：Phase 2 识别 **7/7 全对**（0.776–0.892）；Phase 3 陌生人 2/2 全部拒绝。

## 声纹识别（RKNN 路径）

- `SpeakerVerifierConfig::provider = "rknn"` 时，`model_path` 指向 `.rknn` 模型，embedding 在 NPU 计算
- 模型契约：固定 300 帧窗口（3.0 s，10 ms/帧），末窗零填充，逐窗推理后 embedding 取均值；
  元数据（sample_rate / feature_normalize_type / output_dim / window_frames 等）由模型
  custom_string 携带，由 sherpa 侧校验
- 其余语义与 ONNX 路径一致：注册（多条平均）、1:1 验证、1:N 识别、JSON 持久化

实测（2026-09-16，板卡 172.16.58.55）：注册 3 人 × 2 条全部成功；识别 **7/7 全对**
（分数 0.78–0.89，与 BM1684X 历史结果同档）；端到端（wav→特征→NPU→识别）112–345 ms/条。
