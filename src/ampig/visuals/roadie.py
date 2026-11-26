"""Convert the approved Roadie web drawing to a bounded, movable Field picture."""
from copy import deepcopy
import re
from xml.etree import ElementTree as ET

from field_numbers import normalize_svg, verify_svg
from popup_art import add_mode_card

OUTLINES = 12
WET_LEVELS = 6
DRY_LEVELS = 7
GRILLE_LEVELS = 6
WAVE_SEGMENTS = 6
BACKGROUND = (6, 8, 9)
LEAVES = {"path", "rect", "circle", "line", "polyline", "polygon", "ellipse"}
PAINT_DEFAULTS = {"fill": "none", "stroke": "none", "stroke-width": "1",
                  "stroke-linecap": "round", "stroke-linejoin": "round",
                  "fill-opacity": "1", "stroke-opacity": "1"}
SVG_NS = "http://www.w3.org/2000/svg"


def number(value):
    return f"{value:.3f}".rstrip("0").rstrip(".")


def lerp(start, stop, weight):
    return start + (stop - start) * weight


def ink(start, stop, weight):
    return tuple(round(lerp(a, b, weight)) for a, b in zip(start, stop))


def color(rgb):
    return "#%02x%02x%02x" % rgb


def group(parent, **attrs):
    return ET.SubElement(parent, "g", attrs)


def path(parent, geometry, **attrs):
    return ET.SubElement(parent, "path", dict(d=geometry, **attrs))


def move_label(prefix, index):
    return f"{prefix}{index:02}"


def variant(parent, prefix, index, **attrs):
    return group(parent, id=move_label(prefix, index), **attrs)


def solid_ink(value, opacity):
    """Bake the translucent line colors over the Roadie page background."""
    if value == "none" or opacity == 0:
        return "none"
    if opacity == 1:
        return value
    target = tuple(int(value[index:index + 2], 16) for index in (1, 3, 5))
    return color(ink(BACKGROUND, target, opacity))


def write_leaf_paints(node, inherited, opacity=1.0):
    # Field paint is local to each drawable. Keep the binding groups for
    # op1f_move and op1f_show, with their ink on the children that draw it.
    style = {key: node.attrib.pop(key, value) for key, value in inherited.items()}
    opacity *= float(node.attrib.pop("opacity", "1"))
    if node.tag in LEAVES:
        for material in ("fill", "stroke"):
            node.set(material, solid_ink(style[material], opacity * float(style[material + "-opacity"])))
        node.set("stroke-width", style["stroke-width"])
        if node.tag in {"path", "line", "polyline", "polygon"} and node.get("stroke") != "none":
            node.set("stroke-linecap", style["stroke-linecap"])
            node.set("stroke-linejoin", style["stroke-linejoin"])
    for child in node:
        write_leaf_paints(child, style, opacity)


def cubic_path(geometry):
    """The Field path grammar needs cubics where the drawing used quadratics."""
    lengths = dict(M=2, L=2, H=1, V=1, C=6, S=4, Q=4, Z=0)
    tokens = iter(re.findall(r"[MLHVCSQZ]|-?\d+(?:\.\d+)?", geometry))
    x = y = sx = sy = 0
    output = []
    for command in tokens:
        args = [float(next(tokens)) for _ in range(lengths[command])]
        if command == "Q":
            a, b, c, d = args
            command = "C"
            args = [lerp(x, a, 2 / 3), lerp(y, b, 2 / 3),
                    lerp(c, a, 2 / 3), lerp(d, b, 2 / 3), c, d]
        if command in "MLCS":
            x, y = args[-2:]
        if command == "M":
            sx, sy = x, y
        elif command == "H":
            x = args[0]
        elif command == "V":
            y = args[0]
        elif command == "Z":
            x, y = sx, sy
        output.extend([command, *(number(arg) for arg in args)])
    return " ".join(output)


def approved_pig(project):
    source = (project / "artwork/AmPig-Roadie-Interactive-v2.html").read_text()
    approved = source.split('<svg class="screen"', 1)[1].split("</svg>", 1)[0]
    svg = ET.fromstring("<svg" + approved + "</svg>")
    pig = svg.find("./g[@id='pig']")
    for node in pig.iter():
        for key in ("class", "aria-hidden"):
            node.attrib.pop(key, None)
        if node.tag == "path" and node.get("d"):
            node.set("d", cubic_path(node.get("d")))
    return pig


def outlines(pig, approved):
    outline, face, *_, feet = list(approved)
    body = group(pig)
    back = group(pig, id="rBack")
    belly = group(pig, id="rBelly")
    handle = group(pig, id="rHandle")
    shoulder = cubic_path("M299 149 Q294 152 284 152 M187 152 Q178 152 174 149")
    fixed_parts = [outline[i].get("d") for i in (3, 4, 5, 7)] + [face[1].get("d"), shoulder]
    body_path = " ".join(fixed_parts)
    for index in range(OUTLINES):
        # Nine resting Drive inks and three loud-highlight variants.
        power = (min(index, 8) / 8) ** 1.4
        push = max(0, index - 8) * 0.16
        rgb = ink(ink((109, 137, 151), (161, 204, 222), power), (224, 239, 241), push)
        attrs = {"stroke": color(rgb), "stroke-width": number(1.45 + 0.55 * power + 0.24 * push)}
        fixed = variant(body, "o", index, **attrs)
        path(fixed, body_path)
        for source in (outline[6], face[2], face[0], *feet):
            drawable = deepcopy(source)
            drawable.attrib.pop("id", None)
            fixed.append(drawable)
        path(variant(back, "b", index, **attrs),
             "M163 67 H177 C197 67 201 77 221 77 H293 C303 77 307 67 314 67")
        path(variant(belly, "e", index, **attrs),
             "M284 152 C268 152 259 171 237 171 S206 152 187 152")
        through = variant(handle, "h", index, **attrs)
        through.extend(deepcopy(child) for child in list(outline[0]))


def speaker(deck, approved):
    panel = deepcopy(approved.find("./g[@id='cabinet-panel']"))
    panel.remove(panel.find("./g[@id='grille']"))
    panel[0].set("stroke", "#7796a3")
    panel[0].set("stroke-width", "1.45")
    deck.append(panel)
    cone = panel.find("./g[@id='cone']")
    for circle in cone:
        circle.set("cx", "294.2")
        circle.set("cy", "104.5")
    for left, center, width in ((True, 249.2, 12.6), (False, 333, 4.4)):
        louvres = group(panel, id="rToneL" if left else "rToneR")
        geometry = " ".join(f"M{number(center-width)} {number(105 + (i-2)*5.85)} "
                            f"H{number(center+width)}" for i in range(5))
        for index in range(GRILLE_LEVELS):
            weight = index / (GRILLE_LEVELS - 1)
            path(variant(louvres, "l" if left else "f", index), geometry,
                 **{"stroke": "#859a9f", "stroke-width": number(lerp(0.8, 2, weight)),
                    "stroke-opacity": number(lerp(0.22, 0.94, weight))})


def wet_routes(deck, pig):
    wet = group(deck)
    rail = group(pig, id="rRail")
    center = group(pig, id="rRailBend")
    for index in range(WET_LEVELS):
        weight = index / (WET_LEVELS - 1)
        opacity = number(lerp(0.07, 1, weight))
        wire = variant(wet, "w", index, stroke="#6a8de2",
                       **{"stroke-width": "1.75", "stroke-opacity": opacity})
        path(wire, "M219.9 105.5 H233.6")
        for segment in range(WAVE_SEGMENTS):
            # A normalized endpoint segment. Its display-thread transform
            # connects adjacent waveform samples without rewriting a path.
            path(wire, "M0 0 H7.5", id=f"u{index}{segment}")
        rail_attr = {"stroke": "#ed713d", "stroke-width": "1.9",
                     "stroke-opacity": opacity}
        path(variant(rail, "r", index), "M189 134 H211 M276 135 H296", **rail_attr)
        path(variant(center, "n", index),
             "M211 134 C227 134 228 150 242 150 S256 135 276 135", **rail_attr)


def dry_routes(pig):
    route = group(pig, stroke="#a4c3d0", **{"stroke-width": "1.8"})
    bend = group(pig, id="rDryBend", stroke="#a4c3d0", **{"stroke-width": "1.8"})
    for index in range(DRY_LEVELS):
        stroke = {"stroke-opacity": number(1 - index / (DRY_LEVELS - 1))}
        base = variant(route, "y", index, **stroke)
        path(base, cubic_path("M156 110.5 H167 Q174 110.5 179 120 L187 134 "
                              "Q191 141 203 141 H218 M293 141 H321"))
        ET.SubElement(base, "circle", cx="323", cy="141", r="2")
        path(variant(bend, "j", index, **stroke),
             "M218 141 C225 141 230 162 242 162 S277 141 293 141")
    ET.SubElement(pig, "circle", id="rDryPulse", cx="0", cy="0", r="1.6", fill="#ddeae9")


def circuit_parts(deck, approved):
    cases = deepcopy(approved.find("./g[@id='circuit-parts']"))
    deck.append(cases)
    for index, item in enumerate(cases):
        item.set("id", move_label("c", index))
        if index == 0:
            item[0].set("fill-opacity", "0.24")
    path(deck, "M226 0 V5", id="rTickA", stroke="#d48746", **{"stroke-width": "1.45"})
    path(deck, "M231 0 V5", id="rTickB", stroke="#d48746", **{"stroke-width": "1.45"})


def verify_scene(svg):
    # Validate the authored bytes before any DOM can normalize qualified names
    # or apply inherited paints on the device's behalf.
    if re.search(rb"</?[A-Za-z0-9]+:", svg):
        raise ValueError("Field picture tags must be unprefixed")
    root = ET.fromstring(svg)
    if root.tag != f"{{{SVG_NS}}}svg":
        raise ValueError("Field picture needs the default SVG namespace")
    for node in root.iter():
        name = node.tag.removeprefix(f"{{{SVG_NS}}}")
        if name not in LEAVES | {"svg", "g"}:
            raise ValueError(f"Unexpected tag in authored Field picture: {node.tag}")
        if {"display", "opacity", "fill-opacity", "stroke-opacity", "class"} & node.attrib.keys():
            raise ValueError(f"Field Roadie must use local paint and native visibility: {node.attrib}")
        if name == "g" and "transform" in node.attrib:
            raise ValueError("Static group transforms must be flattened in Field Roadie")
        if name in LEAVES and not {"fill", "stroke", "stroke-width"} <= node.attrib.keys():
            raise ValueError(f"Field Roadie is missing local paint: {node.attrib}")
    verify_svg(root)


def scene(project):
    approved = approved_pig(project)
    svg = ET.parse(project / "artwork/field-controls.svg").getroot()
    for node in svg.iter():
        node.tag = node.tag.rsplit("}", 1)[-1]
        node.attrib.pop("display", None)
    add_mode_card(svg)
    svg.set("xmlns", SVG_NS)
    background = ET.Element("rect", x="0", y="0", width="480", height="220", fill=color(BACKGROUND))
    pig = ET.Element("g", fill="none", **{"stroke-linecap": "round", "stroke-linejoin": "round"})
    svg.insert(0, pig)
    svg.insert(0, background)
    outlines(pig, approved)
    deck = group(pig, id="rDeck")
    speaker(deck, approved)
    wet_routes(deck, pig)
    circuit_parts(deck, approved)
    dry_routes(pig)
    write_leaf_paints(svg, PAINT_DEFAULTS)
    normalize_svg(svg)
    serialized = ET.tostring(svg)
    verify_scene(serialized)
    return serialized
