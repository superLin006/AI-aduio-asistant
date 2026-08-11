#include <exception>
#include <iostream>
#include <string>
#include <utility>

#include "ai_audio_assistant/intent_recognizer.hpp"
#include "ai_audio_assistant/offline_asr.hpp"

namespace {

void Usage(const char* program) {
  std::cerr << "Usage: " << program
            << " BMODEL TOKENIZER_DIR AUDIO_WAV LLM_MODEL_DIR TOOLS_JSON EMBEDDING_DIR\n";
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 7) {
    Usage(argv[0]);
    return 2;
  }

  try {
    ai_audio_assistant::OfflineAsrConfig asr_config;
    asr_config.bmodel_path = argv[1];
    asr_config.tokenizer_dir = argv[2];
    ai_audio_assistant::OfflineAsr asr(std::move(asr_config));

    ai_audio_assistant::LocalIntentConfig intent_config;
    intent_config.model_dir = argv[4];
    intent_config.common.tools_json_path = argv[5];
    intent_config.common.embedding_model_dir = argv[6];
    auto intent = ai_audio_assistant::IntentRecognizer::CreateLocal(
        std::move(intent_config));

    const auto transcript = asr.RecognizeFile(argv[3]);
    const auto result = intent->Recognize(transcript.text);
    std::cout << "transcript=" << transcript.text << '\n'
              << "asr_ms=" << transcript.inference_seconds * 1000.0 << '\n'
              << "intent_ms=" << result.elapsed_milliseconds << '\n'
              << "semantic_plan=" << result.semantic_plan << '\n'
              << "final_json=" << result.final_json << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "unified demo failed: " << error.what() << '\n';
    return 1;
  }
}
