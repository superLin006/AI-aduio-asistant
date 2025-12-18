package com.k2fsa.sherpa.onnx.simulate.streaming.asr

import android.content.Context
import android.content.res.AssetManager
import android.util.Log

object VoiceAssistantManager {
    private const val TAG = "VoiceAssistantManager"
    
    private var _assistant: VoiceAssistant? = null
    val assistant: VoiceAssistant?
        get() = _assistant
    
    fun initVoiceAssistant(
        assetManager: AssetManager,
        context: Context,  // ✨ 新增参数
        kwsModelType: Int = 0,
        timeout: Long = 5000L
    ): Boolean {
        if (_assistant != null) {
            Log.w(TAG, "VoiceAssistant already initialized")
            return true
        }
        
        try {
            Log.i(TAG, "Initializing VoiceAssistant...")
            
            _assistant = VoiceAssistant(
                assetManager = assetManager,
                context = context,  // ✨ 传递 context
                kwsModelType = kwsModelType,
                timeout = timeout
            )
            
            val success = _assistant!!.initialize()
            
            if (!success) {
                Log.e(TAG, "Failed to initialize VoiceAssistant")
                _assistant = null
                return false
            }
            
            Log.i(TAG, "✓ VoiceAssistant initialized successfully")
            return true
            
        } catch (e: Exception) {
            Log.e(TAG, "Error initializing VoiceAssistant", e)
            _assistant = null
            return false
        }
    }
    
    fun isInitialized(): Boolean {
        return _assistant != null
    }
    
    fun release() {
        _assistant?.release()
        _assistant = null
        Log.i(TAG, "VoiceAssistant released")
    }
}