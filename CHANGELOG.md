# Release notes

## Midnight94 v0.14

- **Ampig v11** puts each Circuit and Recover choice on a rounded, two-line popup with its own icon. It includes v10’s Field-oriented repair to the corner labels, numbers and cabinet outline. The v11 audio replay matches v10 on bases v38 and v39. [Popup preview](validation/deltas/ampig-v11-popups-preview.png) · [Audio comparison](validation/deltas/ampig-v11-audio.json)
- The full collection targets **Open-Field 0.24/base v42 with loader N2** on OS 1.7.3+0. It contains the same twelve other device files as the refreshed v1.3 bundle. Earlier supported host pairs are 0.20/base v38 and 0.21/base v39. [Validation](VALIDATION.md)
- The seven-page manual includes the current Ampig drawing and keeps the devices in alphabetical order within each section. Installation and rollback stay in the separate guide. [Illustrated manual](docs/OP-1-Field-Quick-Reference-v0.14.pdf) · [Install or roll back](START-HERE.md)

## v1.3 refresh

- **Ampig v9 boot repair.** The original v1.3 package contained Ampig v8. The published M4 boot loader rejected v7 and v8 in replay because an unsigned-division helper was declared as an unresolved import. v9 supplies the helper locally. Its audio output and peak cycle count matched v8 in the Blackfin comparison on both tested bases. The other twelve device files match the original v1.3 bundle byte for byte. [Runtime validation](VALIDATION.md)
- **Reordered illustrated manual.** The manual now lists synths alphabetically, followed by effects alphabetically. The separate Start Here file covers installation, upgrades and recovery. [Current manual](docs/OP-1-Field-Quick-Reference-v0.14.pdf) · [Start Here](START-HERE.md)

## Original v1.3

- **Ampig v8 Meltdown.** Drive reaches about 4.7 dB more gain before the circuit at `99` than in v7. High Sag deepens the wet signal’s drop under load. With both controls at `99`, sustained overload repeatedly trips and restarts the modeled supply. Press Ochre to choose Flow, Fast or Slow recovery. [Ampig audition](START-HERE.md#try-ampig-meltdown)
- **Full collection.** All 13 install files are included. The other 12 match v1.2 byte for byte. Ampig keeps the `ampig` effect ID and `ampig_mode` settings key introduced in v1.2.
- **Updated illustrated manual.** The v1.3 edition covers the current pack versions, Ampig’s Roadie controls, Pong’s Table timing and Syrup’s Freq gauge. [Current manual](docs/OP-1-Field-Quick-Reference-v0.14.pdf)
- The initial SDK and placement checks passed on bases v38 and v39 but missed an unresolved import. The later boot replay reproduced Ampig v8’s rejection on both bases. [Validation](VALIDATION.md)

## v1.2

- **Ampig replaces Sag.** The new file and effect ID are `ampig.pack` and `ampig`. Drive adds stronger saturation to the audio. The Roadie combo cabinet shows input and cone movement, supply sag, grille opening and the wet/dry route. Blue presses select the input circuit and Ochre presses select recovery. Back up `sag.pack` and reselect Ampig for any sounds or effect chains that previously used Sag. [Installation](START-HERE.md)
- **Pong v10** renames Court to **Table** on the blue encoder, its popup and the corresponding LFO destination. The delay timing, repeats, stereo image and saved mode settings carry over. The MS and Sync readouts keep the stock Delay typography and blue inks.
- **Syrup v11** gives the Freq needle, ticks, center dot and endpoints the same light gray as the Res and Drive values. The Freq title stays blue to identify the encoder. Its filter, bottle animation and controls carry over.
- The set still has six synth engines and seven stereo effects. Syrup, Split and Skatter retain their names and filenames. [Device versions](README.md) · [Current manual](docs/OP-1-Field-Quick-Reference-v0.14.pdf)

## v1.1

- Bundles all 13 Midnight94 devices for Open-Field 0.20 and base v38. Preserves the same per-device install filenames, parameter settings, and synth preset identities.
- Standardizes 32 numeric readouts across the set to `00`–`99`, including Fret Position, Mute, and Amp. Specific units, ratios, counts, and meters retain their scales.
- Matches the effects’ encoder turns to the synths at approximately 3% per detent and 1% with Shift. Retains the existing click actions and modulation behavior.
- Revises Bark’s **Felt Room** voicing with softer strike and pickup drive and a lighter Spring mix. The other seven factory sounds stay the same.
- Compresses Organ’s wavetable losslessly and applies the stock FM control colors. The pack falls from 266,206 to 161,898 bytes.
- Stores Pong’s court once, with its own FX Off card, and matches the Court title and value to stock Delay. The pack falls from 1,349,157 to 703,470 bytes.
- Carries SID v8 forward byte for byte. All six synth engines include eight factory presets.

The v1.0 published release is the source and license baseline. The preceding public main snapshot documents the older Open-Field 0.14 host. [v1.0 release](https://github.com/midnightops94/open-field-midnight94/tree/2a5fc6ade1356409d2b7a3db6410314127de4486) · [Previous public main](https://github.com/midnightops94/open-field-midnight94/tree/849eefc5e5dac26acee29cbb3b6a2782e3197c10)
