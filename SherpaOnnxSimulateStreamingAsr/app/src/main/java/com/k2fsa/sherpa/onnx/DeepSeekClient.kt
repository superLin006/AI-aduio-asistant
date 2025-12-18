package com.k2fsa.sherpa.onnx.simulate.streaming.asr

import android.util.Log
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONArray
import org.json.JSONObject
import java.io.BufferedReader
import java.io.InputStreamReader
import java.io.OutputStreamWriter
import java.net.HttpURLConnection
import java.net.URL

/**
 * DeepSeek API客户端
 * 用于将语音识别的文本转换为结构化指令
 */
class DeepSeekClient(
    private val apiKey: String,
    private val apiUrl: String = "https://api.deepseek.com/v1/chat/completions"
) {
    private val TAG = "DeepSeekClient"
    
    companion object {
        // 教室场景支持的设备和操作
        private val SYSTEM_PROMPT = """
你是一个教室智能助手的意图识别系统。你的任务是将老师的语音指令转换为结构化的JSON格式。

## 支持的设备:
- whiteboard (白板)
- projector (投影仪)
- curtain (窗帘)
- light (灯光)
- air_conditioner (空调)
- speaker (音响)

## 支持的操作:
- open (打开/启动)
- close (关闭/停止)
- adjust (调整)
- query (查询状态)

## 输出格式:
必须输出纯JSON,不要有任何额外文字,格式如下:
{
  "action": "操作类型",
  "target": "设备名称",
  "parameters": {
    "value": "具体参数(可选)",
    "unit": "单位(可选)"
  },
  "confidence": 0.0-1.0,
  "original_text": "原始文本"
}

## 示例:
输入: "帮我打开白板"
输出: {"action": "open", "target": "whiteboard", "parameters": {}, "confidence": 0.95, "original_text": "帮我打开白板"}

输入: "把空调温度调到26度"
输出: {"action": "adjust", "target": "air_conditioner", "parameters": {"value": "26", "unit": "celsius"}, "confidence": 0.90, "original_text": "把空调温度调到26度"}

输入: "今天天气怎么样"
输出: {"action": "query", "target": "unknown", "parameters": {}, "confidence": 0.0, "original_text": "今天天气怎么样"}

注意: 如果无法识别为教室设备指令,confidence设为0.0,target设为"unknown"。
""".trimIndent()
    }
    
    /**
     * 解析指令的结果
     */
    data class IntentResult(
        val action: String,           // 操作类型: open, close, adjust, query
        val target: String,           // 目标设备
        val parameters: Map<String, String> = emptyMap(),  // 参数
        val confidence: Float,        // 置信度 0.0-1.0
        val originalText: String,     // 原始文本
        val isValid: Boolean          // 是否是有效的教室指令
    ) {
        /**
         * 转换为可读的描述
         */
        fun toReadableString(): String {
            val actionText = when (action) {
                "open" -> "打开"
                "close" -> "关闭"
                "adjust" -> "调整"
                "query" -> "查询"
                else -> action
            }
            
            val targetText = when (target) {
                "whiteboard" -> "白板"
                "projector" -> "投影仪"
                "curtain" -> "窗帘"
                "light" -> "灯光"
                "air_conditioner" -> "空调"
                "speaker" -> "音响"
                else -> target
            }
            
            return if (parameters.isNotEmpty()) {
                val params = parameters.entries.joinToString(", ") { "${it.key}=${it.value}" }
                "$actionText$targetText ($params)"
            } else {
                "$actionText$targetText"
            }
        }
    }
    
    /**
     * 解析语音指令,返回结构化意图
     * @param text ASR识别的文本
     * @return 解析后的意图结果
     */
    suspend fun parseIntent(text: String): IntentResult? = withContext(Dispatchers.IO) {
        try {
            Log.i(TAG, "🤖 Parsing intent for: $text")
            
            // 构建请求
            val requestBody = JSONObject().apply {
                put("model", "deepseek-chat")
                put("messages", JSONArray().apply {
                    put(JSONObject().apply {
                        put("role", "system")
                        put("content", SYSTEM_PROMPT)
                    })
                    put(JSONObject().apply {
                        put("role", "user")
                        put("content", text)
                    })
                })
                put("temperature", 0.3)  // 低温度,更确定性的输出
                put("max_tokens", 200)
            }
            
            // 发送请求
            val responseJson = sendRequest(requestBody.toString())
            
            // 解析响应
            if (responseJson != null) {
                val result = parseResponse(responseJson, text)
                
                if (result != null) {
                    Log.i(TAG, "✅ Intent parsed: ${result.toReadableString()}")
                    Log.i(TAG, "   Confidence: ${result.confidence}, Valid: ${result.isValid}")
                } else {
                    Log.w(TAG, "❌ Failed to parse intent from response")
                }
                
                return@withContext result
            } else {
                Log.e(TAG, "❌ API request failed")
                return@withContext null
            }
            
        } catch (e: Exception) {
            Log.e(TAG, "Error parsing intent", e)
            return@withContext null
        }
    }
    
    /**
     * 发送HTTP请求到DeepSeek API
     */
    private fun sendRequest(requestBody: String): String? {
        var connection: HttpURLConnection? = null
        
        try {
            val url = URL(apiUrl)
            connection = url.openConnection() as HttpURLConnection
            
            // 设置请求
            connection.requestMethod = "POST"
            connection.setRequestProperty("Content-Type", "application/json")
            connection.setRequestProperty("Authorization", "Bearer $apiKey")
            connection.doOutput = true
            connection.connectTimeout = 10000  // 10秒超时
            connection.readTimeout = 10000
            
            // 发送请求体
            OutputStreamWriter(connection.outputStream).use { writer ->
                writer.write(requestBody)
                writer.flush()
            }
            
            // 读取响应
            val responseCode = connection.responseCode
            
            if (responseCode == HttpURLConnection.HTTP_OK) {
                val response = BufferedReader(InputStreamReader(connection.inputStream)).use { reader ->
                    reader.readText()
                }
                return response
            } else {
                val errorResponse = BufferedReader(InputStreamReader(connection.errorStream)).use { reader ->
                    reader.readText()
                }
                Log.e(TAG, "API error ($responseCode): $errorResponse")
                return null
            }
            
        } catch (e: Exception) {
            Log.e(TAG, "HTTP request failed", e)
            return null
        } finally {
            connection?.disconnect()
        }
    }
    
    /**
     * 解析DeepSeek API响应
     */
    private fun parseResponse(responseJson: String, originalText: String): IntentResult? {
        try {
            val json = JSONObject(responseJson)
            val choices = json.getJSONArray("choices")
            
            if (choices.length() == 0) {
                Log.w(TAG, "No choices in response")
                return null
            }
            
            val message = choices.getJSONObject(0).getJSONObject("message")
            val content = message.getString("content").trim()
            
            Log.d(TAG, "DeepSeek response: $content")
            
            // 解析JSON响应
            val intentJson = try {
                // 尝试直接解析
                JSONObject(content)
            } catch (e: Exception) {
                // 如果失败,尝试提取JSON部分
                val jsonStart = content.indexOf("{")
                val jsonEnd = content.lastIndexOf("}") + 1
                if (jsonStart >= 0 && jsonEnd > jsonStart) {
                    JSONObject(content.substring(jsonStart, jsonEnd))
                } else {
                    throw Exception("Cannot extract JSON from response")
                }
            }
            
            // 提取字段
            val action = intentJson.optString("action", "unknown")
            val target = intentJson.optString("target", "unknown")
            val confidence = intentJson.optDouble("confidence", 0.0).toFloat()
            val originalTextFromJson = intentJson.optString("original_text", originalText)
            
            // 提取parameters
            val parameters = mutableMapOf<String, String>()
            if (intentJson.has("parameters")) {
                val paramsJson = intentJson.getJSONObject("parameters")
                paramsJson.keys().forEach { key ->
                    parameters[key] = paramsJson.getString(key)
                }
            }
            
            // 判断是否是有效指令
            val isValid = confidence >= 0.5 && target != "unknown"
            
            return IntentResult(
                action = action,
                target = target,
                parameters = parameters,
                confidence = confidence,
                originalText = originalTextFromJson,
                isValid = isValid
            )
            
        } catch (e: Exception) {
            Log.e(TAG, "Failed to parse response", e)
            return null
        }
    }
    
    /**
     * 测试API连接
     */
    suspend fun testConnection(): Boolean = withContext(Dispatchers.IO) {
        try {
            Log.i(TAG, "Testing DeepSeek API connection...")
            
            val testResult = parseIntent("打开白板")
            
            if (testResult != null) {
                Log.i(TAG, "✅ DeepSeek API connection successful")
                return@withContext true
            } else {
                Log.e(TAG, "❌ DeepSeek API test failed")
                return@withContext false
            }
            
        } catch (e: Exception) {
            Log.e(TAG, "Connection test failed", e)
            return@withContext false
        }
    }
}