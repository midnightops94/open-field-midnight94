"""Build with the published Open-Field 0.20 SDK and its Blackfin compiler."""
import argparse
import json
import os
import re
import sys
from pathlib import Path
from xml.etree import ElementTree as ET
from curve_art import cubic_path, field_numbers
from waist import prepare
from stock_fx_off import compose, literal

SOURCE = Path(__file__).resolve().parent


def art(fallback=False):
    root = ET.parse(SOURCE / 'syrup-art.svg').getroot()
    prepare(root)
    ET.register_namespace('', 'http://www.w3.org/2000/svg')
    source = ET.tostring(root,encoding='unicode')
    source = re.sub(r' d="([^"]+)"', lambda m: ' d="' + cubic_path(m[1]) + '"', source)
    def placeholder(match):
        a = ET.fromstring(match[0]).attrib
        x, y, h = float(a['x']), float(a['y']), float(a['height'])
        return (f'<g id="{a["id"]}"><path fill="none" stroke="{a["stroke"]}" '
                f'stroke-width="{a["stroke-width"]}" stroke-linecap="square" '
                f'd="M{x:g},{y+h:g}V{y:g}"/></g>')
    source, converted = re.subn(r'<rect id="syrup(?:Label[0-3]|Value[12]|Fallback)"[^>]+/>', placeholder, source)
    assert converted == 7
    if fallback:
        from standalone_fx_off import FALLBACK
        source = source.replace('</svg>', FALLBACK.decode() + '</svg>')
    return source.encode()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--toolchain', type=Path, help='directory containing bfin-elf-g++ and bfin-elf-as')
    args = parser.parse_args()
    sdk = args.sdk.resolve()
    output, build = args.out.resolve(), args.build.resolve()
    build.mkdir(parents=True,exist_ok=True)
    toolchain = args.toolchain.resolve() if args.toolchain else None
    os.chdir(sdk)
    sys.path.insert(0,str(sdk / 'tools'))
    import module_build
    import pack_build
    import picture_make
    if toolchain: module_build.TOOLCHAIN = str(toolchain)
    module_build.CXX_FLAGS += ['-std=gnu++17','-fno-builtin','-fno-strict-aliasing','-fstack-usage','-I'+str(SOURCE/'include')]
    data = art()
    field_numbers(data)
    (build/'syrup.svg.make').write_bytes(compose(data))
    (build/'syrup-safe.svg.make').write_bytes(literal(art(fallback=True)))
    recipe = json.loads((SOURCE/'recipe.json').read_text())
    recipe['sources'] = [str((build if p.endswith('.s') else SOURCE) / p) for p in recipe['sources']]
    for picture in recipe['pictures']:
        picture['file'] = str(build/picture['file'])
    path = build/'recipe.json'
    path.write_text(json.dumps(recipe,indent=2)+'\n')
    pack_build.main(['card','--recipe',str(path),'--out',str(build/'card.s')])
    pack_build.main(['pictures','--recipe',str(path),'--out',str(build/'pictures.s')])
    result = pack_build.main(['build3','--recipe',str(path),'--profile','profiles/op1f-1.7.3.json',
                     '--build-dir',str(build),'--out',str(output)])
    if result: raise SystemExit(result)
    check = pack_build.main(['check','--pack',str(output)])
    if check: raise SystemExit(check)


if __name__ == '__main__':
    main()
