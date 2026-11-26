# Syrup v11

Syrup is a sixth-order Butterworth low-pass filter with resonance and drive. Freq sweeps the cutoff and rotates the bottle. Res increases the resonance and the pour. Drive adds saturation and pinches the bottle. Mix blends the filtered signal with the input and colors the plate.

Freq uses a needle gauge in the same light gray (`#CDD0C2`) as the Res and Drive values. Res displays `00` through `99`, Drive reads from `0.0` to `18.0` in tenths of a decibel, and Mix uses a bar graph. The gauge, plate and values follow LFO modulation.

Select Syrup from a synth, drum or master FX page. Hold Shift while turning for finer adjustment.

## Build

Install NumPy and svgpathtools in the Python 3 environment used for the build. Set `SDK` to the absolute path of the Open-Field 0.20 checkout at `25a8d7f524ec3f46902cbd8a96779df32fa5e9bc` with its Blackfin toolchain installed. Run the commands from this directory. [Open-Field build guide](https://github.com/LaP0573/open-field/blob/25a8d7f524ec3f46902cbd8a96779df32fa5e9bc/docs/build.md)

```sh
OUT=$(mktemp -d)
python3 build.py --sdk "$SDK" --build "$OUT/syrup-objects" --out "$OUT/syrup.pack"
cmp "$OUT/syrup.pack" ../../packs/syrup.pack
```

The 271,885-byte v11 pack has SHA-256 `4be2788cab1446e8028f4680901167bb9ed4e34eb4941617ebd45eb46390a33b`. The exact v1.1 Syrup v10 baseline is the same size with SHA-256 `26aaed537dec798595795c905247c8f067b5bbcb0fc1126a4f4fdde8ed72a0d8`. The Open-Field 0.14 checkout at `bb901bbe037db4114d294b8e3c4a8f9a7ddf28aa` produces the same v11 pack bytes.
