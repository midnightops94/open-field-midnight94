# open-field-midnight94

Community packs for [open-field](https://github.com/LaP0573/open-field), maintained by midnightops94.

## Downloads

Published `.pack` files go in [`packs/`](packs/). Check each pack's notes for supported firmware, the required `base.pack` version, dependencies, and device test results.

With open-field installed, enter disk mode, copy the `.pack` file into the device's `packs` folder, exit disk mode, and restart. To remove a pack, delete its file and restart. Hold **Help** during power-on to start with packs disabled. [Open-field installation guide](https://github.com/LaP0573/open-field/blob/main/docs/guide.md#adding-a-feature)

## Publish a pack

1. Commit the original `.pack` binary under `packs/`, using the name from its header followed by `.pack`. Put the corresponding source and build instructions under `src/<pack-id>/` for review. Keep firmware dumps and private build inputs outside this public repository.
2. Record the firmware, open-field version, and base version you tested on the device, along with what happened.
3. Inspect the binary from an open-field checkout with `python -I tools/pack_build.py check --pack /path/to/file.pack`. For a commit-pinned listing, record the full commit SHA, the file's path, its byte count, and its SHA-256.
4. Add the entry to `catalog/packs.json` in your open-field fork. From that checkout, run `python -I tools/catalog_check.py --files /path/to/open-field-midnight94/packs`. Open a pull request that changes only `catalog/packs.json`, links to the pack's source, and describes the device tests. [Open-field catalog submission guide](https://github.com/LaP0573/open-field/blob/main/docs/catalog.md#getting-your-pack-listed)
