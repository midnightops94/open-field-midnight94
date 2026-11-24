"""Express source quadratic segments as cubics supported by the public picture reader."""
import re
from collections import Counter
from xml.etree import ElementTree as ET

TOKENS = re.compile(r'[A-Za-z]|[-+]?(?:\d*\.\d+|\d+\.?\d*)(?:[eE][-+]?\d+)?')


# The Field SVG parser accepts 0–4 fractional digits. Three is the
# precision used by Open-Field when it writes mapped path coordinates.
FIELD_DECIMAL_LIMIT = 4
COORDINATE_DECIMALS = 3
NUMBER = re.compile(r'[-+]?(?:\d*\.\d+|\d+\.?\d*)(?:[eE][-+]?\d+)?')
NUMERIC_ATTRIBUTES = {
    'd', 'points', 'viewBox', 'transform', 'gradientTransform', 'width', 'height',
    'x', 'y', 'x1', 'y1', 'x2', 'y2', 'cx', 'cy', 'r', 'rx', 'ry', 'offset',
    'opacity', 'fill-opacity', 'stroke-opacity', 'stroke-width', 'stroke-miterlimit',
    'stroke-dasharray', 'stroke-dashoffset', 'font-size', 'letter-spacing',
}


def field_numbers(svg):
    """Check the final SVG, including numeric attributes outside rewritten paths."""
    counts = Counter()
    for element in ET.fromstring(svg).iter():
        for name, content in element.attrib.items():
            if name not in NUMERIC_ATTRIBUTES:
                continue
            for number in NUMBER.findall(content):
                decimals = len(number.partition('.')[2])
                if 'e' in number.lower() or decimals > FIELD_DECIMAL_LIMIT:
                    raise ValueError(f"unsupported Field SVG number {number!r} in {name!r}; "
                                     f"expected plain numbers with up to {FIELD_DECIMAL_LIMIT} decimals")
                counts[decimals] += 1
    return dict(sorted(counts.items()))


def cubic_path(path):
    if 'Q' not in path.upper():
        return path
    arity = dict(M=2, L=2, H=1, V=1, C=6, Q=4, Z=0)
    tokens = TOKENS.findall(path)
    output = []
    at = 0
    point, start = (0, 0), (0, 0)
    def fmt(x):
        return f'{x:.{COORDINATE_DECIMALS}f}'.rstrip('0').rstrip('.') or '0'
    while at < len(tokens):
        if tokens[at].isalpha():
            command = tokens[at]
            at += 1
        kind = command.upper()
        n = arity[kind]
        vals = list(map(float,tokens[at:at+n]))
        at += n
        x, y = point
        rel = command.islower()
        if kind == 'Z':
            point = start
        elif kind == 'H':
            point = ((x if rel else 0)+vals[0], y)
        elif kind == 'V':
            point = (x, (y if rel else 0)+vals[0])
        else:
            point = ((x if rel else 0)+vals[-2], (y if rel else 0)+vals[-1])
        out_cmd = command
        if kind == 'Q':
            qx, qy = vals[0]+(x if rel else 0), vals[1]+(y if rel else 0)
            ex, ey = point
            vals = [x+(qx-x)*2/3, y+(qy-y)*2/3, ex+(qx-ex)*2/3, ey+(qy-ey)*2/3, ex, ey]
            out_cmd = 'C'
        output.append(out_cmd + ' '.join(map(fmt,vals)))
        if kind == 'M':
            start = point
            command = 'l' if rel else 'L'
    return ' '.join(output)
