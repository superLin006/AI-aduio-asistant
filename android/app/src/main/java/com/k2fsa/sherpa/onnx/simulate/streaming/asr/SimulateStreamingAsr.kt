package com.k2fsa.sherpa.onnx.simulate.streaming.asr

import android.app.Application
import android.content.res.AssetManager
import android.util.Log
import com.k2fsa.sherpa.onnx.OfflineRecognizer
import com.k2fsa.sherpa.onnx.OfflineRecognizerConfig
import com.k2fsa.sherpa.onnx.Vad
import com.k2fsa.sherpa.onnx.getOfflineModelConfig
import com.k2fsa.sherpa.onnx.getVadModelConfig
import com.k2fsa.sherpa.onnx.config.ModelConfig

/**
 * 模型管理器（单例）
 * 负责所有ONNX模型的生命周期管理和业务封装
 */
object SimulateStreamingAsr {
    private var _recognizer: OfflineRecognizer? = null
    val recognizer: OfflineRecognizer
        get() {
            return _recognizer!!
        }

    private var _vad: Vad? = null
    val vad: Vad
        get() {
            return _vad!!
        }

    // ========== 模型初始化方法 ==========

    /**
     * 初始化离线ASR识别器
     */
    fun initOfflineRecognizer(assetManager: AssetManager? = null, application: Application) {
        synchronized(this) {
            if (_recognizer != null) {
                return
            }
            Log.i(TAG, "初始化sherpa-onnx离线识别器")

            val asrModelType = ModelConfig.Selection.ASR_MODEL_TYPE
            Log.i(TAG, "选择ASR模型类型: $asrModelType")

            val config = OfflineRecognizerConfig(
                modelConfig = getOfflineModelConfig(type = asrModelType)!!,
            )

            // 使用配置中的线程数
            if (config.modelConfig.numThreads <= 0) {
                config.modelConfig.numThreads = ModelConfig.Runtime.ASR_NUM_THREADS
            }

            // 设置解码参数
            config.maxActivePaths = ModelConfig.Runtime.ASR_MAX_ACTIVE_PATHS

            _recognizer = OfflineRecognizer(
                assetManager = assetManager,
                config = config,
            )

            Log.i(TAG, "sherpa-onnx离线识别器初始化完成")
        }
    }

    /**
     * 初始化VAD（语音活动检测）
     */
    fun initVad(assetManager: AssetManager? = null) {
        if (_vad != null) {
            return
        }
        val type = ModelConfig.Selection.VAD_MODEL_TYPE
        Log.i(TAG, "选择VAD模型类型: $type")
        val config = getVadModelConfig(type)

        _vad = Vad(
            assetManager = assetManager,
            config = config!!,
        )
        Log.i(TAG, "sherpa-onnx VAD初始化完成")
    }

    // ========== 业务封装方法 ==========

    /**
     * 执行ASR识别（创建stream并返回结果）
     * @param samples 音频采样
     * @param sampleRate 采样率
     * @return 识别文本
     */
    fun recognizeAudio(samples: FloatArray, sampleRate: Int): String {
        val stream = _recognizer?.createStream() ?: return ""
        try {
            stream.acceptWaveform(samples, sampleRate)
            _recognizer?.decode(stream)
            val result = _recognizer?.getResult(stream)
            return result?.text ?: ""
        } finally {
            stream.release()
        }
    }

    /**
     * 检查VAD是否检测到语音
     */
    fun isSpeechDetected(): Boolean {
        return _vad?.isSpeechDetected() ?: false
    }

    /**
     * 重置VAD状态
     */
    fun resetVad() {
        _vad?.reset()
    }

    /**
     * 检查是否已初始化
     */
    fun isInitialized(): Boolean {
        return _recognizer != null && _vad != null
    }

    // ========== 资源管理 ==========

    /**
     * 释放所有模型资源
     */
    fun releaseAll() {
        _vad?.release()
        _vad = null

        _recognizer?.release()
        _recognizer = null

        Log.i(TAG, "所有资源已释放")
    }
}