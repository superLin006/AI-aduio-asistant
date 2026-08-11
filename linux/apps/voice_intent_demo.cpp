#include <algorithm>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "ai_audio_assistant/assistant_pipeline.hpp"
#include "ai_audio_assistant/intent_recognizer.hpp"
#include "sherpa-onnx/c-api/cxx-api.h"

namespace {

std::string Value(int argc, char** argv, const std::string& name) {
  for (int i = 1; i + 1 < argc; ++i) {
    if (argv[i] == name) return argv[i + 1];
  }
  return {};
}

void Feed(ai_audio_assistant::AssistantPipeline& pipeline,
          const sherpa_onnx::cxx::Wave& wave, bool stop_after_wake = false) {
  constexpr size_t kChunk = 1600;
  for (size_t offset = 0; offset < wave.samples.size(); offset += kChunk) {
    const int32_t count = static_cast<int32_t>(
        std::min(kChunk, wave.samples.size() - offset));
    pipeline.FeedAudio(wave.samples.data() + offset, count, wave.sample_rate);
    if (stop_after_wake &&
        pipeline.state() != ai_audio_assistant::AssistantState::kListening) {
      return;
    }
  }
}

std::unique_ptr<ai_audio_assistant::IntentRecognizer> CreateIntent(
    int argc, char** argv) {
  ai_audio_assistant::IntentCommonConfig common;
  common.tools_json_path = Value(argc, argv, "--tools");
  common.embedding_model_dir = Value(argc, argv, "--embedding");
  const std::string backend = Value(argc, argv, "--intent-backend");

  if (backend == "local") {
    ai_audio_assistant::LocalIntentConfig config;
    config.common = std::move(common);
    config.model_dir = Value(argc, argv, "--llm-model");
    return ai_audio_assistant::IntentRecognizer::CreateLocal(std::move(config));
  }
  if (backend == "deepseek") {
    const char* api_key = std::getenv("DEEPSEEK_API_KEY");
    if (api_key == nullptr || api_key[0] == '\0') {
      throw std::runtime_error("DEEPSEEK_API_KEY is not set");
    }
    ai_audio_assistant::DeepSeekIntentConfig config;
    config.common = std::move(common);
    config.tokenizer_json_path = Value(argc, argv, "--llm-tokenizer");
    config.api_key = api_key;
    return ai_audio_assistant::IntentRecognizer::CreateDeepSeek(std::move(config));
  }
  throw std::invalid_argument("--intent-backend must be local or deepseek");
}

}  // namespace

int main(int argc, char** argv) {
  try {
    auto intent = CreateIntent(argc, argv);
    ai_audio_assistant::AssistantConfig config;
    const std::string kws = Value(argc, argv, "--kws-dir");
    config.keyword_spotter.encoder_path =
        kws + "/encoder-epoch-12-avg-2-chunk-16-left-64.int8.onnx";
    config.keyword_spotter.decoder_path =
        kws + "/decoder-epoch-12-avg-2-chunk-16-left-64.onnx";
    config.keyword_spotter.joiner_path =
        kws + "/joiner-epoch-12-avg-2-chunk-16-left-64.int8.onnx";
    config.keyword_spotter.tokens_path = kws + "/tokens.txt";
    config.keyword_spotter.keywords_path = kws + "/keywords.txt";
    config.vad.model_path = Value(argc, argv, "--vad");
    config.asr.bmodel_path = Value(argc, argv, "--asr-bmodel");
    config.asr.tokenizer_dir = Value(argc, argv, "--asr-tokenizer");
    config.asr.hotwords = Value(argc, argv, "--asr-hotwords");

    const auto wake = sherpa_onnx::cxx::ReadWave(Value(argc, argv, "--wake-audio"));
    const auto command =
        sherpa_onnx::cxx::ReadWave(Value(argc, argv, "--command-audio"));
    if (wake.samples.empty() || command.samples.empty()) {
      throw std::invalid_argument("invalid wake or command audio");
    }

    bool received_result = false;
    ai_audio_assistant::AssistantPipeline pipeline(
        std::move(config), [&](const ai_audio_assistant::AssistantEvent& event) {
          if (event.type == ai_audio_assistant::EventType::kWakeWordDetected) {
            std::cout << "wake_word=" << event.text << '\n';
          } else if (event.type == ai_audio_assistant::EventType::kTranscriptReady) {
            const auto result = intent->Recognize(event.text);
            std::cout << "transcript=" << event.text << '\n'
                      << "asr_ms=" << event.recognition.inference_seconds * 1000.0 << '\n'
                      << "intent_ms=" << result.elapsed_milliseconds << '\n'
                      << "semantic_plan=" << result.semantic_plan << '\n'
                      << "final_json=" << result.final_json << '\n';
            received_result = true;
          } else if (event.type == ai_audio_assistant::EventType::kError) {
            throw std::runtime_error(event.text);
          }
        });

    Feed(pipeline, wake, true);
    const float wake_guard[8000] = {};
    pipeline.FeedAudio(wake_guard, 8000);
    Feed(pipeline, command);
    const float silence[1600] = {};
    for (int i = 0; i < 12; ++i) pipeline.FeedAudio(silence, 1600);
    pipeline.Flush();
    if (!received_result) throw std::runtime_error("pipeline produced no intent result");
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "voice intent demo failed: " << error.what() << '\n';
    return 1;
  }
}
