"""Compose the released OS 1.7.3 FX status artwork using public Open-Field P2 references."""
from zlib import crc32

MEMBER = "lfo_popup.svg"
STOCK_LENGTH = 9429
STOCK_CRC32 = 0xc015753e
# Frame and black text pane; then the active FX tile and its stroked lettering.
SPANS = ((404, 389), (6345, 3076))
OPEN = b'<g id="stockFxOff"><rect x="0" y="0" width="480" height="220" fill="#000000" fill-opacity="0.8"/>'
CLOSE = b'</g>'


def literal(svg):
    return b'op1f picture 2\nsize %d\na %d\n' % (len(svg),len(svg)) + svg + b'\n'


def compose(svg):
    """Append the stock card over a dimmed copy of the existing scene in paint order."""
    end = svg.rfind(b'</svg>')
    if end < 0 or b'id="stockFxOff"' in svg:
        raise ValueError("expected one source SVG without an existing stock FX card")
    before = svg[:end] + OPEN
    after = CLOSE + svg[end:]
    result_size = len(before) + len(after) + sum(length for offset, length in SPANS)
    return (b'op1f picture 2\nsize %d\n' % result_size +
            b'from %s %d %08x\n' % (MEMBER.encode('ascii'), STOCK_LENGTH, STOCK_CRC32) +
            b'a %d\n' % len(before) + before + b'\n' +
            b''.join(b'c 0 %d %d\n' % span for span in SPANS) +
            b'a %d\n' % len(after) + after + b'\n')


def verified_stock(path):
    """Optional private input for owner-side validation. The build needs no private asset."""
    data = path.read_bytes()
    if len(data) != STOCK_LENGTH or crc32(data) != STOCK_CRC32:
        raise ValueError("stock FX popup is not the OS 1.7.3 asset")
    return data
