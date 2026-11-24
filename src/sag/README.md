# Sag v4

Sag is a stereo amplifier effect whose supply voltage drops as the signal demands more current. It runs on OP-1 Field OS 1.7.3+0 with Open-Field 0.14, base 32. Select Sag from a synth, drum or master FX page.

## Controls

Drive pushes the input, Sag controls supply reduction, Tone shapes the speaker response and Mix sets the wet/dry balance. Press Drive to choose Valve, Diode or Starved. Press Sag to select Flow, Fast or Slow recovery. The amp and speaker animation follows the signal and controls.

The FX Off card reads the frame, tile and lettering from the Field’s installed factory picture through Open-Field’s public picture API. A self-contained drawn card supplies the alternate display when that picture differs or is unavailable.

## Build

Use the Open-Field SDK at `bb901bbe037db4114d294b8e3c4a8f9a7ddf28aa` with its Blackfin toolchain set up under `open-xy/local/toolchain`. From this directory, set `OPEN_FIELD` to that checkout’s absolute path and run the following in a shell.

```sh
OUT=$(mktemp -d)
python build.py "$OPEN_FIELD" "$OPEN_FIELD/open-xy/local/toolchain" "$OUT"
sha256sum "$OUT/sag.pack"
```

Release SHA-256 is `41af9c4b38145fabc1e43a905e26b97d2617262240cce7a1a9e5ca377f6f32b2`.

[Open-Field SDK and build instructions](https://github.com/LaP0573/open-field/tree/bb901bbe037db4114d294b8e3c4a8f9a7ddf28aa)
