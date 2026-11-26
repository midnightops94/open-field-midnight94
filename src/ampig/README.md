# Ampig v11

Ampig adds stereo amp saturation and models supply sag. Hold a note or loop with Drive, Sag and Mix at `99` to hear repeating supply dropouts. Press Drive to choose Valve, Diode or Starved. Press Sag to choose Flow, Fast or Slow recovery. [Controls and installation](../../START-HERE.md#try-ampig-meltdown)

`visuals/popup_art.py` builds the rounded frame, six icons and native text anchors. The Field’s own text function uses the same 15-pixel row geometry as Squash. The independently drawn `visuals/popup_letters.py` strokes appear when either text row is refused. The display callback changes the visible card and lettering group when a mode changes. The v10 Roadie drawing, sound and settings carry over. [Install and check](../../START-HERE.md) · [Popup preview](../../validation/deltas/ampig-v11-popups-preview.png)

## Rebuild

Use the Open-Field 0.20 SDK at commit `25a8d7f524ec3f46902cbd8a96779df32fa5e9bc`. Install its Blackfin compiler, set `SDK` to the checkout path and run from this directory. [SDK build instructions](https://github.com/LaP0573/open-field/blob/25a8d7f524ec3f46902cbd8a96779df32fa5e9bc/docs/build.md)

```sh
out=$(mktemp -d)
python3 -B build.py "$SDK" "$SDK/open-xy/local/toolchain" "$out"
cmp "$out/ampig.pack" ../../packs/ampig.pack
```

The v11 pack is 156,026 bytes, SHA-256 `7546d16ff2a7439d72585f6f45bc078ec9e5de1fcf85fb93b2fb3024b962af7d`. [Checksums](../../SHA256SUMS) · [Open-Field 0.24 validation](../../validation/open-field-0.24.json)
