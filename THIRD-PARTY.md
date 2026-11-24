# Credits and source terms

The Field Engine Lab synths, effects and device illustrations are published by `midnightops94`. The source directories contain the native DSP and UI, the inputs used to build each pack, and the corresponding build instructions.

The Field Engine Lab team created all eight Wave wavetable banks and confirmed that they may be redistributed in this release. [Wave source and build](src/wave)

The packs use the [Open-Field SDK](https://github.com/LaP0573/open-field/tree/bb901bbe037db4114d294b8e3c4a8f9a7ddf28aa). Open-Field’s MIT notice applies to its code. This repository includes the [upstream license](LICENSES/Open-Field-MIT.txt). The original pack materials covered by this project’s license are available under the [midnight94 Pack License](LICENSE).

The seven effects request the FX Off card through Open-Field’s picture API. The Field supplies the installed artwork locally. Each effect includes a self-contained fallback card for installations where that picture is unavailable. The packs also use the Field’s installed text renderer.
