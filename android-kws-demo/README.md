# 小慧 Android KWS Demo

独立的最小 Android 工程，只实现：

```text
麦克风 16 kHz PCM -> sherpa-onnx Zipformer KWS (CPU) -> 检测“小慧”
```

不包含 VAD、ASR、意图识别、网络请求或设备控制。当前发布包只支持
Android 8.1（API 27）以上的 `arm64-v8a` 平板。

## 构建

```sh
./gradlew assembleDebug
```

APK：`app/build/outputs/apk/debug/app-debug.apk`

安装：

```sh
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

首次打开允许麦克风权限，点击“开始监听”，然后说“小慧”。界面会显示
检测时间和累计次数。模型使用 INT8 encoder/joiner、FP32 decoder，推理
provider 固定为 `cpu`。

## 修改唤醒词

编辑 `app/src/main/assets/kws-wenetspeech/keywords.txt`。文件内容必须是
sherpa-onnx KWS token 格式，不能直接只写汉字。当前内容为：

```text
x iǎo h uì @小慧
```

模型和 JNI 均随 APK 打包，运行时不下载文件、不访问网络。平板必须是
ARM64；可用 `adb shell getprop ro.product.cpu.abi` 确认返回 `arm64-v8a`。
