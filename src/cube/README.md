# Cube

Cube is a stereo resonator effect for Open-Field 0.14. Shape moves between the Wood, Voice and Metal resonator families. Morph retunes the three bands. Reso changes their resonance and the room response. Mix blends the resonator with the dry signal. The wireframe follows the effective controls, including LFO modulation.

The audio kernel lives in `source/cube-engine.hpp` and `source/cube-room.hpp`. The frame and surface use `source/frame.inc` and `source/cube_points.hpp`. Edit `source/chamber.svg` for the background and native text anchors. The bypass card uses a public stock-picture reference that the Field resolves locally. A self-contained alternate card covers installations with a different stock picture.

## Build

Use the BF707 toolchain specified by Open-Field 0.14 and checkout `bb901bbe037db4114d294b8e3c4a8f9a7ddf28aa`. Run this command from this directory. The second path contains the toolchain’s `bin/` directory. [Open-Field 0.14 build documentation](https://github.com/LaP0573/open-field/tree/bb901bbe037db4114d294b8e3c4a8f9a7ddf28aa)

```sh
python3 build.py /path/to/open-field /path/to/bfin-tools
```

The resulting `dist/cube.pack` is Cube v5. Its SHA-256 is `d8400c058b5959a4c7e8e31f7c935198e6333e535c382a592a0e6eba847a1072`.
