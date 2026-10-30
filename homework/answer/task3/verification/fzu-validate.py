"""Verify scene generation in a source-only copy and render normal diagnostics."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import uuid


TASK = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    temporary = TASK / "verification/tmp" / ("fzu-generator-" + uuid.uuid4().hex[:8])
    temporary.mkdir(parents=True)
    for directory in ("code", "scripts"):
        shutil.copytree(TASK / directory, temporary / directory)
    assert not (temporary / "verification").exists()
    generation = subprocess.run([sys.executable, "-X", "utf8", str(temporary / "scripts/create_fzu_scene.py")],
                                cwd=temporary, capture_output=True, text=True, timeout=30)
    expected = TASK / "code/dae/fzu/FZU-gallery.dae"
    generated = temporary / "code/dae/fzu/FZU-gallery.dae"
    same = generation.returncode == 0 and digest(expected) == digest(generated)
    assert same, generation.stderr
    assert (temporary / "verification/fzu-scene.json").is_file()
    preview = TASK / "verification/fzu-previews/fzu-preview-normals.png"
    preview.parent.mkdir(parents=True, exist_ok=True)
    command = [str(TASK / "bin/pathtracer.exe"), "-N", "-s", "4", "-t", "2",
               "-r", "320", "240", "-f", str(preview), str(expected)]
    normal = subprocess.run(command, cwd=TASK, capture_output=True, text=True, timeout=30)
    assert normal.returncode == 0 and preview.is_file(), normal.stderr
    result = {"source_only_directory": str(temporary), "copied_directories": ["code", "scripts"],
              "verification_absent_before_generation": True,
              "generator_exit_code": generation.returncode, "identical_dae_bytes": same,
              "scene_sha256": digest(expected), "normal_preview_exit_code": normal.returncode,
              "normal_preview_command": command,
              "normal_preview_log": normal.stdout + normal.stderr}
    (TASK / "verification/fzu-subset.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print("PASS: source-only generation creates verification and reproduces identical DAE bytes; normal render succeeds")


if __name__ == "__main__":
    main()
