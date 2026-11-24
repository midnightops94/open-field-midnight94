"""Share a zero at each adjacent row boundary in the resident wave table."""
from hashlib import sha256
from pathlib import Path
import re
import struct


source = Path(__file__).resolve().parents[1]
group_tables, = source.rglob('group_tables.hpp')
original = group_tables.read_text().split('static const int16_t wave[205][513]={', 1)[1].split('};', 1)[0]
rows = [tuple(map(int, re.findall(r'-?\d+', row))) for row in original.splitlines() if row.startswith('{')]
assert len(rows) == 205 and all(len(row) == 513 and row[0] == row[-1] == 0 for row in rows)
original_bytes = b''.join(struct.pack('<513h', *row) for row in rows)
assert sha256(original_bytes).hexdigest() == '33248ab7f333d47e69821961ddf27e9e54fcd3d02c2db847acc1842f54f69812'

# Lookup takes a pointer into this flat array, so its final interpolation
# cell is the following row's zero. The last row uses the terminal zero.
packed = b''.join(struct.pack('<512h', *row[:-1]) for row in rows) + b'\x00\x00'
assert len(original_bytes) - len(packed) == 408
(source / 'waves.i16le').write_bytes(packed)
