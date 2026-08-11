package com.k2fsa.sherpa.onnx.simulate.streaming.asr

import android.content.Context
import android.media.MediaPlayer
import android.util.Log
import java.io.IOException

/**
 * 音频播放工具类
 * 用于播放提示音
 */
object AudioPlayer {
    private const val TAG = "AudioPlayer"
    
    private var mediaPlayer: MediaPlayer? = null
    private var isPlayingFlag = false  // ✅ 改名避免与函数冲突
    
    /**
     * 播放音频文件
     * @param context 上下文
     * @param assetFileName assets目录下的音频文件名，例如 "sounds/listening.mp3"
     * @param onComplete 播放完成回调
     */
    fun play(
        context: Context,
        assetFileName: String,
        onComplete: (() -> Unit)? = null
    ) {
        // 如果正在播放，先停止
        stop()
        
        try {
            mediaPlayer = MediaPlayer().apply {
                // 从 assets 加载音频
                val afd = context.assets.openFd(assetFileName)
                setDataSource(afd.fileDescriptor, afd.startOffset, afd.length)
                afd.close()
                
                // 设置播放完成监听
                setOnCompletionListener {
                    Log.d(TAG, "Audio playback completed: $assetFileName")
                    isPlayingFlag = false
                    onComplete?.invoke()
                    release()
                }
                
                // 设置错误监听
                setOnErrorListener { _, what, extra ->
                    Log.e(TAG, "MediaPlayer error: what=$what, extra=$extra")
                    isPlayingFlag = false
                    false
                }
                
                // 准备并播放
                prepare()
                start()
                isPlayingFlag = true
                
                Log.i(TAG, "🔊 Playing audio: $assetFileName")
            }
        } catch (e: IOException) {
            Log.e(TAG, "Failed to play audio: $assetFileName", e)
            onComplete?.invoke()
        } catch (e: Exception) {
            Log.e(TAG, "Unexpected error playing audio: $assetFileName", e)
            onComplete?.invoke()
        }
    }
    
    /**
     * 停止播放
     */
    fun stop() {
        try {
            mediaPlayer?.apply {
                if (isPlaying) {
                    stop()
                }
                release()
            }
            mediaPlayer = null
            isPlayingFlag = false
        } catch (e: Exception) {
            Log.e(TAG, "Error stopping audio", e)
        }
    }
    
    /**
     * 是否正在播放
     */
    fun isPlaying(): Boolean {
        return isPlayingFlag
    }
    
    /**
     * 释放资源
     */
    fun release() {
        stop()
    }
}

/**
 * 提示音文件路径常量
 */
object SoundPaths {
    const val LISTENING = "sounds/listening.mp3"      // "你说我在听"
    const val PROCESSING = "sounds/processing.mp3"    // "我来帮你操作"
    const val COMPLETED = "sounds/completed.mp3"      // "操作已完成"
}