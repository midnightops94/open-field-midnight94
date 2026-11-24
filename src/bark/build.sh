#!/bin/sh
# Build against an Open-Field 0.14 SDK checkout with its documented toolchain.
set -eu
project=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
sdk=$(CDPATH= cd -- "${1:?Supply the Open-Field SDK directory}" && pwd)
outdir=$(dirname -- "${2:?Supply the output pack path}")
outdir=$(CDPATH= cd -- "$outdir" && pwd)
out="$outdir/$(basename -- "$2")"
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
for part in tools profiles module linker; do cp -R "$sdk/$part" "$stage/$part"; done
mkdir -p "$stage/open-xy" "$stage/examples"
ln -s "$sdk/open-xy/tooling" "$stage/open-xy/tooling"
ln -s "$sdk/open-xy/local" "$stage/open-xy/local"
cp -R "$project/source" "$stage/examples/bark"
cd "$stage"
python3 -I tools/pack_build.py card --recipe examples/bark/recipe.json --out examples/bark/bark_card.s
python3 -I tools/pack_build.py pictures --recipe examples/bark/recipe.json --out examples/bark/bark_pictures.s
python3 -I tools/pack_build.py build3 --recipe examples/bark/recipe.json --profile profiles/op1f-1.7.3.json --out "$out"
python3 -I tools/pack_build.py check --pack "$out"
