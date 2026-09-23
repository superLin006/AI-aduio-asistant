# AI Audio Assistant

面向智慧教室的跨平台语音助手。仓库保留 Android 应用，并提供基于
Sophon BM1684X 和 Qwen3-ASR-0.6B 的纯 C++ Linux SDK。

## 目录

```text
apps/      可运行应用与演示（android = Kotlin 应用；android-kws-demo = “小慧”CPU 唤醒词；
           rk3588-speaker-demo = RK3588 声纹）
sdk/       C++17 SDK（include/ + src/ + tests/）及其 CLI 程序（cli/）
tools/     构建 / 运行 / 评测 / 数据脚本（build/ run/ eval/ data/）
assets/    无密钥配置模板与模型资产索引（configs/ models/）
docs/      架构与平台文档
build/     本地构建产物（忽略）
delivery/  本地交付产物（忽略；RK3588 声纹交付包所在）
```

![语音助手处理链路](docs/architecture/voice-assistant-pipeline.png)

## Linux / Sophon 快速验证

依赖：

- sherpa-onnx Sophon SDK（ORT 1.24.4）：`/home/xh/itc_project/sherpa-onnx-2025-1217/build-sophon-ort1244-verified/install`
  （由 `tools/build/build_sherpa_sophon_ort1244.sh` 生成）
- LLM/调度 SDK：`/home/xh/itc_project/deliver/deliver_dispatch_sdk_v10_w8bf16`
- Qwen3-ASR 模型：`/home/xh/itc_project/Sophon_model_zoo/Qwen3-ASR`
- Sophon SDK / Docker 镜像：`Sophon_model_zoo/0_Toolkits/soc-sdk-sp4`、`sophon-cross-build:latest`

```sh
sh tools/build/build_linux_sophon.sh
sh tools/run/run_offline_demo.sh
```

公共头文件为 `sdk/include/ai_audio_assistant/offline_asr.hpp`。Sophon 后端使用
`provider="sophon"`，并通过 sherpa-onnx 的 `qwen3_asr.encoder` 字段加载合并的
encoder + LLM BModel。单段音频上限为 30 秒，生产 Pipeline 应由 VAD 切段。

完整语音入口位于 `sdk/include/ai_audio_assistant/assistant_pipeline.hpp`，提供
KWS、VAD、唤醒保护窗口和 Qwen3-ASR 的类型化事件接口。

`ai_audio_voice_intent_demo` 在同一进程内连接完整语音 Pipeline 与所选意图后端，
对应运行脚本为 `tools/run/run_voice_intent_demo.sh`。
Linux 默认唤醒词为“小慧”，唯一配置位于 `assets/configs/kws/keywords_xiaohui.txt`。

意图识别公共接口位于 `sdk/include/ai_audio_assistant/intent_recognizer.hpp`，
同时支持板卡本地 Qwen3-0.6B 和 DeepSeek API。两者共用现有调度工具合同与
解析链路。Linux/Sophon 全链路统一使用 ONNX Runtime 1.24.4，并由同一个
`libai_audio_assistant.so` 提供；详见
[`docs/intent-backends.md`](docs/intent-backends.md)。

## Android 构建

先按 `assets/models/README.md` 补齐 Android KWS、ASR 模型，然后：

```sh
cd apps/android
./gradlew assembleDebug
```

DeepSeek 密钥不再写入源码。若需要在线意图识别，将下面配置写入用户级
`~/.gradle/gradle.properties`：

```properties
DEEPSEEK_API_KEY=your-local-key
```

没有密钥时应用会跳过在线意图识别，KWS/VAD/ASR 仍可独立运行。

## Linux / RK3588 声纹（RKNN）

声纹模块支持 RK3588 内置 NPU（sherpa-onnx RKNN 后端，`provider="rknn"`），独立 demo 工程见
[`apps/rk3588-speaker-demo/`](apps/rk3588-speaker-demo/)（与 `apps/android-kws-demo/` 同一种组织方式）：

```sh
cd apps/rk3588-speaker-demo
sh build.sh                 # 交叉编译（aarch64 / glibc 2.31；离线 + 在线两个程序）
BOARD_PASS=... sh deploy.sh # 部署 + 板端 demo（离线：注册→识别→陌生人拒绝；在线：VAD 流式 + 麦克风实时）
```

板端实测：离线注册 3 人 × 2 条、识别 **7/7**，陌生人 2/2 拒绝，端到端 112–345 ms/条；
在线（拼接对话流）8/8 语音段全部识别正确（**声纹在 NPU；VAD 默认 CPU**，NPU VAD 为可选实验项、
长流实测有漏检，详见 demo README），麦克风采集链路已验证。
第二款声纹模型 CAM++（`models/campplus_T300_fp.rknn`）同口径通过（7/7 + 拒绝 2/2），
单窗 32.7 ms（ERes2NetV2 为 111–120 ms）；交付包见 `delivery/`。

## 模型策略

Linux 默认使用已经在 BM1684X 验证的 Qwen3-ASR-0.6B W4BF16 group-64 合并模型。
需要重新获取原始模型时优先使用 ModelScope，其次使用 Hugging Face 镜像；编译后的
BModel 由 `Sophon_model_zoo` 管理，本仓库不重复保存。

注意：当前 sherpa-onnx Qwen tokenizer API 读取 `vocab.json + merges.txt`，不能只传
Hugging Face 的 `tokenizer.json`。部署脚本应把这两个文件放进同一个 tokenizer 目录。
