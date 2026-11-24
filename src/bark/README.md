# Bark v10

Bark models four electric pianos. The central illustration shows the mechanism for the selected piano and moves with the controls.

| Encoder | Turn | Press |
| --- | --- | --- |
| Blue | Color | Select Rhodes, Wurli, Pianet or CP-70 |
| Ochre | Strike intensity | Cycle the piano’s original contact, Felt, Rubber and Wood |
| Gray | Bark | |
| Red | Tremolo | |

Hold Shift while turning for finer control. The original contact is Tine Hammer on Rhodes, Reed Hammer on Wurli, Sticky Pad on Pianet and String Hammer on CP-70. Play a new note to hear a contact change.

## Factory presets

Smoked Tines, Velvet Tide, Redline Reed, Reed Clav, Finger Pop, Rebound Bass, Stage Eighty and Felt Room. Each preset stores volume 75.

## Build

Use Linux, Python 3 and the Blackfin toolchain configured in an Open-Field 0.14 checkout at commit `bb901bbe037db4114d294b8e3c4a8f9a7ddf28aa`. The pack targets OP-1 Field OS 1.7.3+0 and Open-Field base v32. [Open-Field SDK](https://github.com/LaP0573/open-field/tree/bb901bbe037db4114d294b8e3c4a8f9a7ddf28aa)

From this directory, run the following commands with `SDK` set to that checkout.

```sh
sha256sum -c SHA256SUMS
sh build.sh "$SDK" /tmp/bark.pack
sha256sum /tmp/bark.pack
```

The released pack SHA-256 is `773267239b7070c23a1b5f4158524afc6fef1d6717e0eed46aac2893bd5c1add`. The `source/` directory contains the engine, page, editable SVG, build recipe, factory presets and tables used in that build.
