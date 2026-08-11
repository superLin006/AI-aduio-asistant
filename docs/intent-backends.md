# 意图识别双后端

Linux 版本通过同一个 `IntentRecognizer` 接口提供两条平行链路：

- `IntentBackend::kLocal`：BM1684X 上的 LoRA Qwen3-0.6B W8BF16。
- `IntentBackend::kDeepSeek`：DeepSeek OpenAI-compatible API。

两条链路都复用现有 `llm-sdk` 的 `IntentService`，因此共享 `tools_dispatch.json`、
BGE 召回、候选工具 Prompt、合同校验、语义动作和 Resolver。业务控制层只消费
`IntentResult::semantic_plan` 或注册运行时上下文后的 `final_json`，不按后端维护分支。

## 统一运行时

Linux/Sophon 统一使用 ONNX Runtime 1.24.4：

```text
KWS/VAD/Qwen3-ASR
    │ ASR text
    ▼
local Qwen or DeepSeek intent
    │ semantic plan / final JSON
    ▼
DispatchExecutor + runtime DispatchContext
```

重新构建的 sherpa-onnx 和现有 `libllmsdk.so` 链接同一份 1.24.4，所有功能由
`libai_audio_assistant.so` 导出，可以在一个进程内运行。部署包只能包含一份
`libonnxruntime.so`；构建和 ELF 检查必须拒绝 `VERS_1.17.1`。

生产上仍可按故障隔离需求拆服务，但这不再是依赖约束。设备控制只允许消费合同
校验后的动作，不直接执行模型原始输出。

## 板卡运行

已有部署包默认路径为：

```sh
/data/deliver_dispatch_sdk_v9.5d_candidate_w8bf16
```

本地链路：

```sh
sh scripts/run_intent_demo.sh local '把信号源一在主屏上开一个窗口'
```

完整语音链路使用同一进程中的常驻模型对象：

```sh
WAKE_AUDIO=/path/to/wake.wav COMMAND_AUDIO=/path/to/command.wav \
  sh scripts/run_voice_intent_demo.sh local
```

云端链路（密钥仅通过进程环境提供）：

```sh
DEEPSEEK_API_KEY='...' sh scripts/run_intent_demo.sh deepseek \
  '把信号源一在主屏上开一个窗口'
```

DeepSeek 临时配置目录权限为 `0700`、配置文件为 `0600`，`LLMService` 初始化后
立即删除。禁止把密钥写进 JSON、脚本、Android 源码或 Git。

生产路由应显式选择 `local` 或 `deepseek`；若增加降级策略，建议默认
`local -> cloud`，并且仅在初始化、超时或传输失败时降级，不在模型正常返回
“无意图”时重试另一后端，以免同一句话产生两次设备操作。

## 设备执行边界

`llm-sdk` 的 `DispatchExecutor` 已负责多动作顺序执行、每步刷新
`DispatchContext`、超时和设备确认。本项目不复制这套逻辑。接入真实控制前，业务侧
还需提供 `DispatchDeviceAdapter`，把当前 VMC WebSocket/protobuf 客户端映射为实时的
墙、场景、窗口、信号源查询与 `ExecuteAndConfirm`。缺少真实上下文时只输出语义动作，
不得把占位字段直接下发到设备。
