# Fret v7

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

## Build

Use Linux, Python 3 and the Blackfin toolchain configured in an Open-Field 0.14 checkout at commit `bb901bbe037db4114d294b8e3c4a8f9a7ddf28aa`. The pack targets OP-1 Field OS 1.7.3+0 and Open-Field base v32. [Open-Field SDK](https://github.com/LaP0573/open-field/tree/bb901bbe037db4114d294b8e3c4a8f9a7ddf28aa)

From this directory, run the following commands with `SDK` set to that checkout.

```sh
sha256sum -c SHA256SUMS
sh build.sh "$SDK" /tmp/fret.pack
sha256sum /tmp/fret.pack
```

The released pack SHA-256 is `38791f4422d7ac590223caa2ab9dd59a37a5dce19401811213f95e39926fffc1`. The `source/` directory contains the engine, page, editable SVG, build recipe, factory presets and tables used in that build.
