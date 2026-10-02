"""Run independent-reference AA experiments and retain reproducible evidence."""
from pathlib import Path
import csv
import hashlib
import json
import os
import subprocess
from PIL import Image, ImageDraw, ImageFont

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'verification/aa'
OUT.mkdir(parents=True,exist_ok=True)
ENV=os.environ.copy()
ENV['PATH']=r'C:\msys64\mingw64\bin;'+ENV.get('PATH','')

def main():
    result=subprocess.run([str(ROOT/'build/analytic_tests.exe')],cwd=ROOT,env=ENV,
                          capture_output=True,text=True,timeout=120)
    (OUT/'analytic-tests.log').write_text(result.stdout+result.stderr,encoding='utf8')
    if result.returncode: raise RuntimeError(result.stdout+result.stderr)
    result=subprocess.run([str(ROOT/'build/aa_benchmark.exe'),str(OUT)],cwd=ROOT,env=ENV,
                          capture_output=True,text=True,timeout=240)
    (OUT/'aa-benchmark.log').write_text(result.stdout+result.stderr,encoding='utf8')
    if result.returncode: raise RuntimeError(result.stdout+result.stderr)
    rows=list(csv.DictReader((OUT/'aa-summary.csv').open(encoding='utf8')))
    print(result.stdout,flush=True)
    font=ImageFont.truetype(r'C:\Windows\Fonts\consola.ttf',22)
    title_font=ImageFont.truetype(r'C:\Windows\Fonts\consolab.ttf',25)
    modes=['ssaa_1','ssaa_16','analytic','reference']
    scenes=['diagonal','thin','shared_edge','overlap']
    canvas=Image.new('RGB',(1640,1810),'white');draw=ImageDraw.Draw(canvas)
    for col,name in enumerate(['SSAA 1 spp','SSAA 16 spp','Analytic area','4096 spp reference']):
        draw.text((12+col*410,12),name,fill='#151515',font=font)
    for row,scene in enumerate(scenes):
        y=55+row*430
        draw.text((12,y),scene,fill='#151515',font=title_font)
        for col,mode in enumerate(modes):
            with Image.open(OUT/f'{scene}_{mode}.png') as image:
                image=image.convert('RGB').resize((384,384),Image.Resampling.NEAREST)
            canvas.paste(image,(12+col*410,y+35))
            draw.rectangle((12+col*410,y+35,395+col*410,y+418),outline='#dddddd')
    canvas.save(ROOT/'docs/screenshots/aa_comparison.png')
    sources=['src/analytic_rasterizer.cpp','src/analytic_rasterizer.h',
             'verification/aa_benchmark.cpp','verification/analytic_tests.cpp']
    audit={'resolution':[96,96],'reference':'Independent 64x64 center-grid per pixel; direct frontmost point-in-triangle tests',
           'reference_spp':4096,'reference_is_numerical_not_exact':True,
           'timing':'clear + triangle submission + resolve; steady_clock; 2 warmups + 11 timed repetitions',
           'p95_definition':'nearest-rank p95 for 11 observations (maximum)',
           'memory_definition':'retained primitive/bin capacity or SSAA sample storage; excludes common RGB framebuffer and temporary clipping polygons',
           'analytic_scope':'exact opaque geometric coverage and affine RGB integral; texture centroid approximation; point/line keep full-pixel starter treatment',
           'rows':rows,'source_hashes':{p:hashlib.sha256((ROOT/'code'/p).read_bytes()).hexdigest() for p in sources},
           'panel':'Nearest-neighbor 4x enlargement of actual 96x96 framebuffer PNGs, not GUI screenshot',
           'validation_log':result.stdout}
    (OUT/'audit.json').write_text(json.dumps(audit,ensure_ascii=False,indent=2),encoding='utf8')

if __name__=='__main__': main()
