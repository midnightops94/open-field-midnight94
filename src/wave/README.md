# Wave v8

Wave is a wavetable synth with eight banks. It sweeps through a bank’s frames and can blend in a second oscillator. The screen shows the selected bank and the two traces.

| Encoder | Turn | Press |
| --- | --- | --- |
| Blue | **Wave** selects a position in the wavetable. | Cycles Hollow, Glass, Metal, Reed, Choir, Pulse, Octave and Broken. |
| Ochre | **Sweep** sets how far the motion travels across the wavetable. | Cycles Fall, Rise and Cycle. |
| Gray | **Pulse** sets the oscillator’s phase warp. | |
| Red | **Twin** blends in the second oscillator and increases detune and wavetable spread. | |

Hold Shift while turning for finer adjustment. The factory sounds are Clock Bass, Frozen Vowel, Hollow Init, Ice Window, Twin Wires, Reed Stab, Choir Haze and Broken Bells. Open-Field keeps preset files already on the Field, so earlier installs may also show the old prefixed names. [Open-Field preset handling](https://github.com/LaP0573/open-field/blob/bb901bbe037db4114d294b8e3c4a8f9a7ddf28aa/docs/level-9.md#your-engines-own-sounds)

## Build

Use Open-Field 0.14 commit `bb901bbe037db4114d294b8e3c4a8f9a7ddf28aa` and its documented Blackfin toolchain. Run these commands from this directory with a new output path.

```sh
sh build.sh /path/to/open-field /tmp/wave.pack
sha256sum /tmp/wave.pack
```

Expected SHA-256 is `15085d474ed88bafb06e948098a25e127b66379abc09a1237cde72aa9664cebf` (161,028 bytes). The build uses the wavetable assets, authored picture and eight preset JSON files under `source/`. [Open-Field build guide](https://github.com/LaP0573/open-field/blob/bb901bbe037db4114d294b8e3c4a8f9a7ddf28aa/docs/build.md)

## Wavetable origin

`midnightops94` created all eight banks and confirmed they may be publicly redistributed with Wave. Hollow, Glass and Metal came from the project’s additive prototype. Reed, Choir, Pulse, Octave and Broken came from the same author’s eight-bank audition. The packed sample and row-index files reconstruct all eight banks exactly. See [Wavetable assets](WAVETABLES.md) for their checksums.

Wave v8 redraws the same artwork while reusing unchanged trace coordinates and readouts. Host and relocated Blackfin replays matched v7 audio bit for bit. The pack author reported playing Wave v8 on the Field without the switch-off error seen in an earlier trial. [Open-Field render cost and protection](https://github.com/LaP0573/open-field/blob/bb901bbe037db4114d294b8e3c4a8f9a7ddf28aa/docs/level-9.md#the-cost-you-declare)
