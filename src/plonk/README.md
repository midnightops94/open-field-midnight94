# Plonk v7

Plonk is a percussive synthesizer with wavefolding and a spring effect. The screen’s ribbons follow Fold and Ratio. Chance introduces variation between strikes.

| Encoder | Turn | Press |
| --- | --- | --- |
| Blue | Fold | Select Sine, Triangle or Soft Square |
| Ochre | Ratio | Step through 0.50, 1.00, 1.50, 2.00, 3.00 and 4.00 |
| Gray | Strike | Select Spring Off, Short or Long |
| Red | Chance | |

Hold Shift while turning for finer control. The internal Spring runs inside the engine, leaving the instrument’s T3 effect available.

## Factory presets

Aquatic, Brass Rivet, Glass Droplets, Init, Low Tine, Rubber Bass, Tank Bell and Wood Block. Each preset stores volume 75.

## Build

Use Linux, Python 3 and the Blackfin toolchain configured in an Open-Field 0.14 checkout at commit `bb901bbe037db4114d294b8e3c4a8f9a7ddf28aa`. The pack targets OP-1 Field OS 1.7.3+0 and Open-Field base v32. [Open-Field SDK](https://github.com/LaP0573/open-field/tree/bb901bbe037db4114d294b8e3c4a8f9a7ddf28aa)

From this directory, run the following commands with `SDK` set to that checkout.

```sh
sha256sum -c SHA256SUMS
sh build.sh "$SDK" /tmp/plonk.pack
sha256sum /tmp/plonk.pack
```

The released pack SHA-256 is `f2854cf0debae6ea50c42f78953c7e2907533854615fe6058a042b95025ae78e`. The `source/` directory contains the engine, page, editable SVG, build recipe, factory presets and tables used in that build.
