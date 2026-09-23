package com.k2fsa.sherpa.onnx.pipeline

import android.media.AudioRecord
import android.media.MediaRecorder
import android.util.Log
import com.k2fsa.sherpa.onnx.config.ModelConfig
import kotlinx.coroutines.*

/**
 * 音频录制封装类
 * 负责音频采集和数据回调，与业务逻辑解耦
 */
class AudioRecorder {
    private val tag = "AudioRecorder"

    private var audioRecord: AudioRecord? = null
    private var recordingJob: Job? = null
    private var isRecording = false

    // 音频数据回调
    private var onAudioData: ((FloatArray) -> Unit)? = null

    /**
     * 初始化AudioRecord
     */
    fun initialize(): Boolean {
        return try {
            val sampleRate = ModelConfig.Runtime.SAMPLE_RATE
            val channelConfig = ModelConfig.Runtime.CHANNEL_CONFIG
            val audioFormat = ModelConfig.Runtime.AUDIO_FORMAT

            val minBufferSize = AudioRecord.getMinBufferSize(
                sampleRate,
                channelConfig,
                audioFormat
            )

            if (minBufferSize == AudioRecord.ERROR || minBufferSize == AudioRecord.ERROR_BAD_VALUE) {
                logError("AudioRecord初始化失败：无法获取缓冲区大小")
                return false
            }

            // 使用更大的缓冲区以提高稳定性
            val bufferSize = maxOf(minBufferSize, ModelConfig.Runtime.BUFFER_SIZE_IN_SAMPLES * 2)

            audioRecord = AudioRecord(
                MediaRecorder.AudioSource.MIC,
                sampleRate,
                channelConfig,
                audioFormat,
                bufferSize
            )

            if (audioRecord?.state != AudioRecord.STATE_INITIALIZED) {
                logError("AudioRecord状态异常")
                return false
            }

            log("AudioRecord初始化成功，缓冲区大小: $bufferSize")
            true
        } catch (e: Exception) {
            logError("AudioRecord初始化异常: ${e.message}")
            false
        }
    }

    /**
     * 开始录制
     * @param onAudioDataCallback 音频数据回调（FloatArray: 归一化的音频采样）
     */
    fun start(onAudioDataCallback: (FloatArray) -> Unit) {
        if (isRecording) {
            logWarning("录制已在进行中")
            return
        }

        val recorder = audioRecord
        if (recorder == null) {
            logError("AudioRecord未初始化")
            return
        }

        onAudioData = onAudioDataCallback
        isRecording = true

        try {
            recorder.startRecording()
            log("开始录制音频")

            // 在IO线程池中处理音频采集
            recordingJob = CoroutineScope(Dispatchers.IO).launch {
                val bufferSize = ModelConfig.Runtime.BUFFER_SIZE_IN_SAMPLES
                val audioBuffer = ShortArray(bufferSize)

                while (isActive && isRecording) {
                    val readSize = recorder.read(audioBuffer, 0, bufferSize)

                    if (readSize > 0) {
                        // 将Short数组转换为归一化的Float数组
                        val samples = FloatArray(readSize) { i ->
                            audioBuffer[i] / 32768.0f
                        }

                        // 回调音频数据
                        onAudioData?.invoke(samples)
                    } else if (readSize == AudioRecord.ERROR_INVALID_OPERATION) {
                        logError("录制错误：INVALID_OPERATION")
                        break
                    } else if (readSize == AudioRecord.ERROR_BAD_VALUE) {
                        logError("录制错误：BAD_VALUE")
                        break
                    }
                }
            }
        } catch (e: Exception) {
            logError("启动录制异常: ${e.message}")
            isRecording = false
        }
    }

    /**
     * 停止录制
     */
    fun stop() {
        if (!isRecording) {
            return
        }

        log("停止录制音频")
        isRecording = false

        // 取消录制协程
        recordingJob?.cancel()
        recordingJob = null

        // 停止AudioRecord
        try {
            audioRecord?.stop()
        } catch (e: Exception) {
            logError("停止录制异常: ${e.message}")
        }

        onAudioData = null
    }

    /**
     * 释放资源
     */
    fun release() {
        stop()

        try {
            audioRecord?.release()
            audioRecord = null
            log("AudioRecord资源已释放")
        } catch (e: Exception) {
            logError("释放AudioRecord异常: ${e.message}")
        }
    }

    /**
     * 检查是否正在录制
     */
    fun isRecording(): Boolean = isRecording

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
