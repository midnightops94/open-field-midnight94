"""Original wire lettering for Ampig's two popup titles and six choices."""

WORDS = {"circuit", "recover", "valve", "diode", "starved", "flow", "fast", "slow"}

# Each sketch uses a 15-pixel cap height. The first number is its ink width.
# Coordinates are absolute pairs in the letter's own space, including the
# control points of each cubic. The caller supplies the paint and cap shape.
LETTERS = {
    "a": (8, "M 0 15 L 3.5 0 L 4.5 0 L 8 15 M 1.5 9.5 L 6.5 9.5"),
    "c": (7.5, "M 7.5 2 C 6.5 0.5 5.5 0 4 0 C 1.5 0 0 2 0 5 "
               "L 0 10 C 0 13 1.5 15 4 15 C 5.5 15 6.5 14.5 7.5 13"),
    "d": (7.5, "M 0 15 L 0 0 L 3 0 C 6 0 7.5 2 7.5 5.5 "
               "L 7.5 9.5 C 7.5 13 6 15 3 15 L 0 15"),
    "e": (7, "M 7 0 L 0 0 L 0 15 L 7 15 M 0 7.25 L 5.5 7.25"),
    "f": (7, "M 0 15 L 0 0 L 7 0 M 0 7.25 L 5.5 7.25"),
    "i": (5.75, "M 0 0 L 5.75 0 M 2.875 0 L 2.875 15 M 0 15 L 5.75 15"),
    "l": (6.5, "M 0 0 L 0 15 L 6.5 15"),
    "o": (8, "M 4 0 C 6.5 0 8 2 8 5 L 8 10 C 8 13 6.5 15 4 15 "
             "C 1.5 15 0 13 0 10 L 0 5 C 0 2 1.5 0 4 0"),
    "r": (8, "M 0 15 L 0 0 L 4 0 C 6.5 0 7.5 1.5 7.5 4 "
             "C 7.5 6.5 6 8 3.5 8 L 0 8 M 3.5 8 L 8 15"),
    "s": (7.5, "M 7.5 2 C 6.5 0.5 5.5 0 4 0 C 1.5 0 0 1.5 0 3.75 "
               "C 0 8 7.5 7 7.5 11.25 C 7.5 13.75 6 15 3.75 15 "
               "C 2 15 0.75 14.25 0 13"),
    "t": (7.5, "M 0 0 L 7.5 0 M 3.75 0 L 3.75 15"),
    "u": (8, "M 0 0 L 0 10.5 C 0 13.5 1.5 15 4 15 "
             "C 6.5 15 8 13.5 8 10.5 L 8 0"),
    "v": (8, "M 0 0 L 3.75 15 L 4.25 15 L 8 0"),
    "w": (8.75, "M 0 0 L 1.5 15 L 2.25 15 L 4.375 6.5 "
                "L 6.5 15 L 7.25 15 L 8.75 0"),
}


def fallback_paths(name, lower_row=False):
    """Return one centered, absolute SVG path for a supported popup word."""
    if name not in WORDS:
        raise ValueError(f"Unknown Ampig popup word: {name!r}")

    tracking = 2.25
    width = sum(LETTERS[letter][0] for letter in name) + (len(name) - 1) * tracking
    left = 273.5 - width / 2
    top = 114.35 if lower_row else 89.25
    result = []
    for letter in name:
        ink_width, sketch = LETTERS[letter]
        tokens = iter(sketch.split())
        for command in tokens:
            result.append(command)
            for _ in range(3 if command == "C" else 1):
                x = left + float(next(tokens))
                y = top + float(next(tokens))
                result.extend((f"{x:.3f}".rstrip("0").rstrip("."),
                               f"{y:.3f}".rstrip("0").rstrip(".")))
        left += ink_width + tracking
    return " ".join(result)
