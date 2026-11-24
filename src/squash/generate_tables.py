"""Quantize the approved Squash curves for the fixed-point target."""

import math
from pathlib import Path


Q = 1 << 20
DB = 1 << 16
SAMPLE_RATE = 44100


def coefficient(ms):
    return 1 - math.exp(-1000 / (ms * SAMPLE_RATE))


def table(name, values, scale=Q):
    words = [round(value * scale) for value in values]
    lines = [', '.join(str(x) for x in words[i:i + 8]) for i in range(0, len(words), 8)]
    return f'constexpr int {name}[] = {{\n    ' + ',\n    '.join(lines) + '\n};\n'


def time_profile(low, high, factor=1):
    return (coefficient(factor * low * (high / low) ** (i / 128)) for i in range(129))


data = [
    '/* Generated from the mathematical curves in generate_tables.py. */\n#pragma once\nnamespace squash {\n',
    f'constexpr int smooth = {round(Q * coefficient(7))};\n',
    f'constexpr int rms_speed = {round(Q * coefficient(8))};\n',
    f'constexpr int bass_speed = {round(Q * (1 - math.exp(-2 * math.pi * 138 / SAMPLE_RATE)))};\n',
    table('log_db', (20 * math.log10(1 + i / 256) for i in range(257)), DB),
    table('root_norm', (math.sqrt(1 + i / 256) for i in range(257))),
    table('make_gain', (10 ** ((-96 + i / 8) / 20) for i in range(865))),
    table('curve_tanh', (math.tanh(i / 32) for i in range(257))),
    table('inverse_knee', [8] + [8 / i for i in range(1, 194)]),
    table('inverse_pillow', (1 / (1 + 6.5 * i / 256) for i in range(257))),
    table('strength_curve', (1 - 20 ** (-i / 256) for i in range(257))),
]

for mode, (alo, ahi, rlo, rhi) in enumerate(((.18, 29, 25, 380), (2.2, 55, 65, 650), (.45, 8, 110, 1150))):
    data += [table(f'attack_{mode}', time_profile(alo, ahi)),
             table(f'hard_{mode}', time_profile(alo, ahi, .36)),
             table(f'release_{mode}', time_profile(rlo, rhi))]
data.append('}\n')
Path(__file__).with_name('source').joinpath('squash-tables.hpp').write_text(''.join(data))
