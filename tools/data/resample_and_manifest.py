#!/usr/bin/env python3
"""Resample 24k TTS output to 16k + generate manifest."""
import os, re, sys

wav24k = os.path.join(os.path.dirname(__file__) or ".", "..", "..",
                      ".cache", "models", "speaker_eval", "wav24k")
wav16k = os.path.join(os.path.dirname(__file__) or ".", "..", "..",
                      ".cache", "models", "speaker_eval", "wav16k")
out_dir = os.path.abspath(wav16k)
os.makedirs(out_dir, exist_ok=True)

# Map wav filename to (speaker, text_index)
files = sorted(os.listdir(wav24k))
samples = []
for f in files:
    m = re.match(r"(\w+)_t(\d+)\.wav$", f)
    if not m:
        continue
    spk, idx = m.group(1), int(m.group(2))
    samples.append((f, spk, idx))

if not samples:
    print("No wavs found")
    sys.exit(1)

print(f"Found {len(samples)} wavs in {wav24k}")

# Resample 24000→16000 using linear interpolation
import numpy as np
import soundfile as sf

manifest_lines = []
for fname, spk, idx in samples:
    src_path = os.path.join(wav24k, fname)
    audio, sr = sf.read(src_path)
    # Linear resample: 24000 → 16000
    if sr == 24000:
        n_out = int(len(audio) * 16000 / sr)
        x_old = np.linspace(0, 1, len(audio))
        x_new = np.linspace(0, 1, n_out)
        audio_16k = np.interp(x_new, x_old, audio).astype(audio.dtype)
    else:
        audio_16k = audio
    dst_path = os.path.join(out_dir, fname)
    sf.write(dst_path, audio_16k, 16000)
    # text from filename is not available here; we'll fill from batch file later
    manifest_lines.append(f"{fname}\t{spk}\t")

# Write provisional manifest (text to be filled from batch file)
manifest_path = os.path.join(os.path.dirname(out_dir), "manifest.tsv")
with open(manifest_path, "w", encoding="utf-8") as f:
    f.write("wav\tspeaker\ttext\n")
    for line in manifest_lines:
        f.write(line + "\n")

print(f"Resampled {len(manifest_lines)} files to 16k")
print(f"Manifest: {manifest_path} (text column needs filling)")