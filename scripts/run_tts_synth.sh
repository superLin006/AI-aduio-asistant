#!/bin/bash
set -e
cd /home/xh/itc_project/superlin/AI-aduio-asistant
export CUDA_VISIBLE_DEVICES=0
export TOKENIZERS_PARALLELISM=false
mkdir -p .cache/models/speaker_eval/wav16k
/home/xh/miniconda3/envs/sophon-qwen3-tts/bin/python -u scripts/tts_synth.py --batch_size 1 2>&1 | tail -15