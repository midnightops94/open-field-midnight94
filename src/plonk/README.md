# Plonk v8

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

Fold, Strike and Chance show their effective values on a two-digit `00`–`99` scale. Ratio displays the oscillator frequency ratio.

## Build

Set `SDK` to the absolute path of the Open-Field 0.20 checkout at `25a8d7f524ec3f46902cbd8a96779df32fa5e9bc` with its Blackfin toolchain installed. Run the commands from this directory. [Open-Field build guide](https://github.com/LaP0573/open-field/blob/25a8d7f524ec3f46902cbd8a96779df32fa5e9bc/docs/build.md)

```sh
OUT=$(mktemp -d)
sh build.sh "$SDK" "$OUT/plonk.pack"
sha256sum "$OUT/plonk.pack"
```

Expected SHA-256 for the 58,582-byte pack is `fd8a04e80fd5543959de5d0ff956c3548bcb08b69c2ecaa3d7cb78752df814c2`.
