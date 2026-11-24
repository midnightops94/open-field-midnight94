# Syrup v8

Syrup is a sixth-order Butterworth low-pass filter with resonance and drive. It runs on OP-1 Field OS 1.7.3+0 with Open-Field 0.14, base 32. Select Syrup from a synth, drum or master FX page.

## Controls

Freq sweeps the cutoff and rotates the bottle. Res increases the resonance and the pour. Drive adds 0.0–18.0 dB of saturation and pinches the bottle. Mix blends the filtered signal with the input and colors the plate. The gauge, plate, and values follow LFO motion.

The FX Off card reads the frame, tile and lettering from the Field’s installed factory picture through Open-Field’s public picture API. A self-contained drawn card supplies the alternate display when that picture differs or is unavailable.

## Build

Use Python 3 with NumPy and svgpathtools for the bottle geometry. Use the Open-Field SDK at `bb901bbe037db4114d294b8e3c4a8f9a7ddf28aa` with its Blackfin toolchain set up under `open-xy/local/toolchain`. From this directory, set `OPEN_FIELD` to that checkout’s absolute path and run the following in a shell.

```sh
OUT=$(mktemp -d)
python build.py --sdk "$OPEN_FIELD" --build "$OUT/syrup-objects" --out "$OUT/syrup.pack"
sha256sum "$OUT/syrup.pack"
```

Release SHA-256 is `5afdcffaeddb074c91cebecdd8fe5efe3ba2692eeed009cfdec9af0262143244`.

[Open-Field SDK and build instructions](https://github.com/LaP0573/open-field/tree/bb901bbe037db4114d294b8e3c4a8f9a7ddf28aa)
