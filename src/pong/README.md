# Pong v10

Pong is a stereo delay. Table sets the delay time in milliseconds or a beat division. Rally sets the repeat count, Spin moves the stereo image, and Input sets the send level. Closing Input lets existing echoes decay.

| Encoder | Turn | Press |
| --- | --- | --- |
| Blue | Table | Switch between MS and Sync |
| Ochre | Rally | Select Foam, Wood, or Concrete |
| Gray | Spin | Select Singles or Doubles |
| Red | Input | Select 12-bit, 8-bit, or 4-bit texture |

Select Pong from a synth, drum, or master FX page. Hold Shift while turning for finer adjustment. The Table title and value use stock Delay’s 10 px letter height and two blue inks. Spin uses a needle and Input uses a 12-segment meter. The authored FX Off card is part of the main illustration.

## Build

Use the Open-Field SDK with its Blackfin toolchain installed. Open-Field 0.20 commit `25a8d7f524ec3f46902cbd8a96779df32fa5e9bc` and 0.14 commit `bb901bbe037db4114d294b8e3c4a8f9a7ddf28aa` produced the same pack bytes. Set `SDK` to the absolute path of your checkout and run from this directory. [Open-Field build guide](https://github.com/LaP0573/open-field/blob/25a8d7f524ec3f46902cbd8a96779df32fa5e9bc/docs/build.md)

```sh
OUT=$(mktemp -d)
sh build.sh "$SDK" "$OUT/pong.pack"
cmp "$OUT/pong.pack" ../../packs/pong.pack
```

The v1.1 parent is Pong v9, 703,470 bytes with SHA-256 `7456892afd868ce7addbbd7aec0eeb8c7c6d79d674ce3b314aa24a45743bee7d`. Pong v10 is 703,470 bytes with SHA-256 `a902f79bb6aed1f4c705f17961cc847bf89ba62154ec7d439c2071ba5bc2b31a`.
