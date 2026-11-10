"""Create the lab 3 DOCX, printable HTML and PDF from the lab 1 cover template.

Windows: py -3.11 tools/make_report.py (python-docx, Pillow, pywin32, Word).
Linux: python3 tools/make_report.py (python-docx, Pillow, WeasyPrint).
"""
from argparse import ArgumentParser
from html import escape
from pathlib import Path
import shutil
import sys

from docx import Document
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Mm, Pt
from PIL import Image

from report_content import PAGES

ROOT = Path(__file__).resolve().parents[1]
REPORT = ROOT / 'report'
NAME = '314_Шитов_Лаб3'
IMAGE_WIDTHS = {'img': 130, 'imgwide': 160, 'imgtexture': 130}


def prepare_materials():
    REPORT.mkdir(exist_ok=True)
    (REPORT / 'screenshots').mkdir(exist_ok=True)
    (REPORT / 'checks').mkdir(exist_ok=True)
    shutil.copyfile(ROOT / 'assets/torus.bmp', ROOT / 'materials/torus.bmp')
    with Image.open(ROOT / 'assets/torus.bmp') as texture:
        texture.save(ROOT / 'materials/torus.png')
        texture.save(REPORT / 'screenshots/texture.png')
    for filename in ('default.png', 'side.png', 'ambient.png'):
        shutil.copyfile(ROOT / 'checks/screenshots' / filename, REPORT / 'screenshots' / filename)
    for source in (ROOT / 'checks').glob('*.log'):
        shutil.copyfile(source, REPORT / 'checks' / source.name)


def create_docx():
    doc = Document(ROOT / 'materials/report_template.docx')
    replacements = {
        'Лабораторная работа №X': 'Лабораторная работа №3',
        'Выполнил: И. О. Фамилия': 'Выполнил: Шитов Никита Владиславович',
        'Группа: М8О-3XXБ-2X': 'Группа: М80-314БВ-24',
        'Основы 3D графики': 'Текстурирование. Вариант 7 — тор',
    }
    for paragraph in doc.paragraphs:
        if paragraph.text in replacements:
            value = replacements[paragraph.text]
            if paragraph.runs:
                paragraph.runs[0].text = value
                for run in paragraph.runs[1:]:
                    run.text = ''
            else:
                paragraph.add_run(value)
    # Keep the original template's cover, replacing its example body.
    for paragraph in list(doc.paragraphs)[37:]:
        paragraph._element.getparent().remove(paragraph._element)
    for section in doc.sections:
        section.page_width = Mm(210)
        section.page_height = Mm(297)
        section.left_margin = Mm(22)
        section.right_margin = Mm(18)
        section.top_margin = section.bottom_margin = Mm(20)
        section.different_first_page_header_footer = True
        footer = section.footer.paragraphs[0]
        footer.alignment = WD_ALIGN_PARAGRAPH.CENTER
        field = OxmlElement('w:fldSimple')
        field.set(qn('w:instr'), 'PAGE')
        footer._p.append(field)
        footer.style.font.name = 'Times New Roman'
        footer.style.font.size = Pt(10)
    doc.core_properties.title = 'Лабораторная работа №3 — Текстурирование'
    doc.core_properties.author = 'Шитов Никита Владиславович'
    doc.core_properties.subject = 'Компьютерная графика. Вариант 7 — тор'
    for page in PAGES:
        doc.add_page_break()
        for kind, value in page:
            paragraph = doc.add_paragraph()
            layout = paragraph.paragraph_format
            layout.space_before = Pt(0)
            layout.space_after = Pt(5)
            layout.line_spacing = 1.05
            if kind.startswith('img'):
                paragraph.alignment = WD_ALIGN_PARAGRAPH.CENTER
                layout.space_after = Pt(2)
                layout.keep_with_next = True
                paragraph.add_run().add_picture(
                    str(REPORT / 'screenshots' / value), width=Mm(IMAGE_WIDTHS[kind]))
                continue
            run = paragraph.add_run(value)
            run.font.name = 'Times New Roman'
            run.font.size = Pt(12)
            if kind in ('h', 's'):
                run.bold = True
                run.font.size = Pt(16 if kind == 'h' else 13)
                layout.keep_with_next = True
            elif kind == 'f':
                run.font.name = 'Consolas'
                run.font.size = Pt(10)
                layout.keep_together = True
            elif kind == 'cap':
                run.font.size = Pt(10)
                layout.keep_together = True
            else:
                paragraph.alignment = WD_ALIGN_PARAGRAPH.JUSTIFY
                layout.first_line_indent = Mm(8)
    path = REPORT / f'{NAME}.docx'
    doc.save(path)
    return path


def create_html():
    css = '''
@page { size: A4; margin: 20mm 18mm 20mm 22mm;
  @bottom-center { content: counter(page); font: 10pt "Times New Roman", serif; } }
@page:first { @bottom-center { content: none; } }
body { font: 12pt "Times New Roman", "Liberation Serif", serif;
       line-height: 1.12; color: #111; }
.page { break-before: page; }
h1 { font-size: 16pt; margin: 0 0 4mm; }
h2 { font-size: 13pt; margin: 4mm 0 2mm; }
p { text-align: justify; text-indent: 8mm; margin: 0 0 3mm; }
pre { font: 10pt "Consolas", "DejaVu Sans Mono", monospace; line-height: 1.2;
      margin: 2mm 0 3mm; white-space: pre-wrap; }
figure { margin: 0 0 3mm; text-align: center; break-inside: avoid; }
figure img { width: 130mm; }
figure.wide img { width: 160mm; }
.caption { font-size: 10pt; text-indent: 0; text-align: left; margin: 0 0 4mm; }
.cover { height: 252mm; position: relative; text-align: center; font-size: 14pt; }
.cover .institution { line-height: 1.3; }
.cover .unit { margin-top: 9mm; font-size: 12pt; }
.cover .title { margin-top: 48mm; line-height: 1.5; }
.cover .topic { margin-top: 14mm; }
.cover .author { margin-top: 48mm; text-align: right; line-height: 1.5; font-size: 12pt; }
.cover .year { position: absolute; bottom: 5mm; width: 100%; }
'''
    cover = '''<div class="cover">
<div class="institution">МОСКОВСКИЙ АВИАЦИОННЫЙ ИНСТИТУТ<br>(НАЦИОНАЛЬНЫЙ ИССЛЕДОВАТЕЛЬСКИЙ УНИВЕРСИТЕТ)</div>
<div class="unit">Институт №8 «Компьютерные науки и прикладная математика»<br>Кафедра 806 «Вычислительная математика и программирование»</div>
<div class="title">Лабораторная работа №3<br>по курсу «Компьютерная графика»</div>
<div class="topic">Текстурирование<br>Вариант 7 — тор</div>
<div class="author">Выполнил: Шитов Никита Владиславович<br>Группа: М80-314БВ-24<br>Преподаватель: В. Д. Бахарев</div>
<div class="year">Москва, 2026</div></div>'''
    parts = [f'<!doctype html><html lang="ru"><meta charset="utf-8"><title>{NAME}</title>',
             f'<style>{css}</style><body>', cover]
    for page in PAGES:
        parts.append('<div class="page">')
        for kind, value in page:
            value = escape(value)
            if kind in ('h', 's'):
                tag = 'h1' if kind == 'h' else 'h2'
                parts.append(f'<{tag}>{value}</{tag}>')
            elif kind.startswith('img'):
                cls = 'wide' if kind == 'imgwide' else ''
                parts.append(f'<figure class="{cls}"><img src="screenshots/{value}" alt="{value}"></figure>')
            elif kind == 'f':
                parts.append(f'<pre>{value}</pre>')
            else:
                cls = 'caption' if kind == 'cap' else ''
                parts.append(f'<p class="{cls}">{value}</p>')
        parts.append('</div>')
    parts.append('</body></html>')
    path = REPORT / 'report.html'
    path.write_text('\n'.join(parts), encoding='utf-8')
    return path


def create_pdf(docx, html, engine):
    path = REPORT / f'{NAME}.pdf'
    if engine == 'word':
        from win32com.client import DispatchEx
        word = DispatchEx('Word.Application')
        word.Visible = False
        word.DisplayAlerts = 0
        document = None
        try:
            document = word.Documents.Open(str(docx), ReadOnly=True, AddToRecentFiles=False)
            document.Repaginate()
            document.ExportAsFixedFormat(str(path), 17)
        finally:
            if document is not None:
                document.Close(False)
            word.Quit()
    else:
        from weasyprint import HTML
        HTML(filename=str(html)).write_pdf(path)
    return path


def main():
    parser = ArgumentParser(description=__doc__)
    parser.add_argument('--pdf-engine', choices=('word', 'weasyprint'),
                        default='word' if sys.platform == 'win32' else 'weasyprint')
    args = parser.parse_args()
    prepare_materials()
    docx = create_docx()
    html = create_html()
    pdf = create_pdf(docx, html, args.pdf_engine)
    print(f'Created {docx.name}, {pdf.name} and {html.name}')


if __name__ == '__main__':
    main()
