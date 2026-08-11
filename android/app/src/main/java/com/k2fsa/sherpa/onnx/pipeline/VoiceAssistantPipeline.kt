package com.k2fsa.sherpa.onnx.pipeline

import android.content.Context
import android.util.Log
import com.k2fsa.sherpa.onnx.config.ModelConfig
import com.k2fsa.sherpa.onnx.simulate.streaming.asr.*
import kotlinx.coroutines.*
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.channels.BufferOverflow

/**
 * Pipeline处理结果数据类
 */
data class PipelineResult(
    val timestamp: String,
    val text: String,
    val isWakeWord: Boolean = false,
    val isFinal: Boolean = true,
    val resultType: ResultType = ResultType.ASR
)

enum class ResultType {
    WAKE_WORD,      // 唤醒词检测结果
    ASR_INTERIM,    // ASR中间结果
    ASR_FINAL,      // ASR最终结果
    ASR,            // 普通ASR结果
    INTENT_SUCCESS, // 意图执行成功
    INTENT_FAILED   // 意图执行失败
}

/**
 * 语音助手Pipeline
 * 负责协调音频流处理：KWS（唤醒词）→ VAD → ASR → Intent识别 → 设备控制
 */
class VoiceAssistantPipeline(
    private val context: Context,
    private val onIntermediateResult: (PipelineResult) -> Unit,
    private val onFinalResult: (PipelineResult) -> Unit,
    private val recordingStartTime: Long
) {
    private val tag = "VoiceAssistantPipeline"

    // 有界音频队列：处理端落后时丢弃最旧块，避免长时间运行耗尽内存。
    private val samplesChannel = Channel<FloatArray>(
        capacity = ModelConfig.Runtime.AUDIO_QUEUE_CAPACITY,
        onBufferOverflow = BufferOverflow.DROP_OLDEST
    )

    // 处理协程
    private var processingJob: Job? = null
    private var isRunning = false

    // VAD和ASR状态
    private var buffer = arrayListOf<Float>()
    private var offset = 0
    private val windowSize = ModelConfig.Runtime.VAD_WINDOW_SIZE
    private var isSpeechStarted = false
    private var speechStartTime = 0L
    private var startTime = System.currentTimeMillis()
    private var lastText = ""
    private var added = false
    private var currentIndex = -1
    private var asrStartTime = 0L

    // Intent识别状态
    private var intentRecognitionStarted = false
    private var intentRecognitionCompleted = false

    // 上一个状态（用于检测状态变化）
    private var lastState = AssistantState.LISTENING

    /**
     * 启动Pipeline处理
     */
    fun start() {
        if (isRunning) {
            logWarning("Pipeline已在运行")
            return
        }

        isRunning = true
        log("启动Pipeline处理")

        // 重置VAD和VoiceAssistant
        SimulateStreamingAsr.vad.reset()
        VoiceAssistantManager.assistant?.reset()

        // 启动音频处理协程
        processingJob = CoroutineScope(Dispatchers.Default).launch {
            processAudioStream()
        }
    }

    /**
     * 停止Pipeline处理
     */
    fun stop() {
        if (!isRunning) {
            return
        }

        log("停止Pipeline处理")
        isRunning = false

        // 取消处理协程
        processingJob?.cancel()
        processingJob = null

        // 发送空数组标记结束
        samplesChannel.trySend(FloatArray(0))
    }

    /**
     * 喂入音频数据
     * @param samples 归一化的音频采样（Float数组）
     */
    fun feedAudio(samples: FloatArray) {
        if (!isRunning) {
            return
        }

        // 发送到通道（非阻塞）
        samplesChannel.trySend(samples)
    }

    /**
     * 核心音频流处理循环
     */
    private suspend fun processAudioStream() {
        log("音频处理循环启动")

        while (isRunning) {
            for (s in samplesChannel) {
                if (s.isEmpty()) {
                    break
                }

                val assistant = VoiceAssistantManager.assistant
                if (assistant == null) {
                    logWarning("VoiceAssistant未初始化")
                    continue
                }

                // 检测状态变化
                val currentState = assistant.state.value
                if (currentState != lastState) {
                    log("状态变化: $lastState -> $currentState")

                    if (currentState == AssistantState.LISTENING) {
                        log("清理状态，回到LISTENING")

                        // 重置所有状态
                        buffer.clear()
                        offset = 0
                        isSpeechStarted = false
                        added = false
                        asrStartTime = 0L
                        lastText = ""
                        intentRecognitionStarted = false
                        intentRecognitionCompleted = false

                        try {
                            delay(100)
                            SimulateStreamingAsr.vad.reset()
                            log("VAD重置完成")
                        } catch (e: Exception) {
                            logError("VAD重置异常: ${e.message}")
                        }
                    }

                    lastState = currentState
                }

                // 根据状态处理音频
                when (assistant.state.value) {
                    AssistantState.LISTENING -> {
                        processListeningState(assistant, s)
                    }

                    AssistantState.ACTIVATED -> {
                        processActivatedState(assistant)
                    }

                    AssistantState.PROCESSING -> {
                        processProcessingState(assistant, s)
                    }
                }
            }
        }

        log("音频处理循环停止")
    }

    /**
     * 处理LISTENING状态：监听唤醒词
     */
    private fun processListeningState(assistant: VoiceAssistant, samples: FloatArray) {
        assistant.processAudio(samples, ModelConfig.Runtime.SAMPLE_RATE)

        val result = assistant.result.value
        if (result != null && result.state == AssistantState.ACTIVATED) {
            val timestamp = formatTimestamp(System.currentTimeMillis() - recordingStartTime)

            // 回调唤醒词结果
            onFinalResult(
                PipelineResult(
                    timestamp = timestamp,
                    text = "🔔 唤醒词: ${result.keyword}",
                    isWakeWord = true,
                    isFinal = true,
                    resultType = ResultType.WAKE_WORD
                )
            )

            log("检测到唤醒词: ${result.keyword}")
            log("转换到PROCESSING状态")
            assistant.transitionToProcessing()

            // 重置状态
            SimulateStreamingAsr.vad.reset()
            buffer = arrayListOf()
            offset = 0
            isSpeechStarted = false
            added = false
            asrStartTime = System.currentTimeMillis()
            intentRecognitionStarted = false
        }
    }

    /**
     * 处理ACTIVATED状态：强制转换到PROCESSING
     */
    private fun processActivatedState(assistant: VoiceAssistant) {
        log("ACTIVATED状态，强制转换到PROCESSING")
        assistant.transitionToProcessing()
        asrStartTime = System.currentTimeMillis()
    }

    /**
     * 处理PROCESSING状态：VAD + ASR + Intent识别
     */
    private suspend fun processProcessingState(assistant: VoiceAssistant, samples: FloatArray) {
        val totalElapsed = System.currentTimeMillis() - asrStartTime

        // 超时检查（7秒）
        if (assistant.state.value == AssistantState.PROCESSING &&
            asrStartTime > 0 && totalElapsed > 7000 && totalElapsed < 10000 &&
            !intentRecognitionStarted && !intentRecognitionCompleted && lastText.isNotBlank()
        ) {
            logWarning("超时检查触发！totalElapsed=$totalElapsed")
            log("使用lastText进行意图识别: $lastText")

            handleTimeout(assistant, lastText)
            return
        }

        // 添加音频到缓冲区
        buffer.addAll(samples.toList())

        if (assistant.state.value != AssistantState.PROCESSING) {
            logWarning("状态已改变，跳过VAD处理")
            return
        }

        // VAD处理
        processVAD(assistant)

        // 实时ASR（中间结果）
        processInterimASR(assistant)

        // 处理完整的语音段（VAD检测到结束）
        processFinalASR(assistant)
    }

    /**
     * VAD处理：语音活动检测
     */
    private fun processVAD(assistant: VoiceAssistant) {
        while (offset + windowSize < buffer.size) {
            if (assistant.state.value != AssistantState.PROCESSING) {
                logWarning("状态已改变，停止VAD处理")
                break
            }

            try {
                SimulateStreamingAsr.vad.acceptWaveform(
                    buffer.subList(offset, offset + windowSize).toFloatArray()
                )
            } catch (e: Exception) {
                logError("VAD acceptWaveform错误，重置VAD: ${e.message}")
                try {
                    SimulateStreamingAsr.vad.reset()
                } catch (resetException: Exception) {
                    logError("VAD重置错误: ${resetException.message}")
                }
                buffer.clear()
                offset = 0
                break
            }
            offset += windowSize

            if (!isSpeechStarted && SimulateStreamingAsr.vad.isSpeechDetected()) {
                isSpeechStarted = true
                startTime = System.currentTimeMillis()
                speechStartTime = System.currentTimeMillis()
                log("检测到语音")
            }
        }
    }

    /**
     * 处理实时ASR（中间结果）
     */
    private fun processInterimASR(assistant: VoiceAssistant) {
        if (!ModelConfig.Features.ENABLE_INTERIM_RESULTS) {
            return
        }

        val elapsed = System.currentTimeMillis() - startTime
        if (isSpeechStarted && elapsed > ModelConfig.Pipeline.ASR_INTERIM_RESULT_INTERVAL_MS && elapsed < 6500) {
            val stream = SimulateStreamingAsr.recognizer.createStream()
            stream.acceptWaveform(
                buffer.subList(0, offset).toFloatArray(),
                ModelConfig.Runtime.SAMPLE_RATE
            )
            SimulateStreamingAsr.recognizer.decode(stream)
            val result = SimulateStreamingAsr.recognizer.getResult(stream)
            stream.release()

            lastText = result.text

            if (lastText.isNotBlank()) {
                val timestamp = formatTimestamp(speechStartTime - recordingStartTime)

                val tempResult = PipelineResult(
                    timestamp = timestamp,
                    text = lastText,
                    isWakeWord = false,
                    isFinal = false,
                    resultType = ResultType.ASR_INTERIM
                )

                if (!added) {
                    currentIndex = -1  // 由UI层管理索引
                    added = true
                }

                // 回调中间结果
                onIntermediateResult(tempResult)
            }

            startTime = System.currentTimeMillis()
        }
    }

    /**
     * 处理最终ASR（VAD检测到语音段结束）
     */
    private suspend fun processFinalASR(assistant: VoiceAssistant) {
        while (!SimulateStreamingAsr.vad.empty()) {
            if (intentRecognitionStarted) {
                SimulateStreamingAsr.vad.pop()
                continue
            }

            try {
                val speechSegment = SimulateStreamingAsr.vad.front()
                val samples = speechSegment.samples

                log("处理最终语音段")

                val stream = SimulateStreamingAsr.recognizer.createStream()
                stream.acceptWaveform(samples, ModelConfig.Runtime.SAMPLE_RATE)
                SimulateStreamingAsr.recognizer.decode(stream)
                val result = SimulateStreamingAsr.recognizer.getResult(stream)
                stream.release()

                log("最终ASR结果: ${result.text}")

                isSpeechStarted = false
                SimulateStreamingAsr.vad.pop()

                buffer = arrayListOf()
                offset = 0

                if (result.text.isNotBlank()) {
                    val timestamp = formatTimestamp(speechStartTime - recordingStartTime)

                    val finalResult = PipelineResult(
                        timestamp = timestamp,
                        text = result.text,
                        isWakeWord = false,
                        isFinal = true,
                        resultType = ResultType.ASR_FINAL
                    )

                    // 回调最终结果
                    onFinalResult(finalResult)

                    log("发送ASR结果到意图识别: ${result.text}")

                    // 处理意图识别
                    processIntent(assistant, result.text)

                    added = false
                    asrStartTime = System.currentTimeMillis() + 999999999L
                    lastText = ""
                }
            } catch (e: Exception) {
                logError("处理语音段异常: ${e.message}")
                SimulateStreamingAsr.vad.pop()
                buffer = arrayListOf()
                offset = 0
            }
        }
    }

    /**
     * 处理意图识别
     */
    private suspend fun processIntent(assistant: VoiceAssistant, text: String) {
        if (!ModelConfig.Features.ENABLE_INTENT_RECOGNITION) {
            assistant.onAsrResult(text, isFinal = true)
            return
        }

        assistant.cancelTimeout()

        // 播放处理提示音
        if (ModelConfig.Features.ENABLE_AUDIO_FEEDBACK) {
            withContext(Dispatchers.Main) {
                AudioPlayer.play(context, SoundPaths.PROCESSING) {
                    log("处理提示音播放完成")
                }
            }
        }

        if (IntentManager.isReady()) {
            log("开始意图识别...")

            intentRecognitionStarted = true

            IntentManager.processCommand(text) { intentResult ->
                when (intentResult.state) {
                    IntentProcessingState.COMPLETED -> {
                        val msg = intentResult.commandResult?.message ?: "完成"
                        log("意图识别成功: $msg")

                        intentRecognitionCompleted = true

                        // 回调成功结果
                        onFinalResult(
                            PipelineResult(
                                timestamp = formatTimestamp(System.currentTimeMillis() - recordingStartTime),
                                text = "✅ $msg",
                                isWakeWord = false,
                                isFinal = true,
                                resultType = ResultType.INTENT_SUCCESS
                            )
                        )

                        assistant.returnToListening("Command executed")
                    }

                    IntentProcessingState.FAILED -> {
                        val reason = if (intentResult.intent?.isValid == false) {
                            "无法识别的指令"
                        } else {
                            intentResult.commandResult?.message ?: "执行失败"
                        }
                        logWarning("意图识别失败: $reason")

                        intentRecognitionCompleted = true

                        // 回调失败结果
                        onFinalResult(
                            PipelineResult(
                                timestamp = formatTimestamp(System.currentTimeMillis() - recordingStartTime),
                                text = "❌ $reason",
                                isWakeWord = false,
                                isFinal = true,
                                resultType = ResultType.INTENT_FAILED
                            )
                        )

                        assistant.returnToListening("Command failed")
                    }

                    else -> {}
                }
            }
        } else {
            logWarning("IntentManager未就绪，跳过意图识别")
            assistant.onAsrResult(text, isFinal = true)
        }
    }

    /**
     * 处理超时情况
     */
    private suspend fun handleTimeout(assistant: VoiceAssistant, text: String) {
        intentRecognitionStarted = true

        // 标记当前结果为最终结果
        if (currentIndex >= 0) {
            onFinalResult(
                PipelineResult(
                    timestamp = formatTimestamp(speechStartTime - recordingStartTime),
                    text = text,
                    isWakeWord = false,
                    isFinal = true,
                    resultType = ResultType.ASR_FINAL
                )
            )
        }

        assistant.cancelTimeout()

        // 处理意图识别
        processIntent(assistant, text)

        SimulateStreamingAsr.vad.clear()
        buffer = arrayListOf()
        offset = 0
        isSpeechStarted = false
        added = false
        asrStartTime = System.currentTimeMillis() + 999999999L
        lastText = ""
    }

    /**
     * 格式化时间戳
     */
    private fun formatTimestamp(millis: Long): String {
        val seconds = millis / 1000
        val minutes = seconds / 60
        val remainingSeconds = seconds % 60
        return String.format("%02d:%02d", minutes, remainingSeconds)
    }

    // 日志辅助方法
    private fun log(message: String) {
        if (ModelConfig.Features.ENABLE_LOGGING) {
            Log.i(tag, message)
        }
    }

    private fun logWarning(message: String) {
        if (ModelConfig.Features.ENABLE_LOGGING) {
            Log.w(tag, message)
        }
    }

    private fun logError(message: String) {
        if (ModelConfig.Features.ENABLE_LOGGING) {
            Log.e(tag, message)
        }
    }
}
