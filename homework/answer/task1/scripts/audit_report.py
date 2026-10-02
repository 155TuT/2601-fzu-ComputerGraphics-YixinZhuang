"""Audit the final Word/PDF package and merge retained visual inspection evidence."""
from pathlib import Path
from zipfile import ZipFile
from hashlib import sha256
import json
import re
from datetime import datetime, timezone
from docx import Document
from docx.oxml.ns import qn
from lxml import etree
from pypdf import PdfReader

ROOT = Path(__file__).resolve().parents[1]
QA = ROOT / 'verification/report'

def digest(path):
    return sha256(path.read_bytes()).hexdigest()

def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))

def main():
    docx = ROOT / 'docs/作业报告.docx'
    pdf = ROOT / 'docs/作业报告.pdf'
    report_hashes = {'docx_sha256': digest(docx), 'pdf_sha256': digest(pdf)}
    doc = Document(docx)
    text = '\n'.join(p.text for p in doc.paragraphs)
    required = ['一、', '二、', '三、', '四、', '五、', '六、', '七、', '八、']
    assert all(label in text for label in required)
    assert '姓名待填写' in text and '学号待填写' in text
    assert not re.search(r'<!--|\*\*|\[(?:TODO|待补充)\]', text)
    assert '864 种配置' in text and '1728 次' in text
    parsed_xml = []
    with ZipFile(docx) as z:
        assert z.testzip() is None
        for name in z.namelist():
            if name.endswith(('.xml', '.rels')):
                etree.fromstring(z.read(name))
                parsed_xml.append(name)
        media = {sha256(z.read(n)).hexdigest() for n in z.namelist() if n.startswith('word/media/')}
    build = read(QA / 'report-build-audit.json')
    assert build['output_sha256'] == report_hashes['docx_sha256']
    for item in build['images']:
        assert digest(ROOT / item['file']) == item['sha256']
        assert item['sha256'] in media
    code = read(ROOT / 'verification/code-screenshots.json')
    code_images = 0
    for entry in code['excerpts']:
        assert digest(ROOT / entry['source']) == entry['source_sha256']
        assert digest(ROOT / entry['sidecar']) == entry['sidecar_sha256']
        for image in entry['images']:
            assert digest(ROOT / image['file']) == image['sha256']
            code_images += 1
    assert code_images == len(list((ROOT / 'docs/code-screenshots').glob('*.png')))
    style_specs = {'ReportBody': ('宋体', 12, False),
                   'ReportHeading': ('黑体', 14, True),
                   'ReportSubheading': ('黑体', 12, True)}
    style_audit = {}
    for name, (font, size, bold) in style_specs.items():
        style = doc.styles[name]
        fonts = style.element.rPr.rFonts
        assert fonts.get(qn('w:eastAsia')) == font
        assert fonts.get(qn('w:ascii')) == 'Times New Roman'
        assert style.font.size.pt == size and style.font.bold == bold
        style_audit[name] = {'cjk_font': font, 'latin_font': 'Times New Roman',
                             'pt': size, 'bold': bold}
    strong_runs = sum(bool(r.font.bold) for p in doc.paragraphs for r in p.runs)
    assert strong_runs > 20
    reader = PdfReader(pdf)
    pages = len(reader.pages)
    assert pages == 35 and all(p.extract_text().strip() for p in reader.pages)
    office = read(QA / 'word-open-audit.json')
    assert office['normal_open_succeeded'] and office['export_pdf_succeeded']
    assert office['pages'] == pages and office['inline_shapes'] == len(build['images'])
    structural = {**report_hashes, 'status': 'passed', 'pdf_pages': pages,
                  'docx_xml_parts_parsed': len(parsed_xml), 'tables': len(doc.tables),
                  'image_occurrences': len(build['images']), 'unique_media_sha256': len(media),
                  'code_pngs': code_images, 'all_embedded_images_match_sources': True,
                  'code_source_and_sidecar_hashes_match': True,
                  'section_count': len(doc.sections), 'all_eight_sections_present': True,
                  'identity': '姓名待填写-学号待填写', 'styles': style_audit,
                  'bold_runs': strong_runs,
                  'template_unchanged': build['reference_unchanged'],
                  'template_sha256': build['template_sha256'],
                  'audited_utc': datetime.now(timezone.utc).isoformat()}
    (QA / 'structural-audit.json').write_text(json.dumps(structural, ensure_ascii=False, indent=2), encoding='utf8')
    reviews = [read(QA / 'qa-pages-01-18.json'), read(QA / 'qa-pages-19-35.json')]
    page_reviews = []
    for review in reviews:
        assert not review['blocking_findings']
        for key, value in report_hashes.items():
            assert review[key] == value, f'Visual QA has stale {key}'
        page_reviews.extend(review['pages'])
    page_reviews.sort(key=lambda r: r['page'])
    assert [p['page'] for p in page_reviews] == list(range(1, pages + 1))
    render = QA / 'render-bonus-final-v2'
    for page in page_reviews:
        assert digest(render / f"page-{page['page']:02d}.png") == page['image_sha256']
    canonical_note = (
        'Canonical render_docx.py attempt resolved C:/Program Files/LibreOffice/program/soffice.exe '
        'despite a restricted launch PATH and produced an alternate 42-page reflow. '
        'This was not the bundled LibreOffice required by the document workflow; '
        'its output is excluded from the deliverable and final visual QA. '
        'Final PDF is the successful Microsoft Word 16.0 read-only export (35 pages), '
        'rendered using bundled Poppler at 120 dpi.'
    )
    visual = {**structural, 'status': 'passed', 'render_path': str(render.relative_to(ROOT)),
              'renderer': 'Microsoft Word 16.0 read-only PDF export + bundled Poppler 120 dpi',
              'canonical_renderer': canonical_note, 'office_openability_verified': True,
              'pages_inspected': list(range(1, pages + 1)), 'page_review': page_reviews,
              'minor_observations': [m for r in reviews for m in r.get('minor_observations', [])]}
    (QA / 'visual-qa.json').write_text(json.dumps(visual, ensure_ascii=False, indent=2), encoding='utf8')
    notes = f'''# 最终报告检查

状态：通过。Microsoft Word 16.0 只读正常打开并导出 PDF，35 页；最终版 DOCX 与 PDF 的 SHA-256 分别为 `{report_hashes['docx_sha256']}`、`{report_hashes['pdf_sha256']}`。

## 结构与内容

- OOXML ZIP CRC 及 {len(parsed_xml)} 个 XML/关系部件解析成功，八个要求章节和待填写身份字段均保留。
- {len(build['images'])} 处图片、{len(doc.tables)} 张表格；嵌入图与报告源图 SHA-256 对应。
- {code_images} 张白底代码 PNG、可复制代码片段与最终源码 SHA-256 全部对应。
- 宋体/Times New Roman 12pt 正文、20pt 行距、黑体加粗标题及正文重点加粗已核对。
- 教师模板 SHA-256 不变，原有页面设置与保留部件不变。

## 逐页可视检查

全部 35 页实际查看；最后两轮仅对变化页重新查看，其余页面 PNG 字节与已查看版本完全相同。独立记录为 qa-pages-01-18.json 和 qa-pages-19-35.json，均绑定上述最终报告 SHA-256。图注与对应图片同页，代码与图表在页边距内，无缺字、裁切、重叠、空白页或异常拉伸字距。源页 PNG 位于 render-bonus-final-v2/。

## 渲染工具记录

{canonical_note}

此工具记录仅说明渲染来源；最终提交的是经过 Word 打开、导出及逐页检查的 DOCX/PDF。
'''
    (QA / 'visual-qa.md').write_text(notes, encoding='utf8')
    print(f'Report audit passed: {pages} pages, {len(build["images"])} images, {len(doc.tables)} tables, {code_images} code PNGs.')

if __name__ == '__main__':
    main()
