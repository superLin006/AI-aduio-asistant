package com.superlin.xiaohui.kwsdemo

import android.content.res.AssetManager
import com.k2fsa.sherpa.onnx.FeatureConfig
import com.k2fsa.sherpa.onnx.KeywordSpotter
import com.k2fsa.sherpa.onnx.KeywordSpotterConfig
import com.k2fsa.sherpa.onnx.OnlineModelConfig
import com.k2fsa.sherpa.onnx.OnlineStream
import com.k2fsa.sherpa.onnx.OnlineTransducerModelConfig

class KwsEngine(assetManager: AssetManager) : AutoCloseable {
    private val spotter: KeywordSpotter
    private val stream: OnlineStream

    init {
        val modelDir = "kws-wenetspeech"
        val config = KeywordSpotterConfig(
            featConfig = FeatureConfig(sampleRate = SAMPLE_RATE, featureDim = 80),
            modelConfig = OnlineModelConfig(
                transducer = OnlineTransducerModelConfig(
                    encoder = "$modelDir/encoder.int8.onnx",
                    decoder = "$modelDir/decoder.onnx",
                    joiner = "$modelDir/joiner.int8.onnx",
                ),
                tokens = "$modelDir/tokens.txt",
                numThreads = 2,
                provider = "cpu",
                modelType = "zipformer2",
            ),
            keywordsFile = "$modelDir/keywords.txt",
            keywordsScore = 1.0f,
            keywordsThreshold = 0.25f,
            numTrailingBlanks = 2,
        )
        spotter = KeywordSpotter(assetManager, config)
        stream = spotter.createStream()
    }

    fun accept(samples: FloatArray): String? {
        stream.acceptWaveform(samples, SAMPLE_RATE)
        while (spotter.isReady(stream)) {
            spotter.decode(stream)
            val keyword = spotter.getResult(stream).keyword
            if (keyword.isNotEmpty()) {
                spotter.reset(stream)
                return keyword
            }
        }
        return null
    }

    override fun close() {
        stream.release()
        spotter.release()
    }

    companion object { const val SAMPLE_RATE = 16000 }
}
