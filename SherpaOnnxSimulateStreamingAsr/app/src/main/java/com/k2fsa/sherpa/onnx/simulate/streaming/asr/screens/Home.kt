package com.k2fsa.sherpa.onnx.simulate.streaming.asr.screens

import android.Manifest
import android.app.Activity
import android.content.pm.PackageManager
import android.util.Log
import android.widget.Toast
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalClipboardManager
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.app.ActivityCompat
import com.k2fsa.sherpa.onnx.pipeline.AudioRecorder
import com.k2fsa.sherpa.onnx.pipeline.PipelineResult
import com.k2fsa.sherpa.onnx.pipeline.VoiceAssistantPipeline
import com.k2fsa.sherpa.onnx.simulate.streaming.asr.AssistantState
import com.k2fsa.sherpa.onnx.simulate.streaming.asr.TAG
import com.k2fsa.sherpa.onnx.simulate.streaming.asr.VoiceAssistantManager
import kotlinx.coroutines.launch

/**
 * 主页面（重构版）
 * UI层只负责展示，业务逻辑全部在Pipeline中
 */
@Composable
fun HomeScreen() {
    val context = LocalContext.current
    val clipboardManager = LocalClipboardManager.current
    val activity = LocalContext.current as Activity

    // UI状态
    var isStarted by remember { mutableStateOf(false) }
    val resultList: MutableList<PipelineResult> = remember { mutableStateListOf() }
    val lazyColumnListState = rememberLazyListState()
    val coroutineScope = rememberCoroutineScope()

    // 语音助手状态
    val assistantState by VoiceAssistantManager.assistant?.state?.collectAsState()
        ?: remember { mutableStateOf(AssistantState.LISTENING) }

    // 录制开始时间
    val recordingStartTime = remember { mutableStateOf(0L) }

    // Pipeline和AudioRecorder实例
    var pipeline: VoiceAssistantPipeline? by remember { mutableStateOf(null) }
    var audioRecorder: AudioRecorder? by remember { mutableStateOf(null) }

    // ========== 回调函数 ==========

    /**
     * Pipeline中间结果回调
     */
    val onIntermediateResult: (PipelineResult) -> Unit = { result ->
        // 添加或更新中间结果
        if (resultList.isEmpty() || resultList.last().isFinal) {
            resultList.add(result)
        } else {
            resultList[resultList.lastIndex] = result
        }

        // 滚动到底部
        coroutineScope.launch {
            lazyColumnListState.animateScrollToItem(resultList.size - 1)
        }
    }

    /**
     * Pipeline最终结果回调
     */
    val onFinalResult: (PipelineResult) -> Unit = { result ->
        // 如果最后一个结果不是final，则替换；否则添加新的
        if (resultList.isNotEmpty() && !resultList.last().isFinal && !result.isWakeWord) {
            resultList[resultList.lastIndex] = result
        } else {
            resultList.add(result)
        }

        // 滚动到底部
        coroutineScope.launch {
            lazyColumnListState.animateScrollToItem(resultList.size - 1)
        }
    }

    /**
     * 录制按钮点击事件
     */
    val onRecordingButtonClick = fun() {
        isStarted = !isStarted

        if (isStarted) {
            // 检查权限
            if (ActivityCompat.checkSelfPermission(
                    activity,
                    Manifest.permission.RECORD_AUDIO
                ) != PackageManager.PERMISSION_GRANTED
            ) {
                Log.i(TAG, "录音权限未授予")
                isStarted = false
                Toast.makeText(context, "请授予录音权限", Toast.LENGTH_SHORT).show()
                return
            }

            // 记录开始时间
            recordingStartTime.value = System.currentTimeMillis()

            // 创建AudioRecorder
            audioRecorder = AudioRecorder()
            if (!audioRecorder!!.initialize()) {
                Log.e(TAG, "AudioRecorder初始化失败")
                isStarted = false
                Toast.makeText(context, "音频录制初始化失败", Toast.LENGTH_SHORT).show()
                return
            }

            // 创建Pipeline
            pipeline = VoiceAssistantPipeline(
                context = context,
                onIntermediateResult = onIntermediateResult,
                onFinalResult = onFinalResult,
                recordingStartTime = recordingStartTime.value
            )

            // 启动Pipeline
            pipeline?.start()

            // 启动音频录制
            audioRecorder?.start { samples ->
                pipeline?.feedAudio(samples)
            }

            Log.i(TAG, "🎤 录制和Pipeline已启动")
        } else {
            // 停止录制
            pipeline?.stop()
            audioRecorder?.stop()
            audioRecorder?.release()

            pipeline = null
            audioRecorder = null

            Log.i(TAG, "ℹ️ 录制和Pipeline已停止")
        }
    }

    /**
     * 复制按钮点击事件
     */
    val onCopyButtonClick: () -> Unit = {
        if (resultList.isNotEmpty()) {
            val s = resultList.mapIndexed { i, result ->
                "${i + 1}: [${result.timestamp}] ${result.text}"
            }.joinToString(separator = "\n")
            clipboardManager.setText(AnnotatedString(s))
            Toast.makeText(context, "已复制到剪贴板", Toast.LENGTH_SHORT).show()
        } else {
            Toast.makeText(context, "暂无内容", Toast.LENGTH_SHORT).show()
        }
    }

    /**
     * 清空按钮点击事件
     */
    val onClearButtonClick: () -> Unit = {
        resultList.clear()
    }

    // ========== UI布局 ==========

    Box(
        modifier = Modifier.fillMaxSize(),
        contentAlignment = Alignment.TopCenter,
    ) {
        Column(modifier = Modifier) {
            StatusIndicator(state = assistantState)

            HomeButtonRow(
                isStarted = isStarted,
                onRecordingButtonClick = onRecordingButtonClick,
                onCopyButtonClick = onCopyButtonClick,
                onClearButtonClick = onClearButtonClick
            )

            if (resultList.size > 0) {
                LazyColumn(
                    modifier = Modifier
                        .fillMaxWidth()
                        .fillMaxHeight(),
                    contentPadding = PaddingValues(16.dp),
                    state = lazyColumnListState
                ) {
                    itemsIndexed(resultList) { index, result ->
                        Row(modifier = Modifier.padding(vertical = 4.dp)) {
                            Text(
                                text = "${index + 1}: ",
                                color = MaterialTheme.colorScheme.onSurface,
                                fontSize = 14.sp
                            )
                            Text(
                                text = "[${result.timestamp}] ",
                                color = Color(0xFF9E9E9E),
                                fontSize = 13.sp,
                                fontWeight = FontWeight.Normal
                            )
                            Text(
                                text = result.text,
                                color = if (result.isWakeWord) Color(0xFF4CAF50) else MaterialTheme.colorScheme.onSurface,
                                fontSize = 14.sp,
                                fontWeight = if (result.isFinal) FontWeight.Normal else FontWeight.Light
                            )
                        }
                    }
                }
            }
        }
    }

    // 清理资源
    DisposableEffect(Unit) {
        onDispose {
            pipeline?.stop()
            audioRecorder?.release()
        }
    }
}

/**
 * 状态指示器
 */
@Composable
fun StatusIndicator(state: AssistantState) {
    val (bgColor, textColor, statusText) = when (state) {
        AssistantState.LISTENING -> Triple(
            Color(0xFF2196F3),
            Color.White,
            "🎧 监听中 - 请说唤醒词"
        )
        AssistantState.ACTIVATED -> Triple(
            Color(0xFF4CAF50),
            Color.White,
            "✅ 已唤醒 - 准备接收指令"
        )
        AssistantState.PROCESSING -> Triple(
            Color(0xFFFF9800),
            Color.White,
            "⚙️ 处理中 - 正在识别"
        )
    }

    Box(
        modifier = Modifier
            .fillMaxWidth()
            .padding(16.dp)
    ) {
        Surface(
            modifier = Modifier.fillMaxWidth(),
            color = bgColor,
            shape = MaterialTheme.shapes.medium
        ) {
            Text(
                text = statusText,
                color = textColor,
                fontSize = 16.sp,
                fontWeight = FontWeight.Bold,
                modifier = Modifier.padding(16.dp)
            )
        }
    }
}

/**
 * 按钮行
 */
@Composable
fun HomeButtonRow(
    isStarted: Boolean,
    onRecordingButtonClick: () -> Unit,
    onCopyButtonClick: () -> Unit,
    onClearButtonClick: () -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 16.dp, vertical = 8.dp),
        horizontalArrangement = Arrangement.SpaceEvenly
    ) {
        Button(
            onClick = onRecordingButtonClick,
            colors = ButtonDefaults.buttonColors(
                containerColor = if (isStarted) Color(0xFFE53935) else Color(0xFF4CAF50)
            )
        ) {
            Text(
                text = if (isStarted) "⏹ 停止" else "▶ 开始",
                fontSize = 16.sp
            )
        }

        Button(
            onClick = onCopyButtonClick,
            enabled = !isStarted
        ) {
            Text(text = "📋 复制", fontSize = 16.sp)
        }

        Button(
            onClick = onClearButtonClick,
            enabled = !isStarted
        ) {
            Text(text = "🗑 清空", fontSize = 16.sp)
        }
    }
}
