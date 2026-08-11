#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "ai_audio_assistant/intent_recognizer.hpp"
#include "ai_audio_assistant/offline_asr.hpp"
#include "json/json.hpp"

int main(int argc, char** argv) {
  if (argc != 8 && argc != 9) {
    std::cerr << "Usage: " << argv[0]
              << " BMODEL TOKENIZER_DIR MANIFEST_TSV AUDIO_DIR LLM_MODEL_DIR"
                 " TOOLS_JSON EMBEDDING_DIR [HOTWORDS_FILE]\n";
    return 2;
  }

  try {
    ai_audio_assistant::OfflineAsrConfig asr_config;
    asr_config.bmodel_path = argv[1];
    asr_config.tokenizer_dir = argv[2];
    if (argc == 9) {
      std::ifstream hotwords(argv[8]);
      if (!hotwords) throw std::runtime_error("cannot open hotwords file");
      asr_config.hotwords.assign(std::istreambuf_iterator<char>(hotwords),
                                 std::istreambuf_iterator<char>());
    }
    ai_audio_assistant::OfflineAsr asr(std::move(asr_config));

    ai_audio_assistant::LocalIntentConfig intent_config;
    intent_config.model_dir = argv[5];
    intent_config.common.tools_json_path = argv[6];
    intent_config.common.embedding_model_dir = argv[7];
    auto intent = ai_audio_assistant::IntentRecognizer::CreateLocal(
        std::move(intent_config));

    std::ifstream manifest(argv[3]);
    if (!manifest) throw std::runtime_error("cannot open manifest");
    const std::filesystem::path audio_dir = argv[4];
    std::string line;
    int total = 0;
    int errors = 0;
    while (std::getline(manifest, line)) {
      if (!line.empty() && line.back() == '\r') line.pop_back();
      const auto first = line.find('\t');
      const auto second = first == std::string::npos ? first : line.find('\t', first + 1);
      if (first == std::string::npos || second == std::string::npos ||
          line.substr(0, first) == "id") {
        continue;
      }
      const std::string id = line.substr(0, first);
      const std::string wav = line.substr(first + 1, second - first - 1);
      const std::string source = line.substr(second + 1);
      nlohmann::json row;
      row["id"] = id;
      row["wav"] = wav;
      row["source_text"] = source;
      try {
        const auto transcript = asr.RecognizeFile((audio_dir / wav).string());
        const auto result = intent->Recognize(transcript.text);
        const auto source_result = intent->Recognize(source);
        const auto asr_plan = nlohmann::json::parse(result.semantic_plan);
        const auto source_plan = nlohmann::json::parse(source_result.semantic_plan);
        row["asr_text"] = transcript.text;
        row["asr_ms"] = transcript.inference_seconds * 1000.0;
        row["asr_rtf"] = transcript.real_time_factor;
        row["intent_ms"] = result.elapsed_milliseconds;
        row["semantic_plan"] = asr_plan;
        row["source_semantic_plan"] = source_plan;
        row["plan_match"] = asr_plan == source_plan;
        row["final_result"] = nlohmann::json::parse(result.final_json);
        row["ok"] = true;
      } catch (const std::exception& error) {
        row["ok"] = false;
        row["error"] = error.what();
        ++errors;
      }
      ++total;
      std::cout << "JSONL: " << row.dump() << '\n';
    }
    std::cerr << "batch regression complete: total=" << total
              << " errors=" << errors << '\n';
    return total > 0 && errors == 0 ? 0 : 1;
  } catch (const std::exception& error) {
    std::cerr << "batch regression failed: " << error.what() << '\n';
    return 1;
  }
}
