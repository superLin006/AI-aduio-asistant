#ifndef AI_AUDIO_ASSISTANT_OFFLINE_ASR_HPP_
#define AI_AUDIO_ASSISTANT_OFFLINE_ASR_HPP_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#if defined(_WIN32)
#define AIAUDIO_API __declspec(dllexport)
#else
#define AIAUDIO_API __attribute__((visibility("default")))
#endif

namespace ai_audio_assistant {

struct AIAUDIO_API OfflineAsrConfig {
  std::string bmodel_path;
  std::string tokenizer_dir;
  std::string hotwords;
  int32_t max_total_length = 512;
  int32_t max_new_tokens = 128;
  int32_t max_audio_seconds = 30;

  void Validate() const;
};

struct AIAUDIO_API RecognitionResult {
  std::string text;
  double audio_seconds = 0.0;
  double inference_seconds = 0.0;
  double real_time_factor = 0.0;
};

class AIAUDIO_API OfflineAsr final {
 public:
  explicit OfflineAsr(OfflineAsrConfig config);
  ~OfflineAsr();

  OfflineAsr(OfflineAsr &&) noexcept;
  OfflineAsr &operator=(OfflineAsr &&) noexcept;
  OfflineAsr(const OfflineAsr &) = delete;
  OfflineAsr &operator=(const OfflineAsr &) = delete;

  RecognitionResult Recognize(const float *samples, int32_t sample_count,
                              int32_t sample_rate = 16000) const;
  RecognitionResult RecognizeFile(const std::string &wav_path) const;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace ai_audio_assistant

#endif  // AI_AUDIO_ASSISTANT_OFFLINE_ASR_HPP_
