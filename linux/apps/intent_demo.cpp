#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <string>

#include "ai_audio_assistant/intent_recognizer.hpp"

namespace {

void Usage(const char* program) {
  std::cerr << "Usage:\n  " << program
            << " local MODEL_DIR TOOLS_JSON EMBEDDING_DIR TEXT\n  " << program
            << " deepseek TOKENIZER_JSON TOOLS_JSON EMBEDDING_DIR TEXT\n\n"
               "DeepSeek mode reads its credential from DEEPSEEK_API_KEY.\n";
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 6) {
    Usage(argv[0]);
    return 2;
  }

  try {
    ai_audio_assistant::IntentCommonConfig common;
    common.tools_json_path = argv[3];
    common.embedding_model_dir = argv[4];
    std::unique_ptr<ai_audio_assistant::IntentRecognizer> recognizer;

    const std::string backend = argv[1];
    if (backend == "local") {
      ai_audio_assistant::LocalIntentConfig config;
      config.common = common;
      config.model_dir = argv[2];
      recognizer = ai_audio_assistant::IntentRecognizer::CreateLocal(std::move(config));
    } else if (backend == "deepseek") {
      const char* api_key = std::getenv("DEEPSEEK_API_KEY");
      if (api_key == nullptr || api_key[0] == '\0') {
        throw std::runtime_error("DEEPSEEK_API_KEY is not set");
      }
      ai_audio_assistant::DeepSeekIntentConfig config;
      config.common = common;
      config.tokenizer_json_path = argv[2];
      config.api_key = api_key;
      recognizer = ai_audio_assistant::IntentRecognizer::CreateDeepSeek(std::move(config));
    } else {
      Usage(argv[0]);
      return 2;
    }

    const auto result = recognizer->Recognize(argv[5]);
    std::cout << "backend=" << backend << '\n'
              << "elapsed_ms=" << result.elapsed_milliseconds << '\n'
              << "embedding_blocked=" << (result.embedding_blocked ? "true" : "false")
              << '\n';
    for (const auto& candidate : result.candidates) {
      std::cout << "candidate=" << candidate.name << ", score=" << candidate.score << '\n';
    }
    std::cout << "semantic_plan=" << result.semantic_plan << '\n'
              << "final_json=" << result.final_json << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "intent demo failed: " << error.what() << '\n';
    return 1;
  }
}
