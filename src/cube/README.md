# Cube v7

Cube is a stereo resonator. Shape moves between the Wood, Voice and Metal resonator families. Morph retunes the three bands. Reso changes their resonance and the room response. Mix blends the resonator with the dry signal. The wireframe follows all four controls, including LFO modulation.

The four corner values use the `00`–`99` scale. The audio kernel lives in `source/cube-engine.hpp` and `source/cube-room.hpp`. Edit `source/chamber.svg` for the background and native text anchors.

Select Cube from a synth, drum or master FX page. Hold Shift while turning for finer adjustment.

## Build

Set `SDK` to the absolute path of the Open-Field 0.20 checkout at `25a8d7f524ec3f46902cbd8a96779df32fa5e9bc` with its Blackfin toolchain installed. Run the commands from this directory. [Open-Field build guide](https://github.com/LaP0573/open-field/blob/25a8d7f524ec3f46902cbd8a96779df32fa5e9bc/docs/build.md)

```sh
OUT=$(mktemp -d)
python3 build.py "$SDK" "$SDK/open-xy/local/toolchain" "$OUT"
sha256sum "$OUT/cube.pack"
```

Expected SHA-256 for the 18,290-byte pack is `0af690a65017443f9eb172d5a6b308e6d34e8cf5dafda6f56a3223faadf421d5`.
