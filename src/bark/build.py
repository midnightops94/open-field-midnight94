"""Build a Bark pack using the public release SDK and a clean build directory."""
import argparse
import hashlib
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

here = Path(__file__).resolve().parent
cli = argparse.ArgumentParser(description=__doc__)
cli.add_argument('--sdk', type=Path, required=True)
cli.add_argument('--source', type=Path, default=here / 'source')
cli.add_argument('--out', type=Path, required=True)
cli.add_argument('--object-hashes', type=Path, help='Save compiled audio and page object identities')
args = cli.parse_args()
sdk = args.sdk.resolve()
output = args.out.resolve()
with tempfile.TemporaryDirectory() as temp:
    stage = Path(temp)
    for part in ('tools', 'profiles', 'module', 'linker'):
        shutil.copytree(sdk / part, stage / part)
    (stage / 'open-xy').mkdir()
    for part in ('tooling', 'local'):
        (stage / 'open-xy' / part).symlink_to(sdk / 'open-xy' / part)
    project = stage / 'examples/bark'
    shutil.copytree(args.source, project)
    tool = [sys.executable, '-I', 'tools/pack_build.py']
    recipe = 'examples/bark/recipe.json'
    for kind in ('card', 'pictures'):
        subprocess.run([*tool, kind, '--recipe', recipe, '--out', f'examples/bark/bark_{kind}.s'], cwd=stage, check=True)
    subprocess.run([*tool, 'build3', '--recipe', recipe, '--profile', 'profiles/op1f-1.7.3.json', '--out', output], cwd=stage, check=True)
    subprocess.run([*tool, 'check', '--pack', output], cwd=stage, check=True)
    if args.object_hashes:
        objects = ('bark.cpp.o', 'page.cpp.o')
        hashes = {name: hashlib.sha256((stage/'build'/name).read_bytes()).hexdigest() for name in objects}
        args.object_hashes.write_text(json.dumps(hashes, indent=2)+'\n')
