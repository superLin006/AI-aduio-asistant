#include "ai_audio_assistant/speaker_verifier.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <utility>

#include "json/json.hpp"
#include "sherpa-onnx/c-api/cxx-api.h"

namespace ai_audio_assistant {
namespace {

void RequireFile(const std::string &path, const char *name) {
  if (path.empty() || !std::filesystem::is_regular_file(path)) {
    throw std::invalid_argument(std::string(name) + " does not exist: " + path);
  }
}

}  // namespace

void SpeakerVerifierConfig::Validate() const {
  RequireFile(model_path, "speaker embedding model");
  if (threshold < 0.0F || threshold > 1.0F) {
    throw std::invalid_argument("speaker threshold must be in [0, 1]");
  }
  if (num_threads <= 0) throw std::invalid_argument("num_threads must be positive");
}

class SpeakerVerifier::Impl {
 public:
  explicit Impl(SpeakerVerifierConfig config) : config_(std::move(config)) {
    config_.Validate();
    SherpaOnnxSpeakerEmbeddingExtractorConfig extractor_config;
    extractor_config.model = config_.model_path.c_str();
    extractor_config.num_threads = config_.num_threads;
    extractor_config.debug = 0;
    extractor_config.provider = config_.provider.c_str();
    extractor_ = SherpaOnnxCreateSpeakerEmbeddingExtractor(&extractor_config);
    if (extractor_ == nullptr) {
      throw std::runtime_error("failed to create speaker embedding extractor: " +
                               config_.model_path);
    }
    dim_ = SherpaOnnxSpeakerEmbeddingExtractorDim(extractor_);
    manager_ = SherpaOnnxCreateSpeakerEmbeddingManager(dim_);
    if (manager_ == nullptr) {
      throw std::runtime_error("failed to create speaker embedding manager");
    }
    if (!config_.speakers_path.empty()) Load(config_.speakers_path);
  }

  ~Impl() {
    if (manager_ != nullptr) SherpaOnnxDestroySpeakerEmbeddingManager(manager_);
    if (extractor_ != nullptr) SherpaOnnxDestroySpeakerEmbeddingExtractor(extractor_);
  }

  int32_t dim() const { return dim_; }
  const std::string &speakers_path() const { return config_.speakers_path; }
  int32_t num_speakers() const {
    return SherpaOnnxSpeakerEmbeddingManagerNumSpeakers(manager_);
  }
  bool Contains(const std::string &name) const {
    return SherpaOnnxSpeakerEmbeddingManagerContains(manager_, name.c_str()) == 1;
  }
  std::vector<std::string> Speakers() const {
    std::vector<std::string> names;
    names.reserve(embeddings_.size());
    for (const auto &entry : embeddings_) names.push_back(entry.first);
    return names;
  }

  bool Register(const std::string &name, const float *samples,
                int32_t sample_count, int32_t sample_rate) {
    if (name.empty() || samples == nullptr || sample_count <= 0) return false;
    std::vector<float> embedding;
    if (!ComputeEmbedding(samples, sample_count, sample_rate, &embedding)) return false;
    embeddings_[name].push_back(std::move(embedding));
    if (!SyncManager(name, embeddings_[name])) {
      embeddings_[name].pop_back();
      return false;
    }
    return true;
  }

  bool RegisterFile(const std::string &name, const std::string &wav_path) {
    if (!std::filesystem::is_regular_file(wav_path)) return false;
    const auto wave = sherpa_onnx::cxx::ReadWave(wav_path);
    if (wave.samples.empty()) return false;
    return Register(name, wave.samples.data(),
                    static_cast<int32_t>(wave.samples.size()), wave.sample_rate);
  }

  bool Remove(const std::string &name) {
    const bool removed = SherpaOnnxSpeakerEmbeddingManagerRemove(manager_, name.c_str()) == 1;
    if (removed) embeddings_.erase(name);
    return removed;
  }

  bool Verify(const std::string &name, const float *samples, int32_t sample_count,
              int32_t sample_rate) {
    if (samples == nullptr || sample_count <= 0) return false;
    std::vector<float> embedding;
    if (!ComputeEmbedding(samples, sample_count, sample_rate, &embedding)) return false;
    return SherpaOnnxSpeakerEmbeddingManagerVerify(
               manager_, name.c_str(), embedding.data(), config_.threshold) == 1;
  }

  SpeakerMatch Identify(const float *samples, int32_t sample_count,
                        int32_t sample_rate) {
    SpeakerMatch match;
    if (samples == nullptr || sample_count <= 0) return match;
    std::vector<float> embedding;
    if (!ComputeEmbedding(samples, sample_count, sample_rate, &embedding)) {
      return match;
    }
    const auto *result = SherpaOnnxSpeakerEmbeddingManagerGetBestMatches(
        manager_, embedding.data(), config_.threshold, 1);
    if (result == nullptr) return match;
    if (result->count > 0 && result->matches != nullptr) {
      match.name = result->matches[0].name != nullptr ? result->matches[0].name : "";
      match.score = result->matches[0].score;
      match.matched = !match.name.empty();
    }
    SherpaOnnxSpeakerEmbeddingManagerFreeBestMatches(result);
    return match;
  }

  SpeakerMatch BestMatch(const float *samples, int32_t sample_count,
                         int32_t sample_rate) {
    SpeakerMatch match;
    if (samples == nullptr || sample_count <= 0) return match;
    std::vector<float> embedding;
    if (!ComputeEmbedding(samples, sample_count, sample_rate, &embedding)) {
      return match;
    }
    const auto *result = SherpaOnnxSpeakerEmbeddingManagerGetBestMatches(
        manager_, embedding.data(), 0.0F, 1);
    if (result == nullptr) return match;
    if (result->count > 0 && result->matches != nullptr) {
      match.name =
          result->matches[0].name != nullptr ? result->matches[0].name : "";
      match.score = result->matches[0].score;
      match.matched = !match.name.empty();
    }
    SherpaOnnxSpeakerEmbeddingManagerFreeBestMatches(result);
    return match;
  }

  bool Save(const std::string &path) {
    if (path.empty()) return false;
    nlohmann::json root;
    root["dim"] = dim_;
    nlohmann::json speakers = nlohmann::json::object();
    for (const auto &entry : embeddings_) {
      speakers[entry.first] = entry.second;
    }
    root["speakers"] = std::move(speakers);
    std::ofstream out(path);
    if (!out) return false;
    out << root.dump(2) << '\n';
    return out.good();
  }

  bool Load(const std::string &path) {
    if (path.empty()) return false;
    std::ifstream in(path);
    if (!in) return false;
    nlohmann::json root;
    try {
      in >> root;
    } catch (const nlohmann::json::exception &) {
      return false;
    }
    if (!root.contains("speakers") || !root["speakers"].is_object()) return false;
    if (root.contains("dim") && root["dim"].get<int32_t>() != dim_) return false;
    std::map<std::string, std::vector<std::vector<float>>> parsed;
    for (auto it = root["speakers"].begin(); it != root["speakers"].end(); ++it) {
      if (!it.value().is_array()) return false;
      std::vector<std::vector<float>> embeddings;
      for (const auto &item : it.value()) {
        if (!item.is_array() || item.size() != static_cast<size_t>(dim_)) return false;
        std::vector<float> embedding = item.get<std::vector<float>>();
        if (embedding.size() != static_cast<size_t>(dim_)) return false;
        embeddings.push_back(std::move(embedding));
      }
      parsed[it.key()] = std::move(embeddings);
    }
    for (const auto &entry : parsed) {
      if (!SyncManager(entry.first, entry.second)) return false;
    }
    embeddings_ = std::move(parsed);
    return true;
  }

 private:
  // one averaged embedding per speaker, so a second Register for the same
  // name must re-add the whole list.
  bool SyncManager(const std::string &name,
                   const std::vector<std::vector<float>> &list) {
    if (list.empty()) return false;
    SherpaOnnxSpeakerEmbeddingManagerRemove(manager_, name.c_str());
    if (list.size() == 1) {
      return SherpaOnnxSpeakerEmbeddingManagerAdd(manager_, name.c_str(),
                                                  list[0].data()) == 1;
    }
    std::vector<float> flattened;
    flattened.reserve(list.size() * dim_);
    for (const auto &embedding : list) {
      flattened.insert(flattened.end(), embedding.begin(), embedding.end());
    }
    return SherpaOnnxSpeakerEmbeddingManagerAddListFlattened(
               manager_, name.c_str(), flattened.data(),
               static_cast<int32_t>(list.size())) == 1;
  }

  bool ComputeEmbedding(const float *samples, int32_t sample_count,
                        int32_t sample_rate, std::vector<float> *out) const {
    const auto *stream = SherpaOnnxSpeakerEmbeddingExtractorCreateStream(extractor_);
    if (stream == nullptr) return false;
    SherpaOnnxOnlineStreamAcceptWaveform(stream, sample_rate, samples, sample_count);
    const bool ready = SherpaOnnxSpeakerEmbeddingExtractorIsReady(extractor_, stream) == 1;
    bool ok = false;
    if (ready) {
      const float *embedding =
          SherpaOnnxSpeakerEmbeddingExtractorComputeEmbedding(extractor_, stream);
      if (embedding != nullptr) {
        out->assign(embedding, embedding + dim_);
        SherpaOnnxSpeakerEmbeddingExtractorDestroyEmbedding(embedding);
        ok = true;
      }
    }
    SherpaOnnxDestroyOnlineStream(stream);
    return ok;
  }

  SpeakerVerifierConfig config_;
  int32_t dim_ = 0;
  const SherpaOnnxSpeakerEmbeddingExtractor *extractor_ = nullptr;
  const SherpaOnnxSpeakerEmbeddingManager *manager_ = nullptr;
  // Own copies of registered embeddings so Save()/Load() do not depend on the
  // C manager's internal storage.
  std::map<std::string, std::vector<std::vector<float>>> embeddings_;
};

SpeakerVerifier::SpeakerVerifier(SpeakerVerifierConfig config)
    : impl_(std::make_unique<Impl>(std::move(config))) {}
SpeakerVerifier::~SpeakerVerifier() = default;
SpeakerVerifier::SpeakerVerifier(SpeakerVerifier &&) noexcept = default;
SpeakerVerifier &SpeakerVerifier::operator=(SpeakerVerifier &&) noexcept = default;

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
SpeakerMatch SpeakerVerifier::Identify(const float *samples, int32_t sample_count,
                                       int32_t sample_rate) {
  return impl_->Identify(samples, sample_count, sample_rate);
}
SpeakerMatch SpeakerVerifier::BestMatch(const float *samples, int32_t sample_count,
                                        int32_t sample_rate) {
  return impl_->BestMatch(samples, sample_count, sample_rate);
}
bool SpeakerVerifier::Save() { return impl_->Save(impl_->speakers_path()); }
bool SpeakerVerifier::Load() { return impl_->Load(impl_->speakers_path()); }
bool SpeakerVerifier::Save(const std::string &path) { return impl_->Save(path); }
bool SpeakerVerifier::Load(const std::string &path) { return impl_->Load(path); }

}  // namespace ai_audio_assistant
