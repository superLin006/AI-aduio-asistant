package com.k2fsa.sherpa.onnx.simulate.streaming.asr

import android.app.Application
import android.content.res.AssetManager
import android.util.Log
import com.k2fsa.sherpa.onnx.OfflineRecognizer
import com.k2fsa.sherpa.onnx.OfflineRecognizerConfig
import com.k2fsa.sherpa.onnx.Vad
import com.k2fsa.sherpa.onnx.getOfflineModelConfig
import com.k2fsa.sherpa.onnx.getVadModelConfig

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

    fun initOfflineRecognizer(assetManager: AssetManager? = null, application: Application) {
        synchronized(this) {
            if (_recognizer != null) {
                return
            }
            Log.i(TAG, "Initializing sherpa-onnx offline recognizer")
            
            val asrModelType = 39
            Log.i(TAG, "Select model type $asrModelType for ASR")

            val config = OfflineRecognizerConfig(
                modelConfig = getOfflineModelConfig(type = asrModelType)!!,
            )

            if (config.modelConfig.numThreads == 1) {
                config.modelConfig.numThreads = 2
            }

            _recognizer = OfflineRecognizer(
                assetManager = assetManager,
                config = config,
            )

            Log.i(TAG, "sherpa-onnx offline recognizer initialized")
        }
    }

    fun initVad(assetManager: AssetManager? = null) {
        if (_vad != null) {
            return
        }
        val type = 0
        Log.i(TAG, "Select VAD model type $type")
        val config = getVadModelConfig(type)

        _vad = Vad(
            assetManager = assetManager,
            config = config!!,
        )
        Log.i(TAG, "sherpa-onnx vad initialized")
    }
    
    fun releaseAll() {
        _vad?.release()
        _vad = null
        
        _recognizer?.release()
        _recognizer = null
        
        Log.i(TAG, "All resources released")
    }
}