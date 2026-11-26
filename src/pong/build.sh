#!/bin/sh
set -eu
project=$(cd "$(dirname "$0")" && pwd)
sdk=$(cd "${1:?Give the Open-Field SDK checkout}" && pwd)
output=${2:-"$project/release/pong.pack"}
mkdir -p "$(dirname "$output")"
output="$(cd "$(dirname "$output")" && pwd)/$(basename "$output")"
if [ "$#" -ge 3 ]; then
    hashes_dir=$(cd "$(dirname "$3")" && pwd)
    hashes="$hashes_dir/$(basename "$3")"
fi
staging=$(mktemp -d)
trap 'rm -rf "$staging"' EXIT
for folder in tools module profiles linker
do
    cp -R "$sdk/$folder" "$staging/$folder"
done
mkdir -p "$staging/examples" "$staging/open-xy/local"
cp -R "$project/source" "$staging/examples/userpong"
python "$staging/examples/userpong/make_overlay.py"
ln -s "$sdk/open-xy/tooling" "$staging/open-xy/tooling"
ln -s "$sdk/open-xy/local/toolchain" "$staging/open-xy/local/toolchain"
cd "$staging"
recipe=examples/userpong/recipe.json
python -I tools/pack_build.py card --recipe "$recipe" --out examples/userpong/pong_card.s
python -I tools/pack_build.py pictures --recipe "$recipe" --out examples/userpong/pong_pictures.s
python -I "$project/build_pack.py" "$staging" build3 --recipe "$recipe" --profile profiles/op1f-1.7.3.json --out pong.pack
python -I tools/pack_build.py check --pack pong.pack
cp pong.pack "$output"
if [ "$#" -ge 3 ]; then
    (cd build && sha256sum *.cpp.o) > "$hashes"
fi
