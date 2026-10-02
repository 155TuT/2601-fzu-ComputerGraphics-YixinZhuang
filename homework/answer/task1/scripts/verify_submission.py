"""Verify the final ZIP, extract once, and smoke-test its own draw.exe.

Usage: python scripts/verify_submission.py [--zip path/to/task1-submit.zip]
Uses only the Python standard library. Never deletes or overwrites an existing
extraction directory, and never modifies the source tree, reports or archive.
"""
from pathlib import Path, PurePosixPath
from zipfile import ZipFile
import argparse
import ctypes
from datetime import datetime, timedelta, timezone
import hashlib
import json
import os
import shutil
import stat
import struct
import subprocess
import sys
import zlib

ROOT = Path(__file__).resolve().parents[1]
VERIFY = ROOT / "verification"
DESTINATION = VERIFY / "submission-extracted-bonus-final"
JSON_PATH = VERIFY / "submission-smoke.json"
LOG_PATH = VERIFY / "submission-smoke.log"


def timestamp():
    return datetime.now(timezone(timedelta(hours=8))).isoformat(timespec="seconds")


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def safe_relative(name):
    """Archive paths must be ordinary relative POSIX paths, without Windows ADS."""
    if not isinstance(name, str) or not name or "\\" in name or ":" in name or "\0" in name:
        raise ValueError(f"Unsafe archive path: {name!r}")
    path = PurePosixPath(name)
    if path.is_absolute() or any(part in (".", "..") for part in name.split("/")):
        raise ValueError(f"Unsafe archive path: {name!r}")
    return path


def unpack_checked(archive, audit, log):
    with ZipFile(archive) as package:
        entries = package.infolist()
        names = [entry.filename for entry in entries]
        if len(names) != len(set(names)):
            raise ValueError("ZIP contains duplicate member names")
        for entry in entries:
            relative = safe_relative(entry.filename.rstrip("/"))
            if relative.parts[0] != "task1":
                raise ValueError(f"ZIP member outside task1/: {entry.filename}")
            if stat.S_ISLNK(entry.external_attr >> 16):
                raise ValueError(f"ZIP symlink is not permitted: {entry.filename}")
            target = DESTINATION.joinpath(*relative.parts).resolve()
            if not target.is_relative_to(DESTINATION.resolve()):
                raise ValueError(f"ZIP target escaped extraction directory: {entry.filename}")
        damaged = package.testzip()
        if damaged is not None:
            raise ValueError(f"ZIP CRC failure: {damaged}")
        audit["zip_crc"] = "all members passed"
        manifest_data = package.read("task1/manifest.json")
        manifest = json.loads(manifest_data.decode("utf8"))
        records = manifest.get("files")
        if not isinstance(records, list) or not records:
            raise ValueError("Manifest has no file records")
        record_paths = []
        for record in records:
            path = safe_relative(record["path"])
            if path.as_posix() != record["path"]:
                raise ValueError(f"Noncanonical manifest path: {record['path']}")
            record_paths.append(record["path"])
            data = package.read("task1/" + record["path"])
            if len(data) != int(record["bytes"]):
                raise ValueError(f"Manifest size mismatch: {record['path']}")
            if hashlib.sha256(data).hexdigest() != record["sha256"].lower():
                raise ValueError(f"Manifest SHA256 mismatch: {record['path']}")
        if len(record_paths) != len(set(record_paths)):
            raise ValueError("Manifest lists duplicate paths")
        actual_files = {entry.filename for entry in entries if not entry.is_dir()}
        expected_files = {"task1/" + name for name in record_paths} | {"task1/manifest.json"}
        if actual_files != expected_files:
            raise ValueError("ZIP file set differs from manifest plus manifest.json")
        audit["manifest_sha256"] = hashlib.sha256(manifest_data).hexdigest()
        audit["manifest_files_checked"] = len(records)
        log(f"ZIP CRC and {len(records)} manifest hashes/sizes passed")

        # Fail atomically if anything already occupies this exact destination.
        # No recursive deletion, overwriting or recovery-by-cleanup is attempted.
        DESTINATION.mkdir(parents=False, exist_ok=False)
        for entry in entries:
            relative = safe_relative(entry.filename.rstrip("/"))
            target = DESTINATION.joinpath(*relative.parts)
            if entry.is_dir():
                target.mkdir(parents=True, exist_ok=True)
            else:
                target.parent.mkdir(parents=True, exist_ok=True)
                with package.open(entry) as source, target.open("xb") as output:
                    shutil.copyfileobj(source, output)
        for record in records:
            path = DESTINATION / "task1" / record["path"]
            if path.stat().st_size != int(record["bytes"]) or sha256(path) != record["sha256"].lower():
                raise ValueError(f"Extracted manifest mismatch: {record['path']}")
        audit["extracted_files_checked"] = len(records)
        log(f"Extracted {len(records)} manifest files and verified them again")
    return DESTINATION / "task1"


def paeth(a, b, c):
    p = a + b - c
    distances = (abs(p - a), abs(p - b), abs(p - c))
    return (a, b, c)[distances.index(min(distances))]


def verify_png(path):
    """CRC-check and fully reconstruct a noninterlaced PNG, including palettes."""
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("Invalid PNG signature")
    offset, header, palette, transparency, compressed = 8, None, None, b"", bytearray()
    ended, chunks = False, 0
    while offset < len(data):
        if offset + 12 > len(data):
            raise ValueError("Truncated PNG chunk")
        length = struct.unpack_from(">I", data, offset)[0]
        kind = data[offset + 4:offset + 8]
        stop = offset + 12 + length
        if stop > len(data):
            raise ValueError("Truncated PNG chunk data")
        payload = data[offset + 8:offset + 8 + length]
        stored_crc = struct.unpack_from(">I", data, offset + 8 + length)[0]
        if zlib.crc32(kind + payload) & 0xffffffff != stored_crc:
            raise ValueError(f"PNG chunk CRC failure: {kind!r}")
        if chunks == 0 and kind != b"IHDR":
            raise ValueError("IHDR must be the first PNG chunk")
        if kind == b"IHDR":
            if header is not None or length != 13:
                raise ValueError("Invalid/duplicate PNG header")
            header = struct.unpack(">IIBBBBB", payload)
        elif kind == b"PLTE":
            if length == 0 or length % 3 or length > 768:
                raise ValueError("Invalid PNG palette")
            palette = [tuple(payload[i:i + 3]) for i in range(0, length, 3)]
        elif kind == b"tRNS":
            transparency = payload
        elif kind == b"IDAT":
            compressed.extend(payload)
        elif kind == b"IEND":
            if length != 0 or stop != len(data):
                raise ValueError("Invalid PNG IEND or trailing data")
            ended = True
            break
        elif kind[0] & 0x20 == 0:
            raise ValueError(f"Unsupported critical PNG chunk: {kind!r}")
        offset, chunks = stop, chunks + 1
    if not ended or header is None or not compressed:
        raise ValueError("PNG missing header, image stream or end marker")
    width, height, depth, color_type, compression, filtering, interlace = header
    if (width, height) != (400, 400):
        raise ValueError(f"Expected 400x400 PNG, got {width}x{height}")
    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}.get(color_type)
    valid_depths = {0: (1, 2, 4, 8, 16), 2: (8, 16), 3: (1, 2, 4, 8), 4: (8, 16), 6: (8, 16)}
    if channels is None or depth not in valid_depths[color_type] or compression or filtering or interlace:
        raise ValueError(f"Unsupported/invalid PNG encoding: {header}")
    if color_type == 3 and (palette is None or len(palette) > 1 << depth):
        raise ValueError("Indexed PNG has no valid palette")
    stride = (width * channels * depth + 7) // 8
    bytes_per_pixel = max(1, (channels * depth + 7) // 8)
    expected_length = height * (stride + 1)
    decoder = zlib.decompressobj()
    raw = decoder.decompress(bytes(compressed), expected_length + 1)
    if len(raw) != expected_length or not decoder.eof or decoder.unused_data or decoder.unconsumed_tail:
        raise ValueError("PNG IDAT stream length/completion mismatch")
    previous = bytearray(stride)
    foreground, nonopaque = 0, 0
    for y in range(height):
        start = y * (stride + 1)
        filter_type, row = raw[start], bytearray(raw[start + 1:start + 1 + stride])
        if filter_type > 4:
            raise ValueError("Invalid PNG scanline filter")
        for i in range(stride):
            left = row[i - bytes_per_pixel] if i >= bytes_per_pixel else 0
            up = previous[i]
            upper_left = previous[i - bytes_per_pixel] if i >= bytes_per_pixel else 0
            predictor = (0, left, up, (left + up) // 2, paeth(left, up, upper_left))[filter_type]
            row[i] = (row[i] + predictor) & 255
        for x in range(width):
            if depth < 8:
                bit = x * depth
                value = (row[bit // 8] >> (8 - depth - bit % 8)) & ((1 << depth) - 1)
                values = [value]
            elif depth == 8:
                values = row[x * channels:(x + 1) * channels]
            else:
                values = [struct.unpack_from(">H", row, 2 * (x * channels + c))[0] for c in range(channels)]
            maximum, alpha = (1 << depth) - 1, 255
            if color_type == 3:
                if values[0] >= len(palette):
                    raise ValueError("PNG palette index out of range")
                rgb = palette[values[0]]
                alpha = transparency[values[0]] if values[0] < len(transparency) else 255
            elif color_type in (0, 4):
                rgb = (255 * values[0] // maximum,) * 3
                if color_type == 4:
                    alpha = 255 * values[1] // maximum
            else:
                rgb = tuple(255 * component // maximum for component in values[:3])
                if color_type == 6:
                    alpha = 255 * values[3] // maximum
            foreground += rgb != (255, 255, 255)
            nonopaque += alpha != 255
        previous = row
    if foreground == 0 or nonopaque:
        raise ValueError(f"Unexpected empty or nonopaque framebuffer: foreground={foreground}, nonopaque={nonopaque}")
    return {"width": width, "height": height, "bit_depth": depth, "color_type": color_type,
            "foreground_pixels": foreground, "nonopaque_pixels": nonopaque,
            "validation": "all chunk CRCs, complete IDAT decode, all scanline filters and pixels"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--zip", type=Path, default=ROOT / "submission/task1-submit.zip")
    args = parser.parse_args()
    if DESTINATION.exists():
        # Preserve both the previous extraction and its existing smoke evidence.
        print(f"REFUSED: extraction directory already exists: {DESTINATION}", file=sys.stderr)
        return 2
    if os.name != "nt":
        print("This validation specifically requires Windows/System32", file=sys.stderr)
        return 2
    VERIFY.mkdir(exist_ok=True)
    audit = {"status": "running", "zip": str(args.zip.resolve()),
             "extraction": str(DESTINATION.resolve()), "tests": [],
             "timezone": "Asia/Shanghai", "started_at": timestamp()}
    logs = []

    def log(message):
        logs.append(message)
        print(message, flush=True)
        LOG_PATH.write_text("\n".join(logs) + "\n", encoding="utf8")

    def save():
        JSON_PATH.write_text(json.dumps(audit, ensure_ascii=False, indent=2), encoding="utf8")

    try:
        archive = args.zip.resolve(strict=True)
        audit["zip_bytes"], audit["zip_sha256"] = archive.stat().st_size, sha256(archive)
        log(f"Archive: {archive}\nSHA256: {audit['zip_sha256']}")
        extracted = unpack_checked(archive, audit, log)
        executable = extracted / "bin/draw.exe"
        if not executable.is_file():
            raise FileNotFoundError(executable)
        robot = extracted / "code/svg/transforms/robot.svg"
        if not robot.is_file():
            alternatives = sorted((extracted / "code/svg").rglob("*robot*.svg"))
            if not alternatives:
                raise FileNotFoundError("No packaged robot SVG")
            robot = alternatives[0]
        scenes = [("basic_test4", extracted / "code/svg/basic/test4.svg"),
                  ("teacher_robot", robot), ("my_robot", extracted / "docs/my_robot.svg"),
                  ("texture_demo", extracted / "docs/texture_demo.svg")]
        env = os.environ.copy()
        env["PATH"] = str(Path(os.environ["SystemRoot"]) / "System32")
        audit["runtime_path"] = env["PATH"]
        audit["runtime_search"] = "System32-only PATH; DLLs next to extracted draw.exe; fresh isolated cwd"
        audit["draw_sha256"] = sha256(executable)
        ctypes.windll.kernel32.SetErrorMode(0x0001 | 0x0002 | 0x8000)
        save()
        for name, svg in scenes:
            if not svg.is_file():
                raise FileNotFoundError(svg)
            cwd = DESTINATION / "smoke-output" / name
            cwd.mkdir(parents=True, exist_ok=False)
            command = [str(executable), str(svg), "nogl", "400", "400"]
            log(f"RUN {name}: {command!r}\nCWD: {cwd}\nPATH: {env['PATH']}")
            test = {"name": name, "svg": str(svg), "cwd": str(cwd), "command": command,
                    "status": "running"}
            audit["tests"].append(test)
            save()
            result = subprocess.run(command, cwd=cwd, env=env, capture_output=True,
                                    timeout=60, creationflags=subprocess.CREATE_NO_WINDOW)
            stdout, stderr = result.stdout.decode("utf8", errors="replace"), result.stderr.decode("utf8", errors="replace")
            (cwd / "stdout.log").write_text(stdout, encoding="utf8")
            (cwd / "stderr.log").write_text(stderr, encoding="utf8")
            test.update(exit_code=result.returncode, stdout=stdout, stderr=stderr)
            log(f"EXIT {result.returncode}\nSTDOUT:\n{stdout}\nSTDERR:\n{stderr}")
            if result.returncode != 0:
                raise RuntimeError(f"{name}: draw.exe exited {result.returncode}")
            if any(message in stderr.lower() for message in ("could not", "no svg files", "invalid svg")):
                raise RuntimeError(f"{name}: runtime reported load/write failure")
            png = cwd / "test.png"
            if not png.is_file():
                raise FileNotFoundError(f"{name}: expected draw.exe output {png}")
            test.update(png=str(png), png_bytes=png.stat().st_size,
                        png_sha256=sha256(png), png_info=verify_png(png), status="passed")
            log(f"PASS {name}: 400x400 PNG fully decoded, SHA256 {test['png_sha256']}")
            save()
        audit["status"] = "passed"
        audit["passed_tests"] = len(audit["tests"])
        log(f"PASS: ZIP CRC, all manifest hashes before/after extraction, {len(scenes)} clean-PATH smoke tests")
        return 0
    except Exception as error:
        audit["status"], audit["error"] = "failed", repr(error)
        if audit["tests"] and audit["tests"][-1]["status"] == "running":
            audit["tests"][-1].update(status="failed", error=repr(error))
        log(f"FAIL: {error!r}")
        return 1
    finally:
        audit["finished_at"] = timestamp()
        save()


if __name__ == "__main__":
    raise SystemExit(main())
