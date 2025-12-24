#!/usr/bin/env python3
"""Generate procedural sound effects for Heligoland"""

import struct
import math
import random
import os

def write_wav(filename, samples, sample_rate=44100):
    """Write samples to a WAV file (16-bit mono)"""
    # Normalize and convert to 16-bit integers
    max_val = max(abs(s) for s in samples) if samples else 1
    if max_val > 1.0:
        samples = [s / max_val for s in samples]

    int_samples = [int(max(-32768, min(32767, s * 32767))) for s in samples]

    num_samples = len(int_samples)
    data_size = num_samples * 2  # 16-bit = 2 bytes per sample

    with open(filename, 'wb') as f:
        # RIFF header
        f.write(b'RIFF')
        f.write(struct.pack('<I', 36 + data_size))
        f.write(b'WAVE')

        # Format chunk
        f.write(b'fmt ')
        f.write(struct.pack('<I', 16))  # Chunk size
        f.write(struct.pack('<H', 1))   # PCM format
        f.write(struct.pack('<H', 1))   # Mono
        f.write(struct.pack('<I', sample_rate))
        f.write(struct.pack('<I', sample_rate * 2))  # Byte rate
        f.write(struct.pack('<H', 2))   # Block align
        f.write(struct.pack('<H', 16))  # Bits per sample

        # Data chunk
        f.write(b'data')
        f.write(struct.pack('<I', data_size))
        for s in int_samples:
            f.write(struct.pack('<h', s))

    print(f"Generated {filename} ({len(samples)} samples, {len(samples)/sample_rate:.2f}s)")

def noise():
    """Generate white noise sample"""
    return random.uniform(-1, 1)

def lowpass(samples, cutoff, sample_rate=44100):
    """Simple one-pole lowpass filter"""
    rc = 1.0 / (2.0 * math.pi * cutoff)
    dt = 1.0 / sample_rate
    alpha = dt / (rc + dt)

    filtered = []
    prev = 0
    for s in samples:
        prev = prev + alpha * (s - prev)
        filtered.append(prev)
    return filtered

def highpass(samples, cutoff, sample_rate=44100):
    """Simple one-pole highpass filter"""
    rc = 1.0 / (2.0 * math.pi * cutoff)
    dt = 1.0 / sample_rate
    alpha = rc / (rc + dt)

    filtered = []
    prev_in = 0
    prev_out = 0
    for s in samples:
        prev_out = alpha * (prev_out + s - prev_in)
        prev_in = s
        filtered.append(prev_out)
    return filtered

def envelope(samples, attack, decay, sustain, release, sample_rate=44100):
    """Apply ADSR envelope"""
    attack_samples = int(attack * sample_rate)
    decay_samples = int(decay * sample_rate)
    release_samples = int(release * sample_rate)

    result = []
    for i, s in enumerate(samples):
        if i < attack_samples:
            env = i / attack_samples if attack_samples > 0 else 1
        elif i < attack_samples + decay_samples:
            t = (i - attack_samples) / decay_samples if decay_samples > 0 else 1
            env = 1.0 - (1.0 - sustain) * t
        elif i < len(samples) - release_samples:
            env = sustain
        else:
            t = (i - (len(samples) - release_samples)) / release_samples if release_samples > 0 else 1
            env = sustain * (1.0 - t)
        result.append(s * env)
    return result

def generate_cannon():
    """Generate cannon fire sound - deep boom with crack"""
    sample_rate = 44100
    duration = 0.6
    num_samples = int(duration * sample_rate)

    samples = []
    for i in range(num_samples):
        t = i / sample_rate

        # Initial crack/pop (very short, high frequency burst)
        crack = 0
        if t < 0.02:
            crack = noise() * (1 - t / 0.02) * 2.0

        # Deep boom (low frequency with pitch drop)
        freq = 80 * math.exp(-t * 8)  # Frequency drops from 80Hz
        boom = math.sin(2 * math.pi * freq * t) * math.exp(-t * 6)

        # Add some rumble noise
        rumble = noise() * math.exp(-t * 10) * 0.3

        samples.append(crack + boom + rumble)

    # Apply lowpass to smooth it
    samples = lowpass(samples, 2000, sample_rate)

    return samples

def generate_splash():
    """Generate water splash sound"""
    sample_rate = 44100
    duration = 0.8
    num_samples = int(duration * sample_rate)

    samples = []
    for i in range(num_samples):
        t = i / sample_rate

        # Initial impact
        impact = noise() * math.exp(-t * 15) * 0.8

        # Bubbling/splashing (filtered noise with varying envelope)
        bubble = noise() * math.exp(-t * 3) * 0.5

        # Add some low frequency thump
        thump = math.sin(2 * math.pi * 60 * t) * math.exp(-t * 20) * 0.4

        samples.append(impact + bubble + thump)

    # Bandpass-ish: lowpass then slight highpass
    samples = lowpass(samples, 3000, sample_rate)
    samples = highpass(samples, 200, sample_rate)

    return samples

def generate_explosion():
    """Generate explosion/hit sound"""
    sample_rate = 44100
    duration = 1.0
    num_samples = int(duration * sample_rate)

    samples = []
    for i in range(num_samples):
        t = i / sample_rate

        # Initial blast
        blast = noise() * math.exp(-t * 8) * 1.5

        # Deep boom
        freq = 50 * math.exp(-t * 3)
        boom = math.sin(2 * math.pi * freq * t) * math.exp(-t * 4) * 1.2

        # Crackling debris
        crackle = noise() * math.exp(-t * 2) * 0.3
        if random.random() > 0.7:
            crackle *= 2

        # Secondary rumble
        rumble = math.sin(2 * math.pi * 30 * t) * math.exp(-t * 2) * 0.5

        samples.append(blast + boom + crackle + rumble)

    samples = lowpass(samples, 4000, sample_rate)

    return samples

def generate_engine():
    """Generate looping engine/ambient sound"""
    sample_rate = 44100
    duration = 2.0  # Will be looped
    num_samples = int(duration * sample_rate)

    samples = []
    for i in range(num_samples):
        t = i / sample_rate

        # Base engine drone (multiple harmonics)
        engine = 0
        base_freq = 35
        for harmonic in range(1, 6):
            freq = base_freq * harmonic
            amp = 1.0 / harmonic
            # Add slight frequency wobble
            wobble = 1 + 0.02 * math.sin(2 * math.pi * 0.5 * t * harmonic)
            engine += math.sin(2 * math.pi * freq * wobble * t) * amp
        engine *= 0.3

        # Add some chug/pulsing
        chug_freq = 4  # 4 Hz pulsing
        chug = 0.7 + 0.3 * math.sin(2 * math.pi * chug_freq * t)
        engine *= chug

        # Add subtle noise for texture
        texture = noise() * 0.05

        samples.append(engine + texture)

    samples = lowpass(samples, 500, sample_rate)

    # Crossfade loop point (fade last 0.1s into first 0.1s)
    fade_samples = int(0.1 * sample_rate)
    for i in range(fade_samples):
        t = i / fade_samples
        samples[i] = samples[i] * t + samples[-(fade_samples - i)] * (1 - t)

    # Trim the faded end
    samples = samples[:-fade_samples]

    return samples

def generate_collision():
    """Generate boat collision/crunch sound"""
    sample_rate = 44100
    duration = 0.5
    num_samples = int(duration * sample_rate)

    samples = []
    for i in range(num_samples):
        t = i / sample_rate

        # Initial impact thud (low frequency)
        thud = math.sin(2 * math.pi * 80 * t) * math.exp(-t * 12) * 1.2

        # Creaking/crunching noise
        crunch = noise() * math.exp(-t * 8) * 0.8

        # Wood stress sound (mid frequency with some harmonics)
        wood_freq = 200 + 50 * math.sin(2 * math.pi * 8 * t)  # Slight wobble
        wood = math.sin(2 * math.pi * wood_freq * t) * math.exp(-t * 6) * 0.4

        # Scraping texture
        scrape = noise() * math.exp(-t * 4) * 0.3

        samples.append(thud + crunch + wood + scrape)

    # Bandpass to give it a wooden character
    samples = lowpass(samples, 2000, sample_rate)
    samples = highpass(samples, 80, sample_rate)

    return samples

if __name__ == "__main__":
    script_dir = os.path.dirname(os.path.abspath(__file__))
    assets_dir = os.path.join(script_dir, "..", "assets")
    os.makedirs(assets_dir, exist_ok=True)

    random.seed(42)  # Reproducible

    write_wav(os.path.join(assets_dir, "cannon.wav"), generate_cannon())
    write_wav(os.path.join(assets_dir, "splash.wav"), generate_splash())
    write_wav(os.path.join(assets_dir, "explosion.wav"), generate_explosion())
    write_wav(os.path.join(assets_dir, "engine.wav"), generate_engine())
    write_wav(os.path.join(assets_dir, "collision.wav"), generate_collision())

    print("\nAll sounds generated!")
