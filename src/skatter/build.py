"""Build this standalone effect with the unmodified Open-Field 0.14 toolkit."""
from pathlib import Path
import json, hashlib, subprocess, shutil,sys,tempfile
root=Path(__file__).resolve().parent
name=json.loads((root/'source/recipe.json').read_text())['name']
up=Path(sys.argv[1]).resolve()
toolchain=Path(sys.argv[2]).resolve() if len(sys.argv)>2 else up/'open-xy/local/toolchain'
out=Path(sys.argv[3]).resolve() if len(sys.argv)>3 else root/'dist'
out.mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory() as temp:
    stage=Path(temp)
    for folder in ('tools','module','profiles','linker'):
        shutil.copytree(up/folder,stage/folder)
    (stage/'open-xy/local').mkdir(parents=True)
    (stage/'open-xy/tooling').symlink_to(up/'open-xy/tooling')
    (stage/'open-xy/local/toolchain').symlink_to(toolchain)
    pub=stage/'examples'/name
    shutil.copytree(root/'source',pub)
    from fx_art import prepare
    scene,fallback=prepare(pub,'skatter_off.svg','night.svg')
    recipe=f'examples/{name}/recipe.json'
    tool=[sys.executable,'-I','tools/pack_build.py']
    def run(args):
        res=subprocess.run(args,cwd=stage,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        print(res.stdout,end='')
        res.check_returncode()
    run([*tool,'card','--recipe',recipe,'--out',f'examples/{name}/card.s'])
    if json.loads((pub/'recipe.json').read_text())['pictures']:
        run([*tool,'pictures','--recipe',recipe,'--out',f'examples/{name}/pictures.s'])
    run([*tool,'build3','--recipe',recipe,'--profile','profiles/op1f-1.7.3.json','--out',f'{name}.pack'])
    run([*tool,'check','--pack',f'{name}.pack'])
    built=stage/(name+'.pack')
    shutil.copyfile(built,out/built.name)
    print('SHA256',hashlib.sha256(built.read_bytes()).hexdigest())
