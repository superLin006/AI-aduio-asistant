#include <algorithm>
#include <exception>
#include <iostream>
#include <string>

#include "ai_audio_assistant/assistant_pipeline.hpp"
#include "sherpa-onnx/c-api/cxx-api.h"

namespace {
std::string Value(int argc, char **argv, const std::string &name) {
  for (int i = 1; i + 1 < argc; ++i)
    if (argv[i] == name) return argv[i + 1];
  return {};
}
void Feed(ai_audio_assistant::AssistantPipeline &pipeline,
          const sherpa_onnx::cxx::Wave &wave, bool stop_after_wake = false) {
  constexpr size_t kChunk = 1600;
  for (size_t offset = 0; offset < wave.samples.size(); offset += kChunk) {
    const auto n = static_cast<int32_t>(
        std::min(kChunk, wave.samples.size() - offset));
    pipeline.FeedAudio(wave.samples.data() + offset, n, wave.sample_rate);
    if (stop_after_wake &&
        pipeline.state() != ai_audio_assistant::AssistantState::kListening)
      return;
  }
}
}  // namespace

int main(int argc, char **argv) {
  ai_audio_assistant::AssistantConfig config;
  const std::string kws = Value(argc, argv, "--kws-dir");
  const std::string stem = "epoch-12-avg-2-chunk-16-left-64.int8.onnx";
  config.keyword_spotter.encoder_path = kws + "/encoder-" + stem;
  config.keyword_spotter.decoder_path =
      kws + "/decoder-epoch-12-avg-2-chunk-16-left-64.onnx";
  config.keyword_spotter.joiner_path = kws + "/joiner-" + stem;
  config.keyword_spotter.tokens_path = kws + "/tokens.txt";
  config.keyword_spotter.keywords_path = kws + "/keywords.txt";
  config.vad.model_path = Value(argc, argv, "--vad");
  config.asr.bmodel_path = Value(argc, argv, "--bmodel");
  config.asr.tokenizer_dir = Value(argc, argv, "--tokenizer");

  const auto wake = sherpa_onnx::cxx::ReadWave(Value(argc, argv, "--wake-audio"));
  const auto command = sherpa_onnx::cxx::ReadWave(Value(argc, argv, "--command-audio"));
  if (wake.samples.empty() || command.samples.empty()) {
    std::cerr << "Invalid --wake-audio or --command-audio\n";
    return 2;
  }
  try {
    ai_audio_assistant::AssistantPipeline pipeline(
        std::move(config), [](const ai_audio_assistant::AssistantEvent &event) {
          std::cout << "[Event] type=" << static_cast<int>(event.type)
                    << " text=" << event.text << '\n';
          if (event.type == ai_audio_assistant::EventType::kTranscriptReady)
            std::cout << "[Timing] RTF=" << event.recognition.real_time_factor << '\n';
        });
    Feed(pipeline, wake, true);
    const float wake_tail[8000] = {};
    pipeline.FeedAudio(wake_tail, 8000);
    Feed(pipeline, command);
    const float silence[1600] = {};
    for (int i = 0; i < 12; ++i) pipeline.FeedAudio(silence, 1600);
    pipeline.Flush();
  } catch (const std::exception &e) {
    std::cerr << "error: " << e.what() << '\n';
    return 1;
  }
  return 0;
}
