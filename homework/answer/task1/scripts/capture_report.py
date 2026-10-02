"""Create report evidence using the real DrawRend callbacks and OpenGL S path."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import time
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'docs/screenshots'
WORK = OUT / '.capture'
WORK.mkdir(parents=True, exist_ok=True)
env = os.environ.copy()
env['PATH'] = r'C:\msys64\mingw64\bin;' + env.get('PATH', '')
scenes = []
def add(name, svg, spp=1, psm=0, lsm=0, inspector=0, cursor=(400,400), zoom=1):
    scenes.append(dict(name=name, svg=svg, spp=spp, psm=psm, lsm=lsm,
                       inspector=inspector, cursor=cursor, zoom=zoom))
add('triangle_default', 'code/svg/basic/test4.svg')
add('triangle_inspector', 'code/svg/basic/test4.svg', inspector=1, cursor=(166,378))
for rate in (1,4,16):
    add(f'ssaa_{rate}', 'code/svg/basic/test4.svg', spp=rate, inspector=1, cursor=(166,378))
add('robot_original', 'code/svg/transforms/robot.svg', spp=16)
add('robot_custom', 'docs/my_robot.svg', spp=16)
add('colorwheel', 'code/svg/basic/test7.svg')
add('barycentric', 'docs/barycentric.svg')
for psm,name in ((0,'nearest'),(1,'linear')):
    for rate in (1,16):
        add(f'pixel_{name}_{rate}', 'code/svg/texmap/test4.svg', spp=rate, psm=psm,
            inspector=1, cursor=(348,230), zoom=.4)
for lsm,level in ((0,'zero'),(1,'nearest'),(2,'linear')):
    for psm,pixel in ((0,'nearest'),(1,'linear')):
        add(f'mipmap_{level}_{pixel}', 'docs/texture_demo.svg', psm=psm, lsm=lsm,
            inspector=1, cursor=(400,230))
records=[]
for scene in scenes:
    before=set(WORK.glob('screenshot_*.png'))
    args=[str(ROOT/'build/screenshot_runner.exe'), str(ROOT/scene['svg']),
          str(scene['spp']), str(scene['psm']), str(scene['lsm']), str(scene['inspector']),
          *map(str,scene['cursor']), str(scene['zoom']), '800']
    start=time.perf_counter()
    result=subprocess.run(args,cwd=WORK,env=env,capture_output=True,text=True,timeout=90)
    duration=time.perf_counter()-start
    if result.returncode:
        raise RuntimeError(f"{scene['name']} exit {result.returncode}: {result.stdout}\n{result.stderr}")
    files=set(WORK.glob('screenshot_*.png'))-before
    assert len(files)==1, (scene['name'],files)
    dest=OUT/(scene['name']+'.png')
    next(iter(files)).replace(dest)
    with Image.open(dest) as im:
        assert im.size==(800,800), im.size
        assert im.getextrema()!=((255,255),)*4, 'Blank image'
    record={**scene,'width':800,'height':800,'png':str(dest.relative_to(ROOT)),
            'sha256':hashlib.sha256(dest.read_bytes()).hexdigest(),
            'export_seconds':round(duration,4), 'stdout':result.stdout,'stderr':result.stderr,
            'capture':'OpenGL hidden window; real P/L/=/Z/S DrawRend keyboard callbacks'}
    records.append(record)
    print(scene['name'], 'OK', f'{duration:.3f}s',flush=True)
(ROOT/'verification/screenshots.json').write_text(json.dumps(records,ensure_ascii=False,indent=2),encoding='utf8')
print(f'Generated {len(records)} original PNG screenshots')
