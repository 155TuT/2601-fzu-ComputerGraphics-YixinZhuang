from pathlib import Path
p=Path('homework/answer/task2/code/src/meshEdit.cpp');s=p.read_text(encoding='utf-8-sig');s=s.replace('else if (operation == "solid") shadingMode = true;', '''else if (operation == "solid") shadingMode = true;
  else if (operation == "phong") { shadingMode = true; key_event('1'); }''');s=s.replace('float eyePos[] = {cx, cy, cz};','float eyePos[] = {static_cast<float>(cx), static_cast<float>(cy), static_cast<float>(cz)};');p.write_text(s,encoding='utf-8')
