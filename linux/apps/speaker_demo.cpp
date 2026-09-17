#include <cstdlib>
#include <exception>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "ai_audio_assistant/speaker_verifier.hpp"
#include "sherpa-onnx/c-api/cxx-api.h"

namespace {

std::string Value(int argc, char **argv, const std::string &name) {
  for (int i = 1; i + 1 < argc; ++i) {
    if (argv[i] == name) return argv[i + 1];
  }
  return {};
}

std::vector<std::string> Values(int argc, char **argv,
                                const std::string &name) {
  std::vector<std::string> values;
  for (int i = 1; i + 1 < argc; ++i) {
    if (argv[i] == name) values.emplace_back(argv[i + 1]);
  }
  return values;
}

std::pair<std::string, std::string> SplitNameWav(const std::string &arg) {
  const auto pos = arg.find('=');
  if (pos == std::string::npos || pos == 0 || pos + 1 >= arg.size()) {
    throw std::invalid_argument("expected name=wav, given: " + arg);
  }
  return {arg.substr(0, pos), arg.substr(pos + 1)};
}

}  // namespace

int main(int argc, char **argv) {
  const std::string model = Value(argc, argv, "--speaker-model");
  const std::string threshold_str = Value(argc, argv, "--threshold");
  const std::string provider = Value(argc, argv, "--speaker-provider");
  const auto register_args = Values(argc, argv, "--register");
  const auto verify_args = Values(argc, argv, "--verify");
  const auto identify_args = Values(argc, argv, "--identify");
  const auto best_match_args = Values(argc, argv, "--best-match");
  const std::string save_path = Value(argc, argv, "--save");
  const std::string load_path = Value(argc, argv, "--load");

  if (model.empty()) {
    std::cerr << "--speaker-model is required\n";
    return 2;
  }
  if (register_args.empty() && verify_args.empty() &&
      identify_args.empty() && best_match_args.empty()) {
    std::cerr << "need at least one of --register, --verify, --identify, --best-match\n";
    return 2;
  }

  try {
    ai_audio_assistant::SpeakerVerifierConfig config;
    config.model_path = model;
    config.speakers_path = load_path;
    if (!threshold_str.empty()) config.threshold = std::stof(threshold_str);
    if (!provider.empty()) config.provider = provider;

    ai_audio_assistant::SpeakerVerifier verifier(std::move(config));

    for (const auto &arg : register_args) {
      const auto [name, wav] = SplitNameWav(arg);
      const bool ok = verifier.RegisterFile(name, wav);
      std::cout << "register=" << name << ':' << wav << " -> ok=" << ok << '\n';
    }

    for (const auto &arg : verify_args) {
      const auto [name, wav] = SplitNameWav(arg);
      const auto wave = sherpa_onnx::cxx::ReadWave(wav);
      if (wave.samples.empty()) {
        std::cerr << "failed to read WAV: " << wav << '\n';
        return 1;
      }
      const bool matched = verifier.Verify(
          name, wave.samples.data(), static_cast<int32_t>(wave.samples.size()),
          wave.sample_rate);
      std::cout << "verify=" << name << ':' << wav << " -> matched=" << matched
                << '\n';
    }

    for (const auto &wav : identify_args) {
      const auto wave = sherpa_onnx::cxx::ReadWave(wav);
      if (wave.samples.empty()) {
        std::cerr << "failed to read WAV: " << wav << '\n';
        return 1;
      }
      const auto match = verifier.Identify(
          wave.samples.data(), static_cast<int32_t>(wave.samples.size()),
          wave.sample_rate);
      std::cout << "identify=" << wav << " -> name=" << match.name
                << " score=" << match.score << " matched=" << match.matched
                << '\n';
    }

    for (const auto &wav : best_match_args) {
      const auto wave = sherpa_onnx::cxx::ReadWave(wav);
      if (wave.samples.empty()) {
        std::cerr << "failed to read WAV: " << wav << '\n';
        return 1;
      }
      const auto match = verifier.BestMatch(
          wave.samples.data(), static_cast<int32_t>(wave.samples.size()),
          wave.sample_rate);
      std::cout << "best=" << wav << " -> name=" << match.name
                << " score=" << match.score << " matched=" << match.matched
                << '\n';
    }

    if (!save_path.empty()) {
      const bool ok = verifier.Save(save_path);
      std::cout << "save=" << save_path << " -> ok=" << ok << '\n';
    }
    std::cout << "speakers=" << verifier.num_speakers() << '\n';
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "speaker demo failed: " << error.what() << '\n';
    return 1;
  }
}
