"""Encode the approved Organ rows without changing a single waveform cell."""
from hashlib import sha256
from pathlib import Path
import re
import struct


source = Path(__file__).resolve().parents[1]
group_tables, = source.rglob('group_tables.hpp')
original = group_tables.read_text().split('static const int16_t wave[205][513]={', 1)[1].split('};', 1)[0]
rows = [tuple(map(int, re.findall(r'-?\d+', row))) for row in original.splitlines() if row.startswith('{')]
assert len(rows) == 205 and all(len(row) == 513 and row[0] == row[-1] == 0 for row in rows)
cell_bytes = b''.join(struct.pack('<513h', *row) for row in rows)
assert sha256(cell_bytes).hexdigest() == '33248ab7f333d47e69821961ddf27e9e54fcd3d02c2db847acc1842f54f69812'

encoded = bytearray()
for row in rows:
    encoded.extend(struct.pack('<h', row[1]))
    curvature = (next_cell - 2 * cell + previous for previous, cell, next_cell in zip(row, row[1:], row[2:]))
    encoded.extend(struct.pack('<511b', *curvature))
assert len(encoded) * 2 == len(cell_bytes)
(source / 'waves.delta').write_bytes(encoded)
