package com.superlin.xiaohui.kwsdemo

import android.Manifest
import android.app.Activity
import android.content.pm.PackageManager
import android.graphics.Color
import android.media.AudioFormat
import android.media.AudioRecord
import android.media.MediaRecorder
import android.os.Bundle
import android.os.SystemClock
import android.view.Gravity
import android.view.ViewGroup
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors
import java.util.concurrent.Future
import java.util.concurrent.atomic.AtomicBoolean

class MainActivity : Activity() {
    private lateinit var stateText: TextView
    private lateinit var resultText: TextView
    private lateinit var toggleButton: Button
    private val recording = AtomicBoolean(false)
    private val worker: ExecutorService = Executors.newSingleThreadExecutor()
    private var task: Future<*>? = null
    private var engine: KwsEngine? = null
    private var audioRecord: AudioRecord? = null
    private var detectionCount = 0

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        buildUi()
        toggleButton.setOnClickListener {
            if (recording.get()) stopListening() else ensurePermissionAndStart()
        }
    }

    private fun buildUi() {
        val density = resources.displayMetrics.density
        val padding = (24 * density).toInt()
        val layout = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER_HORIZONTAL
            setPadding(padding, padding, padding, padding)
            setBackgroundColor(Color.rgb(245, 247, 250))
        }
        val title = TextView(this).apply {
            text = "小慧唤醒词 Demo"
            textSize = 28f
            setTextColor(Color.rgb(30, 41, 59))
            gravity = Gravity.CENTER
        }
        stateText = TextView(this).apply {
            text = "点击开始监听"
            textSize = 20f
            setTextColor(Color.DKGRAY)
            gravity = Gravity.CENTER
            setPadding(0, padding, 0, padding)
        }
        resultText = TextView(this).apply {
            text = "唤醒词：小慧\n检测次数：0"
            textSize = 18f
            setTextColor(Color.rgb(51, 65, 85))
            gravity = Gravity.CENTER
        }
        toggleButton = Button(this).apply {
            text = "开始监听"
            textSize = 18f
        }
        layout.addView(title, ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT)
        layout.addView(stateText, ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT)
        layout.addView(resultText, ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT)
        layout.addView(toggleButton, ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT)
        setContentView(layout)
    }

    private fun ensurePermissionAndStart() {
        if (checkSelfPermission(Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED) {
            startListening()
        } else {
            requestPermissions(arrayOf(Manifest.permission.RECORD_AUDIO), REQUEST_AUDIO)
        }
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<out String>,
        grantResults: IntArray,
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == REQUEST_AUDIO && grantResults.firstOrNull() == PackageManager.PERMISSION_GRANTED) {
            startListening()
        } else if (requestCode == REQUEST_AUDIO) {
            stateText.text = "需要麦克风权限才能测试"
        }
    }

    private fun startListening() {
        if (!recording.compareAndSet(false, true)) return
        stateText.text = "正在加载 CPU 关键词模型…"
        toggleButton.text = "停止监听"
        task = worker.submit {
            try {
                val kws = KwsEngine(assets).also { engine = it }
                val minBytes = AudioRecord.getMinBufferSize(
                    KwsEngine.SAMPLE_RATE,
                    AudioFormat.CHANNEL_IN_MONO,
                    AudioFormat.ENCODING_PCM_16BIT,
                )
                require(minBytes > 0) { "设备不支持 16 kHz 单声道录音" }
                val recorder = AudioRecord(
                    MediaRecorder.AudioSource.VOICE_RECOGNITION,
                    KwsEngine.SAMPLE_RATE,
                    AudioFormat.CHANNEL_IN_MONO,
                    AudioFormat.ENCODING_PCM_16BIT,
                    maxOf(minBytes, 3200),
                ).also { audioRecord = it }
                check(recorder.state == AudioRecord.STATE_INITIALIZED) { "麦克风初始化失败" }
                recorder.startRecording()
                runOnUiThread { stateText.text = "监听中，请说“小慧”" }
                val pcm = ShortArray(1600)
                while (recording.get()) {
                    val count = recorder.read(pcm, 0, pcm.size)
                    if (count <= 0) continue
                    val samples = FloatArray(count) { pcm[it] / 32768.0f }
                    val keyword = kws.accept(samples) ?: continue
                    val time = SimpleDateFormat("HH:mm:ss", Locale.getDefault()).format(Date())
                    ++detectionCount
                    runOnUiThread {
                        stateText.text = "已唤醒 ✓"
                        resultText.text = "检测结果：$keyword\n时间：$time\n检测次数：$detectionCount"
                    }
                    SystemClock.sleep(600)
                    runOnUiThread {
                        if (recording.get()) stateText.text = "监听中，请说“小慧”"
                    }
                }
            } catch (error: Throwable) {
                runOnUiThread { stateText.text = "启动失败：${error.message ?: error.javaClass.simpleName}" }
            } finally {
                releaseRuntime()
                recording.set(false)
                runOnUiThread { toggleButton.text = "开始监听" }
            }
        }
    }

    private fun stopListening() {
        recording.set(false)
        try { audioRecord?.stop() } catch (_: Exception) {}
        stateText.text = "已停止监听"
    }

    private fun releaseRuntime() {
        try { audioRecord?.release() } catch (_: Exception) {}
        audioRecord = null
        try { engine?.close() } catch (_: Exception) {}
        engine = null
    }

    override fun onStop() {
        stopListening()
        super.onStop()
    }

    override fun onDestroy() {
        stopListening()
        task?.cancel(true)
        worker.shutdownNow()
        super.onDestroy()
    }

    companion object { private const val REQUEST_AUDIO = 1001 }
}
