// 在线（流式）声纹识别 demo
//   流式进音 -> silero VAD(CPU) 分段 -> 每段结束即做声纹识别(RKNN) -> 实时输出说话人
//
// 输入方式：
//   --wav <file>   按实时节奏喂入 WAV 文件（模拟在线流，可复现）
//   --stdin        从标准输入读 16 kHz 单声道 s16le 裸 PCM（如 arecord 管道）
//
// 用法：
//   online_speaker_demo --speaker-model <model.rknn> --vad <silero_vad.onnx>
//     --register name=wav [--register name=wav ...]
//     (--wav <conversation.wav> | --stdin)
//
// 注册语义与离线 demo 一致（多条注册取平均；RKNN provider 在 NPU 上计算 embedding）。
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <utility>
#include <vector>

#include "ai_audio_assistant/speaker_verifier.hpp"
#include "sherpa-onnx/c-api/c-api.h"
#include "sherpa-onnx/c-api/cxx-api.h"

namespace {

constexpr int32_t kSampleRate = 16000;

double NowSec() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec / 1e9;
}

std::string Value(int argc, char **argv, const std::string &name) {
  for (int i = 1; i + 1 < argc; ++i) {
    if (argv[i] == name) return argv[i + 1];
  }
  return {};
}

std::vector<std::string> Values(int argc, char **argv, const std::string &name) {
  std::vector<std::string> values;
  for (int i = 1; i + 1 < argc; ++i) {
    if (argv[i] == name) values.emplace_back(argv[i + 1]);
  }
  return values;
}

bool HasFlag(int argc, char **argv, const std::string &name) {
  for (int i = 1; i < argc; ++i) {
    if (argv[i] == name) return true;
  }
  return false;
}

std::pair<std::string, std::string> SplitNameWav(const std::string &arg) {
  const auto pos = arg.find('=');
  if (pos == std::string::npos || pos == 0 || pos + 1 >= arg.size()) {
    fprintf(stderr, "expected name=wav, given: %s\n", arg.c_str());
    exit(2);
  }
  return {arg.substr(0, pos), arg.substr(pos + 1)};
}

}  // namespace

int main(int argc, char **argv) {
  const std::string model = Value(argc, argv, "--speaker-model");
  const std::string provider = Value(argc, argv, "--speaker-provider");
  const std::string vad_model = Value(argc, argv, "--vad");
  const std::string threshold_str = Value(argc, argv, "--threshold");
  const std::string wav_in = Value(argc, argv, "--wav");
  const bool use_stdin = HasFlag(argc, argv, "--stdin");
  const auto register_args = Values(argc, argv, "--register");

  if (model.empty() || vad_model.empty() || register_args.empty() ||
      (wav_in.empty() && !use_stdin)) {
    fprintf(stderr,
            "usage: %s --speaker-model M.rknn --vad silero_vad.onnx "
            "--register name=wav ... (--wav file | --stdin)\n",
            argv[0]);
    return 2;
  }

  const float threshold = threshold_str.empty() ? 0.5f : std::stof(threshold_str);

  ai_audio_assistant::SpeakerVerifierConfig config;
  config.model_path = model;
  if (!provider.empty()) config.provider = provider;
  config.threshold = threshold;

  try {
    ai_audio_assistant::SpeakerVerifier verifier(std::move(config));
    for (const auto &arg : register_args) {
      const auto [name, wav] = SplitNameWav(arg);
      const bool ok = verifier.RegisterFile(name, wav);
      printf("register=%s:%s -> ok=%d\n", name.c_str(), wav.c_str(), ok);
      if (!ok) return 1;
    }
    printf("enrolled=%d speakers (threshold=%.2f, provider=%s)\n",
           verifier.num_speakers(), threshold,
           provider.empty() ? "cpu" : provider.c_str());

    SherpaOnnxVadModelConfig vad_config;
    memset(&vad_config, 0, sizeof(vad_config));
    vad_config.silero_vad.model = vad_model.c_str();
    vad_config.silero_vad.threshold = 0.5f;
    vad_config.silero_vad.min_silence_duration = 0.6f;
    vad_config.silero_vad.min_speech_duration = 0.25f;
    vad_config.silero_vad.max_speech_duration = 12.0f;
    vad_config.silero_vad.window_size = 512;
    vad_config.sample_rate = kSampleRate;
    vad_config.num_threads = 1;
    vad_config.provider = "cpu";

    const SherpaOnnxVoiceActivityDetector *vad =
        SherpaOnnxCreateVoiceActivityDetector(&vad_config, 30.0f);
    if (vad == NULL) {
      fprintf(stderr, "failed to create VAD (model: %s)\n", vad_model.c_str());
      return 1;
    }

    const int32_t chunk = kSampleRate / 10;  // 100 ms
    int32_t total_samples = 0;
    int32_t num_segments = 0;
    std::vector<std::pair<std::string, int>> speaker_counts;
    auto count_speaker = [&speaker_counts](const std::string &name) {
      for (auto &p : speaker_counts) {
        if (p.first == name) {
          ++p.second;
          return;
        }
      }
      speaker_counts.emplace_back(name, 1);
    };

    auto handle_segments = [&]() {
      while (!SherpaOnnxVoiceActivityDetectorEmpty(vad)) {
        const SherpaOnnxSpeechSegment *seg =
            SherpaOnnxVoiceActivityDetectorFront(vad);
        if (seg == NULL) break;

        const auto match = verifier.BestMatch(seg->samples, seg->n, kSampleRate);
        const bool matched = !match.name.empty() && match.score >= threshold;
        printf("[%6.1fs] speaker=%s score=%.4f matched=%d len=%.1fs\n",
               seg->start / static_cast<double>(kSampleRate),
               match.name.empty() ? "(none)" : match.name.c_str(), match.score,
               matched ? 1 : 0, seg->n / static_cast<double>(kSampleRate));
        fflush(stdout);
        if (!match.name.empty()) count_speaker(match.name);
        ++num_segments;

        SherpaOnnxDestroySpeechSegment(seg);
        SherpaOnnxVoiceActivityDetectorPop(vad);
      }
    };

    if (!wav_in.empty()) {
      const auto wave = sherpa_onnx::cxx::ReadWave(wav_in);
      if (wave.samples.empty()) {
        fprintf(stderr, "failed to read WAV: %s\n", wav_in.c_str());
        return 1;
      }
      const double t0 = NowSec();
      std::vector<float> buf(chunk);
      for (size_t i = 0; i < wave.samples.size(); i += chunk) {
        const int32_t n =
            static_cast<int32_t>(std::min(chunk, static_cast<int32_t>(wave.samples.size() - i)));
        memcpy(buf.data(), wave.samples.data() + i, n * sizeof(float));
        SherpaOnnxVoiceActivityDetectorAcceptWaveform(vad, buf.data(), n);
        total_samples += n;
        handle_segments();
        const double target = t0 + total_samples / static_cast<double>(kSampleRate);
        const double now = NowSec();
        if (target > now) {
          struct timespec req;
          req.tv_sec = static_cast<time_t>(target - now);
          req.tv_nsec = static_cast<long>((target - now - req.tv_sec) * 1e9);
          nanosleep(&req, NULL);
        }
      }
      printf("stream end: %.1fs fed\n", total_samples / static_cast<double>(kSampleRate));
    } else {
      std::vector<int16_t> raw(chunk);
      std::vector<float> buf(chunk);
      while (true) {
        const size_t got = fread(raw.data(), sizeof(int16_t), chunk, stdin);
        if (got == 0) break;
        for (size_t i = 0; i < got; ++i) {
          buf[i] = raw[i] / 32768.0f;
        }
        SherpaOnnxVoiceActivityDetectorAcceptWaveform(vad, buf.data(), static_cast<int32_t>(got));
        total_samples += static_cast<int32_t>(got);
        handle_segments();
      }
      printf("stdin end: %.1fs fed\n", total_samples / static_cast<double>(kSampleRate));
      if (total_samples == 0) {
        fprintf(stderr, "no input from stdin (arecord failed or device busy)\n");
        return 1;
      }
    }

    SherpaOnnxVoiceActivityDetectorFlush(vad);
    handle_segments();

    printf("speech segments=%d\n", num_segments);
    for (const auto &p : speaker_counts) {
      printf("  %s: %d segment(s)\n", p.first.c_str(), p.second);
    }
    SherpaOnnxDestroyVoiceActivityDetector(vad);
    printf("ONLINE_DEMO_DONE\n");
    return 0;
  } catch (const std::exception &error) {
    fprintf(stderr, "online speaker demo failed: %s\n", error.what());
    return 1;
  }
}
