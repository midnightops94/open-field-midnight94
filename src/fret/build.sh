#!/bin/sh
# Build against an pinned Open-Field SDK checkout with its documented toolchain.
set -eu
project=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
sdk=$(CDPATH= cd -- "${1:?Supply the Open-Field SDK directory}" && pwd)
outdir=$(dirname -- "${2:?Supply the output pack path}")
outdir=$(CDPATH= cd -- "$outdir" && pwd)
out="$outdir/$(basename -- "$2")"
if [ "$#" -ge 3 ]; then
    hashes_dir=$(CDPATH= cd -- "$(dirname -- "$3")" && pwd)
    hashes="$hashes_dir/$(basename -- "$3")"
fi
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
for part in tools profiles module linker; do cp -R "$sdk/$part" "$stage/$part"; done
mkdir -p "$stage/open-xy"
ln -s "$sdk/open-xy/tooling" "$stage/open-xy/tooling"
ln -s "$sdk/open-xy/local" "$stage/open-xy/local"
cp -R "$project/source" "$stage/fret"
cd "$stage"
python3 -I tools/pack_build.py card --recipe fret/recipe.json --out fret/fret_card.s
python3 -I tools/pack_build.py pictures --recipe fret/recipe.json --out fret/fret_pictures.s
python3 -I tools/pack_build.py build3 --recipe fret/recipe.json --profile profiles/op1f-1.7.3.json --out "$out"
python3 -I tools/pack_build.py check --pack "$out"
if [ "$#" -ge 3 ]; then
    (cd build && sha256sum audio.cpp.o page.cpp.o) > "$hashes"
fi
