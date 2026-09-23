#ifndef AI_AUDIO_ASSISTANT_SPEAKER_VERIFIER_HPP_
#define AI_AUDIO_ASSISTANT_SPEAKER_VERIFIER_HPP_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "ai_audio_assistant/offline_asr.hpp"

namespace ai_audio_assistant {

struct AIAUDIO_API SpeakerVerifierConfig {
  // Path to a sherpa-onnx speaker embedding model: an ONNX model (wespeaker,
  // 3d-speaker or nemo) for provider "cpu", or a .rknn model for provider
  // "rknn". Empty disables speaker recognition.
  std::string model_path;
  // Optional JSON file used by Save()/Load() to persist registered speakers.
  // It is loaded at construction when the file exists; a missing file starts
  // with an empty database; a file that exists but cannot be parsed makes the
  // constructor throw.
  std::string speakers_path;
  // Cosine similarity threshold used by Identify()/Verify().
  float threshold = 0.5F;
  // For provider "rknn" this is not used as a thread count; any value >= 1
  // selects automatic (multi-core) NPU execution.
  int32_t num_threads = 2;
  // sherpa-onnx execution provider, e.g. "cpu" or "rknn" (RK NPU).
  std::string provider = "cpu";

  void Validate() const;
};

struct AIAUDIO_API SpeakerMatch {
  std::string name;
  float score = 0.0F;
  bool matched = false;
};

// Thin adapter over sherpa_onnx::cxx::SpeakerVerifier, which owns the
// enrollment/verification implementation shared with the shipped SDK.
class AIAUDIO_API SpeakerVerifier final {
 public:
  explicit SpeakerVerifier(SpeakerVerifierConfig config);
  ~SpeakerVerifier();

  SpeakerVerifier(SpeakerVerifier &&) noexcept;
  SpeakerVerifier &operator=(SpeakerVerifier &&) noexcept;
  SpeakerVerifier(const SpeakerVerifier &) = delete;
  SpeakerVerifier &operator=(const SpeakerVerifier &) = delete;

  int32_t dim() const;
  int32_t num_speakers() const;
  bool Contains(const std::string &name) const;
  std::vector<std::string> Speakers() const;

  // Registers one utterance for the given speaker. Calling Register with the
  // same name again appends another enrollment embedding (the manager keeps
  // one averaged embedding per speaker), which improves Verify()/Identify()
  // robustness. Returns false on failure (e.g. audio too short to compute an
  // embedding).
  bool Register(const std::string &name, const float *samples,
                int32_t sample_count, int32_t sample_rate = 16000);
  bool RegisterFile(const std::string &name, const std::string &wav_path);

  bool Remove(const std::string &name);

  // 1:1 verification against a registered speaker.
  bool Verify(const std::string &name, const float *samples,
              int32_t sample_count, int32_t sample_rate = 16000);

  // 1:N identification; returns the best match above threshold, or an empty
  // SpeakerMatch when no registered speaker matches.
  SpeakerMatch Identify(const float *samples, int32_t sample_count,
                        int32_t sample_rate = 16000);

  // Same as Identify but unconditionally returns the top-1 match (always
  // returns the highest score even if below threshold).
  SpeakerMatch BestMatch(const float *samples, int32_t sample_count,
                         int32_t sample_rate = 16000);

  // Uses config.speakers_path.
  bool Save();
  bool Load();
  bool Save(const std::string &path);
  bool Load(const std::string &path);

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace ai_audio_assistant
#endif  // AI_AUDIO_ASSISTANT_SPEAKER_VERIFIER_HPP_
