# Organ v7

Organ is a drawbar synthesizer with four generator models, two drive and percussion circuits, and a stereo cabinet with a shared rotor clock.

| Encoder | Turn | Press |
| --- | --- | --- |
| Blue | **Tone** shapes the generator from dark to bright. | **Type** cycles Hammond, Vox, Farfisa and Church. |
| Ochre | **Low** sets the low drawbars. | **Drive** cycles Off, 1, 2 and 1+2. |
| Gray | **Mid** sets the middle drawbars. | **Percussion** cycles Off, 1, 2 and 1+2. |
| Red | **High** sets the high drawbars. | **Spin** cycles Stop, Slow and Fast. |

Hold Shift while turning for finer adjustment. The factory bank contains Compact, Continental, Gospel, Init, Pipe, Rotor Cabinet, Chapel Air and Garage Reed. Chapel Air has a soft Church sound with the rotor stopped. Garage Reed has a bright Farfisa sound with the rotor fast.

The drawbars and selected rocker switches use the four active encoder colors from the Field’s stock FM screen. Type, Drive, Percussion and Spin show their own selections when you press an encoder.

## Build

Set `SDK` to the absolute path of the Open-Field 0.20 checkout at `25a8d7f524ec3f46902cbd8a96779df32fa5e9bc` with its Blackfin toolchain installed. Run the commands from this directory. [Open-Field build guide](https://github.com/LaP0573/open-field/blob/25a8d7f524ec3f46902cbd8a96779df32fa5e9bc/docs/build.md)

```sh
OUT=$(mktemp -d)
sh build.sh "$SDK" "$OUT/organ.pack"
sha256sum "$OUT/organ.pack"
```

Expected SHA-256 for the 161,898-byte pack is `4db08b447cb5175d7d5efef4d147fed7a22d154e77817729e5336da5fa874ec6`.
