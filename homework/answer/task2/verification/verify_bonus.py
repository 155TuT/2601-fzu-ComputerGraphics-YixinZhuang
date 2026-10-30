"""Check model reproducibility and render with only Windows system tools on PATH."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    files = [ROOT / "docs/models/humanoid.obj", ROOT / "code/dae/custom/humanoid.dae"]
    before = [digest(path) for path in files]
    subprocess.run([sys.executable, str(ROOT / "scripts/build_humanoid.py")],
                   check=True, timeout=20, capture_output=True, text=True)
    assert before == [digest(path) for path in files], "The model generator must reproduce the same files"
    environment = os.environ.copy()
    environment["PATH"] = str(Path(os.environ["SystemRoot"]) / "System32")
    runtime = ROOT / "verification/runtime"
    runtime.mkdir(exist_ok=True)
    cases = [
        ("humanoid-env", "dae/custom/humanoid.dae", "hud-off,loop,loop,smooth,env", ["0.35", "0.15", "1.05"]),
        ("teapot-smooth", "dae/teapot.dae", "hud-off,smooth,phong", ["0.4", "0.3", "1.5"]),
    ]
    results = []
    for name, scene, operations, view in cases:
        output = runtime / ("bonus-" + name + ".png")
        process = subprocess.run([str(ROOT / "bin/meshedit.exe"), scene, "--screenshot", str(output),
                                  "--ops", operations, "--view", *view],
                                 cwd=ROOT / "code", env=environment, capture_output=True, text=True, timeout=20)
        expected = ROOT / "docs/screenshots" / (name + ".png")
        matches = process.returncode == 0 and output.is_file() and digest(output) == digest(expected)
        results.append({"scene": scene, "ops": operations, "view": view, "exit_code": process.returncode,
                        "identical_to_report_image": matches, "stdout": process.stdout, "stderr": process.stderr})
        assert matches, f"Clean-PATH image differs: {name}: {process.stderr}"
    result = {"generator_reproducible": True, "path": environment["PATH"], "renders": results}
    (ROOT / "verification/bonus-runtime.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print("PASS: deterministic OBJ/DAE generation, clean-PATH environment render, unchanged Phong render")


if __name__ == "__main__":
    main()
