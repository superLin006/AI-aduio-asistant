#include "ai_audio_assistant/offline_asr.hpp"

#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <utility>

#include "sherpa-onnx/c-api/cxx-api.h"

namespace ai_audio_assistant {
namespace {

void RequireFile(const std::string &path, const char *name) {
  if (path.empty() || !std::filesystem::is_regular_file(path)) {
    throw std::invalid_argument(std::string(name) + " does not exist: " + path);
  }
}

void RequireDirectory(const std::string &path, const char *name) {
  if (path.empty() || !std::filesystem::is_directory(path)) {
    throw std::invalid_argument(std::string(name) + " does not exist: " + path);
  }
}

}  // namespace

void OfflineAsrConfig::Validate() const {
  RequireFile(bmodel_path, "Qwen3-ASR bmodel");
  RequireDirectory(tokenizer_dir, "Qwen3-ASR tokenizer directory");
  RequireFile(tokenizer_dir + "/vocab.json", "vocab.json");
  RequireFile(tokenizer_dir + "/merges.txt", "merges.txt");
  if (max_total_length <= 0 || max_new_tokens <= 0) {
    throw std::invalid_argument("token limits must be positive");
  }
  if (max_audio_seconds <= 0 || max_audio_seconds > 30) {
    throw std::invalid_argument("max_audio_seconds must be in [1, 30]");
  }
}

class OfflineAsr::Impl {
 public:
  explicit Impl(OfflineAsrConfig config) : config_(std::move(config)) {
    config_.Validate();
    sherpa_onnx::cxx::OfflineRecognizerConfig recognizer_config;
    recognizer_config.model_config.provider = "sophon";
    // The Sophon backend uses qwen3_asr.encoder for the merged encoder+LLM bmodel.
    recognizer_config.model_config.qwen3_asr.encoder = config_.bmodel_path;
    recognizer_config.model_config.qwen3_asr.tokenizer = config_.tokenizer_dir;
    recognizer_config.model_config.qwen3_asr.hotwords = config_.hotwords;
    recognizer_config.model_config.qwen3_asr.max_total_len =
        config_.max_total_length;
    recognizer_config.model_config.qwen3_asr.max_new_tokens =
        config_.max_new_tokens;
    recognizer_ = std::make_unique<sherpa_onnx::cxx::OfflineRecognizer>(
        sherpa_onnx::cxx::OfflineRecognizer::Create(recognizer_config));
  }

  RecognitionResult Recognize(const float *samples, int32_t sample_count,
                              int32_t sample_rate) const {
    if (samples == nullptr || sample_count <= 0) {
      throw std::invalid_argument("audio samples must not be empty");
    }
    if (sample_rate <= 0) throw std::invalid_argument("sample_rate must be positive");
    const double audio_seconds =
        static_cast<double>(sample_count) / static_cast<double>(sample_rate);
    if (audio_seconds > config_.max_audio_seconds) {
      throw std::invalid_argument("audio exceeds configured offline segment limit");
    }

    auto stream = recognizer_->CreateStream();
    stream.AcceptWaveform(sample_rate, samples, sample_count);
    const auto started = std::chrono::steady_clock::now();
    recognizer_->Decode(&stream);
    const auto result = recognizer_->GetResult(&stream);
    const double inference_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - started).count();
    return {result.text, audio_seconds, inference_seconds,
            audio_seconds > 0.0 ? inference_seconds / audio_seconds : 0.0};
  }

  OfflineAsrConfig config_;
  std::unique_ptr<sherpa_onnx::cxx::OfflineRecognizer> recognizer_;
};

OfflineAsr::OfflineAsr(OfflineAsrConfig config)
    : impl_(std::make_unique<Impl>(std::move(config))) {}
OfflineAsr::~OfflineAsr() = default;
OfflineAsr::OfflineAsr(OfflineAsr &&) noexcept = default;
OfflineAsr &OfflineAsr::operator=(OfflineAsr &&) noexcept = default;

RecognitionResult OfflineAsr::Recognize(const float *samples,
                                        int32_t sample_count,
                                        int32_t sample_rate) const {
  return impl_->Recognize(samples, sample_count, sample_rate);
}

RecognitionResult OfflineAsr::RecognizeFile(const std::string &wav_path) const {
  RequireFile(wav_path, "WAV input");
  const auto wave = sherpa_onnx::cxx::ReadWave(wav_path);
  if (wave.samples.empty()) throw std::runtime_error("failed to read WAV: " + wav_path);
  return Recognize(wave.samples.data(), static_cast<int32_t>(wave.samples.size()),
                   wave.sample_rate);
}

}  // namespace ai_audio_assistant
