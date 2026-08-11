#include <cassert>
#include <stdexcept>

#include "ai_audio_assistant/offline_asr.hpp"

int main() {
  ai_audio_assistant::OfflineAsrConfig config;
  bool rejected = false;
  try { config.Validate(); } catch (const std::invalid_argument &) { rejected = true; }
  assert(rejected);

  config.max_audio_seconds = 31;
  rejected = false;
  try { config.Validate(); } catch (const std::invalid_argument &) { rejected = true; }
  assert(rejected);
  return 0;
}
