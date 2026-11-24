# Pong v6

Pong is a stereo delay with selectable delay divisions, feedback materials and texture. It runs on OP-1 Field OS 1.7.3+0 with Open-Field 0.14, base 32. Select Pong from a synth, drum or master FX page.

## Controls

Court sets the delay time, Rally sets the repeat count, Spin moves the stereo image and Input sets the send level. Press those encoders to switch between milliseconds and tempo sync, cycle the wall material, choose singles or doubles, and select the texture. Closing Input lets existing echoes decay.

The FX Off card reads the frame, tile and lettering from the Field’s installed factory picture through Open-Field’s public picture API. A self-contained drawn card supplies the alternate display when that picture differs or is unavailable.

## Build

Use the Open-Field SDK at `bb901bbe037db4114d294b8e3c4a8f9a7ddf28aa` with its Blackfin toolchain set up under `open-xy/local/toolchain`. From this directory, set `OPEN_FIELD` to that checkout’s absolute path and run the following in a shell.

```sh
OUT=$(mktemp -d)
sh build.sh "$OPEN_FIELD" "$OUT/pong.pack"
sha256sum "$OUT/pong.pack"
```

Release SHA-256 is `f7bc5df47ea9e26994f1200c6ec144ba76fa003b4065221613231e3720a268e6`.

[Open-Field SDK and build instructions](https://github.com/LaP0573/open-field/tree/bb901bbe037db4114d294b8e3c4a8f9a7ddf28aa)
