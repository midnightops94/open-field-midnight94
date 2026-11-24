#!/bin/sh
set -eu
project=$(cd "$(dirname "$0")" && pwd)
upstream=$(realpath "${1:?Supply an Open-Field 0.14 source checkout}")
output=$(realpath -m "${2:?Supply a new output path}")
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
for item in tools profiles module linker; do cp -R "$upstream/$item" "$tmp/$item"; done
mkdir -p "$tmp/open-xy"
cp -R "$upstream/open-xy/tooling" "$tmp/open-xy/tooling"
ln -s "$(realpath "$upstream/open-xy/local")" "$tmp/open-xy/local"
cp -R "$project/source" "$tmp/sid"
cd "$tmp"
python3 -I tools/pack_build.py card --recipe sid/recipe.json --out sid/sid_card.s
python3 -I tools/pack_build.py pictures --recipe sid/recipe.json --out sid/sid_pictures.s
python3 -I tools/pack_build.py build3 --recipe sid/recipe.json --profile profiles/op1f-1.7.3.json --out "$output"
python3 -I tools/pack_build.py check --pack "$output"
