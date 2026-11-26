# SID v8

SID is a chip-style synthesizer with blended waveforms, oscillator sync and ring modulation, a resonant filter, and the DIP Beetle display.

| Encoder | Turn | Press |
| --- | --- | --- |
| Blue | **Wave** blends the oscillator shapes. | **Stack** cycles Tight, Sub and Fifth. |
| Ochre | **Duty** sets pulse width. | **Link** cycles Off, Sync and Ring. |
| Gray | **Cutoff** sets the filter frequency. | **Response** cycles low-pass, band-pass and high-pass. |
| Red | **Circuit** increases drive and adjusts the selected waveform's direct path. | **Route** cycles Blend, Pulse and Wire. |

Hold Shift while turning for finer adjustment. The factory bank contains Ceramic, Elastic Sync, Glass Wire, Pocket Bass, Small Lantern, Pulse Clavi, Foil Ticks and Raster Lead.

The four main controls use the beetle illustration as their readout. A press opens the corresponding mode card.

## Build

Set `SDK` to the absolute path of the Open-Field 0.20 checkout at `25a8d7f524ec3f46902cbd8a96779df32fa5e9bc` with its Blackfin toolchain installed. Run the commands from this directory. [Open-Field build guide](https://github.com/LaP0573/open-field/blob/25a8d7f524ec3f46902cbd8a96779df32fa5e9bc/docs/build.md)

```sh
OUT=$(mktemp -d)
sh build.sh "$SDK" "$OUT/sid.pack"
sha256sum "$OUT/sid.pack"
```

Expected SHA-256 for the 44,395-byte pack is `efddad065b91c49ad3dd37413ab6660c6c1ddf81cc307f0b5504ebc9d41093e5`.
