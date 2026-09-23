package com.k2fsa.sherpa.onnx

import android.content.res.AssetManager

data class KeywordSpotterConfig(
    var featConfig: FeatureConfig = FeatureConfig(),
    var modelConfig: OnlineModelConfig = OnlineModelConfig(),
    var maxActivePaths: Int = 4,
    var keywordsFile: String = "",
    var keywordsScore: Float = 1.0f,
    var keywordsThreshold: Float = 0.25f,
    var numTrailingBlanks: Int = 2,
)

data class KeywordSpotterResult(
    val keyword: String,
    val tokens: Array<String>,
    val timestamps: FloatArray,
)

class KeywordSpotter(assetManager: AssetManager, val config: KeywordSpotterConfig) {
    private var ptr = newFromAsset(assetManager, config)

    fun createStream(): OnlineStream = OnlineStream(createStream(ptr, ""))
    fun isReady(stream: OnlineStream): Boolean = isReady(ptr, stream.ptr)
    fun decode(stream: OnlineStream) = decode(ptr, stream.ptr)
    fun reset(stream: OnlineStream) = reset(ptr, stream.ptr)

    fun getResult(stream: OnlineStream): KeywordSpotterResult {
        val result = getResult(ptr, stream.ptr)
        @Suppress("UNCHECKED_CAST")
        return KeywordSpotterResult(
            keyword = result[0] as String,
            tokens = result[1] as Array<String>,
            timestamps = result[2] as FloatArray,
        )
    }

    fun release() {
        if (ptr != 0L) {
            delete(ptr)
            ptr = 0
        }
    }

    private external fun newFromAsset(assetManager: AssetManager, config: KeywordSpotterConfig): Long
    private external fun delete(ptr: Long)
    private external fun createStream(ptr: Long, keywords: String): Long
    private external fun isReady(ptr: Long, streamPtr: Long): Boolean
    private external fun decode(ptr: Long, streamPtr: Long)
    private external fun reset(ptr: Long, streamPtr: Long)
    private external fun getResult(ptr: Long, streamPtr: Long): Array<Any>

    companion object {
        init { System.loadLibrary("sherpa-onnx-jni") }
    }
}
