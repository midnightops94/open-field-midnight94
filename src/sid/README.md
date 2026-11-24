# SID v8

SID is a chip-style synthesizer with blended waveforms, oscillator sync and ring modulation, a resonant filter, and the DIP Beetle display.

| Encoder | Turn | Press |
| --- | --- | --- |
| Blue | **Wave** blends the oscillator shapes. | **Stack** cycles Tight, Sub and Fifth. |
| Ochre | **Duty** sets pulse width. | **Link** cycles Off, Sync and Ring. |
| Gray | **Cutoff** sets the filter frequency. | **Response** cycles low-pass, band-pass and high-pass. |
| Red | **Circuit** increases drive and adjusts the selected waveform's direct path. | **Route** cycles Blend, Pulse and Wire. |

Hold Shift while turning for finer adjustment. The factory bank contains Ceramic, Elastic Sync, Glass Wire, Pocket Bass, Small Lantern, Pulse Clavi, Foil Ticks and Raster Lead.

## Build

Use Open-Field 0.14 commit `bb901bbe037db4114d294b8e3c4a8f9a7ddf28aa` and its documented Blackfin toolchain. Run from this directory with a new output path.

```sh
sh build.sh /path/to/open-field /tmp/sid.pack
sha256sum /tmp/sid.pack
```

Expected SHA-256 is `efddad065b91c49ad3dd37413ab6660c6c1ddf81cc307f0b5504ebc9d41093e5` (44,395 bytes). The fixed-point DSP tables, picture and all eight editable preset JSON files are in `source/`. Text is rendered by the base on the device. [Open-Field build guide](https://github.com/LaP0573/open-field/blob/bb901bbe037db4114d294b8e3c4a8f9a7ddf28aa/docs/build.md) · [Open-Field MIT license](https://github.com/LaP0573/open-field/blob/bb901bbe037db4114d294b8e3c4a8f9a7ddf28aa/LICENSE)
