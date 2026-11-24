# Squash v3

Squash is a linked stereo compressor with a choice of detector, knee and timing. It runs on OP-1 Field OS 1.7.3+0 with Open-Field 0.14, base 32. Select Squash from a synth, drum or master FX page.

## Controls

Lid sets the compression amount, Cushion shapes the knee, Timing sets the response and Mix blends the compressed and dry signals. Press Lid to cycle Peaks, Body and Bass Safe detection. Press Cushion for Rubber, Hard or Pillow, Timing for Tight, Glue or Pump, and Mix for Cool or Hot.

The FX Off card reads the frame, tile and lettering from the Field’s installed factory picture through Open-Field’s public picture API. A self-contained drawn card supplies the alternate display when that picture differs or is unavailable.

## Build

Use the Open-Field SDK at `bb901bbe037db4114d294b8e3c4a8f9a7ddf28aa` with its Blackfin toolchain set up under `open-xy/local/toolchain`. From this directory, set `OPEN_FIELD` to that checkout’s absolute path and run the following in a shell.

```sh
OUT=$(mktemp -d)
python build.py "$OPEN_FIELD" "$OPEN_FIELD/open-xy/local/toolchain" "$OUT"
sha256sum "$OUT/squash.pack"
```

Release SHA-256 is `a515f95a283114f61f6d24303b4a6331de025efe4840ebb0e9f863bec088064c`.

[Open-Field SDK and build instructions](https://github.com/LaP0573/open-field/tree/bb901bbe037db4114d294b8e3c4a8f9a7ddf28aa)
