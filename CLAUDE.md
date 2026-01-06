# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

这是一个面向课堂自动化的AI语音助手Android应用，专为RK3576和MTKG520硬件平台设计。应用实现了完整的语音交互流程：唤醒词检测 → 语音命令识别 → 意图解析 → 设备控制。

**技术栈**: Android Kotlin、Jetpack Compose、Sherpa-ONNX（语音识别）、DeepSeek API（LLM意图解析）、C++ JNI原生库（ONNX/RKNN推理）。

## 构建与开发命令

### 构建项目
```bash
# 构建debug APK
cd SherpaOnnxSimulateStreamingAsr
./gradlew assembleDebug

# 构建release APK
./gradlew assembleRelease

# 安装到连接的设备
./gradlew installDebug

# 构建并安装
./gradlew build installDebug
```

### 运行与测试
```bash
# 清理构建
./gradlew clean

# 通过adb安装运行
adb install -r app/build/outputs/apk/debug/app-debug.apk

# 查看语音助手相关日志
adb logcat | grep -E "VoiceAssistant|KeywordSpotter|OfflineRecognizer"
```

### 项目位置
所有Gradle命令必须在 `SherpaOnnxSimulateStreamingAsr/` 目录下执行。

## 架构概览

### 语音助手状态机

应用遵循由 `VoiceAssistant.kt` 控制的三状态流程：

1. **LISTENING（监听中）**: 通过KeywordSpotter监听唤醒词
2. **ACTIVATED（已激活）**: 检测到唤醒词，播放"listening.mp3"，准备接收命令
3. **PROCESSING（处理中）**: VAD + ASR激活，识别语音命令

**主处理循环** (Home.kt:118-527):
- 音频采集运行在Dispatchers.IO（16kHz，0.1秒块）
- 处理运行在Dispatchers.Default
- 状态转换触发不同的音频处理路径

### 核心组件

**语音识别层** (`com.k2fsa.sherpa.onnx` 包):
- `KeywordSpotter.kt` - 唤醒词检测（如"你好军哥"、"小艺小艺"）
- `OfflineRecognizer.kt` - 唤醒后的批量ASR（支持44+种模型类型）
- `OnlineRecognizer.kt` - 流式ASR（27+种模型，当前未使用）
- `Vad.kt` - 语音活动检测（Silero VAD模型）

**语音助手** (`com.k2fsa.sherpa.onnx.simulate.streaming.asr` 包):
- `VoiceAssistant.kt` - 状态机控制器
- `VoiceAssistantManager.kt` - 单例生命周期管理器
- `MainActivity.kt` - 入口点，初始化所有模型
- `screens/Home.kt` - UI和主音频处理循环

**意图与命令执行**:
- `IntentManager.kt` - 基于LLM的自然语言到意图解析器
- `DeepSeekClient.kt` - DeepSeek Chat模型的API客户端
- `CommandExecutor.kt` - 在教室设备上执行解析的意图
- 设备控制器：WhiteboardController、ProjectorController、CurtainController、LightController、AirConditionerController、SpeakerController（当前为模拟实现）

### 初始化序列

在 `MainActivity.onCreate()` 中：
1. 请求RECORD_AUDIO权限
2. `initOfflineRecognizer()` - 加载ASR模型（类型39：zipformer-ctc-small-zh）
3. `initVad()` - 加载VAD模型（类型0：Silero VAD）
4. `VoiceAssistantManager.initVoiceAssistant()` - 设置KWS（类型0：zipformer-wenetspeech）
5. `IntentManager.initialize()` - 设置DeepSeek API客户端

所有模型通过AssetManager从assets/加载，并通过JNI绑定初始化。

## Sherpa-ONNX模型系统

### 当前模型配置

**关键词识别（KWS）** - 类型0:
- 模型：`sherpa-onnx-kws-zipformer-wenetspeech-3.3M-2024-01-01`
- 位置：`app/src/main/assets/sherpa-onnx-kws-zipformer-wenetspeech-3.3M-2024-01-01/`
- 关键词：在`keywords.txt`中定义（9个唤醒词，包括"你好军哥"、"小艺小艺"、"小米小米"）
- 文件：encoder/decoder/joiner ONNX模型 + tokens.txt

**ASR（自动语音识别）** - 类型39:
- 模型：`sherpa-onnx-zipformer-ctc-small-zh-fp16-2025-07-16`
- 类型：ZipformerCtc（中文）
- 文件：`model.fp16.onnx`
- 提供者：CPU（可切换到"rknn"以启用硬件加速）

**VAD（语音活动检测）** - 类型0:
- 模型：Silero VAD
- 文件：`silero_vad.onnx`
- 配置：窗口512采样，最小静音0.8秒，最小语音0.25秒，16kHz采样率

### 添加/更改模型

模型通过各自类中的工厂函数配置：
- KWS：`KeywordSpotter.kt` → `getKwsModelConfig(type: Int)`
- ASR：`OfflineRecognizer.kt` → `getOfflineModelConfig(type: Int)`（第686-790行）
- VAD：`Vad.kt` → `getVadModelConfig(type: Int)`

添加新模型：
1. 将模型文件放入 `app/src/main/assets/`
2. 在相应的 `getXxxModelConfig()` 函数中添加新类型编号
3. 指定模型路径、tokens文件和提供者（cpu/rknn）
4. 如需新文件扩展名，更新 `build.gradle.kts` 中的aaptOptions

### 硬件加速（RKNN）

类型100-102支持RK3576/RK3588硬件的RKNN加速：
- 类型100：SenseVoice（基于Paraformer，多语言）
- 类型101：Whisper medium
- 类型102：Paraformer三语言

在模型配置中设置 `provider = "rknn"` 以启用NPU加速。

## 重要实现细节

### 内存管理
所有ONNX模型必须在生命周期回调中显式释放：
- `MainActivity.onDestroy()` 释放OfflineRecognizer和VAD
- `VoiceAssistant.release()` 释放KeywordSpotter
- 未释放会导致内存泄漏（模型在内存中占用100+ MB）

### 线程安全
- `SimulateStreamingAsr` 单例使用同步初始化
- 音频处理使用独立协程：采集（IO）vs 处理（Default）
- StateFlow提供线程安全的状态更新

### 音频处理
- 采样率：16kHz 单声道 PCM 16位
- 缓冲区大小：0.1秒块（1600采样）
- VAD以512采样窗口处理
- ASR累积采样直到VAD检测到语音段结束

### 错误处理
- VAD/ASR操作包裹在try-catch中，带状态重置回退
- DeepSeekClient中的网络错误记录日志但不会崩溃应用
- assets中缺少模型会导致初始化失败（检查日志）

## 关键文件位置

**入口点**:
- `SherpaOnnxSimulateStreamingAsr/app/src/main/java/com/k2fsa/sherpa/onnx/simulate/streaming/asr/MainActivity.kt` - 应用初始化
- `SherpaOnnxSimulateStreamingAsr/app/src/main/java/com/k2fsa/sherpa/onnx/simulate/streaming/asr/screens/Home.kt` - 主音频处理循环

**语音识别**:
- `SherpaOnnxSimulateStreamingAsr/app/src/main/java/com/k2fsa/sherpa/onnx/KeywordSpotter.kt` - 唤醒词检测
- `SherpaOnnxSimulateStreamingAsr/app/src/main/java/com/k2fsa/sherpa/onnx/OfflineRecognizer.kt` - 支持44+种模型的ASR
- `SherpaOnnxSimulateStreamingAsr/app/src/main/java/com/k2fsa/sherpa/onnx/Vad.kt` - 语音活动检测

**语音助手**:
- `SherpaOnnxSimulateStreamingAsr/app/src/main/java/com/k2fsa/sherpa/onnx/VoiceAssistant.kt` - 状态机
- `SherpaOnnxSimulateStreamingAsr/app/src/main/java/com/k2fsa/sherpa/onnx/IntentManager.kt` - LLM意图解析
- `SherpaOnnxSimulateStreamingAsr/app/src/main/java/com/k2fsa/sherpa/onnx/CommandExecutor.kt` - 设备控制

**资源文件**:
- `SherpaOnnxSimulateStreamingAsr/app/src/main/assets/` - 所有ONNX模型、关键词、音频文件
- `SherpaOnnxSimulateStreamingAsr/app/src/main/jniLibs/` - 原生库（libsherpa-onnx-jni.so、libonnxruntime.so、librga.so）

**构建配置**:
- `SherpaOnnxSimulateStreamingAsr/app/build.gradle.kts` - 应用级构建配置（SDK版本、Compose、依赖）
- `SherpaOnnxSimulateStreamingAsr/build.gradle.kts` - 项目级配置
- `SherpaOnnxSimulateStreamingAsr/settings.gradle.kts` - 仓库镜像（阿里云中国网络优化）

## 意图识别系统

应用使用DeepSeek Chat API进行自然语言理解：
- **端点**: `https://api.deepseek.com/chat/completions`
- **模型**: `deepseek-chat`
- **输入**: 来自ASR的用户语音命令文本
- **输出**: 包含 `action`、`target`、`parameters`、`confidence` 的结构化JSON

**支持的操作**: open（打开）、close（关闭）、adjust（调节）、query（查询）
**支持的设备**: whiteboard（白板）、projector（投影仪）、curtain（窗帘）、light（灯光）、air_conditioner（空调）、speaker（音响）

修改意图识别时：
- 更新 `DeepSeekClient.kt:callApi()` 中的系统提示
- 在 `CommandExecutor.kt` 中添加新设备类型
- 实现实际设备SDK集成（当前为模拟）

## 音频反馈

`app/src/main/assets/sounds/` 中的音效：
- `listening.mp3` - "你说我在听"（检测到唤醒词后播放）
- `processing.mp3` - "我来帮你操作"（执行命令前播放）
- `completed.mp3` - "操作已完成"（成功执行后播放）

由 `AudioPlayer.kt` 单例使用MediaPlayer管理。

## 调试语音识别

调试ASR问题：
1. 检查logcat中的KeywordSpotter/OfflineRecognizer消息
2. 验证 `assets/sherpa-onnx-kws-zipformer-wenetspeech-3.3M-2024-01-01/keywords.txt` 中的唤醒词
3. 通过调整Vad.kt中的 `minSilenceDuration` 和 `minSpeechDuration` 测试VAD灵敏度
4. 通过更改 `MainActivity.initOfflineRecognizer()` 中的类型切换ASR模型
5. 在Home.kt中启用中间结果显示（已实现，每500ms显示一次）
