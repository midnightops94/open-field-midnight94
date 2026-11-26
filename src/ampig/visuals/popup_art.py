"""Author Ampig's retained Circuit and Recover cards for the 480×220 Field."""
from xml.etree import ElementTree as ET

from popup_letters import fallback_paths


CREAM = "#CDD0C2"
BLUE = "#3C6087"
OCHRE = "#8A653C"
ICON_INK = "#E4E6DA"
KAPPA = 0.55228475

# All icon paths use the tile's actual screen coordinates. The characters in
# their corresponding captions come from native text or the retained backup.
CHOICES = (
    ("circuit", "valve", BLUE, (
        "M 132 122 V 103 C 132 99.075 134.101 95.436 137.5 93.474 "
        "C 140.899 91.511 145.101 91.511 148.5 93.474 "
        "C 151.899 95.436 154 99.075 154 103 V 122 Z",
        "M 129 122 H 157 M 148 122 V 128 M 138 122 V 128 "
        "M 136 113 C 138.667 107 141 107 143 113 C 145 119 147.333 119 150 113",
    )),
    ("circuit", "diode", BLUE, (
        "M 124 110 H 132 M 154 110 H 162",
        "M 132 101 L 149 110 L 132 119 Z M 149 101 V 119",
    )),
    ("circuit", "starved", BLUE, (
        "M 126 101 H 156 V 119 H 126 Z M 156 106 H 160 V 114 H 156",
        "M 131 105 V 115 M 137 113 L 141 106 L 145 113 L 149 106",
    )),
    ("recover", "flow", OCHRE, (
        "M 122 102 H 128 C 132 102 133 118 139 118 "
        "C 145 118 147 102 155 102 H 164",
        "M 122 125 H 164",
    )),
    ("recover", "fast", OCHRE, (
        "M 122 102 H 132 L 138 119 L 146 102 H 164",
        "M 122 125 H 164",
    )),
    ("recover", "slow", OCHRE, (
        "M 122 102 H 129 L 136 119 C 145 119 151 114 157 110 L 164 106",
        "M 122 125 H 164",
    )),
)


def group(parent, name):
    return ET.SubElement(parent, "g", id=name)


def rounded_panel(parent, name, x, y, width, height, radius, ink):
    """Keep the corner tangencies literal for the Field's path parser."""
    right, bottom = x + width, y + height
    inner = radius * (1 - KAPPA)
    xy = (
        ("M", x + radius, y),
        ("H", right - radius),
        ("C", right - inner, y, right, y + inner, right, y + radius),
        ("V", bottom - radius),
        ("C", right, bottom - inner, right - inner, bottom, right - radius, bottom),
        ("H", x + radius),
        ("C", x + inner, bottom, x, bottom - inner, x, bottom - radius),
        ("V", y + radius),
        ("C", x, y + inner, x + inner, y, x + radius, y),
    )
    geometry = " ".join(" ".join((command, *(f"{n:.3f}".rstrip("0").rstrip(".") for n in coords)))
                        for command, *coords in xy) + " Z"
    ET.SubElement(parent, "path", id=name, d=geometry,
                  fill=ink, stroke="none", **{"stroke-width": "1"})


def stroke(parent, geometry, color, width, cap):
    ET.SubElement(parent, "path", d=geometry, fill="none", stroke=color,
                  **{"stroke-width": width, "stroke-linecap": cap, "stroke-linejoin": "round"})


def add_mode_card(svg):
    card = group(svg, "modeCard")
    rounded_panel(card, "modeFrame", 107.5, 74.875, 265, 70.25, 12.5, CREAM)
    rounded_panel(card, "modePane", 179, 80, 189, 60, 7.5, "#000000")
    for i, (title, value, tile, strokes) in enumerate(CHOICES):
        choice = group(card, f"modeChoice{i}")
        rounded_panel(choice, f"modeTile{i}", 113, 80, 60, 60, 7.5, tile)
        icon = group(choice, f"modeIcon{i}")
        for geometry in strokes:
            stroke(icon, geometry, ICON_INK, "1.65", "round")
        backup = group(choice, f"modeBackup{i}")
        stroke(backup, fallback_paths(title), CREAM, "1.3", "square")
        stroke(backup, fallback_paths(value, lower_row=True), CREAM, "1.3", "square")

    native = group(card, "modeNative")
    stroke(group(native, "modeTitle"), "M 273.5 104.25 V 89.25", CREAM, "1.3", "square")
    stroke(group(native, "modeValue"), "M 273.5 129.35 V 114.35", CREAM, "1.3", "square")
    return card
