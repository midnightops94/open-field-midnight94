# Release validation

## On a Field

The pack author confirmed the latest trial works on an OP-1 Field with OS 1.7.3+0, Open-Field 0.14 and base 32. The checksums identify the tested handoff files, including Organ v5 from the preset refresh and Wave v8 from the display update. The author reported that Wave v8 ran without the switch-off error seen in earlier trials. Hardware coverage comes from the author’s unit. The other checks listed here ran on a computer.

## Build and engine checks

- Each device rebuilt byte for byte from its published source directory and the pinned Open-Field SDK. The Open-Field pack checker accepted each binary and its cards. The binaries declare zero firmware patch sites and no additional feature-pack dependencies.
- Open-Field 0.14’s native card and metadata validators ran in a Blackfin simulator on these versions, both individually and in two combined loader layouts. With all 13 packs and base alone, the loader model used 1,207,784 of 2,031,616 resident bytes. With 8 Tracks, FX Train, LFO and Sidechain also loaded, it used 1,981,512 bytes, leaving 50,104 bytes. Pack memory depends on the full set you install.
- Each engine supplies eight factory patches. Factory checks cover engine selection, valid envelope ranges and volume 75. Fret’s presets use a bass register at −1 or −2 octaves. Organ v5 preserves the six earlier sounds and adds Chapel Air and Garage Reed.
- Organ’s relocated audio ran through 13,824 emulated render callbacks with polyphony and rotor changes. The test reported zero buffer faults and zero callbacks over the tested audio watchdog thresholds.
- Wave v8 reuses the computed display points when a trace is unchanged. In local comparisons with v7, the updated display drew the same lines and labels and the rendered audio matched sample for sample. Relocated tests exercised steady notes and rapid bank and motion changes.

## Publication review

The release was checked against an owner’s local OS 1.7.3 records and the stock picture used by the FX Off references. The checker flagged coincidental byte and drawing matches. Review traced the SVG matches to standard viewport attributes and the Bark match to compiled zero assignments in a text-fit matrix. The Fret audio table matches were reproduced from the equal-tempered tuning and sine equations. Pong’s match fell inside the generated delay-history decoder table. The effects’ stock picture instructions reference asset spans the device supplies at runtime. The upstream check against a maintainer’s complete bootloader and display reference set is separate. [Open-Field publication check](https://github.com/LaP0573/open-field/blob/378eb86a334c29e5a34acf6e06bedd377b11e4cd/docs/checking.md)

Use the [release checksums](SHA256SUMS) for the binaries and the device source pages for repeatable builds.
