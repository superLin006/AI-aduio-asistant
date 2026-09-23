# RK3588 声纹识别运行验证包

本包用于在 RK3588 板卡上运行预编译的离线和在线声纹 demo。目标平台是 AArch64 Buildroot；不能直接在 x86 WSL 主机上运行包内程序。

## 包含内容

- `bin/`：离线注册/识别程序和在线 VAD/声纹程序。
- `lib/`：与程序配套的 `libsherpa-onnx`、ONNX Runtime、RKNN Runtime 动态库。
- `models/eres2netv2_T300_fp.rknn`：ERes2NetV2 声纹模型。
- `models/campplus_T300_fp.rknn`：CAM++ 声纹模型（可选，见下文「第二个声纹模型」）。
- `models/silero_vad.onnx`：默认 CPU VAD 模型。
- `models/silero_vad.rknn`：可选 RKNN VAD 模型，长对话流识别效果不如 CPU 版，默认不启用。
- `wavs/`：离线注册/测试语音及在线对话流样例。
- `demo.sh`、`online_demo.sh`：板端验证脚本。
- `sdk/`：匹配的 C/C++ 头文件、RKNN 版 sherpa-onnx 库，以及直接调用 C++ 头文件的示例源码。

声纹模型大小为 45,822,764 字节，SHA-256：

```text
4d7bd62274b09beec09c5cfb4a82714768c9d091f36b3fd0b5d7d397f984759b
```

## 板端运行

解压后可用 `sha256sum -c SHA256SUMS` 检查包内文件完整性。

将压缩包复制到 RK3588 板卡并解压：

```sh
# Buildroot/BusyBox 板卡的 tar 不支持 -z，用 gzip 管道解压
gzip -dc rk3588-speaker-verification.tar.gz | tar -xf -
cd rk3588-speaker-verification
sh demo.sh
```

`demo.sh` 会依次执行：

1. 注册 3 位说话人，每人 2 条语音。
2. 加载保存的声纹库并识别 7 条测试语音。
3. 临时排除一位说话人，验证陌生人拒绝和最近邻分数。

在线对话流和麦克风验证：

```sh
sh online_demo.sh
MIC_DEV=plughw:4,0 MIC_SECONDS=8 sh online_demo.sh
```

在线 demo 默认使用 `silero_vad.onnx`。切换到实验性 RKNN VAD 时运行：

```sh
VAD=./models/silero_vad.rknn VAD_PROVIDER=rknn sh online_demo.sh
```

麦克风阶段默认设备为 `plughw:4,0`（XMOS USB 麦克风）。板卡没有该设备时用
`arecord -l` 查实际采集设备并用 `MIC_DEV` 覆盖（例如板载编解码器 `plughw:3,0`）；没有可用麦克风时
可忽略该阶段（对话流阶段已覆盖 VAD 分段与识别逻辑）。

## 通过 C++ 头文件调用

`sdk/include/sherpa-onnx/c-api/cxx-api.h` 声明 `sherpa_onnx::cxx::SpeakerVerifier`，匹配的交叉编译库在 `sdk/lib/`。示例源码 `sdk/speaker-verifier-cxx-api.cc` 在代码里设置模型路径并直接调用 API。

SDK 库与板端 demo 共用包内 `lib/` 的 ONNX Runtime 1.24.4 和 RKNN Runtime。在有 AArch64 GCC 工具链的环境编译：

```sh
  aarch64-linux-gnu-g++ -std=c++17 -I sdk/include \
  sdk/speaker-verifier-cxx-api.cc \
  -L sdk/lib -lsherpa-onnx-cxx-api -L lib -lonnxruntime -lrknnrt \
  -o sdk/speaker-verifier
```

从交付包根目录运行：

```sh
LD_LIBRARY_PATH=./sdk/lib:./lib ./sdk/speaker-verifier
```

程序从 `models/eres2netv2_T300_fp.rknn` 和 `wavs/` 读取模型与样例音频，并生成 `speakers.json`。

示例捕获了构造异常：配置无效、或 `speakers_path` 指向的文件存在但无法解析时，构造会抛出异常（文件不存在则按空库启动）；方法调用（注册/识别/校验）失败通过返回 `false` 表达。

## 第二个声纹模型：CAM++（可选）

除默认的 ERes2NetV2 外，包内还提供 3D-Speaker **CAM++**（zh-cn 16k common）的 NPU 版本
`models/campplus_T300_fp.rknn`（20.7 MiB；源 ONNX 28,281,138 B，
sha256 `f682b514c05d947ee3fa91cd6ec6c5c7543479a128373fa29b1faedccd21fd11`，
ModelScope 标注 Apache-2.0；与本应用 Android 端 asset 逐字节相同）。

```sh
MODEL=./models/campplus_T300_fp.rknn sh demo.sh
MODEL=./models/campplus_T300_fp.rknn sh online_demo.sh
# 或直接用 --speaker-model ./models/campplus_T300_fp.rknn --speaker-provider rknn
# SDK 示例：把 sdk/speaker-verifier-cxx-api.cc 里的模型路径改成 ./models/campplus_T300_fp.rknn 后按上文命令重编即可
```

板端实测（172.16.40.115）：离线注册 3 人、识别 **7/7**（0.757–0.860）、陌生人 **2/2** 拒绝
（最近邻 0.305/0.358 < 0.5）；在线对话流 **8/8**（0.680–0.894）；SDK 示例
`identified=fangjun 0.844312 / verified=yes`。**单窗 32.7 ms**（ERes2NetV2 为 111–120 ms）。

模型契约与 ERes2NetV2 相同（80 维 fbank + 整段逐维减均值、300 帧窗口、192 维 embedding），
额外带 `window_align=right` 元数据：末窗对齐句尾，句长 ≥3 s 时无零填充。CAM++ 的统计池化对
零填充很敏感（左对齐零填充时固定窗口与整段余弦最低 0.758，右对齐为 0.975），因此该模型必须
配合支持 `window_align` 的后端（本包 `lib/` 已包含）。

已知限制：CAM++ 在 RK3588 NPU 上的**逐窗保真度约 0.94**（工具链模拟器 0.999996；全零/常数/
随机输入同样偏移，各优化档位逐位相同），属该架构在 NPU fp16 下的系统性算子差异；
端到端识别不受影响（注册与查询同走 NPU，same/cross 余量 0.81/0.24）。

## 板卡和库说明

- 目标为 RK3588 AArch64 Buildroot；板端需有与固件匹配的 RKNPU 驱动。当前验收环境为 librknnrt 2.3.2、RKNPU 驱动 0.9.8。
- 运行脚本会设置 `LD_LIBRARY_PATH=lib`，使用包内 sherpa-onnx、ONNX Runtime 1.24.4 和 RKNN Runtime 库。SDK 示例设置 `LD_LIBRARY_PATH=./sdk/lib:./lib`，复用同一份 ONNX Runtime 和 RKNN Runtime。
- `sdk/lib/` 中的 sherpa-onnx C/C++ 库由 GitLab `itc` 分支提交 `d6f88c6`（含 RKNN 声纹后端移植 `6a7ee9d`、JSON 加固 `1b4f57f`、审查跟进 `36b80b9` 与 CAM++ 的 `window_align` 支持）交叉编译，启用了 RKNN（含 RKNN 声纹后端），并导出了 `SpeakerVerifier`；它们已针对包内 ONNX Runtime 1.24.4（仓库 cmake 默认版本）重新链接。
- `ai_audio_assistant::SpeakerVerifier` 应用封装已编入两个预编译 demo（内部为对 sherpa `SpeakerVerifier` 的薄适配，与 SDK 示例共用同一实现）。
- `lib/` 中的 sherpa-onnx C/C++ 库来自 GitLab `itc` 分支提交 `d6f88c6`（含后端移植 `6a7ee9d`、JSON 加载加固 `1b4f57f`、审查跟进 `36b80b9` 与 CAM++ 的 `window_align` 支持）；`sdk/lib/` 通过相对符号链接指向这同一组库。预编译 demo 与 SDK 示例共用这组 Sherpa 库、同一份 ONNX Runtime 和 RKNN Runtime。SDK 头文件放在 `sdk/include/`。

`speakers_demo.json` 是运行离线脚本时生成的临时注册库；日志输出在终端。验收 WAV 为内部测试音频，仅用于本次内部验证。
