# Skatter v8

Skatter is a stereo granular effect. Net sets the span of recorded audio the grains can draw from. Grain sets their length and affects their playback speed. Scatter adds variation. Mix blends the grains with the dry input. The four controls display `00` through `99` and the net and grains follow their effective values.

Press Blue to switch between Free and Capture. Press Gray to cycle Forward, Both and Reverse. Both lets each new grain play in either direction. Each synth slot, drum slot and the master remember their own mode choices.

Select Skatter from a synth, drum or master FX page. Hold Shift while turning for finer adjustment.

## Build

Set `SDK` to the absolute path of the Open-Field 0.20 checkout at `25a8d7f524ec3f46902cbd8a96779df32fa5e9bc` with its Blackfin toolchain installed. Run the commands from this directory. [Open-Field build guide](https://github.com/LaP0573/open-field/blob/25a8d7f524ec3f46902cbd8a96779df32fa5e9bc/docs/build.md)

```sh
OUT=$(mktemp -d)
python3 build.py "$SDK" "$SDK/open-xy/local/toolchain" "$OUT"
sha256sum "$OUT/skatter.pack"
```

Expected SHA-256 for the 150,207-byte pack is `bbeb61264035d38b129d023c388043d535942b1b340a30fce4ed7adb39a641be`.
