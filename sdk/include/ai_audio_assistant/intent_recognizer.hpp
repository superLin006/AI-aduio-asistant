#ifndef AI_AUDIO_ASSISTANT_INTENT_RECOGNIZER_HPP_
#define AI_AUDIO_ASSISTANT_INTENT_RECOGNIZER_HPP_

#include <memory>
#include <string>
#include <vector>

#include "ai_audio_assistant/offline_asr.hpp"

namespace ai_audio_assistant {

enum class IntentBackend { kLocal, kDeepSeek };

struct AIAUDIO_API IntentCommonConfig {
  std::string tools_json_path;
  std::string embedding_model_dir;
  std::string expected_domain = "dispatch";
  float embedding_threshold = 0.5F;
  int top_k = 3;
  int max_top_k = 4;

  void Validate() const;
};

struct AIAUDIO_API LocalIntentConfig {
  IntentCommonConfig common;
  std::string model_dir;

  void Validate() const;
};

struct AIAUDIO_API DeepSeekIntentConfig {
  IntentCommonConfig common;
  std::string tokenizer_json_path;
  std::string api_key;
  std::string base_url = "https://api.deepseek.com/v1/chat/completions";
  std::string model = "deepseek-chat";
  int timeout_seconds = 60;
  int max_context_length = 8192;
  int max_new_tokens = 512;
  float temperature = 0.1F;
  float top_p = 0.9F;

  void Validate() const;
};

struct AIAUDIO_API IntentCandidate {
  std::string name;
  float score = 0.0F;
};

struct AIAUDIO_API IntentResult {
  IntentBackend backend = IntentBackend::kLocal;
  std::string input_text;
  std::vector<IntentCandidate> candidates;
  bool embedding_blocked = false;
  std::string raw_model_output;
  std::string semantic_plan;
  std::string final_json;
  long elapsed_milliseconds = 0;
};

class AIAUDIO_API IntentRecognizer final {
 public:
  static std::unique_ptr<IntentRecognizer> CreateLocal(LocalIntentConfig config);
  static std::unique_ptr<IntentRecognizer> CreateDeepSeek(DeepSeekIntentConfig config);

  ~IntentRecognizer();
  IntentRecognizer(IntentRecognizer&&) noexcept;
  IntentRecognizer& operator=(IntentRecognizer&&) noexcept;
  IntentRecognizer(const IntentRecognizer&) = delete;
  IntentRecognizer& operator=(const IntentRecognizer&) = delete;

  IntentBackend backend() const noexcept;
  IntentResult Recognize(const std::string& text) const;

 private:
  class Impl;
  explicit IntentRecognizer(std::unique_ptr<Impl> impl);
  std::unique_ptr<Impl> impl_;
};

}  // namespace ai_audio_assistant

#endif  // AI_AUDIO_ASSISTANT_INTENT_RECOGNIZER_HPP_
