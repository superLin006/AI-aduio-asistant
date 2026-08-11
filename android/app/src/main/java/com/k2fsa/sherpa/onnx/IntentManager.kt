package com.k2fsa.sherpa.onnx.simulate.streaming.asr

import android.content.Context
import android.util.Log
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch

enum class IntentProcessingState {
    IDLE,
    PARSING,
    EXECUTING,
    COMPLETED,
    FAILED
}

data class IntentProcessingResult(
    val state: IntentProcessingState,
    val asrText: String = "",
    val intent: DeepSeekClient.IntentResult? = null,
    val commandResult: CommandResult? = null,
    val timestamp: Long = System.currentTimeMillis()
)

object IntentManager {
    private val TAG = "IntentManager"
    
    private var deepSeekClient: DeepSeekClient? = null
    private var appContext: Context? = null
    
    private val _processingState = MutableStateFlow(IntentProcessingState.IDLE)
    val processingState: StateFlow<IntentProcessingState> = _processingState.asStateFlow()
    
    private val _result = MutableStateFlow<IntentProcessingResult?>(null)
    val result: StateFlow<IntentProcessingResult?> = _result.asStateFlow()
    
    private var isInitialized = false
    
    fun initialize(apiKey: String, context: Context? = null): Boolean {
        if (isInitialized) {
            Log.w(TAG, "IntentManager already initialized")
            return true
        }
        
        try {
            Log.i(TAG, "========================================")
            Log.i(TAG, "Initializing IntentManager")
            Log.i(TAG, "========================================")
            
            appContext = context
            deepSeekClient = DeepSeekClient(apiKey)
            
            CoroutineScope(Dispatchers.IO).launch {
                val testResult = deepSeekClient?.testConnection() ?: false
                if (testResult) {
                    Log.i(TAG, "✓ DeepSeek API connection verified")
                } else {
                    Log.w(TAG, "⚠️ DeepSeek API connection test failed (but continuing)")
                }
            }
            
            isInitialized = true
            
            Log.i(TAG, "✓ IntentManager initialized successfully")
            Log.i(TAG, "========================================")
            
            return true
            
        } catch (e: Exception) {
            Log.e(TAG, "Failed to initialize IntentManager", e)
            return false
        }
    }
    
    fun processCommand(
        asrText: String,
        onComplete: ((IntentProcessingResult) -> Unit)? = null
    ) {
        if (!isInitialized || deepSeekClient == null) {
            Log.e(TAG, "IntentManager not initialized")
            val failedResult = IntentProcessingResult(
                state = IntentProcessingState.FAILED,
                asrText = asrText
            )
            _result.value = failedResult
            onComplete?.invoke(failedResult)
            return
        }
        
        Log.i(TAG, "========================================")
        Log.i(TAG, "🚀 Processing command: $asrText")
        Log.i(TAG, "========================================")
        
        CoroutineScope(Dispatchers.Default).launch {
            try {
                // 1. 解析意图
                _processingState.value = IntentProcessingState.PARSING
                _result.value = IntentProcessingResult(
                    state = IntentProcessingState.PARSING,
                    asrText = asrText
                )
                
                Log.i(TAG, "📊 Step 1: Parsing intent...")
                val intent = deepSeekClient?.parseIntent(asrText)
                
                if (intent == null) {
                    Log.e(TAG, "❌ Failed to parse intent")
                    val failedResult = IntentProcessingResult(
                        state = IntentProcessingState.FAILED,
                        asrText = asrText
                    )
                    _processingState.value = IntentProcessingState.FAILED
                    _result.value = failedResult
                    onComplete?.invoke(failedResult)
                    return@launch
                }
                
                Log.i(TAG, "✅ Intent parsed: ${intent.toReadableString()}")
                Log.i(TAG, "   Confidence: ${intent.confidence}")
                Log.i(TAG, "   Valid: ${intent.isValid}")
                
                // 2. 执行指令
                _processingState.value = IntentProcessingState.EXECUTING
                _result.value = IntentProcessingResult(
                    state = IntentProcessingState.EXECUTING,
                    asrText = asrText,
                    intent = intent
                )
                
                Log.i(TAG, "⚙️ Step 2: Executing command...")
                val commandResult = CommandExecutor.execute(intent)
                
                // 3. 完成
                val finalState = if (commandResult.success) {
                    IntentProcessingState.COMPLETED
                } else {
                    IntentProcessingState.FAILED
                }
                
                val finalResult = IntentProcessingResult(
                    state = finalState,
                    asrText = asrText,
                    intent = intent,
                    commandResult = commandResult
                )
                
                _processingState.value = finalState
                _result.value = finalResult
                
                if (commandResult.success) {
                    Log.i(TAG, "🎉 Command completed successfully!")
                    Log.i(TAG, "   Result: ${commandResult.message}")
                    
                    // 播放完成提示音
                    appContext?.let { ctx ->
                        AudioPlayer.play(ctx, SoundPaths.COMPLETED) {
                            Log.d(TAG, "Completion prompt finished")
                        }
                    }
                } else {
                    Log.w(TAG, "⚠️ Command execution failed")
                    Log.w(TAG, "   Reason: ${commandResult.message}")
                }
                
                Log.i(TAG, "========================================")
                
                onComplete?.invoke(finalResult)
                
            } catch (e: Exception) {
                Log.e(TAG, "❌ Error processing command", e)
                val failedResult = IntentProcessingResult(
                    state = IntentProcessingState.FAILED,
                    asrText = asrText
                )
                _processingState.value = IntentProcessingState.FAILED
                _result.value = failedResult
                onComplete?.invoke(failedResult)
            }
        }
    }
    
    fun reset() {
        _processingState.value = IntentProcessingState.IDLE
        _result.value = null
    }
    
    fun isReady(): Boolean {
        return isInitialized && deepSeekClient != null
    }
    
    fun getSupportedDevices(): List<String> {
        return CommandExecutor.getSupportedDevices()
    }
}