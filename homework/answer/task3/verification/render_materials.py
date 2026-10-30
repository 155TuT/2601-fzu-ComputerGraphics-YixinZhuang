"""Render identical Cornell-box geometry with different sphere materials."""
from pathlib import Path
import json
import subprocess
import time
root=Path(__file__).resolve().parents[1]
jobs=[
    ('materials-mirror-glass','sky/CBspheres.dae'),
    ('materials-mirror','custom/CBspheres_mirror.dae'),
    ('materials-glass','custom/CBspheres_glass.dae'),
    ('materials-metal-a008','custom/CBspheres_metal_a008.dae'),
    ('materials-metal-a025','custom/CBspheres_metal_a025.dae'),
    ('materials-metal-a060','custom/CBspheres_metal_a060.dae'),
]
record_file=root/'verification/materials/renders.json'
records=json.loads(record_file.read_text()) if record_file.exists() else []
for name,scene in jobs:
    if any(item['name']==name for item in records):
        continue
    samples,lights=('256','16') if name=='materials-mirror-glass' else ('128','8')
    command=[str(root/'bin/pathtracer.exe'),'-t','4','-s',samples,'-l',lights,
             '-m','8','-r','480','360','-q','1','-f',f'docs/screenshots/{name}.png',
             f'code/dae/{scene}']
    start=time.perf_counter()
    result=subprocess.run(command,cwd=root,capture_output=True,text=True,encoding='utf-8',errors='replace')
    (root/f'verification/materials/{name}.log').write_text(result.stdout+result.stderr,encoding='utf-8')
    if result.returncode:
        raise RuntimeError(f'{name}: {result.returncode}: {result.stderr}')
    records.append({'name':name,'scene':scene,'command':command,'wall_seconds':time.perf_counter()-start,
                    'image':f'docs/screenshots/{name}.png'})
    (root/'verification/materials/renders.json').write_text(json.dumps(records,indent=2),encoding='utf-8')
    print(f'{name}: {records[-1]["wall_seconds"]:.3f}s',flush=True)
