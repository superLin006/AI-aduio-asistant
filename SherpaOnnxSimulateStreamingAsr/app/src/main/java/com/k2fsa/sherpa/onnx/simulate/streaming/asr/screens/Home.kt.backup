package com.k2fsa.sherpa.onnx.simulate.streaming.asr.screens

import android.Manifest
import android.annotation.SuppressLint
import android.app.Activity
import android.content.pm.PackageManager
import android.media.AudioFormat
import android.media.AudioRecord
import android.media.MediaRecorder
import android.util.Log
import android.widget.Toast
import androidx.compose.foundation.background
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
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.app.ActivityCompat
import com.k2fsa.sherpa.onnx.simulate.streaming.asr.R
import com.k2fsa.sherpa.onnx.simulate.streaming.asr.SimulateStreamingAsr
import com.k2fsa.sherpa.onnx.simulate.streaming.asr.TAG
import com.k2fsa.sherpa.onnx.simulate.streaming.asr.VoiceAssistantManager
import com.k2fsa.sherpa.onnx.simulate.streaming.asr.AssistantState
import com.k2fsa.sherpa.onnx.simulate.streaming.asr.IntentManager
import com.k2fsa.sherpa.onnx.simulate.streaming.asr.IntentProcessingState
import com.k2fsa.sherpa.onnx.simulate.streaming.asr.AudioPlayer
import com.k2fsa.sherpa.onnx.simulate.streaming.asr.SoundPaths
import kotlinx.coroutines.*
import kotlinx.coroutines.channels.Channel

private var audioRecord: AudioRecord? = null
private const val sampleRateInHz = 16000
private var samplesChannel = Channel<FloatArray>(capacity = Channel.UNLIMITED)

data class SimpleResult(
    val timestamp: String,
    val text: String,
    val isWakeWord: Boolean = false,
    val isFinal: Boolean = true
)

@Composable
fun HomeScreen() {
    val context = LocalContext.current
    val clipboardManager = LocalClipboardManager.current
    val activity = LocalContext.current as Activity
    
    var isStarted by remember { mutableStateOf(false) }
    val resultList: MutableList<SimpleResult> = remember { mutableStateListOf() }
    val lazyColumnListState = rememberLazyListState()
    val coroutineScope = rememberCoroutineScope()
    
    val assistantState by VoiceAssistantManager.assistant?.state?.collectAsState() 
        ?: remember { mutableStateOf(AssistantState.LISTENING) }
    
    val recordingStartTime = remember { mutableStateOf(0L) }

    val onRecordingButtonClick: () -> Unit = {
        isStarted = !isStarted
        if (isStarted) {
            if (ActivityCompat.checkSelfPermission(
                    activity,
                    Manifest.permission.RECORD_AUDIO
                ) != PackageManager.PERMISSION_GRANTED
            ) {
                Log.i(TAG, "Recording is not allowed")
            } else {
                recordingStartTime.value = System.currentTimeMillis()
                
                SimulateStreamingAsr.vad.reset()
                VoiceAssistantManager.assistant?.reset()
                
                val audioSource = MediaRecorder.AudioSource.MIC
                val channelConfig = AudioFormat.CHANNEL_IN_MONO
                val audioFormat = AudioFormat.ENCODING_PCM_16BIT
                val numBytes = AudioRecord.getMinBufferSize(sampleRateInHz, channelConfig, audioFormat)
                audioRecord = AudioRecord(
                    audioSource,
                    sampleRateInHz,
                    AudioFormat.CHANNEL_IN_MONO,
                    AudioFormat.ENCODING_PCM_16BIT,
                    numBytes * 2
                )

                // 音频采集协程
                CoroutineScope(Dispatchers.IO).launch {
                    Log.i(TAG, "🎤 Audio recording started")
                    val interval = 0.1
                    val bufferSize = (interval * sampleRateInHz).toInt()
                    val buffer = ShortArray(bufferSize)

                    audioRecord?.let { it ->
                        it.startRecording()

                        while (isStarted) {
                            val ret = audioRecord?.read(buffer, 0, buffer.size)
                            ret?.let { n ->
                                val samples = FloatArray(n) { buffer[it] / 32768.0f }
                                samplesChannel.send(samples)
                            }
                        }
                        val samples = FloatArray(0)
                        samplesChannel.send(samples)
                    }
                }

                // 音频处理协程
                CoroutineScope(Dispatchers.Default).launch {
                    Log.i(TAG, "🔄 Audio processing loop started")
                    
                    var buffer = arrayListOf<Float>()
                    var offset = 0
                    val windowSize = 512
                    var isSpeechStarted = false
                    var speechStartTime = 0L
                    var startTime = System.currentTimeMillis()
                    var lastText = ""
                    var added = false
                    var currentIndex = -1
                    var asrStartTime = 0L
                    
                    var intentRecognitionStarted = false
                    var intentRecognitionCompleted = false
                    
                    var lastState = AssistantState.LISTENING
                    
                    while (isStarted) {
                        for (s in samplesChannel) {
                            if (s.isEmpty()) {
                                break
                            }

                            val assistant = VoiceAssistantManager.assistant
                            if (assistant == null) {
                                Log.w(TAG, "VoiceAssistant not initialized")
                                continue
                            }
                            
                            // 检测状态变化
                            val currentState = assistant.state.value
                            if (currentState != lastState) {
                                Log.d(TAG, "🔄 State changed: $lastState -> $currentState")
                                
                                if (currentState == AssistantState.LISTENING) {
                                    Log.i(TAG, "🧹 Cleaning up for LISTENING state")
                                    
                                    buffer.clear()
                                    offset = 0
                                    isSpeechStarted = false
                                    added = false
                                    asrStartTime = 0L
                                    lastText = ""
                                    intentRecognitionStarted = false
                                    intentRecognitionCompleted = false
                                    
                                    try {
                                        delay(100)
                                        SimulateStreamingAsr.vad.reset()
                                        Log.d(TAG, "VAD reset completed")
                                    } catch (e: Exception) {
                                        Log.e(TAG, "Error resetting VAD", e)
                                    }
                                }
                                
                                lastState = currentState
                            }
                            
                            when (assistant.state.value) {
                                AssistantState.LISTENING -> {
                                    assistant.processAudio(s, sampleRateInHz)
                                    
                                    val result = assistant.result.value
                                    if (result != null && result.state == AssistantState.ACTIVATED) {
                                        val timestamp = formatTimestamp(System.currentTimeMillis() - recordingStartTime.value)
                                        resultList.add(
                                            SimpleResult(
                                                timestamp = timestamp,
                                                text = "🔔 唤醒词: ${result.keyword}",
                                                isWakeWord = true,
                                                isFinal = true
                                            )
                                        )
                                        
                                        coroutineScope.launch {
                                            lazyColumnListState.animateScrollToItem(resultList.size - 1)
                                        }
                                        
                                        Log.i(TAG, "🎯 Wake word detected: ${result.keyword}")
                                        Log.i(TAG, "⚡ Calling transitionToProcessing()")
                                        assistant.transitionToProcessing()
                                        
                                        SimulateStreamingAsr.vad.reset()
                                        buffer = arrayListOf()
                                        offset = 0
                                        isSpeechStarted = false
                                        added = false
                                        asrStartTime = System.currentTimeMillis()
                                        intentRecognitionStarted = false
                                    }
                                }
                                
                                AssistantState.ACTIVATED -> {
                                    Log.i(TAG, "⚡ Still in ACTIVATED, forcing transition")
                                    assistant.transitionToProcessing()
                                    asrStartTime = System.currentTimeMillis()
                                }
                                
                                AssistantState.PROCESSING -> {
                                    val totalElapsed = System.currentTimeMillis() - asrStartTime
                                    
                                    if (asrStartTime > 0 && totalElapsed > 6500 && totalElapsed < 10000) {
                                        Log.d(TAG, "⏱️ Processing elapsed: ${totalElapsed}ms, lastText='$lastText', isSpeechStarted=$isSpeechStarted")
                                    }
                                    
                                    // 超时检查(7秒)
                                    if (assistant.state.value == AssistantState.PROCESSING
                                        && asrStartTime > 0 && totalElapsed > 7000 && totalElapsed < 10000 
                                        && !intentRecognitionStarted && !intentRecognitionCompleted && lastText.isNotBlank()) {
                                        
                                        Log.w(TAG, "⏰ Home.kt timeout check triggered! totalElapsed=$totalElapsed")
                                        Log.i(TAG, "📝 Using lastText for intent: $lastText")
                                        
                                        intentRecognitionStarted = true
                                        
                                        if (currentIndex >= 0 && currentIndex < resultList.size) {
                                            coroutineScope.launch(Dispatchers.Main) {
                                                resultList[currentIndex] = resultList[currentIndex].copy(isFinal = true)
                                            }
                                        }
                                        
                                        assistant.cancelTimeout()
                                        
                                        AudioPlayer.play(context, SoundPaths.PROCESSING) {
                                            Log.d(TAG, "Processing prompt completed")
                                            
                                            if (IntentManager.isReady()) {
                                                IntentManager.processCommand(lastText) { intentResult ->
                                                    when (intentResult.state) {
                                                        IntentProcessingState.COMPLETED -> {
                                                            val msg = intentResult.commandResult?.message ?: "完成"
                                                            Log.i(TAG, "✅ Intent completed: $msg")
                                                            
                                                            intentRecognitionCompleted = true
                                                            
                                                            coroutineScope.launch(Dispatchers.Main) {
                                                                resultList.add(
                                                                    SimpleResult(
                                                                        timestamp = formatTimestamp(System.currentTimeMillis() - recordingStartTime.value),
                                                                        text = "✅ $msg",
                                                                        isWakeWord = false,
                                                                        isFinal = true
                                                                    )
                                                                )
                                                                lazyColumnListState.animateScrollToItem(resultList.size - 1)
                                                            }
                                                            
                                                            assistant.returnToListening("Command executed")
                                                        }
                                                        
                                                        IntentProcessingState.FAILED -> {
                                                            val reason = if (intentResult.intent?.isValid == false) {
                                                                "无法识别的指令"
                                                            } else {
                                                                intentResult.commandResult?.message ?: "执行失败"
                                                            }
                                                            Log.w(TAG, "⚠️ Intent failed: $reason")
                                                            
                                                            intentRecognitionCompleted = true
                                                            
                                                            coroutineScope.launch(Dispatchers.Main) {
                                                                resultList.add(
                                                                    SimpleResult(
                                                                        timestamp = formatTimestamp(System.currentTimeMillis() - recordingStartTime.value),
                                                                        text = "❌ $reason",
                                                                        isWakeWord = false,
                                                                        isFinal = true
                                                                    )
                                                                )
                                                                lazyColumnListState.animateScrollToItem(resultList.size - 1)
                                                            }
                                                            
                                                            assistant.returnToListening("Command failed")
                                                        }
                                                        
                                                        else -> {}
                                                    }
                                                }
                                            }
                                        }
                                        
                                        SimulateStreamingAsr.vad.clear()
                                        buffer = arrayListOf()
                                        offset = 0
                                        isSpeechStarted = false
                                        added = false
                                        asrStartTime = System.currentTimeMillis() + 999999999L
                                        lastText = ""
                                        continue
                                    }
                                    
                                    buffer.addAll(s.toList())
                                    
                                    if (assistant.state.value != AssistantState.PROCESSING) {
                                        Log.w(TAG, "⚠️ State changed during buffer processing, skipping VAD")
                                        continue
                                    }
                                    
                                    // VAD处理
                                    while (offset + windowSize < buffer.size) {
                                        if (assistant.state.value != AssistantState.PROCESSING) {
                                            Log.w(TAG, "⚠️ State changed during VAD processing, stopping")
                                            break
                                        }
                                        
                                        try {
                                            SimulateStreamingAsr.vad.acceptWaveform(
                                                buffer.subList(offset, offset + windowSize).toFloatArray()
                                            )
                                        } catch (e: Exception) {
                                            Log.e(TAG, "VAD acceptWaveform error, resetting", e)
                                            try {
                                                SimulateStreamingAsr.vad.reset()
                                            } catch (resetException: Exception) {
                                                Log.e(TAG, "Error resetting VAD", resetException)
                                            }
                                            buffer.clear()
                                            offset = 0
                                            break
                                        }
                                        offset += windowSize
                                        
                                        if (!isSpeechStarted && SimulateStreamingAsr.vad.isSpeechDetected()) {
                                            isSpeechStarted = true
                                            startTime = System.currentTimeMillis()
                                            speechStartTime = System.currentTimeMillis()
                                            Log.d(TAG, "🗣️ Speech detected")
                                        }
                                    }

                                    // 实时ASR(中间结果)
                                    val elapsed = System.currentTimeMillis() - startTime
                                    if (isSpeechStarted && elapsed > 500 && elapsed < 6500) {
                                        val stream = SimulateStreamingAsr.recognizer.createStream()
                                        stream.acceptWaveform(
                                            buffer.subList(0, offset).toFloatArray(),
                                            sampleRateInHz
                                        )
                                        SimulateStreamingAsr.recognizer.decode(stream)
                                        val result = SimulateStreamingAsr.recognizer.getResult(stream)
                                        stream.release()

                                        lastText = result.text

                                        if (lastText.isNotBlank()) {
                                            val timestamp = formatTimestamp(speechStartTime - recordingStartTime.value)
                                            
                                            val tempResult = SimpleResult(
                                                timestamp = timestamp,
                                                text = lastText,
                                                isWakeWord = false,
                                                isFinal = false
                                            )
                                            
                                            if (!added || resultList.isEmpty()) {
                                                resultList.add(tempResult)
                                                currentIndex = resultList.size - 1
                                                added = true
                                            } else {
                                                if (currentIndex >= 0 && currentIndex < resultList.size) {
                                                    resultList[currentIndex] = tempResult
                                                }
                                            }

                                            coroutineScope.launch {
                                                lazyColumnListState.animateScrollToItem(resultList.size - 1)
                                            }
                                        }

                                        startTime = System.currentTimeMillis()
                                    }

                                    // 处理完整的语音段(VAD检测到结束)
                                    while (!SimulateStreamingAsr.vad.empty()) {
                                        if (intentRecognitionStarted) {
                                            SimulateStreamingAsr.vad.pop()
                                            continue
                                        }
                                        
                                        try {
                                            val speechSegment = SimulateStreamingAsr.vad.front()
                                            val samples = speechSegment.samples
                                            
                                            Log.i(TAG, "📝 Processing final speech segment")
                                            
                                            val stream = SimulateStreamingAsr.recognizer.createStream()
                                            stream.acceptWaveform(samples, sampleRateInHz)
                                            SimulateStreamingAsr.recognizer.decode(stream)
                                            val result = SimulateStreamingAsr.recognizer.getResult(stream)
                                            stream.release()
                                            
                                            Log.i(TAG, "✅ Final ASR result: ${result.text}")

                                            isSpeechStarted = false
                                            SimulateStreamingAsr.vad.pop()

                                            buffer = arrayListOf()
                                            offset = 0
                                            
                                            if (result.text.isNotBlank()) {
                                                val timestamp = formatTimestamp(speechStartTime - recordingStartTime.value)
                                                
                                                val finalResult = SimpleResult(
                                                    timestamp = timestamp,
                                                    text = result.text,
                                                    isWakeWord = false,
                                                    isFinal = true
                                                )
                                                
                                                if (added && currentIndex >= 0 && currentIndex < resultList.size) {
                                                    resultList[currentIndex] = finalResult
                                                } else {
                                                    resultList.add(finalResult)
                                                    currentIndex = resultList.size - 1
                                                }

                                                coroutineScope.launch {
                                                    lazyColumnListState.animateScrollToItem(resultList.size - 1)
                                                }
                                                
                                                Log.i(TAG, "📤 Sending ASR result to VoiceAssistant: ${result.text}")
                                                
                                                assistant.cancelTimeout()
                                                
                                                AudioPlayer.play(context, SoundPaths.PROCESSING) {
                                                    Log.d(TAG, "Processing prompt completed")
                                                    
                                                    if (IntentManager.isReady()) {
                                                        Log.i(TAG, "🤖 Starting intent recognition...")
                                                        
                                                        intentRecognitionStarted = true
                                                        
                                                        IntentManager.processCommand(result.text) { intentResult ->
                                                            when (intentResult.state) {
                                                                IntentProcessingState.COMPLETED -> {
                                                                    val msg = intentResult.commandResult?.message ?: "完成"
                                                                    Log.i(TAG, "✅ Intent processing completed: $msg")
                                                                    
                                                                    intentRecognitionCompleted = true
                                                                    
                                                                    coroutineScope.launch(Dispatchers.Main) {
                                                                        resultList.add(
                                                                            SimpleResult(
                                                                                timestamp = formatTimestamp(System.currentTimeMillis() - recordingStartTime.value),
                                                                                text = "✅ $msg",
                                                                                isWakeWord = false,
                                                                                isFinal = true
                                                                            )
                                                                        )
                                                                        lazyColumnListState.animateScrollToItem(resultList.size - 1)
                                                                    }
                                                                    
                                                                    assistant.returnToListening("Command executed")
                                                                }
                                                                
                                                                IntentProcessingState.FAILED -> {
                                                                    val reason = if (intentResult.intent?.isValid == false) {
                                                                        "无法识别的指令"
                                                                    } else {
                                                                        intentResult.commandResult?.message ?: "执行失败"
                                                                    }
                                                                    Log.w(TAG, "⚠️ Intent processing failed: $reason")
                                                                    
                                                                    intentRecognitionCompleted = true
                                                                    
                                                                    coroutineScope.launch(Dispatchers.Main) {
                                                                        resultList.add(
                                                                            SimpleResult(
                                                                                timestamp = formatTimestamp(System.currentTimeMillis() - recordingStartTime.value),
                                                                                text = "❌ $reason",
                                                                                isWakeWord = false,
                                                                                isFinal = true
                                                                            )
                                                                        )
                                                                        lazyColumnListState.animateScrollToItem(resultList.size - 1)
                                                                    }
                                                                    
                                                                    assistant.returnToListening("Command failed")
                                                                }
                                                                
                                                                else -> {}
                                                            }
                                                        }
                                                    } else {
                                                        Log.w(TAG, "IntentManager not ready, skipping intent recognition")
                                                        assistant.onAsrResult(result.text, isFinal = true)
                                                    }
                                                }
                                                
                                                added = false
                                                asrStartTime = System.currentTimeMillis() + 999999999L
                                                lastText = ""
                                            }
                                        } catch (e: Exception) {
                                            Log.e(TAG, "Error processing speech segment", e)
                                            SimulateStreamingAsr.vad.pop()
                                            buffer = arrayListOf()
                                            offset = 0
                                        }
                                    }
                                }
                            }
                        }
                    }
                    
                    Log.i(TAG, "🛑 Audio processing loop stopped")
                }
            }
        } else {
            audioRecord?.stop()
            audioRecord?.release()
            audioRecord = null
            
            Log.i(TAG, "ℹ️ Recording stopped")
        }
    }
    
    Box(
        modifier = Modifier.fillMaxSize(),
        contentAlignment = Alignment.TopCenter,
    ) {
        Column(modifier = Modifier) {
            StatusIndicator(state = assistantState)
            
            HomeButtonRow(
                isStarted = isStarted,
                onRecordingButtonClick = onRecordingButtonClick,
                onCopyButtonClick = {
                    if (resultList.isNotEmpty()) {
                        val s = resultList.mapIndexed { i, result -> 
                            "${i + 1}: [${result.timestamp}] ${result.text}"
                        }.joinToString(separator = "\n")
                        clipboardManager.setText(AnnotatedString(s))
                        Toast.makeText(context, "Copied to clipboard", Toast.LENGTH_SHORT).show()
                    } else {
                        Toast.makeText(context, "Nothing to copy", Toast.LENGTH_SHORT).show()
                    }
                },
                onClearButtonClick = {
                    resultList.clear()
                }
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
}

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
            .background(bgColor)
            .padding(12.dp),
        contentAlignment = Alignment.Center
    ) {
        Text(
            text = statusText,
            color = textColor,
            fontSize = 16.sp,
            fontWeight = FontWeight.Bold
        )
    }
}

fun formatTimestamp(milliseconds: Long): String {
    val totalSeconds = milliseconds / 1000
    val minutes = totalSeconds / 60
    val seconds = totalSeconds % 60
    return String.format("%02d:%02d", minutes, seconds)
}

@SuppressLint("UnrememberedMutableState")
@Composable
private fun HomeButtonRow(
    modifier: Modifier = Modifier,
    isStarted: Boolean,
    onRecordingButtonClick: () -> Unit,
    onCopyButtonClick: () -> Unit,
    onClearButtonClick: () -> Unit,
) {
    Row(
        modifier = modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.Center,
    ) {
        Button(onClick = onRecordingButtonClick) {
            Text(text = stringResource(if (isStarted) R.string.stop else R.string.start))
        }

        Spacer(modifier = Modifier.width(16.dp))

        Button(onClick = onCopyButtonClick) {
            Text(text = stringResource(id = R.string.copy))
        }

        Spacer(modifier = Modifier.width(16.dp))

        Button(onClick = onClearButtonClick) {
            Text(text = stringResource(id = R.string.clear))
        }
        
        Spacer(modifier = Modifier.width(16.dp))
        
        Button(
            onClick = {
                VoiceAssistantManager.assistant?.reset()
            },
            colors = ButtonDefaults.buttonColors(
                containerColor = MaterialTheme.colorScheme.secondary
            )
        ) {
            Text(text = "Reset")
        }
    }
}