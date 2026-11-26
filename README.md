# Midnight94 Open-Field Packs v0.14

Six synth engines and seven stereo effects for the Teenage Engineering OP-1 Field, by **midnight94**. Download each device’s `.pack` individually. Its source page describes the controls and build steps. [Install or update](START-HERE.md) · [Illustrated quick reference](docs/OP-1-Field-Quick-Reference-v0.14.pdf)

## Compatibility

This collection targets **OP-1 Field OS 1.7.3+0** with **Open-Field 0.24**, **base v42** and the **N2 loader**. All 13 packs passed the official checker and loaded in production-source boot replays on both N1 and N2. The runtime checks used host replay and Blackfin simulation. [Open-Field installer](https://lap0573.github.io/open-field/) · [Test results and N1 limits](VALIDATION.md)

## Synth engines

| Device | Download | Size | Sound |
| --- | --- | ---: | --- |
| [Bark](src/bark) | [v12](https://raw.githubusercontent.com/midnightops94/open-field-midnight94/midnight94-v0.14/packs/bark.pack) | 82,239 bytes | Four electric pianos with selectable strike contacts and tremolo. |
| [Fret](src/fret) | [v9](https://raw.githubusercontent.com/midnightops94/open-field-midnight94/midnight94-v0.14/packs/fret.pack) | 64,220 bytes | Electric bass with selectable hands, strings, pickups, mutes and amps. |
| [Organ](src/organ) | [v7](https://raw.githubusercontent.com/midnightops94/open-field-midnight94/midnight94-v0.14/packs/organ.pack) | 161,898 bytes | Four drawbar organs with percussion, drive and a stereo rotary cabinet. |
| [Plonk](src/plonk) | [v8](https://raw.githubusercontent.com/midnightops94/open-field-midnight94/midnight94-v0.14/packs/plonk.pack) | 58,582 bytes | Percussive wavefolding synth with oscillator-ratio detents and a short or long spring. |
| [SID](src/sid) | [v8](https://raw.githubusercontent.com/midnightops94/open-field-midnight94/midnight94-v0.14/packs/sid.pack) | 44,395 bytes | Chip synth with oscillator sync, ring modulation and a multimode filter. |
| [Wave](src/wave) | [v9](https://raw.githubusercontent.com/midnightops94/open-field-midnight94/midnight94-v0.14/packs/wave.pack) | 161,084 bytes | Eight original wavetable banks with sweep, pulse shaping and twin oscillators. |

Each engine includes eight factory sounds. Fret’s factory sounds start one or two octaves below the default register.

## Stereo effects

| Device | Download | Size | Sound |
| --- | --- | ---: | --- |
| [Ampig](src/ampig) | [v11](https://raw.githubusercontent.com/midnightops94/open-field-midnight94/midnight94-v0.14/packs/ampig.pack) | 156,026 bytes | Stereo amp saturation and supply sag. Sustained sound at high Drive and Sag can cause repeating dropouts. |
| [Cube](src/cube) | [v7](https://raw.githubusercontent.com/midnightops94/open-field-midnight94/midnight94-v0.14/packs/cube.pack) | 18,290 bytes | Three-band stereo resonator with wood, voice and metal responses. |
| [Pong](src/pong) | [v10](https://raw.githubusercontent.com/midnightops94/open-field-midnight94/midnight94-v0.14/packs/pong.pack) | 703,470 bytes | Stereo delay with tempo sync, feedback materials and stereo spin. |
| [Skatter](src/skatter) | [v8](https://raw.githubusercontent.com/midnightops94/open-field-midnight94/midnight94-v0.14/packs/skatter.pack) | 150,207 bytes | Granular effect with capture and three playback directions. |
| [Split](src/split) | [v6](https://raw.githubusercontent.com/midnightops94/open-field-midnight94/midnight94-v0.14/packs/split.pack) | 56,709 bytes | Two-band saturation with independent low and high drive. |
| [Squash](src/squash) | [v5](https://raw.githubusercontent.com/midnightops94/open-field-midnight94/midnight94-v0.14/packs/squash.pack) | 59,243 bytes | Linked stereo compressor with selectable detector, knee and timing. |
| [Syrup](src/syrup) | [v11](https://raw.githubusercontent.com/midnightops94/open-field-midnight94/midnight94-v0.14/packs/syrup.pack) | 271,885 bytes | Sixth-order Butterworth low-pass filter with resonance and drive. |

Try Syrup or Skatter on a drum sequence. Assign an LFO to Cube’s Morph or Reso to move through its resonances. Ampig replaces the older Sag effect and uses its own picker entry. [Ampig controls and upgrade notes](START-HERE.md#try-ampig-meltdown)

## Source and documentation

Every published pack rebuilt byte for byte from its source here using the pinned Open-Field SDK. [Build guide](BUILD.md) · [Pack and manual checksums](SHA256SUMS) · [Validation](VALIDATION.md)

The seven-page manual covers encoder turns, presses and screen examples. Earlier public versions and their instructions remain available by revision. [Current PDF](docs/OP-1-Field-Quick-Reference-v0.14.pdf) · [Text reference](docs/QUICK-REFERENCE-v0.14.md) · [Changes](CHANGELOG.md) · [Previous public set for Open-Field 0.14](https://github.com/midnightops94/open-field-midnight94/tree/849eefc5e5dac26acee29cbb3b6a2782e3197c10) · [Previous PDF](https://github.com/midnightops94/open-field-midnight94/blob/849eefc5e5dac26acee29cbb3b6a2782e3197c10/docs/OP-1-Field-Quick-Reference-0.14.pdf)

## License

You can use the packs for paid performances and commercial music releases. You can study the source, build your own versions and share the originals or your forks for free. Keep the license, credit midnight94 visibly, link to this repository and clearly label modified builds. Selling copies or forks, charging for access or including the packs in a paid bundle requires written permission. [Full license](LICENSE) · [Third-party terms](THIRD-PARTY.md)

Open-Field and these packs are independent community projects unaffiliated with Teenage Engineering. [Notice](NOTICE.md)
