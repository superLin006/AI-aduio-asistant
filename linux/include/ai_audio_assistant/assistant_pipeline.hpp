#ifndef AI_AUDIO_ASSISTANT_ASSISTANT_PIPELINE_HPP_
#define AI_AUDIO_ASSISTANT_ASSISTANT_PIPELINE_HPP_

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "ai_audio_assistant/offline_asr.hpp"

namespace ai_audio_assistant {

enum class AssistantState { kListening, kActivated, kRecognizing };
enum class EventType { kWakeWordDetected, kSpeechStarted, kTranscriptReady,
                       kActivationTimedOut, kError };

struct AIAUDIO_API AssistantEvent {
  EventType type;
  AssistantState state;
  std::string text;
  RecognitionResult recognition;
};

struct AIAUDIO_API KeywordSpotterConfig {
  std::string encoder_path;
  std::string decoder_path;
  std::string joiner_path;
  std::string tokens_path;
  std::string keywords_path;
  float score = 1.0F;
  float threshold = 0.25F;
  int32_t trailing_blanks = 2;
};

struct AIAUDIO_API VadConfig {
  std::string model_path;
  float threshold = 0.5F;
  float min_silence_seconds = 0.8F;
  float min_speech_seconds = 0.25F;
  float max_speech_seconds = 25.0F;
};

struct AIAUDIO_API AssistantConfig {
  KeywordSpotterConfig keyword_spotter;
  VadConfig vad;
  OfflineAsrConfig asr;
  float post_wake_guard_seconds = 0.5F;
  float activation_timeout_seconds = 8.0F;
  void Validate() const;
};

using EventCallback = std::function<void(const AssistantEvent &)>;

class AIAUDIO_API AssistantPipeline final {
 public:
  AssistantPipeline(AssistantConfig config, EventCallback callback);
  ~AssistantPipeline();
  AssistantPipeline(AssistantPipeline &&) noexcept;
  AssistantPipeline &operator=(AssistantPipeline &&) noexcept;
  AssistantPipeline(const AssistantPipeline &) = delete;
  AssistantPipeline &operator=(const AssistantPipeline &) = delete;

  void FeedAudio(const float *samples, int32_t sample_count,
                 int32_t sample_rate = 16000);
  void Flush();
  void Reset();
  AssistantState state() const;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace ai_audio_assistant
#endif
