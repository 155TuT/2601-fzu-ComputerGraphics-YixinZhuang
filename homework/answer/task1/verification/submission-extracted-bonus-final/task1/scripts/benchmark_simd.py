"""Reproduce SIMD benchmarks (--run), or summarize retained runs (default)."""
from pathlib import Path
import argparse
import csv
import hashlib
import json
import os
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'verification/performance'

def read(path):
    with path.open(encoding='utf8') as file: return list(csv.DictReader(file))

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--run',action='store_true');args=parser.parse_args()
    OUT.mkdir(parents=True,exist_ok=True)
    env=os.environ.copy();env['PATH']=r'C:\msys64\mingw64\bin;'+env.get('PATH','')
    if args.run:
        for width in (512,1024):
            result=subprocess.run([str(ROOT/'build/rasterizer_benchmark.exe'),str(OUT/str(width)),str(width),'11'],
                                  cwd=ROOT,env=env,capture_output=True,text=True,timeout=1800)
            (OUT/f'{width}.log').write_text(result.stdout+result.stderr,encoding='utf8')
            if result.returncode: raise RuntimeError(result.stdout+result.stderr)
    rows=[];equivalence=[];oracle=[]
    for width in (512,1024):
        rows+=read(OUT/f'{width}_summary.csv')
        equivalence+=read(OUT/f'{width}_equivalence.csv')
        oracle+=read(OUT/f'{width}_oracle.csv')
    assert all(int(r['different_pixels'])==int(r['different_coverage_samples'])==0 for r in equivalence)
    assert all(int(r['different_float_samples'])==0 for r in equivalence if r['reference']=='incremental' and r['candidate']=='simd')
    assert all(int(r['wrong_samples'])==0 for r in oracle)
    audit={'cpu':'AMD Ryzen AI 7 H 350 w/ Radeon 860M (live HKLM ProcessorNameString)',
           'compiler':'GCC 15.1.0, MinGW x64, RelWithDebInfo, -ffp-contract=off, no global AVX flags',
           'seed':20261002,'resolutions':[512,1024],'spp':[1,4,16],'warmups':2,'repetitions':11,
           'timing':'actual triangle setup + coverage + constant shader + sample writes; steady_clock; excludes clear/resolve/allocation/SVG/GUI/PNG/CSV/hash',
           'order':'methods rotate order each repetition',
           'geometry_per_case':{'large':10,'tiny':512,'thin':12,'clipped':12},
           'equivalence_comparisons':len(equivalence),'oracle_comparisons':len(oracle),
           'rgb_and_coverage_differences':0,'simd_vs_incremental_float_sample_differences':0,
           'oracle':'independent long-double endpoint cross products, 72 triangles, both windings, quarter-pixel random and exact boundary cases, spp 1/4/9/16',
           'sse_scalar_limit':'force incremental exercises scalar path; SSE2 source compiled, current AVX machine does not execute hardware without AVX',
           'rows':rows,'source_hashes':{p:hashlib.sha256((ROOT/'code'/p).read_bytes()).hexdigest() for p in ['src/rasterizer.cpp','src/rasterizer.h','verification/rasterizer_benchmark.cpp']}}
    (OUT/'audit.json').write_text(json.dumps(audit,ensure_ascii=False,indent=2),encoding='utf8')
    sys.path.insert(0,str(ROOT/'.tools/python-packages'))
    os.environ['MPLCONFIGDIR']=str(ROOT/'.tools/matplotlib-config')
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    fig,axes=plt.subplots(1,2,figsize=(11,4.2),sharey=True,layout='constrained')
    cases=['large','tiny','thin','clipped']
    for ax,width in zip(axes,[512,1024]):
        for offset,spp in enumerate([1,4,16]):
            values=[float(next(r for r in rows if r['case']==case and int(r['width'])==width and int(r['spp'])==spp and r['method']=='simd')['speedup_vs_incremental']) for case in cases]
            positions=[i+(offset-1)*.24 for i in range(4)]
            bars=ax.bar(positions,values,width=.23,label=f'{spp} spp')
            for bar,value in zip(bars,values):ax.text(bar.get_x()+bar.get_width()/2,value+.045,f'{value:.2f}',ha='center',fontsize=8)
        ax.axhline(1,color='#333333',linewidth=1,linestyle='--');ax.set_xticks(range(4),cases);ax.set_title(f'{width} x {width}');ax.grid(axis='y',alpha=.18);ax.set_axisbelow(True)
    axes[0].set_ylabel('Speedup over incremental scalar (higher is faster)');axes[1].legend(frameon=False)
    fig.savefig(ROOT/'docs/screenshots/simd_speedup.png',dpi=180,facecolor='white');plt.close(fig)
    print('SIMD audit:',len(rows),'timing summaries;',len(equivalence),'pipeline comparisons;',len(oracle),'oracle comparisons; all differences zero')

if __name__=='__main__':main()
