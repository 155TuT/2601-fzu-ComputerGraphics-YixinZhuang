"""Run headless integration cases using the submitted executable and bundled DLLs."""
import json
import os
import pathlib
import subprocess
import time
from PIL import Image, ImageChops

root = pathlib.Path(__file__).resolve().parents[1]
out = root / 'verification/runtime'
out.mkdir(parents=True, exist_ok=True)
environment = dict(os.environ)
environment['PATH'] = str(pathlib.Path(os.environ.get('SystemRoot', os.environ.get('SYSTEMROOT', 'C:/Windows'))) / 'System32')
cases = [
    ('threads1', ['-t', '1']), ('threads4', ['-t', '4']),
    ('brute', ['-t', '1', '-B']), ('hemisphere', ['-t', '2', '-H']),
    ('normals', ['-t', '2', '-N']), ('fixed', ['-t', '2', '-U']),
    ('isolated', ['-t', '2', '-o', '0']), ('zero', ['-t', '2', '-m', '0']),
]
records=[]
for name,extra in cases:
    image = out / f'{name}.png'
    command = [str(root/'bin/pathtracer.exe'), '-s', '8', '-l', '2', '-m', '3',
               '-r', '64', '48', '-q', '912', '-f', str(image), *extra,
               'code/dae/sky/CBspheres_lambertian.dae']
    start=time.perf_counter()
    result=subprocess.run(command, cwd=root, env=environment, capture_output=True,
                          text=True, encoding='utf-8', errors='replace', timeout=60)
    (out/f'{name}.log').write_text(result.stdout+result.stderr,encoding='utf-8')
    assert result.returncode==0, (name,result.stderr)
    with Image.open(image) as im:
        assert im.size==(64,48)
        assert im.convert('RGB').getextrema()!=((0,0),(0,0),(0,0)),name
    records.append({'case':name,'returncode':result.returncode,
                    'seconds':time.perf_counter()-start,'command':command})
def identical(a,b):
    return ImageChops.difference(Image.open(out/f'{a}.png').convert('RGB'),
                                Image.open(out/f'{b}.png').convert('RGB')).getbbox() is None
assert identical('threads1','threads4'), 'worker-count reproducibility'
assert identical('threads1','brute'), 'BVH preserves the rendered image'
assert not identical('threads1','zero'), 'depth changes lighting'
assert not identical('threads1','isolated'), 'bounce selection changes lighting'
help_result=subprocess.run([str(root/'bin/pathtracer.exe'),'-h'],cwd=root,
                           env=environment,capture_output=True,text=True,timeout=10)
assert help_result.returncode==0
report={'cases':records,'clean_path':environment['PATH'],
        'one_vs_four_threads_identical':True,'bvh_vs_brute_identical':True,
        'help_exit_code':help_result.returncode}
(root/'verification/runtime-tests.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print('PASS: eight render modes, clean PATH, 1/4 thread equality, BVH/brute equality, help')

