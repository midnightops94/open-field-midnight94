# Field Engine Lab for Open-Field

Synth engines and stereo effects for the Teenage Engineering OP-1 Field, by `midnightops94`. Download each `.pack` individually. Each device’s source page describes its encoder controls, press actions and build steps. Synth source pages also list their factory sounds.

## Compatibility

This release targets **OP-1 Field OS 1.7.3**, **Open-Field 0.14**, and **`base.pack` v32**. The maintainer confirmed the release trial works on an OP-1 Field running **OS 1.7.3+0**. Each pack uses Open-Field’s level-10 engine or effect API and requires no other feature pack. [Open-Field 0.14 SDK](https://github.com/LaP0573/open-field/tree/bb901bbe037db4114d294b8e3c4a8f9a7ddf28aa)

## Synth engines

| Device | Pack | Size | Sound |
| --- | --- | ---: | --- |
| [Bark](src/bark) | [v10](../../raw/main/packs/bark.pack) | 82,223 bytes | Four electric pianos with selectable strike contacts and tremolo. |
| [Fret](src/fret) | [v7](../../raw/main/packs/fret.pack) | 64,588 bytes | Electric bass with selectable hands, strings, pickups, mutes and amps. |
| [Organ](src/organ) | [v5](../../raw/main/packs/organ.pack) | 266,206 bytes | Four drawbar organ models with percussion, drive and a stereo rotary cabinet. |
| [Plonk](src/plonk) | [v7](../../raw/main/packs/plonk.pack) | 58,638 bytes | Percussive wavefolding synth with ratio detents and a short or long spring. |
| [SID](src/sid) | [v8](../../raw/main/packs/sid.pack) | 44,395 bytes | Chip-style synth with sync, ring modulation and a multimode filter. |
| [Wave](src/wave) | [v8](../../raw/main/packs/wave.pack) | 161,028 bytes | Eight original wavetable banks with sweep, pulse shaping and twin oscillators. |

Each engine ships with eight factory sounds. Fret’s factory sounds start one or two octaves below the default register.

## Stereo effects

| Device | Pack | Size | Sound |
| --- | --- | ---: | --- |
| [Cube](src/cube) | [v5](../../raw/main/packs/cube.pack) | 17,454 bytes | Three-band stereo resonator with wood, voice and metal responses. |
| [Pong](src/pong) | [v6](../../raw/main/packs/pong.pack) | 1,349,157 bytes | Stereo delay with tempo sync, feedback materials and a variable stereo image. |
| [Sag](src/sag) | [v4](../../raw/main/packs/sag.pack) | 86,669 bytes | Stereo amp coloration with dynamic supply sag and selectable recovery. |
| [Skatter](src/skatter) | [v6](../../raw/main/packs/skatter.pack) | 149,335 bytes | Stereo granular effect with capture and three playback directions. |
| [Split](src/split) | [v4](../../raw/main/packs/split.pack) | 55,777 bytes | Two-band saturation with independent low and high drive. |
| [Squash](src/squash) | [v3](../../raw/main/packs/squash.pack) | 58,271 bytes | Linked stereo compression with selectable detector, knee and timing. |
| [Syrup](src/syrup) | [v8](../../raw/main/packs/syrup.pack) | 270,997 bytes | Sixth-order Butterworth low-pass filter with resonance and drive. |

Try Syrup or Skatter on a drum sequence. Assign an LFO to Cube’s Morph or Reso to move through its resonances.

## Install and try

1. With Open-Field installed, download the pack you want. In the [Open-Field installer](https://lap0573.github.io/open-field/), connect your Field, open the packs section, and use **+** to select the `.pack`. Send the change and confirm it on the Field with **T1**.
2. If you transfer files by hand, copy them into `packs` in disk mode, then leave disk mode and restart. In the bootloader’s full-drive MTP mode, the folder is `user/pk`. Keep `base.pack` in place.
3. Open the synth selector for an engine or the FX selector on a synth, drum or master page for an effect. Hold **Shift** and press a sound key to browse synth presets.

If you used a prerelease, back up `user/synth_presets/<engine>` before refreshing the factory sounds. Open-Field seeds missing files once per pack version, and keeps existing files. If it has already seeded that version, copy the desired factory JSON from the engine’s source directory into the affected folder by hand. Keep your own sounds and edits, and load the refreshed sound by name. [Factory sound seeding](https://github.com/LaP0573/open-field/blob/bb901bbe037db4114d294b8e3c4a8f9a7ddf28aa/docs/level-9.md#your-engines-own-sounds)

To remove a pack, remove its `.pack` in the installer or disk mode and restart. Hold **Help** while powering on to start the Field with packs disabled. [Adding, removing and safe start](https://github.com/LaP0573/open-field/blob/main/docs/guide.md#adding-a-feature)

## Build and verify

The source pages pin the Open-Field SDK and identify the Blackfin toolchain and build command for each device. The published sources rebuilt the distributed binaries byte for byte. The Open-Field pack checker validated their headers and cards. [Validation and scope](VALIDATION.md) · [Pack checksums](SHA256SUMS)

From a checkout of this repository, run `sha256sum -c SHA256SUMS` to verify a full clone. To inspect a single pack from an Open-Field checkout, run `python -I tools/pack_build.py check --pack /path/to/bark.pack`.

## Unofficial project

These are independent community packs. Open-Field’s own notice says it is unofficial, unsupported by Teenage Engineering and unaffiliated with them. Read the quoted notices and the upstream installation guidance before installing. [Notice](NOTICE.md) · [Credits and source terms](THIRD-PARTY.md)
