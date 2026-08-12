#!/usr/bin/env python3
"""
Generate tone beep WAV files for ESP32 audio preview.
Generates 100Hz - 2500Hz in 100Hz steps at 22.05kHz, 16-bit PCM mono.
"""

import numpy as np
from scipy.io import wavfile
import os

# Configuration
SAMPLE_RATE = 22050  # Hz (ideal for embedded audio)
DURATION = 1.0       # seconds (adjust to taste)
BIT_DEPTH = 16       # bits

# Output directory
OUTPUT_DIR = "beeps"
os.makedirs(OUTPUT_DIR, exist_ok=True)

# Generate beeps from 100Hz to 2500Hz in 100Hz steps
for freq in range(100, 2600, 100):
    # Generate time array
    t = np.linspace(0, DURATION, int(SAMPLE_RATE * DURATION), False)
    
    # Generate sine wave
    amplitude = 0.7  # 70% of max to avoid clipping
    waveform = amplitude * np.sin(2 * np.pi * freq * t)
    
    # Convert to 16-bit PCM
    waveform_int16 = np.int16(waveform * 32767)
    
    # Write WAV file
    filename = f"{OUTPUT_DIR}/{freq}hz.wav"
    wavfile.write(filename, SAMPLE_RATE, waveform_int16)
    print(f"✓ Created {filename}")

print(f"\nGenerated {25} beep files in '{OUTPUT_DIR}/' directory")
print(f"Format: 22.05kHz, 16-bit PCM mono, ~1.0 sec each")