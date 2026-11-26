# Midnight94 v0.14 validation

The collection contains **1,988,248 bytes across 13 device packs** for OP-1 Field OS 1.7.3+0. Ampig v11 is **156,026 bytes** with SHA-256 `7546d16ff2a7439d72585f6f45bc078ec9e5de1fcf85fb93b2fb3024b962af7d`. The other twelve pack files match the refreshed v1.3 collection. All 13 were rebuilt from the included sources. [Pack manifest](validation/pack-manifest.json) · [Source rebuild](validation/source-rebuild.json)

## Loader and host

The primary target is **Open-Field 0.24/base v42 with loader N2**. The checks below ran in host replay, Blackfin simulation or browser tests. This release has no physical OP-1 Field test result. All 13 device packs passed the official pack checker. Both published loaders completed the simulated boot and passed the card handoff checks. The four optional packs tested together were 8 Tracks v11, FX Train v18, LFO v12 and Sidechain v10. Counts below include the base.

| Loadout | N1 result / bytes free | N2 result / bytes free |
| --- | --- | --- |
| 13 devices | All 14 loaded / 787,936 | All 14 loaded / 15,533,516 |
| 13 devices + four optional packs | All 18 loaded / 8,352 | All 18 loaded / 14,753,932 |
| Same set + Maestro v10 | 16 loaded / 1,764 | All 19 loaded / 14,592,876 |

With all five optional packs on N1, the loader left out Squash, Syrup and Wave. Each device also loaded on its own with base v42 on both loaders. The thirteen relocated cards passed the Blackfin registration checks with both loaders, with and without the four add-ons. Earlier 0.20/base v38 and 0.21/base v39 boot replays each loaded Ampig alone, the complete collection, and the collection with the four optional packs. [Base v42 results](validation/open-field-0.24.json) · [Individual loads](validation/open-field-0.24-standalone.json) · [Registration checks](validation/open-field-0.24-metadata.json) · [Base v38 and v39 results](validation/deltas/ampig-v11-boot.json)

Open-Field 0.24 base v42 works with the N1 and N2 loader generations. The current installer defaults to N2. N1 supports 2 MiB and 32 pack files. N2 supports 16 MiB and 128 files. Updating only the base leaves the existing loader generation installed. [Installer guide](https://github.com/LaP0573/open-field/blob/c65e40de0ef51f601bc0445daa85d906378dea52/docs/guide.md#L126-L132)

## Ampig display and audio

- Ampig v11 includes v10’s explicit cabinet paint, brighter low-Drive outline and repaired corner text anchors. The two-line Circuit and Recover cards each show a choice icon. Browser replay cycled all six choices in the stock-referenced and standalone pictures. It also checked native-font fallback when either text row was refused, popup expiry and FX Off. The v11 screen still needs a physical Field rendering check. [Display tests](validation/deltas/ampig-v11-popups.json) · [Six popup choices](validation/deltas/ampig-v11-popups-preview.png)
- The v10 and v11 callbacks produced the same PCM fingerprint and peak output across 3,200 Blackfin simulator calls per build on both base v38 and v39. The peak was 63,139 simulated cycles. The Meltdown sound retains its high-Drive saturation and the repeated supply dropouts under sustained high Drive and Sag. [v11 audio comparison](validation/deltas/ampig-v11-audio.json) · [Meltdown measurements](validation/deltas/ampig-v8-meltdown.json)

Wave v9’s exact N2-relocated image also ran in Blackfin simulation with an injected memory grant. The table initialized in 32 frames, the first eligible held-note render produced audio, and all eight factory sounds rendered with zero buffer errors. Physical memory allocation and timing remain unverified. [Wave N2 initialization](validation/open-field-0.24-wave-n2.json)

## FX Train v18

FX Train hosts effects on synth and drum sounds. A Midnight94 effect can occupy one of the three wagons away from the lamp. The lit wagon skips pack effects, and tapping a hosted pack effect does not open its custom page. Use the ordinary FX selector to open the device interface or use an effect on the master. FX Train may skip a choice that would exceed the train’s processing budget. In a separate base v39 simulator test, the seven included effects ran individually under FX Train’s hosted slot IDs. Playback of an entire custom-effect chain on a Field remains unverified. [Hosted callback tests](validation/fx-train-hosted.json) · [FX Train v18 instructions](https://github.com/LaP0573/open-field-packs/blob/c006c15dbdf300a820f80c2e0b51f01cd1c16c09/fx%20train/README.md)

The illustrated manual checks cover seven pages, alphabetical device order within the synth and effect sections, and the updated Ampig illustration. Installation instructions are in Start Here. [Manual verification](validation/manual.json) · [Installation](START-HERE.md)

## Publication review

The previous public release records its publication checks and fixtures at its pinned revision. The current pack handoff includes authored or authorized sources and uses on-device references for shared stock FX Off artwork where documented. [Previous publication review](https://github.com/midnightops94/open-field-midnight94/blob/849eefc5e5dac26acee29cbb3b6a2782e3197c10/VALIDATION.md#publication-review) · [Open-Field publication check](https://github.com/LaP0573/open-field/blob/378eb86a334c29e5a34acf6e06bedd377b11e4cd/docs/checking.md)
