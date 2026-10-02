"""Verify viewport math and capture its GUI bonus using real DrawRend callbacks.

The GLFW window and OpenGL/S capture path are real; input is supplied by the
verification program, not by a person pressing keys in a visible window.
"""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess
from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs/screenshots"
WORK = OUT / ".capture/viewport"
WORK.mkdir(parents=True, exist_ok=True)
RUNNER = ROOT / "build/screenshot_runner.exe"
ENV = os.environ.copy()
ENV["PATH"] = r"C:\msys64\mingw64\bin;" + ENV.get("PATH", "")


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(args: list[str]) -> subprocess.CompletedProcess:
    result = subprocess.run([str(RUNNER), *args], cwd=WORK, env=ENV,
                            capture_output=True, text=True, timeout=90)
    if result.returncode:
        raise RuntimeError(f"exit {result.returncode}\n{result.stdout}\n{result.stderr}")
    return result


def main() -> None:
    retained = {path.name: sha256(path) for path in OUT.glob("*.png")
                if not path.name.startswith("viewport_")}
    checks = run(["--viewport-check", str(ROOT / "docs/my_robot.svg"),
                  str(ROOT / "code/svg/basic/test7.svg")])
    result_match = re.search(r"VIEWPORT_RESULT (\d+) passed, (\d+) failed", checks.stdout)
    if not result_match or int(result_match[2]) != 0:
        raise RuntimeError("Viewport numerical verification has no passing summary")
    captures = []
    for name, steps, reset in [("viewport_default", 0, 0),
                               ("viewport_rotated_30", 2, 0),
                               ("viewport_reset", 2, 1)]:
        before = set(WORK.glob("screenshot_*.png"))
        args = [str(ROOT / "docs/my_robot.svg"), "16", "0", "0", "0",
                "400", "400", "1", "800", str(steps), str(reset)]
        result = run(args)
        outputs = set(WORK.glob("screenshot_*.png")) - before
        if len(outputs) != 1:
            raise RuntimeError(f"Expected one S callback PNG for {name}: {outputs}")
        target = OUT / f"{name}.png"
        outputs.pop().replace(target)
        with Image.open(target) as image:
            if image.format != "PNG" or image.size != (800, 800):
                raise RuntimeError(f"Invalid PNG capture: {target}")
        captures.append({"png": str(target.relative_to(ROOT)), "sha256": sha256(target),
                         "svg": "docs/my_robot.svg", "width": 800, "height": 800,
                         "spp": 16, "psm": "P_NEAREST", "lsm": "L_ZERO",
                         "rotation_steps": steps, "angle_degrees_clockwise": 0 if reset else steps * 15,
                         "space_reset": bool(reset), "command_args": args,
                         "stdout": result.stdout, "stderr": result.stderr,
                         "capture": "Hidden GLFW window; automated real DrawRend E/Space/S callbacks"})
    with Image.open(OUT / "viewport_default.png") as default, \
         Image.open(OUT / "viewport_rotated_30.png") as rotated, \
         Image.open(OUT / "viewport_reset.png") as reset:
        default = default.convert("RGB")
        rotated = rotated.convert("RGB")
        reset = reset.convert("RGB")
        rotation_changed = ImageChops.difference(default, rotated).getbbox() is not None
        reset_identical = ImageChops.difference(default, reset).getbbox() is None
    if not rotation_changed or not reset_identical:
        raise AssertionError("Rotation must change the image and Space must exactly restore it")
    preserved = all(sha256(OUT / name) == original for name, original in retained.items())
    if not preserved:
        raise AssertionError("A retained report PNG changed")
    report = {
        "requirement": "Task PDF page 7: additional GUI function, example PNG, SVG-to-NDC-to-screen stack explanation",
        "keys": {"Q": "counterclockwise 15 degrees", "E": "clockwise 15 degrees",
                 "Space": "reset active SVG center, span and angle"},
        "matrix_stack": "ndc_to_screen * T(.5,.5) * R(angle) * T(-.5,-.5) * svg_to_ndc_base * element_transform",
        "svg_state": "Independent center, span, angle retained for each loaded SVG",
        "test_summary": {"passed": int(result_match[1]), "failed": int(result_match[2]),
                         "stdout": checks.stdout, "stderr": checks.stderr},
        "rotation_changes_pixels": rotation_changed,
        "space_restores_identical_pixels": reset_identical,
        "retained_png_count": len(retained), "retained_pngs_unchanged": preserved,
        "manual_gui_validation": False,
        "captures": captures,
    }
    (ROOT / "verification/viewport-tests.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    (ROOT / "verification/viewport-tests.log").write_text(
        checks.stdout + checks.stderr + "\n" + "\n".join(item["stdout"] + item["stderr"] for item in captures),
        encoding="utf-8")
    print(f"Viewport: {result_match[1]} checks passed; 3 PNGs; reset identical; {len(retained)} retained PNGs unchanged")


if __name__ == "__main__":
    main()
