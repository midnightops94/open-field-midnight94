# Quick reference for Midnight94 v0.14

Midnight94 collection **v0.14** targets OP-1 Field OS **1.7.3+0** with **Open-Field 0.24, base v42**. The seven-page illustrated manual lists the six synths alphabetically, then the seven effects alphabetically. [Illustrated v0.14 manual](OP-1-Field-Quick-Reference-v0.14.pdf) · [Installation and migration](../START-HERE.md)

## Play Ampig v11

Select **Ampig** from a synth, drum or master FX page. The Roadie Combo Cabinet shows how the amplifier responds to your input and settings. Press Drive or Sag to open a rounded two-line card with an icon for the selected mode. Ampig uses the Field’s lettering when available and its own stroke lettering as a fallback. Its brighter cabinet outline and four corner readouts accompany the Meltdown sound.

| Encoder | Turn | Press |
| --- | --- | --- |
| Blue | **Drive** pushes the circuit harder. The upper range adds saturation and the outline brightens. | **Circuit** selects Valve, Diode or Starved. |
| Ochre | **Sag** increases supply drop under load. The cabinet sinks between its feet. | **Recover** selects Flow, Fast or Slow. |
| Gray | **Tone** shapes the speaker response and opens the grille. | |
| Red | **Mix** blends the processed and dry signals. The routing lines show the blend. | |

1. Set **Mix to `99`** and play a sustained note or loop. With Sag low, sweep Drive to hear the upper range break up. At `99`, Drive feeds the circuit about 4.7 dB more gain than Ampig v7.
2. Set Drive near the middle and sweep Sag upward. Listen for the processed signal to dip as the input loads the supply.
3. Set **Drive and Sag both to `99`**. Keep the input sounding. The simulated supply can repeatedly give out and recover, breaking the note into bursts. Press Ochre to compare Flow, Fast and Slow. Recover changes the spacing of those bursts. Press Blue to hear the same setting through each circuit.

## Use an effect with FX Train

With the optional **FX Train v18**, press **Shift + T3** on a synth or drum sound and choose **train**. Turn an encoder for one of the three wagons away from the lamp to select a Midnight94 effect. The lit wagon’s list skips pack effects. FX Train also skips effects that would exceed the chain’s processing budget.

The wagon runs the pack effect’s audio. Tapping a wagon that holds a pack effect leaves the train page in place. To open Ampig’s mode cards, Pong’s Table control or another effect’s full page, select that effect directly from the regular FX list. The master uses its regular effect selector. [FX Train v18 controls](https://github.com/LaP0573/open-field-packs/blob/c006c15dbdf300a820f80c2e0b51f01cd1c16c09/fx%20train/README.md#on-the-train)

## Other controls and presets

- **Pong.** Press Blue to switch **Table** between milliseconds and tempo sync. Select Table as the LFO destination to vary delay time or step between beat divisions. Rally sets the repeat count, Spin moves the stereo image and Input sets the send level.
- **Syrup.** The blue **Freq** title identifies the encoder. The needle, ticks and end marks share the light gray of the Res and Drive values. Res reads `00`–`99`, Drive reads `0.0`–`18.0` in tenths of a decibel and Mix uses a bar meter.
- **Encoders and displays.** A regular detent moves a primary control by about 3% of its range. Hold Shift for about 1%. The standard unitless readouts show `00`–`99`, including LFO modulation. [Readout guide](READOUT-AUDIT.md)
- **Bark and SID presets.** Bark includes the revised Felt Room. SID’s eight factory sounds use plain names. See the preset instructions if an older factory copy still appears on the Field. [Preset instructions](../START-HERE.md#presets-and-rollback)
