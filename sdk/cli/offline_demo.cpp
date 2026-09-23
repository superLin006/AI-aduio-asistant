#include <exception>
#include <iomanip>
#include <iostream>
#include <string>

#include "ai_audio_assistant/offline_asr.hpp"

namespace {
void Usage(const char *program) {
  std::cerr << "Usage: " << program
            << " --bmodel FILE --tokenizer DIR --audio FILE [--hotwords TEXT]\n";
}
}  // namespace

int main(int argc, char **argv) {
  ai_audio_assistant::OfflineAsrConfig config;
  std::string audio_path;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (i + 1 >= argc) { Usage(argv[0]); return 2; }
    const std::string value = argv[++i];
    if (arg == "--bmodel") config.bmodel_path = value;
    else if (arg == "--tokenizer") config.tokenizer_dir = value;
    else if (arg == "--audio") audio_path = value;
    else if (arg == "--hotwords") config.hotwords = value;
    else { Usage(argv[0]); return 2; }
  }
  if (audio_path.empty()) { Usage(argv[0]); return 2; }

  try {
    ai_audio_assistant::OfflineAsr recognizer(std::move(config));
    const auto result = recognizer.RecognizeFile(audio_path);
    std::cout << result.text << '\n'
              << std::fixed << std::setprecision(3)
              << "[Timing] audio=" << result.audio_seconds * 1000.0
              << "ms infer=" << result.inference_seconds * 1000.0
              << "ms total=" << result.inference_seconds * 1000.0
              << "ms RTF=" << result.real_time_factor << '\n';
  } catch (const std::exception &e) {
    std::cerr << "error: " << e.what() << '\n';
    return 1;
  }
  return 0;
}
