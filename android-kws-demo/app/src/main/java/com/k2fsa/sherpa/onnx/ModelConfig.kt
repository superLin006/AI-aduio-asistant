package com.k2fsa.sherpa.onnx

data class FeatureConfig(
    var sampleRate: Int = 16000,
    var featureDim: Int = 80,
    var dither: Float = 0.0f,
)

data class OnlineTransducerModelConfig(
    var encoder: String = "",
    var decoder: String = "",
    var joiner: String = "",
)

data class OnlineParaformerModelConfig(var encoder: String = "", var decoder: String = "")
data class OnlineZipformer2CtcModelConfig(var model: String = "")
data class OnlineNeMoCtcModelConfig(var model: String = "")
data class OnlineToneCtcModelConfig(var model: String = "")

data class OnlineModelConfig(
    var transducer: OnlineTransducerModelConfig = OnlineTransducerModelConfig(),
    var paraformer: OnlineParaformerModelConfig = OnlineParaformerModelConfig(),
    var zipformer2Ctc: OnlineZipformer2CtcModelConfig = OnlineZipformer2CtcModelConfig(),
    var neMoCtc: OnlineNeMoCtcModelConfig = OnlineNeMoCtcModelConfig(),
    var toneCtc: OnlineToneCtcModelConfig = OnlineToneCtcModelConfig(),
    var tokens: String = "",
    var numThreads: Int = 2,
    var debug: Boolean = false,
    var provider: String = "cpu",
    var modelType: String = "zipformer2",
    var modelingUnit: String = "",
    var bpeVocab: String = "",
)
