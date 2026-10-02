"""Render genuine C++ function excerpts on a white background with line numbers.

Run with bundled Codex Python. No C++ or report text is changed. Stable default
names cover the six required tasks plus viewport rotation. Long excerpts split
into name.png, name_02.png, ... at display-line boundaries; no source is omitted.
The manifest records source/snippet/image SHA256 and exact original line ranges.

Examples:
  capture_code.py
  capture_code.py --only transforms barycentric
  capture_code.py --source code/src/rasterizer.cpp --function name --name aa_coverage
  capture_code.py --list
"""
from __future__ import annotations

import argparse
from hashlib import sha256
import json
import math
from pathlib import Path
import re
import unicodedata

from PIL import Image, ImageDraw, ImageFont

TASK = Path(__file__).resolve().parents[1]
OUTPUT = TASK / "docs/code-screenshots"
MANIFEST = TASK / "verification/code-screenshots.json"
FONT_PATH = Path("C:/Windows/Fonts/consola.ttf")
CHINESE_FONT_PATH = Path("C:/Windows/Fonts/msyh.ttc")
DEFAULTS = {
    "triangle_setup": ("code/src/rasterizer.cpp", ["struct TriangleSetup"]),
    "triangle_traversal": ("code/src/rasterizer.cpp", ["simd4_samples"]),
    "triangle_dispatch": ("code/src/rasterizer.cpp", ["for_each_triangle_sample"]),
    "triangle_scalar": ("code/src/rasterizer.cpp", ["scalar_pixel"]),
    "sample_buffer_resolve": ("code/src/rasterizer.cpp", ["RasterizerImp::fill_pixel", "RasterizerImp::set_sample_rate", "RasterizerImp::set_framebuffer_target", "RasterizerImp::resolve_to_framebuffer"]),
    "transforms": ("code/src/transforms.cpp", ["translate", "scale", "rotate"]),
    "barycentric": ("code/src/rasterizer.cpp", ["RasterizerImp::rasterize_interpolated_color_triangle"]),
    "texture_mapping": ("code/src/rasterizer.cpp", ["RasterizerImp::rasterize_textured_triangle"]),
    "texture_pixel": ("code/src/texture.cpp", ["Texture::sample_nearest", "Texture::sample_bilinear"]),
    "texture_lod": ("code/src/texture.cpp", ["Texture::sample", "Texture::get_level"]),
    "viewport_rotate": ("code/src/drawrend.cpp", ["DrawRend::set_view", "DrawRend::rotate_view"]),
    "analytic_clip": ("code/src/analytic_rasterizer.cpp", ["clip"]),
    "analytic_visibility": ("code/src/analytic_rasterizer.cpp", ["subtract", "AnalyticRasterizer::resolve_to_framebuffer"]),
}

TOKEN = re.compile(
    r"(?P<comment>//[^\n]*|/\*[\s\S]*?\*/)"
    r"|(?P<string>\"(?:\\.|[^\"\\])*\"|'(?:\\.|[^'\\])*')"
    r"|(?P<preprocessor>^[ \t]*\#[^\n]*)"
    r"|(?P<number>\b(?:0[xX][0-9a-fA-F]+|\d+(?:\.\d*)?(?:[eE][+-]?\d+)?)[uUlLfF]*\b)"
    r"|(?P<word>\b[A-Za-z_]\w*\b)", re.MULTILINE)
KEYWORDS = set("alignas alignof auto bool break case catch char class const constexpr continue default delete do double else enum explicit extern false float for friend if inline int long namespace new noexcept nullptr operator private protected public register return short signed sizeof static static_cast struct switch template this throw true try typedef typename union unsigned using virtual void volatile while".split())
COLORS = {"plain": "#171717", "comment": "#367D39", "string": "#A31515",
          "preprocessor": "#795E26", "number": "#245E57", "keyword": "#0000AD"}


def digest(data: bytes) -> str:
    return sha256(data).hexdigest()


def mask_non_code(text: str) -> str:
    chars = list(text)
    for match in TOKEN.finditer(text):
        if match.lastgroup in {"comment", "string", "preprocessor"}:
            for i in range(match.start(), match.end()):
                if chars[i] != "\n":
                    chars[i] = " "
    return "".join(chars)


def balance_end(masked: str, start: int, opening: str, closing: str) -> int:
    depth = 0
    for i in range(start, len(masked)):
        if masked[i] == opening:
            depth += 1
        elif masked[i] == closing:
            depth -= 1
            if depth == 0:
                return i + 1
    raise ValueError(f"Unbalanced {opening!r} at offset {start}")


def extract_range(text: str, name: str) -> tuple[int, int]:
    masked = mask_non_code(text)
    if name.startswith("struct "):
        pattern = r"(?m)^[ \t]*" + re.escape(name) + r"\b"
    else:
        pattern = (r"(?m)^[ \t]*(?:[A-Za-z_][A-Za-z0-9_:<>&,*]*[ \t]+)*"
                   + re.escape(name) + r"[ \t]*\(")
    candidates = []
    for match in re.finditer(pattern, masked):
        if name.startswith("struct "):
            brace = masked.find("{", match.end())
            if brace == -1 or ";" in masked[match.end():brace]:
                continue
        else:
            paren = masked.index("(", match.start(), match.end())
            after = balance_end(masked, paren, "(", ")")
            suffix = re.match(r"\s*(?:const\s*)?(?:noexcept\s*)?\{", masked[after:])
            if not suffix:
                continue
            brace = after + suffix.end() - 1
        end = balance_end(masked, brace, "{", "}")
        if name.startswith("struct ") and end < len(masked) and masked[end] == ";":
            end += 1
        first = text.count("\n", 0, match.start()) + 1
        last = text.count("\n", 0, end) + 1
        # Include the template line when it immediately precedes this function.
        lines = text.splitlines()
        if first > 1 and lines[first - 2].strip().startswith("template "):
            first -= 1
        candidates.append((first, last))
    if len(candidates) != 1:
        raise ValueError(f"Expected one definition of {name!r}; found {candidates}")
    return candidates[0]


def highlighted_lines(text: str) -> list[list[tuple[str, str]]]:
    spans, previous = [], 0
    for match in TOKEN.finditer(text):
        if match.start() > previous:
            spans.append((text[previous:match.start()], "plain"))
        role = match.lastgroup
        if role == "word":
            role = "keyword" if match.group() in KEYWORDS else "plain"
        spans.append((match.group(), role))
        previous = match.end()
    spans.append((text[previous:], "plain"))
    lines = [[]]
    for value, role in spans:
        pieces = value.split("\n")
        for i, piece in enumerate(pieces):
            if i:
                lines.append([])
            if piece:
                lines[-1].append((piece, role))
    return lines


def cells(char: str) -> int:
    return 2 if unicodedata.east_asian_width(char) in {"F", "W"} else 1


def visual_rows(source_lines, colored, first, last, columns):
    result = []
    for n in range(first, last + 1):
        glyphs, column = [], 0
        # Expand tabs while retaining the original source in the .cpp.txt sidecar.
        for value, role in colored[n - 1]:
            for char in value:
                expanded = " " * (4 - column % 4) if char == "\t" else char
                for displayed in expanded:
                    advance = cells(displayed)
                    glyphs.append((displayed, role))
                    column += advance
        continuation = False
        while glyphs:
            width, limit = 0, 0
            for char, _ in glyphs:
                if width + cells(char) > columns:
                    break
                width += cells(char)
                limit += 1
            if limit < len(glyphs):
                # Prefer whitespace or punctuation, so identifiers stay intact.
                breaks = [j + 1 for j, (char, _) in enumerate(glyphs[:limit])
                          if char.isspace() or char in ",;)"]
                sensible = [j for j in breaks if j >= limit * .4]
                if sensible:
                    limit = sensible[-1]
            result.append({"line": n, "continuation": continuation, "glyphs": glyphs[:limit]})
            continuation, glyphs = True, glyphs[limit:]
        if not column:
            result.append({"line": n, "continuation": False, "glyphs": []})
    return result


def render(path, rows, header, part, total, font_size, columns):
    font = ImageFont.truetype(str(FONT_PATH), font_size)
    chinese = ImageFont.truetype(str(CHINESE_FONT_PATH), font_size)
    small = ImageFont.truetype(str(FONT_PATH), max(15, font_size - 7))
    cw = font.getlength("0")
    line_height = round(font_size * 1.35)
    gutter = round(cw * 6)
    left, top, bottom = 22, 72, 23
    width = round(left + gutter + cw * columns + 24)
    height = top + line_height * len(rows) + bottom
    image = Image.new("RGB", (width, height), "white")
    draw = ImageDraw.Draw(image)
    draw.text((left, 13), header, font=small, fill="#333333")
    draw.text((left, 38), f"Source excerpt   part {part}/{total}   original line numbers",
              font=small, fill="#707070")
    for i, row in enumerate(rows):
        y = top + i * line_height
        if row is None:
            continue
        number = " >" if row["continuation"] else str(row["line"])
        draw.text((left + gutter - cw * (len(number) + 1), y), number, font=font, fill="#999999")
        x = left + gutter
        for char, role in row["glyphs"]:
            draw.text((round(x), y), char, font=chinese if cells(char) == 2 else font, fill=COLORS[role])
            x += cw * cells(char)
    image.save(path, format="PNG", optimize=True, dpi=(180, 180))
    return image.size


def capture(name, source, functions, args):
    path = (TASK / source).resolve()
    if TASK.resolve() not in path.parents:
        raise ValueError("Source must remain inside this task.")
    raw = path.read_bytes()
    text = raw.decode("utf-8-sig")
    # A normalized snapshot is used only for lexer offsets and display. Hash raw bytes.
    text = text.replace("\r\n", "\n").replace("\r", "\n")
    source_lines = text.splitlines()
    colored = highlighted_lines(text)
    ranges = [(function, *extract_range(text, function)) for function in functions]
    rows, fragments = [], []
    sidecar = OUTPUT / (name + ".cpp.txt")
    sidecar_text = []
    for i, (function, first, last) in enumerate(ranges):
        actual = "\n".join(source_lines[first - 1:last]) + "\n"
        fragments.append({"function": function, "first_line": first, "last_line": last,
                          "text_sha256": digest(actual.encode("utf-8"))})
        sidecar_text.append(actual)
        if i:
            rows.append(None)
        rows.extend(visual_rows(source_lines, colored, first, last, args.columns))
    sidecar.write_text("\n".join(sidecar_text), encoding="utf-8", newline="\n")
    # Balance parts rather than leaving a closing brace alone in a final PNG.
    count = max(1, math.ceil(len(rows) / args.max_rows))
    chunk_rows = math.ceil(len(rows) / count)
    chunks = [rows[i:i + chunk_rows] for i in range(0, len(rows), chunk_rows)]
    images = []
    for i, chunk in enumerate(chunks):
        filename = name + ("" if i == 0 else f"_{i + 1:02}") + ".png"
        output = OUTPUT / filename
        dimensions = render(output, chunk, source, i + 1, len(chunks), args.font_size, args.columns)
        original_lines = [row["line"] for row in chunk if row is not None]
        images.append({"file": str(output.relative_to(TASK)).replace("\\", "/"),
                       "pixels": dimensions, "sha256": digest(output.read_bytes()),
                       "first_line": min(original_lines), "last_line": max(original_lines),
                       "part": i + 1, "display_rows": len(chunk)})
    return {"name": name, "source": source, "source_sha256": digest(raw),
            "fragments": fragments, "sidecar": str(sidecar.relative_to(TASK)).replace("\\", "/"),
            "sidecar_sha256": digest(sidecar.read_bytes()), "images": images}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--only", nargs="+", choices=sorted(DEFAULTS))
    parser.add_argument("--source")
    parser.add_argument("--function", action="append")
    parser.add_argument("--name")
    parser.add_argument("--list", action="store_true")
    parser.add_argument("--columns", type=int, default=76)
    parser.add_argument("--max-rows", type=int, default=34)
    parser.add_argument("--font-size", type=int, default=25)
    args = parser.parse_args()
    if any((args.source, args.function, args.name)):
        if not all((args.source, args.function, args.name)):
            parser.error("Custom excerpts need --source, --function and --name together.")
        if not re.fullmatch(r"[a-z][a-z0-9_]*", args.name):
            parser.error("--name must be a lowercase stable filename without extension.")
        selected = {args.name: (args.source, args.function)}
    else:
        selected = {name: DEFAULTS[name] for name in args.only or DEFAULTS}
    if args.list:
        for name, (source, functions) in selected.items():
            text = (TASK / source).read_text(encoding="utf-8-sig")
            print(name, source, [(f, extract_range(text, f)) for f in functions])
        return
    if not 40 <= args.columns <= 160 or not 10 <= args.max_rows <= 60:
        parser.error("Use 40..160 columns and 10..60 rows for readable report excerpts.")
    OUTPUT.mkdir(parents=True, exist_ok=True)
    entries = [capture(name, source, functions, args) for name, (source, functions) in selected.items()]
    previous = json.loads(MANIFEST.read_text(encoding="utf-8")) if MANIFEST.exists() else {}
    combined = {entry["name"]: entry for entry in previous.get("excerpts", [])}
    combined.update({entry["name"]: entry for entry in entries})
    manifest = {"generator": "scripts/capture_code.py", "generator_sha256": digest(Path(__file__).read_bytes()),
                "background": "#FFFFFF", "font": str(FONT_PATH), "font_sha256": digest(FONT_PATH.read_bytes()),
                "columns": args.columns, "max_rows": args.max_rows, "font_size_px": args.font_size,
                "tab_columns": 4, "wrapping": "display-only; continuation rows keep original line number",
                "excerpts": list(combined.values())}
    MANIFEST.parent.mkdir(parents=True, exist_ok=True)
    MANIFEST.write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
    for entry in entries:
        print(entry["name"] + ": " + ", ".join(Path(image["file"]).name for image in entry["images"]))


if __name__ == "__main__":
    main()
