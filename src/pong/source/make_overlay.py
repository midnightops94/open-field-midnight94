"""Put the authored FX Off group above the single retained Pong court."""
from pathlib import Path
import re
from standalone_fx_off import FALLBACK
project = Path(__file__).resolve().parent
# Each SVG command letter separates its operands from the preceding instruction.
# Preserve every command, sign, digit, decimal point and numeric-to-numeric separator.
# Leave the short native font anchors in their original source spelling.
def path_data(match):
    path = match[1]
    if len(path) > 30:
        path = re.sub(rb'\s+(?=[MmLlHhVvCcSsZz])|(?<=[MmLlHhVvCcSsZz])\s+', b'', path)
    return b' d="' + path + b'"'

scene = re.sub(rb' d="([^"]*)"', path_data, (project/'pong-scene.svg').read_bytes())
assert not any(name in scene for name in (b'id="pongoff"', b'id="stockFxOff"', b'id="fxOff"'))
scene = scene.replace(b'</svg>', FALLBACK+b'</svg>')
instructions = b'op1f picture 2\nsize %d\na %d\n' % (len(scene), len(scene))
(project/'pong.svg.make').write_bytes(instructions + scene + b'\n')
