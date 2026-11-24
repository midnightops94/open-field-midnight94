#!/bin/sh
set -eu

project=$(cd "$(dirname "$0")" && pwd)
upstream=$(realpath "${1:?Supply an Open-Field 0.14 checkout with the Blackfin toolchain}")
output=$(realpath -m "${2:?Supply an output pack filename}")
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT

for item in tools profiles module linker; do cp -R "$upstream/$item" "$stage/$item"; done
mkdir -p "$stage/open-xy"
cp -R "$upstream/open-xy/tooling" "$stage/open-xy/tooling"
ln -s "$upstream/open-xy/local" "$stage/open-xy/local"
cp -R "$project/source" "$stage/organ"
cd "$stage"

# This single audio translation unit needs ordinary code sections for its
# out-of-line templates so the published pack relocator can handle them.
sed -i 's/"-std=gnu++11"/"-std=gnu++11", "-fno-weak"/' tools/module_build.py
python3 organ/art/make_waves.py
python3 -I tools/pack_build.py card --recipe organ/recipe.json --out organ/organ_card.s
python3 -I tools/pack_build.py pictures --recipe organ/recipe.json --out organ/organ_pictures.s
python3 -I tools/pack_build.py build3 --recipe organ/recipe.json --profile profiles/op1f-1.7.3.json \
    --build-dir "$(dirname "$output")/build-objects" --out "$output"
python3 -I tools/pack_build.py check --pack "$output"
