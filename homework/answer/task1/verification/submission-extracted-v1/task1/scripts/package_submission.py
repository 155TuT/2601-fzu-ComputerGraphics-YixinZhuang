"""Create an auditable submission ZIP without local toolchains or build caches."""
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
import hashlib
import json

root = Path(__file__).resolve().parents[1]
out = root / 'submission'
out.mkdir(exist_ok=True)
required = [root/'code/src'/f for f in ('rasterizer.cpp','transforms.cpp','texture.cpp')]
required += [root/'bin/draw.exe', root/'docs/作业报告.docx', root/'docs/作业报告.pdf',
             root/'docs/my_robot.svg',root/'docs/sampling_texture.png',root/'docs/texture_demo.svg']
for file in required:
    assert file.is_file() and file.stat().st_size > 0, file
files = [root/'README.md']
for folder in ('code','bin','docs','scripts'):
    files += [p for p in (root/folder).rglob('*') if p.is_file()
              and not any(x in p.relative_to(root).parts for x in ('.capture','__pycache__','.git','outputs'))
              and p.name != '.DS_Store']
for name in ('algorithm-tests.log','build.log','screenshots.json','dependency-assets.json',
             'headless.log','headless-tests.json','gdb.log'):
    files.append(root/'verification'/name)
files.append(root/'verification/requirements/requirements-audit.md')
records = [{'path':p.relative_to(root).as_posix(),'bytes':p.stat().st_size,
            'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in sorted(set(files))]
manifest={'identity':'姓名与学号待填写；填写后重新导出PDF并重新打包',
          'algorithm_checks':41,'svg_smoke_tests':32,'opengl_png_screenshots':19,'files':records}
(out/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf8')
archive=out/'task1-submit.zip'
with ZipFile(archive,'w',ZIP_DEFLATED,compresslevel=6) as z:
    for item in records:
        z.write(root/item['path'],'task1/'+item['path'])
    z.write(out/'manifest.json','task1/manifest.json')
with ZipFile(archive) as z:
    assert z.testzip() is None
    for item in records:
        assert hashlib.sha256(z.read('task1/'+item['path'])).hexdigest()==item['sha256']
print(archive)
print('Files',len(records),'Bytes',archive.stat().st_size)
print('SHA256',hashlib.sha256(archive.read_bytes()).hexdigest())
