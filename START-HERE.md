# Install Midnight94 Open-Field Packs v0.14

Use **OP-1 Field OS 1.7.3+0** with **Open-Field 0.24**, **base v42** and the **N2 loader**. The current Open-Field installer defaults to N2. It has 16 MiB of pack room and can look at 128 pack files. Updating only `base.pack` keeps the existing loader. Follow Open-Field’s guide if you’re starting from stock firmware or choosing a loader. [Open-Field installer](https://lap0573.github.io/open-field/) · [Loader generations](https://github.com/LaP0573/open-field/blob/c65e40de0ef51f601bc0445daa85d906378dea52/docs/guide.md#L126-L132)

## Add the devices

1. In Disk mode, back up the Field’s existing `packs` folder to your computer. Keep `base.pack` on the Field. Ampig replaces Sag and uses its own effect entry. If you’re switching from Sag, move `sag.pack` into your backup and choose Ampig explicitly in sounds or chains that used Sag.
2. Download the `.pack` files from the device list. To use the installer, open its packs section, use **+** to select each file, send the changes and confirm on the Field with **T1**. For a manual transfer, copy the files into the existing `packs` folder in Disk mode. The full-drive MTP path is `user/pk`. Replace matching filenames and keep one copy per device. [Device downloads](README.md)
3. Leave Disk mode and restart. Open the synth selector for an engine or a synth, drum or master FX selector for an effect. Hold **Shift** and press a sound key to browse synth presets. [Device controls](docs/OP-1-Field-Quick-Reference-v0.14.pdf)

The exact set loaded in boot replay with 8 Tracks, FX Train, LFO and Sidechain on N1 and N2. Adding Maestro to those four also fit on N2. If startup leaves a device out, save the latest `logs/` entry and check that the folder contains the intended set. For a fresh check, install the Midnight94 packs alongside the matching base and add other feature packs back after the set loads. [Test results and older loader limits](VALIDATION.md)

## Try Ampig Meltdown

Play a sustained note or loop with Mix at `99`. Increase Drive for saturation, then raise Sag to hear the wet signal dip under load. Try **Drive, Sag and Mix at `99`**. Press Ochre to compare the timing of the dropouts with Flow, Fast and Slow recovery. Press Blue to compare Valve, Diode and Starved. Bypassing Ampig displays the FX Off card. [Ampig popups](validation/deltas/ampig-v11-popups-preview.png)

## Factory sounds and recovery

Open-Field keeps existing synth preset files on upgrade. To refresh Bark’s **Felt Room**, save your existing `user/synth_presets/bark/felt room.json`, copy the updated file to that path and reload the sound. [Updated Felt Room](factory-updates/bark/felt%20room.json)

To undo an update, restore your own backed-up pack files and keep the base that matches the Open-Field version installed on your Field. To remove a pack, delete its file in the installer or in Disk mode and restart. Hold **Help** while powering on to start with packs disabled. [Removal and safe start](https://github.com/LaP0573/open-field/blob/c65e40de0ef51f601bc0445daa85d906378dea52/docs/guide.md#adding-a-feature)
