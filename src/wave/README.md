# Wave v9

Wave is a wavetable synth with eight banks. It sweeps through a bank’s frames and can blend in a second oscillator. The screen shows the selected bank and the two traces.

| Encoder | Turn | Press |
| --- | --- | --- |
| Blue | **Wave** selects a position in the wavetable. | Cycles Hollow, Glass, Metal, Reed, Choir, Pulse, Octave and Broken. |
| Ochre | **Sweep** sets how far the motion travels across the wavetable. | Cycles Fall, Rise and Cycle. |
| Gray | **Pulse** sets the oscillator’s phase warp. | |
| Red | **Twin** blends in the second oscillator and increases detune and wavetable spread. | |

Hold Shift while turning for finer adjustment. The factory sounds are Clock Bass, Frozen Vowel, Hollow Init, Ice Window, Twin Wires, Reed Stab, Choir Haze and Broken Bells.

Wave and Sweep display amounts in wavetable frames. Pulse spans duty values `25` through `75`. Twin shows `00` through `99`. Its second trace begins to move at the first nonzero blend setting.

The Field Engine Lab team authored the eight wavetable banks for Wave and approved their redistribution with the pack and its source. [Wavetable assets](WAVETABLES.md)

## Build

Set `SDK` to the absolute path of the Open-Field 0.20 checkout at `25a8d7f524ec3f46902cbd8a96779df32fa5e9bc` with its Blackfin toolchain installed. Run the commands from this directory. [Open-Field build guide](https://github.com/LaP0573/open-field/blob/25a8d7f524ec3f46902cbd8a96779df32fa5e9bc/docs/build.md)

```sh
OUT=$(mktemp -d)
sh build.sh "$SDK" "$OUT/wave.pack"
sha256sum "$OUT/wave.pack"
```

Expected SHA-256 for the 161,084-byte pack is `01a5d236676bebd6a079c635dcb91d925cb91afcf72314630637fe0364602430`.
