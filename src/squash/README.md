# Squash v5

Squash is a linked stereo compressor. Lid sets the compression amount, Cushion shapes the knee, Timing sets the response and Mix blends the compressed and dry signals. All four controls display `00` through `99`. The Mix meter has twelve segments.

Press Lid to cycle Peaks, Body and Bass Safe detection. Press Cushion for Rubber, Hard or Pillow, Timing for Tight, Glue or Pump, and Mix for Cool or Hot.

Select Squash from a synth, drum or master FX page. Hold Shift while turning for finer adjustment.

## Build

Set `SDK` to the absolute path of the Open-Field 0.20 checkout at `25a8d7f524ec3f46902cbd8a96779df32fa5e9bc` with its Blackfin toolchain installed. Run the commands from this directory. [Open-Field build guide](https://github.com/LaP0573/open-field/blob/25a8d7f524ec3f46902cbd8a96779df32fa5e9bc/docs/build.md)

```sh
OUT=$(mktemp -d)
python3 build.py "$SDK" "$SDK/open-xy/local/toolchain" "$OUT"
sha256sum "$OUT/squash.pack"
```

Expected SHA-256 for the 59,243-byte pack is `ffafe18bb4aeae63cdc834ca9ab441d459081ed2f3c4795789a4b71b4bdcedc8`.
