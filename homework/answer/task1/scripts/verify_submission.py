"""Verify a strict-subset submission ZIP and smoke-test its own draw.exe.

Usage: python scripts/verify_submission.py [--zip path/to/task1-submit.zip]
       [--manifest path/to/package-manifest.json]
Uses only the Python standard library. Compares the external manifest with the
task1 originals, live submission directory, ZIP and fresh extracted files.
Every run preserves older extraction directories and timestamped audit files.
Windows is required for the four draw.exe headless runtime tests.
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
import uuid
import zlib

ROOT = Path(__file__).resolve().parents[1]
VERIFY = ROOT / "verification/submission-layout"
SUBMISSION = ROOT / "submission/task1-submit"
MANIFEST = VERIFY / "package-manifest.json"
PACKAGE_ROOT = "task1-submit"


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
    if path.is_absolute() or any(part in ("", ".", "..") for part in name.split("/")):
        raise ValueError(f"Unsafe archive path: {name!r}")
    for part in path.parts:
        reserved = part.split(".", 1)[0].upper()
        if part.endswith((" ", ".")) or reserved in {"CON", "PRN", "AUX", "NUL"} or reserved in {
                *(f"COM{i}" for i in range(1, 10)), *(f"LPT{i}" for i in range(1, 10))}:
            raise ValueError(f"Unsafe Windows archive path: {name!r}")
    return path


def unpack_checked(archive, manifest_path, destination, audit, log):
    manifest_data = manifest_path.read_bytes()
    manifest = json.loads(manifest_data.decode("utf-8-sig"))
    if manifest.get("schema") != 1 or manifest.get("package_root") != PACKAGE_ROOT:
        raise ValueError("Manifest must use schema 1 and package_root task1-submit")
    if manifest.get("archive_sha256", "").lower() != audit["zip_sha256"]:
        raise ValueError("External manifest archive SHA256 differs from ZIP")
    if manifest.get("archive_bytes") != audit["zip_bytes"]:
        raise ValueError("External manifest archive size differs from ZIP")
    records = manifest.get("files")
    if not isinstance(records, list) or not records:
        raise ValueError("Manifest has no file records")
    record_paths, file_integrity = [], []
    for record in records:
        path = safe_relative(record["path"])
        if path.as_posix() != record["path"] or path.parts[0].lower() == "submission":
            raise ValueError(f"Noncanonical or recursive manifest path: {record['path']}")
        size, digest = record["bytes"], record["sha256"]
        if not isinstance(size, int) or isinstance(size, bool) or size < 0:
            raise ValueError(f"Invalid manifest size: {record['path']}")
        if not isinstance(digest, str) or len(digest) != 64 or any(c not in "0123456789abcdefABCDEF" for c in digest):
            raise ValueError(f"Invalid manifest SHA256: {record['path']}")
        record_paths.append(record["path"])
        source = ROOT.joinpath(*path.parts)
        packaged = SUBMISSION.joinpath(*path.parts)
        for label, candidate, base in (("source", source, ROOT), ("submission", packaged, SUBMISSION)):
            if candidate.is_symlink() or not candidate.resolve().is_relative_to(base.resolve()):
                raise ValueError(f"{label} file escaped its root or is a symlink: {candidate}")
            if not candidate.is_file():
                raise FileNotFoundError(f"Missing {label} file: {candidate}")
            if candidate.stat().st_size != size or sha256(candidate) != digest.lower():
                raise ValueError(f"{label} differs from manifest: {candidate}")
        file_integrity.append({"path": record["path"], "bytes": size, "sha256": digest.lower(),
                               "source": str(source.resolve()), "submission": str(packaged.resolve())})
    if len(record_paths) != len({name.casefold() for name in record_paths}):
        raise ValueError("Manifest lists duplicate or Windows case-colliding paths")
    actual_submission_files = set()
    for item in SUBMISSION.rglob("*"):
        if item.is_symlink() or not item.resolve().is_relative_to(SUBMISSION.resolve()):
            raise ValueError(f"Submission directory contains an escaping path or symlink: {item}")
        if item.is_file():
            actual_submission_files.add(item.relative_to(SUBMISSION).as_posix())
        elif not item.is_dir():
            raise ValueError(f"Submission directory contains a special file: {item}")
    if actual_submission_files != set(record_paths):
        missing = sorted(set(record_paths) - actual_submission_files)
        extra = sorted(actual_submission_files - set(record_paths))
        raise ValueError(f"Submission file set differs from manifest: missing={missing!r}, extra={extra!r}")
    audit.update(manifest_sha256=hashlib.sha256(manifest_data).hexdigest(),
                 manifest_files_checked=len(records), source_files_checked=len(records),
                 submission_files_checked=len(records), file_integrity=file_integrity)
    log(f"External manifest and {len(records)} source/submission file hashes and sizes passed")

    extraction_root = destination / "独立运行 验证"
    with ZipFile(archive) as package:
        entries = package.infolist()
        names = [entry.filename.rstrip("/").casefold() for entry in entries]
        if len(names) != len(set(names)):
            raise ValueError("ZIP contains duplicate or Windows case-colliding member names")
        for entry in entries:
            relative = safe_relative(entry.filename.rstrip("/"))
            if relative.parts[0] != PACKAGE_ROOT:
                raise ValueError(f"ZIP member outside {PACKAGE_ROOT}/: {entry.filename}")
            if stat.S_ISLNK(entry.external_attr >> 16):
                raise ValueError(f"ZIP symlink is not permitted: {entry.filename}")
            target = extraction_root.joinpath(*relative.parts).resolve()
            if not target.is_relative_to(extraction_root.resolve()):
                raise ValueError(f"ZIP target escaped extraction directory: {entry.filename}")
        actual_files = {entry.filename for entry in entries if not entry.is_dir()}
        expected_files = {PACKAGE_ROOT + "/" + name for name in record_paths}
        if actual_files != expected_files:
            missing = sorted(expected_files - actual_files)
            extra = sorted(actual_files - expected_files)
            raise ValueError(f"ZIP file set differs from external manifest: missing={missing!r}, extra={extra!r}")
        damaged = package.testzip()
        if damaged is not None:
            raise ValueError(f"ZIP CRC failure: {damaged}")
        audit["zip_crc"] = "all members passed"
        for record in records:
            digest, total = hashlib.sha256(), 0
            with package.open(PACKAGE_ROOT + "/" + record["path"]) as source:
                for block in iter(lambda: source.read(1024 * 1024), b""):
                    digest.update(block)
                    total += len(block)
            if total != record["bytes"]:
                raise ValueError(f"Manifest size mismatch: {record['path']}")
            if digest.hexdigest() != record["sha256"].lower():
                raise ValueError(f"Manifest SHA256 mismatch: {record['path']}")
        log(f"ZIP CRC and {len(records)} manifest hashes/sizes passed")

        # Fail atomically if anything already occupies this exact destination.
        # No recursive deletion, overwriting or recovery-by-cleanup is attempted.
        destination.mkdir(parents=True, exist_ok=False)
        for entry in entries:
            relative = safe_relative(entry.filename.rstrip("/"))
            target = extraction_root.joinpath(*relative.parts)
            if entry.is_dir():
                target.mkdir(parents=True, exist_ok=True)
            else:
                target.parent.mkdir(parents=True, exist_ok=True)
                with package.open(entry) as source, target.open("xb") as output:
                    shutil.copyfileobj(source, output)
        for record in records:
            path = extraction_root / PACKAGE_ROOT / record["path"]
            if path.stat().st_size != int(record["bytes"]) or sha256(path) != record["sha256"].lower():
                raise ValueError(f"Extracted manifest mismatch: {record['path']}")
        audit["extracted_files_checked"] = len(records)
        log(f"Extracted {len(records)} manifest files and verified them again")
    return extraction_root / PACKAGE_ROOT


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
    parser.add_argument("--manifest", type=Path, default=MANIFEST)
    args = parser.parse_args()
    if os.name != "nt":
        print("Cannot verify the packaged Windows draw.exe on this platform; Windows/System32 is required",
              file=sys.stderr)
        return 2
    VERIFY.mkdir(parents=True, exist_ok=True)
    run_id = datetime.now(timezone(timedelta(hours=8))).strftime("%Y%m%d-%H%M%S-%f") + "-" + uuid.uuid4().hex[:8]
    destination = ROOT / "submission" / ("verification-" + run_id)
    json_path = VERIFY / ("submission-verify-" + run_id + ".json")
    log_path = VERIFY / ("submission-verify-" + run_id + ".log")
    # Exclusive creation guarantees that a collision cannot replace older evidence.
    for path in (json_path, log_path):
        with path.open("x", encoding="utf8"):
            pass
    audit = {"status": "running", "zip": str(args.zip.resolve()),
             "manifest": str(args.manifest.resolve()), "source_root": str(ROOT),
             "submission": str(SUBMISSION.resolve()), "package_root": PACKAGE_ROOT,
             "extraction": str(destination.resolve()), "tests": [],
             "audit_json": str(json_path.resolve()), "audit_log": str(log_path.resolve()),
             "timezone": "Asia/Shanghai", "started_at": timestamp()}
    logs = []

    def log(message):
        logs.append(message)
        print(message, flush=True)
        log_path.write_text("\n".join(logs) + "\n", encoding="utf8")

    def save():
        json_path.write_text(json.dumps(audit, ensure_ascii=False, indent=2) + "\n", encoding="utf8")

    try:
        archive = args.zip.resolve(strict=True)
        audit["zip_bytes"], audit["zip_sha256"] = archive.stat().st_size, sha256(archive)
        log(f"Archive: {archive}\nSHA256: {audit['zip_sha256']}")
        manifest_path = args.manifest.resolve(strict=True)
        extracted = unpack_checked(archive, manifest_path, destination, audit, log)
        audit["extracted_package"] = str(extracted.resolve())
        executable = extracted / "bin/draw.exe"
        if not executable.is_file():
            raise FileNotFoundError(executable)
        robot = extracted / "code/svg/transforms/robot.svg"
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
            cwd = destination / "smoke-output" / name
            cwd.mkdir(parents=True, exist_ok=False)
            command = [str(executable), str(svg), "nogl", "400", "400"]
            log(f"RUN {name}: {command!r}\nCWD: {cwd}\nPATH: {env['PATH']}")
            test = {"name": name, "svg": str(svg), "cwd": str(cwd), "command": command,
                    "svg_sha256": sha256(svg), "executable": str(executable),
                    "executable_sha256": audit["draw_sha256"],
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
        log(f"PASS: strict source/submission/ZIP subset, ZIP CRC, all extracted hashes, "
            f"{len(scenes)} clean-PATH smoke tests")
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
        (VERIFY / "submission-verify-latest.json").write_text(
            json.dumps(audit, ensure_ascii=False, indent=2) + "\n", encoding="utf8")


if __name__ == "__main__":
    raise SystemExit(main())
