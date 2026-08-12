#!/usr/bin/env python3
"""
Generate tone beep MP3 files for ESP32 audio preview.
Generates 100Hz - 2500Hz in 100Hz steps as MP3 files.
"""

import numpy as np
from pydub import AudioSegment
from pydub.generators import Sine
import os

# Configuration
SAMPLE_RATE = 22050  # Hz
DURATION_MS = 500    # milliseconds (0.5 seconds - short, snappy beep)
OUTPUT_DIR = "beeps"

os.makedirs(OUTPUT_DIR, exist_ok=True)

# Generate beeps from 100Hz to 2500Hz in 100Hz steps
for freq in range(100, 2600, 100):
    # Generate sine wave
    sine_wave = Sine(freq, sample_rate=SAMPLE_RATE).to_audio_segment(duration=DURATION_MS)
    
    # Reduce volume slightly to avoid clipping
    sine_wave = sine_wave - 6  # -6dB
    
    # Export as MP3
    filename = f"{OUTPUT_DIR}/{freq}hz.mp3"
    sine_wave.export(filename, format="mp3", bitrate="128k")
    print(f"✓ Created {filename}")

print(f"\nGenerated 25 beep files in '{OUTPUT_DIR}/' directory")
print(f"Format: MP3, 22.05kHz, 128kbps, mono")