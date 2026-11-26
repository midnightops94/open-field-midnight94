# Bark v12

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

Color, Strike, Bark and Trem display `00` through `99` and follow LFO modulation. The eight factory sounds include the revised Felt Room. Open-Field preserves an existing same-name preset on upgrade. The release’s Felt Room preset is in `source/felt room.json`.

## Build

Set `SDK` to the absolute path of the Open-Field 0.20 checkout at `25a8d7f524ec3f46902cbd8a96779df32fa5e9bc` with its Blackfin toolchain installed. Run the commands from this directory. [Open-Field build guide](https://github.com/LaP0573/open-field/blob/25a8d7f524ec3f46902cbd8a96779df32fa5e9bc/docs/build.md)

```sh
OUT=$(mktemp -d)
python3 build.py --sdk "$SDK" --out "$OUT/bark.pack"
sha256sum "$OUT/bark.pack"
```

Expected SHA-256 for the 82,239-byte pack is `1e937f83d91ce3b95ebfbeb3b705780760e308409bf06d9126785b3600cc5d50`.
