#!/bin/sh
set -eu

project=$(CDPATH= cd "$(dirname "$0")" && pwd)
sdk=$(CDPATH= cd "${1:?Pass the Open-Field 0.14 checkout with its Blackfin toolchain}" && pwd)
output=${2:?Pass an output pack filename}
mkdir -p "$(dirname "$output")"
output_dir=$(CDPATH= cd "$(dirname "$output")" && pwd)
output="$output_dir/$(basename "$output")"
scratch=$(mktemp -d)
trap 'rm -rf "$scratch"' EXIT

for item in tools profiles module linker; do
    cp -R "$sdk/$item" "$scratch/$item"
done
mkdir -p "$scratch/open-xy" "$scratch/examples"
cp -R "$sdk/open-xy/tooling" "$scratch/open-xy/tooling"
ln -s "$sdk/open-xy/local" "$scratch/open-xy/local"
cp -R "$project/source" "$scratch/examples/wave"
cd "$scratch"

python3 -I tools/pack_build.py card --recipe examples/wave/recipe.json --out examples/wave/wave_card.s
python3 -I tools/pack_build.py pictures --recipe examples/wave/recipe.json --out examples/wave/wave_pictures.s
python3 -I tools/pack_build.py build3 --recipe examples/wave/recipe.json --profile profiles/op1f-1.7.3.json --build-dir "$scratch/objects" --out "$output"
python3 -I tools/pack_build.py check --pack "$output"
