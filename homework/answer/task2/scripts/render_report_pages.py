"""Rasterize the Word-exported PDF with the document skill renderer."""
from pathlib import Path
import argparse
import importlib.util
import os
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--renderer", required=True, type=Path)
parser.add_argument("--poppler", required=True, type=Path)
args = parser.parse_args()
task = Path(__file__).resolve().parents[1]
temp = task / "verification/tmp"
temp.mkdir(parents=True, exist_ok=True)
tempfile.tempdir = str(temp)
os.environ["PATH"] = str(args.poppler) + os.pathsep + os.environ.get("PATH", "")
spec = importlib.util.spec_from_file_location("document_renderer", args.renderer)
renderer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(renderer)
pdf = task / "docs/作业报告.pdf"
if not pdf.is_file():
    raise FileNotFoundError("Export the report with Word before rendering pages")
# Word is the authoritative layout engine for this retained template.
renderer.convert_to_pdf = lambda *a, **kw: (str(pdf), {"source": "Word export"})
renderer.rasterize(str(task / "docs/作业报告.docx"),
                  str(task / "verification/report/final-pages"),
                  120, False, False)
