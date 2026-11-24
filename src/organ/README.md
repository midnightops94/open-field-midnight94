# Organ v5

Organ is a drawbar synthesizer with four generator models, two drive and percussion circuits, and a stereo cabinet with a shared rotor clock.

| Encoder | Turn | Press |
| --- | --- | --- |
| Blue | **Tone** shapes the generator from dark to bright. | **Type** cycles Hammond, Vox, Farfisa and Church. |
| Ochre | **Low** sets the low drawbars. | **Drive** cycles Off, 1, 2 and 1+2. |
| Gray | **Mid** sets the middle drawbars. | **Percussion** cycles Off, 1, 2 and 1+2. |
| Red | **High** sets the high drawbars. | **Spin** cycles Stop, Slow and Fast. |

Hold Shift while turning for finer adjustment. The factory bank contains Compact, Continental, Gospel, Init, Pipe, Rotor Cabinet, Chapel Air and Garage Reed. Chapel Air has a soft Church sound with the rotor stopped. Garage Reed has a bright Farfisa sound with the rotor fast.

## Build

Use Open-Field 0.14 commit `bb901bbe037db4114d294b8e3c4a8f9a7ddf28aa` and its documented Blackfin toolchain. Run from this directory with a new output path.

```sh
sh build.sh /path/to/open-field /tmp/organ.pack
sha256sum /tmp/organ.pack
```

Expected SHA-256 is `339834573b8e6e52d20706dd74320b19a4cc8b1db8f6f00ebaabe1eff472e173` (266,206 bytes). The build uses the published Open-Field card API and generates the Q15 wave data from the project's Organ tables. The editable picture and eight preset JSON files are in `source/`. Text is rendered by the base on the device. [Open-Field build guide](https://github.com/LaP0573/open-field/blob/bb901bbe037db4114d294b8e3c4a8f9a7ddf28aa/docs/build.md) · [Open-Field MIT license](https://github.com/LaP0573/open-field/blob/bb901bbe037db4114d294b8e3c4a8f9a7ddf28aa/LICENSE)
