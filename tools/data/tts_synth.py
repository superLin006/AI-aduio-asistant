#!/usr/bin/env python3
"""
Qwen3-TTS 多音色批量合成脚本。
加载 CustomVoice 模型（GPU），按 speaker×text 清单批量合成 16k mono wav。

环境要求：transformers>=4.57.4（4.57.3 的 masking_utils 把 sdpa 掩码无条件分派到
torch<2.6 旧路径，qwen_tts 解码 packed 掩码时触发 vmap 内 .item() 崩溃；4.57.6 修复）。
"""

import argparse, os, sys, time

# ===== 9 个预置音色（与板卡 C++ speaker_id 映射一致） =====
SPEAKERS = [
    "Vivian", "Serena", "Uncle_Fu", "Ryan", "Aiden",
    "Ono_Anna", "Sohee", "Eric", "Dylan",
]

# ===== 20 条智慧教室命令短句 =====
CORPUS = [
    "打开投影仪",
    "关闭电灯",
    "把音量调大",
    "空调调到二十六度",
    "播放下一首",
    "暂停播放",
    "开始上课",
    "下课",
    "打开窗帘",
    "调亮灯光",
    "静音",
    "提高亮度到百分之八十",
    "把温度调到二十二度",
    "切换到 HDMI 输入",
    "降低屏幕亮度",
    "今天星期几",
    "帮我查一下明天的天气",
    "开始录制",
    "停止录音",
    "打开麦克风",
]

def main():
    parser = argparse.ArgumentParser(description="TTS 多音色批量合成")
    parser.add_argument("--model_dir",
                        default="/home/xh/itc_project/RK_model_zoo/models/Qwen3-TTS-12Hz-0.6B-CustomVoice")
    parser.add_argument("--out_dir", default=None)  # None = auto in cwd
    parser.add_argument("--batch_size", type=int, default=5)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--dry", action="store_true", help="只打印计划不合成")
    args = parser.parse_args()

    if args.out_dir is None:
        base = os.path.join(os.path.dirname(__file__) or ".", "..", "..",
                            ".cache", "models", "speaker_eval")
        args.out_dir = os.path.abspath(base)
    wav_dir = os.path.join(args.out_dir, "wav16k")
    manifest_path = os.path.join(args.out_dir, "manifest.tsv")
    os.makedirs(wav_dir, exist_ok=True)

    total = len(SPEAKERS) * len(CORPUS)
    print(f"Plan: {len(SPEAKERS)} speakers × {len(CORPUS)} texts = {total} utterances")
    print(f"Output: {wav_dir}/")
    if args.dry:
        for s in SPEAKERS:
            for i, t in enumerate(CORPUS):
                fname = f"{s}_t{i:02d}.wav"
                print(f"  {fname}  speaker={s}  text={t}")
        print("Dry run, exiting.")
        return

    import torch
    # Suppress TF32 for deterministic output
    torch.backends.cudnn.allow_tf32 = False
    torch.backends.cuda.matmul.allow_tf32 = False

    device = "cuda" if torch.cuda.is_available() else "cpu"
    print(f"Device: {device}", flush=True)

    # Load model
    print(f"Loading model from {args.model_dir} ...", flush=True)
    t0 = time.time()
    from qwen_tts import Qwen3TTSModel
    model = Qwen3TTSModel.from_pretrained(
        args.model_dir, device_map=device, torch_dtype=torch.bfloat16
    )
    print(f"Model loaded in {time.time()-t0:.1f}s", flush=True)

    # Synthesize
    manifest_lines = []
    completed = 0
    t_start = time.time()
    for spk in SPEAKERS:
        for i in range(0, len(CORPUS), args.batch_size):
            batch_text = CORPUS[i:i + args.batch_size]
            batch_speakers = [spk] * len(batch_text)
            try:
                audios, sr = model.generate_custom_voice(
                    text=batch_text, speaker=batch_speakers,
                    language=["Chinese"] * len(batch_text),
                    non_streaming_mode=True,
                )
            except Exception as e:
                print(f"[FAIL] {spk} texts {i}-{i+len(batch_text)}: {e}", flush=True)
                continue
            for j, (audio, text) in enumerate(zip(audios, batch_text)):
                idx = i + j
                fname = f"{spk}_t{idx:02d}.wav"
                path = os.path.join(wav_dir, fname)
                try:
                    import soundfile as sf
                    sf.write(path, audio, sr)
                except ImportError:
                    from scipy.io import wavfile
                    wavfile.write(path, sr, audio)
                manifest_lines.append(f"{fname}\t{spk}\t{text}")
                completed += 1
            elapsed = time.time() - t_start
            rate = completed / elapsed if elapsed > 0 else 0
            print(f"  [{spk}] texts {i}-{i+len(batch_text)-1}  "
                  f"({completed}/{total} {rate:.1f}/s)", flush=True)

    # Write manifest
    with open(manifest_path, "w", encoding="utf-8") as f:
        f.write("wav\tspeaker\ttext\n")
        for line in manifest_lines:
            f.write(line + "\n")
    elapsed = time.time() - t_start
    print(f"\nDone: {completed}/{total} synthesized in {elapsed:.0f}s "
          f"({completed/elapsed:.1f}/s)", flush=True)
    print(f"Manifest: {manifest_path}")

if __name__ == "__main__":
    main()