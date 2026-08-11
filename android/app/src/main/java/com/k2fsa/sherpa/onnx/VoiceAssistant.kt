package com.k2fsa.sherpa.onnx.simulate.streaming.asr

import android.content.Context
import android.content.res.AssetManager
import android.util.Log
import com.k2fsa.sherpa.onnx.*
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

enum class AssistantState {
    LISTENING,
    ACTIVATED,
    PROCESSING
}

data class AssistantResult(
    val state: AssistantState,
    val keyword: String = "",
    val asrText: String = "",
    val isFinal: Boolean = false,
    val timestamp: Long = System.currentTimeMillis()
)

class VoiceAssistant(
    private val assetManager: AssetManager,
    private val context: Context,
    private val kwsModelType: Int = 0,
    private val timeout: Long = 5000L
) {
    private val TAG = "VoiceAssistant"
    
    private val _state = MutableStateFlow(AssistantState.LISTENING)
    val state: StateFlow<AssistantState> = _state.asStateFlow()
    
    private val _result = MutableStateFlow<AssistantResult?>(null)
    val result: StateFlow<AssistantResult?> = _result.asStateFlow()
    
    private var keywordSpotter: KeywordSpotter? = null
    private var kwsStream: OnlineStream? = null
    
    private var timeoutJob: Job? = null
    private var isInitialized = false
    
    @Synchronized
    fun initialize(): Boolean {
        if (isInitialized) {
            Log.w(TAG, "VoiceAssistant already initialized")
            return true
        }
        
        try {
            Log.i(TAG, "========================================")
            Log.i(TAG, "Initializing VoiceAssistant")
            Log.i(TAG, "========================================")
            
            initKWS()
            
            if (SimulateStreamingAsr.recognizer == null) {
                Log.e(TAG, "ASR not initialized! Please initialize SimulateStreamingAsr first")
                return false
            }
            
            isInitialized = true
            _state.value = AssistantState.LISTENING
            
            Log.i(TAG, "✓ VoiceAssistant initialized successfully")
            Log.i(TAG, "Initial state: LISTENING")
            Log.i(TAG, "========================================")
            
            return true
            
        } catch (e: Exception) {
            Log.e(TAG, "Failed to initialize VoiceAssistant", e)
            return false
        }
    }
    
    private fun initKWS() {
        Log.i(TAG, "Initializing KWS (type=$kwsModelType)...")
        
        val modelConfig = getKwsModelConfig(kwsModelType)
            ?: throw IllegalStateException("Invalid KWS model type: $kwsModelType")
        
        val kwsConfig = KeywordSpotterConfig(
            featConfig = FeatureConfig(sampleRate = 16000, featureDim = 80),
            modelConfig = modelConfig,
            maxActivePaths = 4,
            keywordsFile = getKeywordsFile(kwsModelType),
            keywordsScore = 1.0f,
            keywordsThreshold = 0.25f,
            numTrailingBlanks = 2
        )
        
        keywordSpotter = KeywordSpotter(
            assetManager = assetManager,
            config = kwsConfig
        )
        
        kwsStream = keywordSpotter!!.createStream()
        
        Log.i(TAG, "✓ KWS initialized with keywords file: ${kwsConfig.keywordsFile}")
    }
    
    fun processAudio(samples: FloatArray, sampleRate: Int) {
        if (!isInitialized) {
            Log.w(TAG, "VoiceAssistant not initialized")
            return
        }
        
        when (_state.value) {
            AssistantState.LISTENING -> {
                processKWS(samples, sampleRate)
            }
            
            AssistantState.ACTIVATED -> {
                // 已唤醒状态，等待 transitionToProcessing() 调用
            }
            
            AssistantState.PROCESSING -> {
                // 处理状态，ASR 在 Home.kt 中完成
            }
        }
    }
    
    private fun processKWS(samples: FloatArray, sampleRate: Int) {
        try {
            kwsStream?.acceptWaveform(samples, sampleRate)
            
            while (keywordSpotter?.isReady(kwsStream!!) == true) {
                keywordSpotter?.decode(kwsStream!!)
                val result = keywordSpotter?.getResult(kwsStream!!)
                
                if (result != null && result.keyword.isNotEmpty()) {
                    Log.i(TAG, "🎤 Keyword detected: ${result.keyword}")
                    onKeywordDetected(result.keyword)
                    break
                }
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error processing KWS", e)
        }
    }
    
    private fun onKeywordDetected(keyword: String) {
        Log.i(TAG, "========================================")
        Log.i(TAG, "🔔 Wake word detected: $keyword")
        Log.i(TAG, "State: LISTENING -> ACTIVATED")
        Log.i(TAG, "========================================")
        
        // 1. 更新状态
        _state.value = AssistantState.ACTIVATED
        
        // 2. 发送结果
        _result.value = AssistantResult(
            state = AssistantState.ACTIVATED,
            keyword = keyword,
            isFinal = false
        )
        
        // 3. 播放提示音："你说我在听"
        AudioPlayer.play(context, SoundPaths.LISTENING) {
            Log.d(TAG, "Listening prompt completed")
        }
        
        // 4. 重置KWS流
        keywordSpotter?.reset(kwsStream!!)
    }
    
    fun transitionToProcessing() {
        if (_state.value != AssistantState.ACTIVATED) {
            Log.w(TAG, "Cannot transition to PROCESSING from ${_state.value}")
            return
        }
        
        Log.i(TAG, "State: ACTIVATED -> PROCESSING")
        _state.value = AssistantState.PROCESSING
        
        // 启动超时计时器
        startTimeoutTimer()
        
        // 重置VAD
        SimulateStreamingAsr.vad.reset()
    }
    
    fun onAsrResult(text: String, isFinal: Boolean) {
        if (_state.value != AssistantState.PROCESSING) {
            return
        }
        
        Log.i(TAG, "ASR result (final=$isFinal): $text")
        
        _result.value = AssistantResult(
            state = AssistantState.PROCESSING,
            asrText = text,
            isFinal = isFinal
        )
        
        if (isFinal) {
            returnToListening("ASR completed")
        }
    }
    
    private fun startTimeoutTimer() {
        timeoutJob?.cancel()
        
        timeoutJob = CoroutineScope(Dispatchers.Default).launch {
            delay(timeout)
            
            if (_state.value == AssistantState.PROCESSING) {
                Log.w(TAG, "⏰ ASR timeout (${timeout}ms)")
                returnToListening("Timeout")
            }
        }
    }
    
    private fun cancelTimeoutTimer() {
        timeoutJob?.cancel()
        timeoutJob = null
    }
    
    /**
     * 取消超时计时器（公开方法，供外部调用）
     */
    fun cancelTimeout() {
        Log.d(TAG, "Timeout cancelled externally")
        cancelTimeoutTimer()
    }
    
    fun returnToListening(reason: String) {
        Log.i(TAG, "========================================")
        Log.i(TAG, "🔄 Returning to LISTENING state")
        Log.i(TAG, "Reason: $reason")
        Log.i(TAG, "========================================")
        
        cancelTimeoutTimer()
        SimulateStreamingAsr.vad.reset()
        keywordSpotter?.reset(kwsStream!!)
        
        _state.value = AssistantState.LISTENING
        _result.value = null
    }
    
    fun reset() {
        Log.i(TAG, "Manual reset")
        returnToListening("Manual reset")
    }
    
    fun release() {
        Log.i(TAG, "Releasing VoiceAssistant resources...")
        
        cancelTimeoutTimer()
        AudioPlayer.release()
        
        kwsStream?.release()
        kwsStream = null
        
        keywordSpotter?.release()
        keywordSpotter = null
        
        isInitialized = false
        
        Log.i(TAG, "✓ VoiceAssistant released")
    }
    
    fun getCurrentState(): AssistantState = _state.value
    fun isListening(): Boolean = _state.value == AssistantState.LISTENING
    fun isActivated(): Boolean = _state.value == AssistantState.ACTIVATED
    fun isProcessing(): Boolean = _state.value == AssistantState.PROCESSING
}