#!/usr/bin/env python3
"""把验收音频拼成一段"多人对话"，作为在线 demo 的流式输入（可复现）。

用法: python tools/make_conversation.py
输出: wavs/conversation.wav（16 kHz 单声道，片段间 0.7 s 静音）
"""

import os

import numpy as np
import soundfile as sf

DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
W = os.path.join(DIR, "wavs")

ORDER = [
    "fangjun-sr-1.wav",
    "leijun-sr-1.wav",
    "liudehua-sr-1.wav",
    "fangjun-test-sr-1.wav",
    "leijun-test-sr-1.wav",
    "liudehua-test-sr-1.wav",
]
GAP_SECONDS = 0.7


def main():
    parts = []
    for name in ORDER:
        x, sr = sf.read(os.path.join(W, name), dtype="float32")
        assert sr == 16000, (name, sr)
        parts.append(x)
        parts.append(np.zeros(int(GAP_SECONDS * sr), dtype=np.float32))

    out = np.concatenate(parts)
    path = os.path.join(W, "conversation.wav")
    sf.write(path, out, 16000)
    print(f"wrote {path}: {len(out) / 16000:.1f}s, clips={len(ORDER)}, gap={GAP_SECONDS}s")


if __name__ == "__main__":
    main()
