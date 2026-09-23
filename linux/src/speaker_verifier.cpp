#include "ai_audio_assistant/speaker_verifier.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "sherpa-onnx/c-api/cxx-api.h"

// The enrollment/verification logic lives in sherpa-onnx's
// sherpa_onnx::cxx::SpeakerVerifier so the app and the shipped SDK share a
// single implementation; this file only adapts config and return types.
namespace ai_audio_assistant {

namespace {

sherpa_onnx::cxx::SpeakerVerifierConfig ToSherpaConfig(
    const SpeakerVerifierConfig &config) {
  sherpa_onnx::cxx::SpeakerVerifierConfig ans;
  ans.model = config.model_path;
  ans.speakers_path = config.speakers_path;
  ans.threshold = config.threshold;
  ans.num_threads = config.num_threads;
  ans.provider = config.provider;
  return ans;
}

SpeakerMatch ToSpeakerMatch(
    const sherpa_onnx::cxx::SpeakerVerificationMatch &match) {
  SpeakerMatch ans;
  ans.name = match.name;
  ans.score = match.score;
  ans.matched = match.matched;
  return ans;
}

}  // namespace

void SpeakerVerifierConfig::Validate() const {
  ToSherpaConfig(*this).Validate();
}

class SpeakerVerifier::Impl {
 public:
  explicit Impl(const SpeakerVerifierConfig &config)
      : verifier_(ToSherpaConfig(config)) {}

  int32_t dim() const { return verifier_.Dim(); }
  int32_t num_speakers() const { return verifier_.NumSpeakers(); }
  bool Contains(const std::string &name) const {
    return verifier_.Contains(name);
  }
  std::vector<std::string> Speakers() const {
    return verifier_.GetAllSpeakers();
  }

  bool Register(const std::string &name, const float *samples,
                int32_t sample_count, int32_t sample_rate) {
    return verifier_.Register(name, samples, sample_count, sample_rate);
  }
  bool RegisterFile(const std::string &name, const std::string &wav_path) {
    return verifier_.RegisterFile(name, wav_path);
  }
  bool Remove(const std::string &name) { return verifier_.Remove(name); }
  bool Verify(const std::string &name, const float *samples,
              int32_t sample_count, int32_t sample_rate) {
    return verifier_.Verify(name, samples, sample_count, sample_rate);
  }
  SpeakerMatch Identify(const float *samples, int32_t sample_count,
                        int32_t sample_rate) {
    return ToSpeakerMatch(
        verifier_.Identify(samples, sample_count, sample_rate));
  }
  SpeakerMatch BestMatch(const float *samples, int32_t sample_count,
                         int32_t sample_rate) {
    return ToSpeakerMatch(
        verifier_.BestMatch(samples, sample_count, sample_rate));
  }
  bool Save() { return verifier_.Save(); }
  bool Load() { return verifier_.Load(); }
  bool Save(const std::string &path) { return verifier_.Save(path); }
  bool Load(const std::string &path) { return verifier_.Load(path); }

 private:
  sherpa_onnx::cxx::SpeakerVerifier verifier_;
};

SpeakerVerifier::SpeakerVerifier(SpeakerVerifierConfig config)
    : impl_(std::make_unique<Impl>(config)) {}
SpeakerVerifier::~SpeakerVerifier() = default;
SpeakerVerifier::SpeakerVerifier(SpeakerVerifier &&) noexcept = default;
SpeakerVerifier &SpeakerVerifier::operator=(SpeakerVerifier &&) noexcept =
    default;

int32_t SpeakerVerifier::dim() const { return impl_->dim(); }
int32_t SpeakerVerifier::num_speakers() const { return impl_->num_speakers(); }
bool SpeakerVerifier::Contains(const std::string &name) const {
  return impl_->Contains(name);
}
std::vector<std::string> SpeakerVerifier::Speakers() const {
  return impl_->Speakers();
}
bool SpeakerVerifier::Register(const std::string &name, const float *samples,
                               int32_t sample_count, int32_t sample_rate) {
  return impl_->Register(name, samples, sample_count, sample_rate);
}
bool SpeakerVerifier::RegisterFile(const std::string &name,
                                   const std::string &wav_path) {
  return impl_->RegisterFile(name, wav_path);
}
bool SpeakerVerifier::Remove(const std::string &name) {
  return impl_->Remove(name);
}
bool SpeakerVerifier::Verify(const std::string &name, const float *samples,
                             int32_t sample_count, int32_t sample_rate) {
  return impl_->Verify(name, samples, sample_count, sample_rate);
}
SpeakerMatch SpeakerVerifier::Identify(const float *samples,
                                       int32_t sample_count,
                                       int32_t sample_rate) {
  return impl_->Identify(samples, sample_count, sample_rate);
}
SpeakerMatch SpeakerVerifier::BestMatch(const float *samples,
                                        int32_t sample_count,
                                        int32_t sample_rate) {
  return impl_->BestMatch(samples, sample_count, sample_rate);
}
bool SpeakerVerifier::Save() { return impl_->Save(); }
bool SpeakerVerifier::Load() { return impl_->Load(); }
bool SpeakerVerifier::Save(const std::string &path) {
  return impl_->Save(path);
}
bool SpeakerVerifier::Load(const std::string &path) {
  return impl_->Load(path);
}

}  // namespace ai_audio_assistant
