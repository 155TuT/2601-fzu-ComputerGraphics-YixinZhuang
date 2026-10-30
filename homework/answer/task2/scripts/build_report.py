"""Build the coursework report from its retained teacher DOCX and Markdown.

Run with the bundled Codex Python. The source template remains unchanged.
Supported Markdown: headings, paragraphs, bold, inline code, pipe tables,
<!-- page --> and <!-- images: image.png | image.png -->.
"""
from __future__ import annotations

from copy import deepcopy
from hashlib import sha256
from io import BytesIO
import json
from pathlib import Path
import re
from zipfile import ZipFile, ZIP_DEFLATED

from docx import Document
from docx.enum.style import WD_STYLE_TYPE
from docx.enum.table import WD_CELL_VERTICAL_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Inches, Pt, RGBColor
from lxml import etree
from PIL import Image

TASK = Path(__file__).resolve().parents[1]
REPO = TASK.parents[2]
REFERENCE = REPO / "homework" / "teacher" / TASK.name / "作业报告.docx"
FINAL = TASK / "docs/作业报告.docx"
MARKDOWN = TASK / "docs/报告.md"
QA = TASK / "verification/report"
TEMPLATE_HASHES = {
    "task2": "4dcd9e375b0a19e4d15648fb750dbb90d2607a82c8e50a96ebfa95e9b3a7df15",
    "task3": "8bad90cb5da925c56a10fcc0f2fc2ff8bb8d67e5d1dc712f6e4a00a4ff1ddd31",
}


def digest(data: bytes) -> str:
    return sha256(data).hexdigest()


def append_xml(original: bytes, children: list[bytes], closing: bytes) -> bytes:
    if not children:
        return original
    if original.count(closing) != 1:
        raise ValueError(f"Ambiguous XML closing marker: {closing!r}")
    return original.replace(closing, b"".join(children) + closing)


def black(rpr) -> None:
    color = rpr.find(qn("w:color"))
    if color is None:
        color = OxmlElement("w:color")
        rpr.append(color)
    color.attrib.clear()
    color.set(qn("w:val"), "000000")


def table_cells(line: str) -> list[str]:
    return [part.strip().replace(r"\|", "|")
            for part in re.split(r"(?<!\\)\|", line.strip().strip("|"))]


def build() -> None:
    reference_hash = TEMPLATE_HASHES[TASK.name]
    if digest(REFERENCE.read_bytes()) != reference_hash:
        raise ValueError("The retained teacher report template changed.")
    contract = QA / "artifact.md"
    if not contract.is_file() or reference_hash not in contract.read_text(encoding="utf-8-sig"):
        raise ValueError("The task-local template contract is missing or mismatched.")
    text = MARKDOWN.read_text(encoding="utf-8")
    unresolved = re.findall(r"<!--\s*pending:.*?-->", text)
    if unresolved:
        raise ValueError(f"Report evidence is incomplete: {unresolved}")

    doc = Document(REFERENCE)
    section_before = deepcopy(doc.sections[0]._sectPr)
    source_paragraphs = list(doc.paragraphs)
    role_index = {"title": 1, "subtitle": 2, "author": 3, "heading": 5,
                  "body": 6, "caption": 6, "list": 6, "table": 6,
                  "subheading": 9 if TASK.name == "task2" else 5}
    roles = {}
    for role, index in role_index.items():
        source = source_paragraphs[index]
        run = next(run for run in source.runs if run.text)
        roles[role] = (deepcopy(source._p.pPr), deepcopy(run._r.rPr))
    body = doc._element.body
    for element in list(body):
        if element.tag != qn("w:sectPr"):
            body.remove(element)

    added_style_ids = []
    if "Title" not in doc.styles:
        style = doc.styles.add_style("Title", WD_STYLE_TYPE.PARAGRAPH)
        style.base_style = doc.styles["Normal"]
        for properties in roles["title"]:
            if properties is not None:
                style.element.append(deepcopy(properties))
        added_style_ids.append(style.style_id)
    figures, table_audit = [], []
    pending_break, page_markers = False, 0
    body_width = (doc.sections[0].page_width - doc.sections[0].left_margin
                  - doc.sections[0].right_margin) / 914400.0

    def inline(p, content: str, role: str, strong: bool = False):
        for token in re.split(r"(\*\*.*?\*\*|`[^`]+`)", content):
            if not token:
                continue
            bold = token.startswith("**") and token.endswith("**")
            code = token.startswith("`") and token.endswith("`")
            run = p.add_run(token[2:-2] if bold else token[1:-1] if code else token)
            properties = deepcopy(roles[role][1])
            if properties is not None:
                run._r.insert(0, properties)
            black(run._r.get_or_add_rPr())
            if strong or bold:
                run.bold = True
            if code:
                fonts = run._r.get_or_add_rPr().get_or_add_rFonts()
                fonts.set(qn("w:ascii"), "Consolas")
                fonts.set(qn("w:hAnsi"), "Consolas")
                run.font.size = Pt(10.5)

    def apply_paragraph_role(p, role):
        if p._p.pPr is not None:
            p._p.remove(p._p.pPr)
        if roles[role][0] is not None:
            p._p.insert(0, deepcopy(roles[role][0]))
        ppr = p._p.get_or_add_pPr()
        mark_properties = ppr.find(qn("w:rPr"))
        if mark_properties is not None:
            black(mark_properties)
        pf = p.paragraph_format
        pf.widow_control = True
        pf.space_after = Pt(6)
        pf.keep_with_next = role in {"title", "subtitle", "author", "heading", "subheading"}
        if role == "title":
            p.style = doc.styles["Title"]
            borders = ppr.find(qn("w:pBdr"))
            if borders is not None:
                ppr.remove(borders)
        if role in {"title", "subtitle", "author", "caption"}:
            p.alignment = WD_ALIGN_PARAGRAPH.CENTER
        if role in {"heading", "subheading"}:
            pf.space_before = Pt(10)
            outline = OxmlElement("w:outlineLvl")
            outline.set(qn("w:val"), "0" if role == "heading" else "1")
            ppr.append(outline)

    def paragraph(content="", role="body"):
        nonlocal pending_break
        p = doc.add_paragraph()
        apply_paragraph_role(p, role)
        if pending_break:
            p.paragraph_format.page_break_before = True
            pending_break = False
        inline(p, content, role)
        return p

    def add_images(spec):
        requested_width, max_height = None, 3.6
        while spec.startswith(("width=", "height=")):
            setting, spec = spec.split(";", 1)
            key, value = setting.strip().split("=", 1)
            if key == "width":
                requested_width = float(value)
            else:
                max_height = float(value)
            spec = spec.strip()
        names = [item.strip() for item in spec.split("|")]
        p = paragraph()
        p.alignment = WD_ALIGN_PARAGRAPH.CENTER
        p.paragraph_format.keep_with_next = True
        p.paragraph_format.line_spacing = 1
        p.paragraph_format.space_after = Pt(3)
        width = (body_width - 0.12 * (len(names) - 1)) / len(names)
        if requested_width is not None:
            width = min(width, requested_width)
        for i, name in enumerate(names):
            path = TASK / "docs/screenshots" / name
            if not path.is_file():
                raise FileNotFoundError(path)
            with Image.open(path) as img:
                if img.format != "PNG":
                    raise ValueError(f"Expected PNG image: {path}")
                pixels = img.size
            image_width = min(width, max_height * pixels[0] / pixels[1])
            picture = p.add_run().add_picture(str(path), width=Inches(image_width))
            picture._inline.docPr.set("descr", name)
            figures.append({"file": str(path.relative_to(TASK)), "pixels": pixels,
                            "width_inches": image_width, "sha256": digest(path.read_bytes())})
            if i + 1 < len(names):
                p.add_run(" ")

    def add_table(rows):
        nonlocal pending_break
        headers, alignments = table_cells(rows[0]), table_cells(rows[1])
        values = [headers] + [table_cells(row) for row in rows[2:]]
        if any(len(row) != len(headers) for row in values):
            raise ValueError("Inconsistent Markdown table columns.")
        table = doc.add_table(rows=len(values), cols=len(headers))
        table.autofit = False
        weights = [max(5, min(18, max(len(row[c]) for row in values)))
                   for c in range(len(headers))]
        total = sum(weights)
        for c, weight in enumerate(weights):
            table.columns[c].width = Inches(body_width * weight / total)
        borders = OxmlElement("w:tblBorders")
        for edge in ("top", "left", "bottom", "right", "insideH", "insideV"):
            border = OxmlElement("w:" + edge)
            for key, value in {"val": "single", "sz": "4", "color": "D9D9D9"}.items():
                border.set(qn("w:" + key), value)
            borders.append(border)
        table._tbl.tblPr.append(borders)
        for r, row in enumerate(table.rows):
            properties = row._tr.get_or_add_trPr()
            properties.append(OxmlElement("w:cantSplit"))
            if r == 0:
                properties.append(OxmlElement("w:tblHeader"))
            for c, cell in enumerate(row.cells):
                cell.width = Inches(body_width * weights[c] / total)
                cell.vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.CENTER
                p = cell.paragraphs[0]
                apply_paragraph_role(p, "table")
                p.paragraph_format.space_after = Pt(3)
                marker = alignments[c]
                p.alignment = (WD_ALIGN_PARAGRAPH.CENTER if marker.startswith(":") and marker.endswith(":")
                               else WD_ALIGN_PARAGRAPH.RIGHT if marker.endswith(":") else WD_ALIGN_PARAGRAPH.LEFT)
                if pending_break and r == 0 and c == 0:
                    p.paragraph_format.page_break_before = True
                    pending_break = False
                inline(p, values[r][c], "table", r == 0)
                if r == 0:
                    shading = OxmlElement("w:shd")
                    shading.set(qn("w:fill"), "F2F2F2")
                    cell._tc.get_or_add_tcPr().append(shading)
        table_audit.append({"rows": len(values), "columns": len(headers), "headers": headers})

    lines, buffer, title_blocks = text.splitlines(), [], 0
    title_passed = False

    def flush():
        nonlocal title_blocks
        if not buffer:
            return
        content = "".join(buffer)
        buffer.clear()
        if title_passed and title_blocks < 2:
            paragraph(content, ("subtitle", "author")[title_blocks])
            title_blocks += 1
        else:
            role = "caption" if re.match(r"(?:图|表)\s*\d+(?:\.\d+)?", content) else "body"
            paragraph(content, role)

    i = 0
    while i < len(lines):
        line = lines[i]
        i += 1
        if not line.strip():
            flush()
        elif line.startswith("### "):
            flush(); paragraph(line[4:], "subheading")
        elif line.startswith("## "):
            flush(); paragraph(line[3:], "heading")
        elif line.startswith("# "):
            flush(); paragraph(line[2:], "title"); title_passed = True
        elif line == "<!-- page -->":
            flush(); pending_break = True; page_markers += 1
        elif re.fullmatch(r"<!-- images: (.+) -->", line):
            flush(); add_images(re.fullmatch(r"<!-- images: (.+) -->", line).group(1))
        elif line.startswith("|") and i < len(lines) and re.fullmatch(r"[\s|:\-]+", lines[i]):
            flush()
            rows = [line, lines[i]]
            i += 1
            while i < len(lines) and lines[i].startswith("|"):
                rows.append(lines[i]); i += 1
            add_table(rows)
        elif re.match(r"^(?:[-*] |\d+\. )", line):
            flush(); paragraph(line, "list")
        elif line.startswith("<!--"):
            flush()
        else:
            buffer.append(line)
    flush()

    def xml_values(element):
        return (element.tag, tuple(sorted(element.attrib.items())),
                tuple(xml_values(child) for child in element))

    if xml_values(section_before) != xml_values(doc.sections[0]._sectPr):
        raise AssertionError("The teacher template section geometry changed.")
    stream = BytesIO()
    doc.save(stream)
    stream.seek(0)
    preserve_checks = []
    with ZipFile(REFERENCE) as original, ZipFile(stream) as generated:
        parts = {name: original.read(name) for name in original.namelist()}
        parts["word/document.xml"] = generated.read("word/document.xml")
        styles = etree.fromstring(generated.read("word/styles.xml"))
        new_styles = [etree.tostring(node) for node in styles
                      if node.get(qn("w:styleId")) in added_style_ids]
        parts["word/styles.xml"] = append_xml(parts["word/styles.xml"], new_styles, b"</w:styles>")
        rel_name = "word/_rels/document.xml.rels"
        old_ids = {node.get("Id") for node in etree.fromstring(original.read(rel_name))}
        relationships = [etree.tostring(node) for node in etree.fromstring(generated.read(rel_name))
                         if node.get("Id") not in old_ids]
        parts[rel_name] = append_xml(parts[rel_name], relationships, b"</Relationships>")
        ct = "[Content_Types].xml"
        old_keys = {(n.tag, n.get("Extension"), n.get("PartName"))
                    for n in etree.fromstring(original.read(ct))}
        types = [etree.tostring(node) for node in etree.fromstring(generated.read(ct))
                 if (node.tag, node.get("Extension"), node.get("PartName")) not in old_keys]
        parts[ct] = append_xml(parts[ct], types, b"</Types>")
        for name in generated.namelist():
            if name.startswith("word/media/"):
                parts[name] = generated.read(name)
        editable = {"word/document.xml", "word/styles.xml", rel_name, ct}
        for name in original.namelist():
            if name in editable:
                continue
            same = parts[name] == original.read(name)
            preserve_checks.append({"part": name, "unchanged": same})
            if not same:
                raise AssertionError(f"Preserve-only part changed: {name}")
    FINAL.parent.mkdir(parents=True, exist_ok=True)
    with ZipFile(FINAL, "w", compression=ZIP_DEFLATED) as output:
        for name, data in parts.items():
            output.writestr(name, data)
    check = Document(FINAL)
    if len(check.sections) != 1 or digest(REFERENCE.read_bytes()) != reference_hash:
        raise AssertionError("Template fidelity checks failed.")
    all_text = "\n".join(p.text for p in check.paragraphs)
    required = (["一、", "二、", "三、", "四、"] if TASK.name == "task2"
                else ["一、", "二、", "三、", "四、", "五、", "六、", "七、"])
    if not all(label in all_text for label in required) or "[姓名待填写-学号待填写]" not in all_text:
        raise AssertionError("Report sections or identity placeholders are incomplete.")
    QA.mkdir(parents=True, exist_ok=True)
    (QA / "report-build-audit.json").write_text(json.dumps({
        "template_sha256": reference_hash, "output_sha256": digest(FINAL.read_bytes()),
        "reference_unchanged": True, "section_count": 1, "manual_page_markers": page_markers,
        "paragraph_count": len(check.paragraphs), "image_occurrences": len(figures),
        "images": figures, "tables": table_audit, "added_styles": added_style_ids,
        "typography": {"source": str(REFERENCE), "declared_font": "Kaiti TC",
                       "body_pt": 14, "title_pt": 24, "subtitle_pt": 18,
                       "body_color": "000000", "inline_code": "Consolas 10.5pt"},
        "preserve_only_parts": preserve_checks,
    }, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"Created {FINAL}; {len(figures)} PNG occurrences; {len(table_audit)} tables.")


if __name__ == "__main__":
    build()
