#include "ai_audio_assistant/assistant_pipeline.hpp"

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <utility>
#include <vector>

#include "sherpa-onnx/c-api/cxx-api.h"

namespace ai_audio_assistant {
namespace {
constexpr int32_t kSampleRate = 16000;
constexpr int32_t kVadWindow = 512;
void RequireFile(const std::string &path, const char *name) {
  if (path.empty() || !std::filesystem::is_regular_file(path))
    throw std::invalid_argument(std::string(name) + " does not exist: " + path);
}
}  // namespace

void AssistantConfig::Validate() const {
  RequireFile(keyword_spotter.encoder_path, "KWS encoder");
  RequireFile(keyword_spotter.decoder_path, "KWS decoder");
  RequireFile(keyword_spotter.joiner_path, "KWS joiner");
  RequireFile(keyword_spotter.tokens_path, "KWS tokens");
  RequireFile(keyword_spotter.keywords_path, "KWS keywords");
  RequireFile(vad.model_path, "VAD model");
  asr.Validate();
  if (activation_timeout_seconds <= 0)
    throw std::invalid_argument("activation timeout must be positive");
  if (post_wake_guard_seconds < 0 || post_wake_guard_seconds > 2)
    throw std::invalid_argument("post-wake guard must be in [0, 2]");
  if (vad.max_speech_seconds <= 0 || vad.max_speech_seconds > 30)
    throw std::invalid_argument("VAD max speech must be in (0, 30]");
}

class AssistantPipeline::Impl {
 public:
  Impl(AssistantConfig config, EventCallback callback)
      : config_(std::move(config)), callback_(std::move(callback)) {
    config_.Validate();
    sherpa_onnx::cxx::KeywordSpotterConfig kc;
    kc.feat_config = {kSampleRate, 80};
    kc.model_config.transducer.encoder = config_.keyword_spotter.encoder_path;
    kc.model_config.transducer.decoder = config_.keyword_spotter.decoder_path;
    kc.model_config.transducer.joiner = config_.keyword_spotter.joiner_path;
    kc.model_config.tokens = config_.keyword_spotter.tokens_path;
    kc.model_config.provider = "cpu";
    kc.keywords_file = config_.keyword_spotter.keywords_path;
    kc.keywords_score = config_.keyword_spotter.score;
    kc.keywords_threshold = config_.keyword_spotter.threshold;
    kc.num_trailing_blanks = config_.keyword_spotter.trailing_blanks;
    kws_ = std::make_unique<sherpa_onnx::cxx::KeywordSpotter>(
        sherpa_onnx::cxx::KeywordSpotter::Create(kc));
    kws_stream_ = std::make_unique<sherpa_onnx::cxx::OnlineStream>(kws_->CreateStream());

    sherpa_onnx::cxx::VadModelConfig vc;
    vc.sample_rate = kSampleRate;
    vc.provider = "cpu";
    vc.silero_vad.model = config_.vad.model_path;
    vc.silero_vad.threshold = config_.vad.threshold;
    vc.silero_vad.min_silence_duration = config_.vad.min_silence_seconds;
    vc.silero_vad.min_speech_duration = config_.vad.min_speech_seconds;
    vc.silero_vad.max_speech_duration = config_.vad.max_speech_seconds;
    vc.silero_vad.window_size = kVadWindow;
    vad_ = std::make_unique<sherpa_onnx::cxx::VoiceActivityDetector>(
        sherpa_onnx::cxx::VoiceActivityDetector::Create(vc, 30));
    asr_ = std::make_unique<OfflineAsr>(config_.asr);
  }

  void Feed(const float *samples, int32_t count, int32_t rate) {
    if (!samples || count <= 0) return;
    if (rate != kSampleRate) throw std::invalid_argument("pipeline requires 16 kHz audio");
    if (state_ == AssistantState::kListening) { ProcessKws(samples, count); return; }
    if (guard_samples_ > 0) {
      const int32_t discarded = static_cast<int32_t>(
          std::min<int64_t>(guard_samples_, count));
      guard_samples_ -= discarded;
      samples += discarded;
      count -= discarded;
      if (count == 0) return;
    }
    activated_samples_ += count;
    pending_.insert(pending_.end(), samples, samples + count);
    while (pending_.size() >= kVadWindow) {
      vad_->AcceptWaveform(pending_.data(), kVadWindow);
      pending_.erase(pending_.begin(), pending_.begin() + kVadWindow);
      if (vad_->IsDetected() && !speech_started_) {
        speech_started_ = true;
        state_ = AssistantState::kRecognizing;
        Emit(EventType::kSpeechStarted, "");
      }
      Drain();
      if (state_ == AssistantState::kListening) return;
    }
    if (!speech_started_ && activated_samples_ >=
        static_cast<int64_t>(config_.activation_timeout_seconds * kSampleRate)) {
      Emit(EventType::kActivationTimedOut, "");
      Reset();
    }
  }

  void Flush() {
    if (state_ == AssistantState::kListening) return;
    if (!pending_.empty()) {
      pending_.resize(kVadWindow, 0);
      vad_->AcceptWaveform(pending_.data(), kVadWindow);
      pending_.clear();
    }
    vad_->Flush();
    Drain();
  }

  void Reset() {
    state_ = AssistantState::kListening;
    speech_started_ = false;
    activated_samples_ = 0;
    guard_samples_ = 0;
    pending_.clear();
    vad_->Reset();
    kws_->Reset(kws_stream_.get());
  }
  AssistantState state() const { return state_; }

 private:
  void ProcessKws(const float *samples, int32_t count) {
    kws_stream_->AcceptWaveform(kSampleRate, samples, count);
    while (kws_->IsReady(kws_stream_.get())) {
      kws_->Decode(kws_stream_.get());
      auto result = kws_->GetResult(kws_stream_.get());
      if (!result.keyword.empty()) {
        state_ = AssistantState::kActivated;
        activated_samples_ = 0;
        guard_samples_ = static_cast<int64_t>(
            config_.post_wake_guard_seconds * kSampleRate);
        speech_started_ = false;
        vad_->Reset();
        kws_->Reset(kws_stream_.get());
        Emit(EventType::kWakeWordDetected, result.keyword);
        return;
      }
    }
  }
  void Drain() {
    while (!vad_->IsEmpty()) {
      auto segment = vad_->Front();
      vad_->Pop();
      if (segment.samples.empty()) continue;
      try {
        auto result = asr_->Recognize(segment.samples.data(), segment.samples.size(), kSampleRate);
        if (callback_) callback_({EventType::kTranscriptReady,
                                 AssistantState::kListening, result.text, result});
      } catch (const std::exception &e) { Emit(EventType::kError, e.what()); }
      Reset();
      return;
    }
  }
  void Emit(EventType type, std::string text) const {
    if (callback_) callback_({type, state_, std::move(text), {}});
  }

  AssistantConfig config_;
  EventCallback callback_;
  AssistantState state_ = AssistantState::kListening;
  bool speech_started_ = false;
  int64_t activated_samples_ = 0;
  int64_t guard_samples_ = 0;
  std::vector<float> pending_;
  std::unique_ptr<sherpa_onnx::cxx::KeywordSpotter> kws_;
  std::unique_ptr<sherpa_onnx::cxx::OnlineStream> kws_stream_;
  std::unique_ptr<sherpa_onnx::cxx::VoiceActivityDetector> vad_;
  std::unique_ptr<OfflineAsr> asr_;
};

AssistantPipeline::AssistantPipeline(AssistantConfig c, EventCallback cb)
    : impl_(std::make_unique<Impl>(std::move(c), std::move(cb))) {}
AssistantPipeline::~AssistantPipeline() = default;
AssistantPipeline::AssistantPipeline(AssistantPipeline &&) noexcept = default;
AssistantPipeline &AssistantPipeline::operator=(AssistantPipeline &&) noexcept = default;
void AssistantPipeline::FeedAudio(const float *p, int32_t n, int32_t r) { impl_->Feed(p, n, r); }
void AssistantPipeline::Flush() { impl_->Flush(); }
void AssistantPipeline::Reset() { impl_->Reset(); }
AssistantState AssistantPipeline::state() const { return impl_->state(); }
}  // namespace ai_audio_assistant
