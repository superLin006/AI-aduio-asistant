# AI Audio Assistant

面向智慧教室的跨平台语音助手。仓库保留 Android 应用，并提供基于
Sophon BM1684X 和 Qwen3-ASR-0.6B 的纯 C++ Linux SDK。

## 目录

```text
android/   Android/Kotlin 应用
android-kws-demo/ 独立的“小慧”CPU 唤醒词 Android Demo
linux/     C++17 公共库、头文件、Demo 和测试
configs/   无密钥配置模板
models/    模型说明（大文件不进入 Git）
scripts/   构建和运行脚本
docs/      架构与平台文档
```

![语音助手处理链路](docs/architecture/voice-assistant-pipeline.png)

## Linux / Sophon 快速验证

依赖：

- sherpa-onnx Sophon SDK：`/home/xh/itc_project/sherpa-onnx-2025-1217/deliver`
- Qwen3-ASR 模型：`/home/xh/itc_project/Sophon_model_zoo/Qwen3-ASR`
- Docker 镜像：`sophon-cross-build:latest`

```sh
sh scripts/build_linux_sophon.sh
sh scripts/run_offline_demo.sh
```

公共头文件为 `linux/include/ai_audio_assistant/offline_asr.hpp`。Sophon 后端使用
`provider="sophon"`，并通过 sherpa-onnx 的 `qwen3_asr.encoder` 字段加载合并的
encoder + LLM BModel。单段音频上限为 30 秒，生产 Pipeline 应由 VAD 切段。

完整语音入口位于 `linux/include/ai_audio_assistant/assistant_pipeline.hpp`，提供
KWS、VAD、唤醒保护窗口和 Qwen3-ASR 的类型化事件接口。

`ai_audio_voice_intent_demo` 在同一进程内连接完整语音 Pipeline 与所选意图后端，
对应运行脚本为 `scripts/run_voice_intent_demo.sh`。
Linux 默认唤醒词为“小慧”，唯一配置位于 `configs/kws/keywords_xiaohui.txt`。

意图识别公共接口位于 `linux/include/ai_audio_assistant/intent_recognizer.hpp`，
同时支持板卡本地 Qwen3-0.6B 和 DeepSeek API。两者共用现有调度工具合同与
解析链路。Linux/Sophon 全链路统一使用 ONNX Runtime 1.24.4，并由同一个
`libai_audio_assistant.so` 提供；详见
[`docs/intent-backends.md`](docs/intent-backends.md)。

## Android 构建

先按 `models/README.md` 补齐 Android KWS、ASR 模型，然后：

```sh
cd android
./gradlew assembleDebug
```

DeepSeek 密钥不再写入源码。若需要在线意图识别，将下面配置写入用户级
`~/.gradle/gradle.properties`：

```properties
DEEPSEEK_API_KEY=your-local-key
```

没有密钥时应用会跳过在线意图识别，KWS/VAD/ASR 仍可独立运行。

## 模型策略

Linux 默认使用已经在 BM1684X 验证的 Qwen3-ASR-0.6B W4BF16 group-64 合并模型。
需要重新获取原始模型时优先使用 ModelScope，其次使用 Hugging Face 镜像；编译后的
BModel 由 `Sophon_model_zoo` 管理，本仓库不重复保存。

注意：当前 sherpa-onnx Qwen tokenizer API 读取 `vocab.json + merges.txt`，不能只传
Hugging Face 的 `tokenizer.json`。部署脚本应把这两个文件放进同一个 tokenizer 目录。
