#include <cassert>
#include <stdexcept>

#include "ai_audio_assistant/speaker_verifier.hpp"

int main() {
  ai_audio_assistant::SpeakerVerifierConfig config;
  bool rejected = false;
  try { config.Validate(); } catch (const std::invalid_argument &) { rejected = true; }
  assert(rejected);

  config.model_path = "/nonexistent/model.onnx";
  rejected = false;
  try { config.Validate(); } catch (const std::invalid_argument &) { rejected = true; }
  assert(rejected);

  config.threshold = 1.5F;
  rejected = false;
  try { config.Validate(); } catch (const std::invalid_argument &) { rejected = true; }
  assert(rejected);

  config.threshold = 0.5F;
  config.num_threads = 0;
  rejected = false;
  try { config.Validate(); } catch (const std::invalid_argument &) { rejected = true; }
  assert(rejected);
  return 0;
}
