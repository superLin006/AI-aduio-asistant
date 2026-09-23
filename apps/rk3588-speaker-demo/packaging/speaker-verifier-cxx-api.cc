// cxx-api-examples/speaker-verifier-cxx-api.cc
//
// This example demonstrates direct use of SpeakerVerifier from cxx-api.h.
// Place the internally converted RKNN model in the working directory as
// eres2netv2_T300_fp.rknn. The model is not stored in this repository.
// Test audio is from https://github.com/csukuangfj/sr-data.

#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "sherpa-onnx/c-api/cxx-api.h"

int32_t main() {
  using namespace sherpa_onnx::cxx;  // NOLINT

  SpeakerVerifierConfig config;
  config.model = "./models/eres2netv2_T300_fp.rknn";
  config.provider = "rknn";
  config.threshold = 0.5F;
  config.speakers_path = "./speakers.json";

  std::unique_ptr<SpeakerVerifier> verifier;
  try {
    verifier = std::make_unique<SpeakerVerifier>(config);
  } catch (const std::exception &e) {
    // The constructor throws when the configuration is invalid or when an
    // existing speakers_path cannot be loaded.
    std::cerr << "Failed to create speaker verifier: " << e.what() << '\n';
    return -1;
  }

  const std::vector<std::pair<std::string, std::string>> enrollments = {
      {"fangjun", "./wavs/fangjun-sr-1.wav"},
      {"fangjun", "./wavs/fangjun-sr-2.wav"},
      {"leijun", "./wavs/leijun-sr-1.wav"},
      {"leijun", "./wavs/leijun-sr-2.wav"},
  };

  for (const auto &enrollment : enrollments) {
    if (!verifier->RegisterFile(enrollment.first, enrollment.second)) {
      std::cerr << "Failed to enroll " << enrollment.first << " from "
                << enrollment.second << '\n';
      return -1;
    }
  }

  const std::string query_file = "./wavs/fangjun-test-sr-1.wav";
  const Wave query = ReadWave(query_file);
  if (query.samples.empty()) {
    std::cerr << "Failed to read " << query_file << '\n';
    return -1;
  }

  const auto match = verifier->Identify(
      query.samples.data(), static_cast<int32_t>(query.samples.size()),
      query.sample_rate);
  std::cout << "identified=" << (match.matched ? match.name : "unknown")
            << " score=" << match.score << '\n';

  const bool verified = verifier->Verify(
      "fangjun", query.samples.data(),
      static_cast<int32_t>(query.samples.size()), query.sample_rate);
  std::cout << "verified as fangjun=" << (verified ? "yes" : "no") << '\n';

  if (!verifier->Save()) {
    std::cerr << "Failed to save speaker embeddings to "
              << config.speakers_path << '\n';
    return -1;
  }
  std::cout << "registered speakers=" << verifier->NumSpeakers() << '\n';
  return 0;
}
