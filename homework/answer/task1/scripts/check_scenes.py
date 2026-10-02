"""Run the packaged application against every supplied SVG with a clean PATH."""
from pathlib import Path
from PIL import Image
import ctypes
import os
import subprocess
import json
import hashlib

root = Path(__file__).resolve().parents[1]
out = root / 'verification/headless'
out.mkdir(exist_ok=True)
env = os.environ.copy()
env['PATH'] = str(Path(os.environ['SystemRoot']) / 'System32')
ctypes.windll.kernel32.SetErrorMode(0x0001 | 0x0002)
records = []
for scene in sorted((root / 'code/svg').rglob('*.svg')):
    name = scene.relative_to(root/'code/svg').as_posix().replace('/', '_').replace('.svg', '.png')
    result = subprocess.run([str(root/'bin/draw.exe'), str(scene), 'nogl', '400', '400'],
                            cwd=out, env=env, capture_output=True, text=True, timeout=30)
    produced = out/'test.png'
    if result.returncode or not produced.exists() or 'could not' in result.stderr.lower():
        raise RuntimeError((name, result.returncode, result.stderr))
    target = out/name
    produced.replace(target)
    with Image.open(target) as image:
        image.load()
        assert image.size == (400, 400)
    records.append({'svg': str(scene.relative_to(root)), 'png': str(target.relative_to(root)),
                    'exit_code': result.returncode, 'sha256': hashlib.sha256(target.read_bytes()).hexdigest()})
    print(name, 'OK', flush=True)
    (root/'verification/headless-tests.json').write_text(json.dumps({
        'runtime_path': 'Windows System32 only; packaged DLLs beside draw.exe',
        'tests': records}, indent=2), encoding='utf8')
print('TOTAL', len(records), 'passed')
