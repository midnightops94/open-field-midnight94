# Split v4

Split applies separate saturation to either side of a stereo crossover. It runs on OP-1 Field OS 1.7.3+0 with Open-Field 0.14, base 32. Select Split from a synth, drum or master FX page.

## Controls

Crossover sets the split frequency. Low Drive and High Drive shape their respective bands. Mix blends the colored and direct signals. Press Crossover for Soft or Focus, Low Drive for Round or Tube, High Drive for Clip or Fold, and Mix for Blend or Delta. Delta isolates the coloration as Mix rises.

The FX Off card reads the frame, tile and lettering from the Field’s installed factory picture through Open-Field’s public picture API. A self-contained drawn card supplies the alternate display when that picture differs or is unavailable.

## Build

Use the Open-Field SDK at `bb901bbe037db4114d294b8e3c4a8f9a7ddf28aa` with its Blackfin toolchain set up under `open-xy/local/toolchain`. From this directory, set `OPEN_FIELD` to that checkout’s absolute path and run the following in a shell.

```sh
OUT=$(mktemp -d)
python build.py "$OPEN_FIELD" "$OPEN_FIELD/open-xy/local/toolchain" "$OUT"
sha256sum "$OUT/split.pack"
```

Release SHA-256 is `6adc6f5e6ddd438080e1d1850c54a6bf8b825240f1c762429edc247df996d5f8`.

[Open-Field SDK and build instructions](https://github.com/LaP0573/open-field/tree/bb901bbe037db4114d294b8e3c4a8f9a7ddf28aa)
