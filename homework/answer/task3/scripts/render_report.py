"""Render report experiments through the submitted path tracer executable."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import statistics
import subprocess
import time

TASK = Path(__file__).resolve().parents[1]
EXE = TASK / "bin/pathtracer.exe"
OUT = TASK / "docs/screenshots"
LOGS = TASK / "verification/experiments"


def render(name, scene="sky/CBbunny.dae", spp=32, lights=8, depth=5,
           flags=(), size=(480, 360), threads=4, seed=20261025):
    path = OUT / f"{name}.png"
    args = [str(EXE), "-t", str(threads), "-s", str(spp), "-l", str(lights),
            "-m", str(depth), "-q", str(seed), "-r", *map(str, size),
            *flags, "-f", str(path), f"code/dae/{scene}"]
    start = time.perf_counter()
    process = subprocess.run(args, cwd=TASK, capture_output=True, timeout=600)
    wall = time.perf_counter() - start
    log = (process.stdout + process.stderr).decode("utf-8", errors="replace")
    (LOGS / f"{name}.log").write_text(log, encoding="utf-8")
    if process.returncode != 0 or not path.is_file():
        raise RuntimeError(f"{name}: exit={process.returncode}, see experiment log")
    rate = path.with_name(path.stem + "_rate.png")
    if rate.is_file():
        rate_output = LOGS / "sample-rates" / rate.name
        rate_output.parent.mkdir(parents=True, exist_ok=True)
        rate.replace(rate_output)
    timings = re.findall(r"100%! \(([0-9.]+)s\)", log)
    tests = re.findall(r"Averaged ([0-9.]+) intersection tests per ray", log)
    rays = re.findall(r"(?:BVH|Brute.force) traced ([0-9]+) rays", log)
    primitives = re.findall(r"([0-9]+) primitives", log)
    result = {"name": name, "scene": scene, "spp": spp, "light_samples": lights,
              "max_depth": depth, "flags": list(flags), "size": list(size),
              "threads": threads, "seed": seed, "command": args,
              "wall_seconds": wall,
              "render_seconds": float(timings[-1]) if timings else None,
              "tests_per_ray": float(tests[-1]) if tests else None,
              "rays": int(rays[-1]) if rays else None,
              "primitives": int(primitives[-1]) if primitives else None,
              "image": str(path.relative_to(TASK)).replace("\\", "/"),
              "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
    print(f"{name}: render={result['render_seconds']}s wall={wall:.3f}s", flush=True)
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--group", choices=["all", "normals", "lighting", "bounces", "benchmark", "fzu"], default="all")
    options = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    LOGS.mkdir(parents=True, exist_ok=True)
    results = []

    def run(*args, **kwargs):
        results.append(render(*args, **kwargs))
        (LOGS / f"{options.group}.json").write_text(json.dumps({
            "executable_sha256": hashlib.sha256(EXE.read_bytes()).hexdigest(),
            "experiments": results}, ensure_ascii=False, indent=2), encoding="utf-8")

    if options.group in ("all", "normals"):
        for name, scene, spp in [("normal_cube", "simple/cube.dae", 4),
                                 ("normal_teapot", "meshedit/teapot.dae", 4),
                                 ("normal_maxplanck", "meshedit/maxplanck.dae", 4),
                                 ("normal_lucy", "sky/CBlucy.dae", 1)]:
            run(name, scene, spp=spp, lights=1, depth=1, flags=["-N"])
    if options.group in ("all", "lighting"):
        run("bunny_hemisphere", spp=16, lights=16, depth=1, flags=["-H"])
        run("bunny_importance", spp=16, lights=16, depth=1)
        for lights in (1, 4, 16):
            run(f"bunny_light_{lights}", spp=4, lights=lights, depth=1)
        run("bunny_gi_16", spp=16, lights=8, depth=5)
        run("bunny_gi_128", spp=128, lights=8, depth=5)
        run("bunny_direct_128", spp=128, lights=8, depth=1)
        run("bunny_fixed_128", spp=128, lights=8, depth=5, flags=["-U"])
        run("spheres_gi", "sky/CBspheres_lambertian.dae", spp=128, lights=8, depth=5)
        run("bunny_rr32", spp=64, lights=8, depth=32, size=(320, 240))
        run("bunny_fixed32", spp=64, lights=8, depth=32, flags=["-U"], size=(320, 240))
    if options.group in ("all", "bounces"):
        for accum in (0, 1):
            for depth in range(6):
                run(f"bunny_o{accum}_m{depth}", "sky/CBbunny.dae", spp=64,
                    lights=8, depth=depth, size=(240, 180), flags=["-o", str(accum), "-U"])
    if options.group in ("all", "fzu"):
        for name, depth in (("fzu-direct", 1), ("fzu-gi", 8)):
            run(name, "fzu/FZU-gallery.dae", spp=128, lights=8, depth=depth, size=(640, 480))
    if options.group in ("all", "benchmark"):
        for repetition in range(3):
            for brute in (False, True):
                run(f"benchmark_{'brute' if brute else 'bvh'}_{repetition}",
                    "meshedit/teapot.dae", spp=1, lights=1, depth=1,
                    size=(160, 120), threads=1, flags=["-N"] + (["-B"] if brute else []))
        summary = {}
        for mode in ("bvh", "brute"):
            subset = [r for r in results if f"benchmark_{mode}_" in r["name"]]
            summary[mode] = {"median_render_seconds": statistics.median(r["render_seconds"] for r in subset),
                             "median_wall_seconds": statistics.median(r["wall_seconds"] for r in subset),
                             "tests_per_ray": subset[0]["tests_per_ray"]}
        summary["image_equality"] = len({r["sha256"] for r in results if r["name"].startswith("benchmark_")}) == 1
        summary["render_speedup"] = summary["brute"]["median_render_seconds"] / summary["bvh"]["median_render_seconds"]
        (LOGS / "benchmark-summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")


if __name__ == "__main__":
    main()
