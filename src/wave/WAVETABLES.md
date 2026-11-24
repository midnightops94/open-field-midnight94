# Wave v8 wavetable assets

`midnightops94` created these banks and authorized their public redistribution in the Wave pack and its rebuild source. Hollow, Glass and Metal originated in the project’s additive prototype. Reed, Choir, Pulse, Octave and Broken were authored for its eight-bank audition.

| Build input under `source/core/native-banks/` | Bytes | SHA-256 |
| --- | ---: | --- |
| `packed-ppg-index.u32le` | 3,072 | `f359ce2e5549b0ed0d68cd7999f320da00bb9a7889e96e2c7d3071f95b9edb47` |
| `packed-ppg-pcm.s16le` | 80,630 | `bc71d3bb6c5773bf7ca9a4cc00117f43555ff76c8ef5b0c99c8120ea2977f3f7` |

`tables.cpp` reconstructs the phase edges while preparing the engine’s working memory, using the signed samples and each row’s recorded symmetry. The 196,608 reconstructed sample bytes in bank, mip level, frame and phase order have SHA-256 `6ca193f7a04ffb9a70699e0f68da7e2128ab49b3f75a6875d3e8e742be4e8dec`.
