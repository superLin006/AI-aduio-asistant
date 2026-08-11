package com.k2fsa.sherpa.onnx.config

/**
 * 应用配置中心
 * 统一管理所有模型选择、运行参数、Pipeline配置和特性开关
 */
object ModelConfig {

    /**
     * 模型选择配置
     */
    object Selection {
        // 关键词识别模型类型
        const val KWS_MODEL_TYPE = 0  // sherpa-onnx-kws-zipformer-wenetspeech-3.3M-2024-01-01

        // VAD模型类型
        const val VAD_MODEL_TYPE = 0  // Silero VAD

        // ASR模型类型
        const val ASR_MODEL_TYPE = 39  // CPU baseline; RKNN models are optional platform assets

        // 音频文件路径
        const val SOUND_LISTENING = "sounds/listening.mp3"      // "你说我在听"
        const val SOUND_PROCESSING = "sounds/processing.mp3"    // "我来帮你操作"
        const val SOUND_COMPLETED = "sounds/completed.mp3"      // "操作已完成"
    }

    /**
     * 运行时参数配置
     */
    object Runtime {
        // 音频采样率
        const val SAMPLE_RATE = 16000

        // 音频格式
        const val CHANNEL_CONFIG = android.media.AudioFormat.CHANNEL_IN_MONO
        const val AUDIO_FORMAT = android.media.AudioFormat.ENCODING_PCM_16BIT

        // 采样缓冲区大小（0.1秒）
        const val BUFFER_DURATION_MS = 100
        const val BUFFER_SIZE_IN_SAMPLES = SAMPLE_RATE * BUFFER_DURATION_MS / 1000  // 1600

        // 最多缓存约 3.2 秒音频，防止推理阻塞导致无界内存增长。
        const val AUDIO_QUEUE_CAPACITY = 32

        // VAD窗口大小
        const val VAD_WINDOW_SIZE = 512

        // VAD参数
        const val VAD_MIN_SILENCE_DURATION = 0.8f  // 最小静音持续时间（秒）
        const val VAD_MIN_SPEECH_DURATION = 0.25f  // 最小语音持续时间（秒）

        // ASR线程数
        const val ASR_NUM_THREADS = 1

        // ASR解码参数
        const val ASR_MAX_ACTIVE_PATHS = 4
    }

    /**
     * Pipeline业务逻辑配置
     */
    object Pipeline {
        // ASR中间结果更新间隔（毫秒）
        const val ASR_INTERIM_RESULT_INTERVAL_MS = 500L

        // 语音段结束后的等待时间（毫秒）
        const val SPEECH_END_WAIT_MS = 100L

        // 唤醒词检测阈值（由模型内部控制）
        const val KWS_THRESHOLD = 0.5f

        // 意图识别最低置信度
        const val INTENT_MIN_CONFIDENCE = 0.6f
    }

    /**
     * 特性开关
     */
    object Features {
        // 是否启用意图识别
        const val ENABLE_INTENT_RECOGNITION = true

        // 是否启用设备控制
        const val ENABLE_DEVICE_CONTROL = true

        // 是否启用音频反馈
        const val ENABLE_AUDIO_FEEDBACK = true

        // 是否在UI显示中间结果
        const val ENABLE_INTERIM_RESULTS = true

        // 是否启用日志输出
        const val ENABLE_LOGGING = true
    }

    /**
     * DeepSeek API配置
     */
    object Api {
        const val DEEPSEEK_BASE_URL = "https://api.deepseek.com"
        const val DEEPSEEK_MODEL = "deepseek-chat"
        val DEEPSEEK_API_KEY: String =
            com.k2fsa.sherpa.onnx.simulate.streaming.asr.BuildConfig.DEEPSEEK_API_KEY

        // API超时配置（秒）
        const val CONNECT_TIMEOUT = 10L
        const val READ_TIMEOUT = 30L
        const val WRITE_TIMEOUT = 30L
    }

    /**
     * 设备控制配置
     */
    object Device {
        // 支持的设备类型
        val SUPPORTED_DEVICES = setOf(
            "whiteboard",      // 白板
            "projector",       // 投影仪
            "curtain",         // 窗帘
            "light",           // 灯光
            "air_conditioner", // 空调
            "speaker"          // 音响
        )

        // 支持的操作类型
        val SUPPORTED_ACTIONS = setOf(
            "open",   // 打开
            "close",  // 关闭
            "adjust", // 调节
            "query"   // 查询
        )
    }
}
