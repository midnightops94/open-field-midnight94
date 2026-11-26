# Split v6

Split applies separate saturation to either side of a stereo crossover. Crossover sets the split frequency. Low Drive and High Drive shape their respective bands. Mix blends the colored and direct signals. All four controls display `00` through `99`.

Press Crossover for Soft or Focus, Low Drive for Round or Tube, High Drive for Clip or Fold, and Mix for Blend or Delta. Delta isolates the coloration as Mix rises.

Select Split from a synth, drum or master FX page. Hold Shift while turning for finer adjustment.

## Build

Set `SDK` to the absolute path of the Open-Field 0.20 checkout at `25a8d7f524ec3f46902cbd8a96779df32fa5e9bc` with its Blackfin toolchain installed. Run the commands from this directory. [Open-Field build guide](https://github.com/LaP0573/open-field/blob/25a8d7f524ec3f46902cbd8a96779df32fa5e9bc/docs/build.md)

```sh
OUT=$(mktemp -d)
python3 build.py "$SDK" "$SDK/open-xy/local/toolchain" "$OUT"
sha256sum "$OUT/split.pack"
```

Expected SHA-256 for the 56,709-byte pack is `3bad3c8bc62b44c2bccb82441f433431268b52cfccec7ba266aad81f8e06ba62`.
