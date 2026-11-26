# Rebuild Midnight94 v0.14 from source

The 13 projects are in `src/<device>/`. Open-Field 0.24/base v42 is the primary runtime host. To reproduce these exact pack files, use Python 3 and Open-Field 0.20 SDK commit `25a8d7f524ec3f46902cbd8a96779df32fa5e9bc` with its installed Blackfin toolchain. Syrup’s build also uses NumPy and svgpathtools. Each device directory has its own build instructions. [Open-Field SDK](https://github.com/LaP0573/open-field/tree/25a8d7f524ec3f46902cbd8a96779df32fa5e9bc) · [Wave wavetable provenance](src/wave/WAVETABLES.md)

From the extracted release, set `SDK` to the absolute path of the checkout and `DEVICE` to a lowercase pack ID, such as `ampig`.

```sh
RELEASE=$(pwd)
OUT=$(mktemp -d)
cd "$RELEASE/src/$DEVICE"
```

| Devices | Build command |
| --- | --- |
| Bark | `python3 build.py --sdk "$SDK" --out "$OUT/bark.pack"` |
| Fret, Organ, Plonk, Pong, SID, Wave | `sh build.sh "$SDK" "$OUT/$DEVICE.pack"` |
| Ampig, Cube, Skatter, Split, Squash | `python3 build.py "$SDK" "$SDK/open-xy/local/toolchain" "$OUT"` |
| Syrup | `python3 build.py --sdk "$SDK" --build "$OUT/syrup-objects" --out "$OUT/syrup.pack"` |

Compare the output with the distributed pack and run the SDK checker.

```sh
cmp "$OUT/$DEVICE.pack" "$RELEASE/packs/$DEVICE.pack"
python3 -I "$SDK/tools/pack_build.py" check --pack "$OUT/$DEVICE.pack"
cd "$RELEASE"
sha256sum -c SHA256SUMS
```

The release receipts identify the source rebuild and pack-file hashes. The separate published-loader replay also checks that every required import resolves during startup. [Source rebuild](validation/source-rebuild.json) · [Pack manifest](validation/pack-manifest.json) · [Open-Field 0.24 boot replay](validation/open-field-0.24.json)
