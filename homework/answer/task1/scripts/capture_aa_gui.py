"""Capture experimental AA through the same A/S callbacks as the GUI."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
WORK=ROOT/'docs/screenshots/.capture/aa'
def main():
    WORK.mkdir(parents=True,exist_ok=True)
    env=os.environ.copy();env['PATH']=r'C:\msys64\mingw64\bin;'+env.get('PATH','')
    records=[]
    for name,analytic in [('aa_gui_ssaa',0),('aa_gui_analytic',1)]:
        before=set(WORK.glob('screenshot_*.png'))
        args=[str(ROOT/'build/screenshot_runner.exe'),str(ROOT/'docs/analytic_demo.svg'),
              '1','0','0','0','400','400','1','800','0','0',str(analytic)]
        result=subprocess.run(args,cwd=WORK,env=env,capture_output=True,text=True,timeout=90)
        if result.returncode:raise RuntimeError(result.stdout+result.stderr)
        files=set(WORK.glob('screenshot_*.png'))-before
        assert len(files)==1
        dest=ROOT/'docs/screenshots'/f'{name}.png';files.pop().replace(dest)
        with Image.open(dest) as image:assert image.size==(800,800) and image.format=='PNG'
        records.append({'png':str(dest.relative_to(ROOT)),'sha256':hashlib.sha256(dest.read_bytes()).hexdigest(),
                        'width':800,'height':800,'svg':'docs/analytic_demo.svg','mode':'analytic' if analytic else 'regular SSAA 1 spp',
                        'capture':'Hidden GLFW window, automated real DrawRend A/S callbacks, GL_NO_ERROR checked',
                        'manual_gui_validation':False,'command':args,'stdout':result.stdout,'stderr':result.stderr})
        print(name,'OK',flush=True)
    (ROOT/'verification/aa/gui-captures.json').write_text(json.dumps(records,ensure_ascii=False,indent=2),encoding='utf8')
if __name__=='__main__':main()
