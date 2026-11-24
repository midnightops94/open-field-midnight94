# Skatter

Skatter is a stereo granular effect for Open-Field 0.14. Net sets the span of recorded audio the grains can draw from. Grain sets their length and affects their playback speed. Scatter adds variation. Mix blends the grains with the dry input. Press Blue to switch between Free and Capture. Press Gray to cycle Forward, Both and Reverse. Both lets each new grain play in either direction. Each synth slot, drum slot and the master remember their own mode choices.

The grain engine lives in `source/splice.cpp`, its parameter curves in `source/curves.hpp`, and the Open-Field adapter in `source/skatter.cpp`. The catcher’s editable vector scene is `source/night.svg`. It follows the effective controls, including LFO modulation. The bypass card uses a public stock-picture reference that the Field resolves locally. A self-contained alternate card covers installations with a different stock picture.

## Build

Use the BF707 toolchain specified by Open-Field 0.14 and checkout `bb901bbe037db4114d294b8e3c4a8f9a7ddf28aa`. Run this command from this directory. The second path contains the toolchain’s `bin/` directory. [Open-Field 0.14 build documentation](https://github.com/LaP0573/open-field/tree/bb901bbe037db4114d294b8e3c4a8f9a7ddf28aa)

```sh
python3 build.py /path/to/open-field /path/to/bfin-tools
```

The resulting `dist/skatter.pack` is Skatter v6. Its SHA-256 is `d5c998d9c14aa0f8920a9147b9170878237aa50eb221c83a3dc13d82e6416b39`.
