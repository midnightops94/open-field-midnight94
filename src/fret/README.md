# Fret v9

Fret models an electric bass. Its screen shows the strings, moving pickup and damper. Press an encoder to open its mode card.

| Encoder | Turn | Press |
| --- | --- | --- |
| Blue | Hand, from Thumb through Finger and Pick to Slap | Roundwound, Flatwound or Steel |
| Ochre | Pickup position from neck to bridge | Single Coil, Split Coil or Twin Coil |
| Gray | Mute amount | Palm, Foam or Gate |
| Red | Amplifier drive | DI, Tube or Fuzz |

Hold Shift while turning for finer control. Pocket, Deep, Growl and Broken Cab recall two octaves down. Rubber, Clank, Pop and Slide recall one octave down.

## Factory presets

Pocket, Rubber, Deep, Clank, Pop, Growl, Broken Cab and Slide. Each preset stores volume 75.

Position, Mute and Amp show `00` through `99`, including LFO modulation. Hand shows one of four playing poses. Pickup position and damping also move the corresponding parts of the bass illustration.

## Build

Set `SDK` to the absolute path of the Open-Field 0.20 checkout at `25a8d7f524ec3f46902cbd8a96779df32fa5e9bc` with its Blackfin toolchain installed. Run the commands from this directory. [Open-Field build guide](https://github.com/LaP0573/open-field/blob/25a8d7f524ec3f46902cbd8a96779df32fa5e9bc/docs/build.md)

```sh
OUT=$(mktemp -d)
sh build.sh "$SDK" "$OUT/fret.pack"
sha256sum "$OUT/fret.pack"
```

Expected SHA-256 for the 64,220-byte pack is `cffdb9701357144dbe80958ac010055c9d32be118413c0a871eb3991d27f11fd`.
