#include "ai_audio_assistant/intent_recognizer.hpp"

#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <system_error>
#include <utility>

#include <sys/stat.h>

#include "IntentService.h"
#include "LLMService.h"
#include "json/json.hpp"

namespace ai_audio_assistant {
namespace {

void RequireFile(const std::string& path, const char* name) {
  if (path.empty() || !std::filesystem::is_regular_file(path)) {
    throw std::invalid_argument(std::string(name) + " is not a regular file: " + path);
  }
}

void RequireDirectory(const std::string& path, const char* name) {
  if (path.empty() || !std::filesystem::is_directory(path)) {
    throw std::invalid_argument(std::string(name) + " is not a directory: " + path);
  }
}

sherpa_llm::IntentConfig ToSdkConfig(const IntentCommonConfig& config) {
  sherpa_llm::IntentConfig result;
  result.toolsJsonPath = config.tools_json_path;
  result.embedThreshold = config.embedding_threshold;
  result.topK = config.top_k;
  result.hybridRecall = true;
  result.dynamicTopK = true;
  result.maxTopK = config.max_top_k;
  result.embedModelDir = config.embedding_model_dir;
  result.expectedDomain = config.expected_domain;
  return result;
}

class TemporaryConfigDirectory {
 public:
  explicit TemporaryConfigDirectory(const DeepSeekIntentConfig& config) {
    const char* runtime_dir = std::getenv("XDG_RUNTIME_DIR");
    std::filesystem::path parent =
        runtime_dir != nullptr && runtime_dir[0] != '\0' ? runtime_dir : "/tmp";
    std::string pattern = (parent / "ai-audio-intent-XXXXXX").string();
    std::vector<char> buffer(pattern.begin(), pattern.end());
    buffer.push_back('\0');
    char* created = ::mkdtemp(buffer.data());
    if (created == nullptr) {
      throw std::runtime_error("cannot create temporary DeepSeek config directory");
    }
    path_ = created;
    if (::chmod(path_.c_str(), S_IRWXU) != 0) {
      throw std::system_error(errno, std::generic_category(), "chmod temporary config directory");
    }

    nlohmann::json root;
    root["llm"]["backend_type"] = "openai";
    root["llm"]["tokenizer_json_path"] = config.tokenizer_json_path;
    auto& cloud = root["llm"]["openai_compatible"];
    cloud["base_url"] = config.base_url;
    cloud["model"] = config.model;
    cloud["api_key"] = config.api_key.rfind("Bearer ", 0) == 0
                           ? config.api_key
                           : "Bearer " + config.api_key;
    cloud["timeout_seconds"] = config.timeout_seconds;
    cloud["max_context_len"] = config.max_context_length;
    cloud["max_new_tokens"] = config.max_new_tokens;
    cloud["temperature"] = config.temperature;
    cloud["top_p"] = config.top_p;
    cloud["top_k"] = 0;
    cloud["frequency_penalty"] = 0.0;
    cloud["presence_penalty"] = 0.0;
    cloud["skip_special_token"] = true;
    root["output"]["language"] = "chinese";

    const auto file = path_ / "config.json";
    std::ofstream output(file, std::ios::out | std::ios::trunc);
    output.exceptions(std::ios::failbit | std::ios::badbit);
    output << root.dump(2);
    output.close();
    if (::chmod(file.c_str(), S_IRUSR | S_IWUSR) != 0) {
      throw std::system_error(errno, std::generic_category(), "chmod temporary config file");
    }
  }

  ~TemporaryConfigDirectory() { Remove(); }
  TemporaryConfigDirectory(const TemporaryConfigDirectory&) = delete;
  TemporaryConfigDirectory& operator=(const TemporaryConfigDirectory&) = delete;

  const std::filesystem::path& path() const noexcept { return path_; }

 private:
  void Remove() noexcept {
    std::error_code error;
    std::filesystem::remove_all(path_, error);
  }
  std::filesystem::path path_;
};

std::shared_ptr<sherpa_llm::LLMService> InitLocalLlm(const LocalIntentConfig& config) {
  auto unique = sherpa_llm::CreateLLMService();
  std::shared_ptr<sherpa_llm::LLMService> llm(std::move(unique));
  if (!llm || llm->Init(config.model_dir) != 0) {
    throw std::runtime_error("failed to initialize local Qwen intent model: " + config.model_dir);
  }
  return llm;
}

std::shared_ptr<sherpa_llm::LLMService> InitDeepSeekLlm(
    const DeepSeekIntentConfig& config) {
  TemporaryConfigDirectory temporary(config);
  auto unique = sherpa_llm::CreateLLMService();
  std::shared_ptr<sherpa_llm::LLMService> llm(std::move(unique));
  if (!llm || llm->Init(temporary.path().string()) != 0) {
    throw std::runtime_error("failed to initialize DeepSeek intent backend");
  }
  return llm;
}

}  // namespace

void IntentCommonConfig::Validate() const {
  RequireFile(tools_json_path, "tools_json_path");
  RequireDirectory(embedding_model_dir, "embedding_model_dir");
  if (expected_domain.empty()) throw std::invalid_argument("expected_domain must not be empty");
  if (embedding_threshold < 0.0F || embedding_threshold > 1.0F)
    throw std::invalid_argument("embedding_threshold must be in [0, 1]");
  if (top_k <= 0 || max_top_k < top_k)
    throw std::invalid_argument("require 0 < top_k <= max_top_k");
}

void LocalIntentConfig::Validate() const {
  common.Validate();
  RequireDirectory(model_dir, "model_dir");
  RequireFile((std::filesystem::path(model_dir) / "config.json").string(), "local config.json");
}

void DeepSeekIntentConfig::Validate() const {
  common.Validate();
  RequireFile(tokenizer_json_path, "tokenizer_json_path");
  if (api_key.empty()) throw std::invalid_argument("DeepSeek api_key must not be empty");
  if (base_url.rfind("https://", 0) != 0)
    throw std::invalid_argument("DeepSeek base_url must use HTTPS");
  if (model.empty()) throw std::invalid_argument("DeepSeek model must not be empty");
  if (timeout_seconds <= 0 || max_context_length <= 0 || max_new_tokens <= 0)
    throw std::invalid_argument("DeepSeek numeric limits must be positive");
}

class IntentRecognizer::Impl {
 public:
  Impl(IntentBackend backend, const IntentCommonConfig& config,
       std::shared_ptr<sherpa_llm::LLMService> llm)
      : backend_(backend), llm_(std::move(llm)), service_(sherpa_llm::CreateIntentService(llm_)) {
    if (!service_ || service_->Init(ToSdkConfig(config)) != 0) {
      throw std::runtime_error("failed to initialize dispatch intent service");
    }
  }

  IntentResult Recognize(const std::string& text) const {
    if (text.empty()) throw std::invalid_argument("intent input text must not be empty");
    sherpa_llm::RecognizeTrace trace;
    IntentResult result;
    result.backend = backend_;
    result.input_text = text;
    const auto started = std::chrono::steady_clock::now();
    result.final_json = service_->RecognizeWithTrace(text, trace);
    const auto finished = std::chrono::steady_clock::now();
    result.embedding_blocked = trace.embedBlocked;
    result.raw_model_output = trace.llmOut;
    result.semantic_plan = trace.semanticPlan;
    result.elapsed_milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                                      finished - started)
                                      .count();
    result.candidates.reserve(trace.embedCands.size());
    for (const auto& candidate : trace.embedCands) {
      result.candidates.push_back({candidate.name, candidate.score});
    }
    return result;
  }

  IntentBackend backend() const noexcept { return backend_; }

 private:
  IntentBackend backend_;
  std::shared_ptr<sherpa_llm::LLMService> llm_;
  std::unique_ptr<sherpa_llm::IntentService> service_;
};

IntentRecognizer::IntentRecognizer(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
IntentRecognizer::~IntentRecognizer() = default;
IntentRecognizer::IntentRecognizer(IntentRecognizer&&) noexcept = default;
IntentRecognizer& IntentRecognizer::operator=(IntentRecognizer&&) noexcept = default;

std::unique_ptr<IntentRecognizer> IntentRecognizer::CreateLocal(LocalIntentConfig config) {
  config.Validate();
  auto llm = InitLocalLlm(config);
  return std::unique_ptr<IntentRecognizer>(new IntentRecognizer(
      std::make_unique<Impl>(IntentBackend::kLocal, config.common, std::move(llm))));
}

std::unique_ptr<IntentRecognizer> IntentRecognizer::CreateDeepSeek(DeepSeekIntentConfig config) {
  config.Validate();
  auto llm = InitDeepSeekLlm(config);
  return std::unique_ptr<IntentRecognizer>(new IntentRecognizer(
      std::make_unique<Impl>(IntentBackend::kDeepSeek, config.common, std::move(llm))));
}

IntentBackend IntentRecognizer::backend() const noexcept { return impl_->backend(); }
IntentResult IntentRecognizer::Recognize(const std::string& text) const {
  return impl_->Recognize(text);
}

}  // namespace ai_audio_assistant
