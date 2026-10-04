"""Create a DOCX from the supplied cover template and a printable PDF."""
from pathlib import Path
from html import escape
from docx import Document
from docx.shared import Pt, Mm
from docx.enum.text import WD_ALIGN_PARAGRAPH
from weasyprint import HTML

ROOT = Path(__file__).resolve().parents[1]
REPORT = ROOT / 'report'
NAME = '314_Шитов_Лаб1'
# Content is shared by the editable report and PDF.
PAGES = [
    [
        ('h', 'Условие'),
        ('p', 'Цель работы — освоить построение простого трёхмерного объекта, проецирование на двумерную плоскость и работу с матрицами. Разработать интерактивное приложение на C++ с использованием Vulkan, GLFW и Dear ImGui.'),
        ('p', 'Номер в списке группы P = 19, количество вариантов N = 12. Номер варианта: ((P − 1) mod N) + 1 = ((19 − 1) mod 12) + 1 = 7. Фигура варианта 7 — тор. Выполнено основное задание: построение и отрисовка одного тора с возможностью осмотра при помощи камеры.'),
        ('h', 'Метод решения'),
        ('s', 'Построение поверхности'),
        ('p', 'Тор задаётся большим радиусом R = 1,4 и радиусом трубки r = 0,5. Ось симметрии совпадает с Y. Параметры u и v изменяются от 0 до 2π:'),
        ('f', 'x = (R + r cos v) cos u\ny = r sin v\nz = (R + r cos v) sin u'),
        ('p', 'Использована сетка 64 × 32: 2048 вершин, 4096 треугольников. Каждая ячейка разбивается на два треугольника, соседние индексы вычисляются по модулю числа сегментов. Оба шва замкнуты. Нормаль поверхности n = (cos v cos u, sin v, cos v sin u). У тора фиксированный синий цвет; простое диффузное освещение показывает объём поверхности.'),
        ('s', 'Камера, проекция и глубина'),
        ('f', 'pclip = P · V · (x, y, z, 1)ᵀ\nP = [ c/a, 0, 0, 0;  0, −c, 0, 0;\n        0, 0, f/(n−f), fn/(n−f);  0, 0, −1, 0 ]\nc = 1 / tan(FOV/2)'),
        ('p', 'Матрица V строится функцией lookAt для орбитальной камеры. ЛКМ изменяет направление взгляда, колесо — расстояние до центра тора. P задаёт перспективную проекцию: FOV = 45°, n = 0,1, f = 100; a — отношение сторон области сцены. Матрицы хранятся по столбцам, в формуле записаны по строкам. Отрицательный знак Y учитывает экранную систему Vulkan; глубина после деления на w принадлежит [0; 1]. Буфер глубины с проверкой LESS скрывает закрытые части поверхности.'),
        ('s', 'Отрисовка Vulkan'),
        ('p', 'Вершины и индексы хранятся в VkBuffer, память выделяется через VMA. Матрица P·V передаётся в единственном uniform-буфере размером 64 байта и одном VkDescriptorSet. GLSL-шейдеры компилируются glslc в SPIR-V. VkPipeline задаёт формат вершин, растеризацию и проверку глубины; vkCmdDrawIndexed рисует тор. Затем ImGui выводит сведения о фигуре и подсказки управления.'),
        ('p', 'Uniform-буфер обновляется после ожидания fence предыдущего кадра. Сборка, два ракурса камеры и изменение размера окна проверены на Linux с Mesa llvmpipe. Тест геометрии и матриц прошёл; Vulkan Validation Layers и проверка синхронизации не зарегистрировали ошибок. Логи находятся в report/checks/.'),
    ],
    [
        ('h', 'Результаты'),
        ('img', 'default.png'),
        ('cap', 'Рисунок 1 — тор в перспективной проекции. Слева отображаются сведения о фигуре и управление камерой.'),
        ('img', 'side.png'),
        ('cap', 'Рисунок 2 — тот же тор с другого ракурса камеры. Геометрия, проекция и цвет фигуры сохраняются.'),
        ('h', 'Выводы'),
        ('p', 'В работе освоены параметрическое построение тора, индексная отрисовка, матрица камеры и перспективная проекция Vulkan. Реализован осмотр одной трёхмерной фигуры с помощью мыши; буфер глубины обеспечивает правильное перекрытие поверхности.'),
    ],
]

# Keep the supplied template's cover and replace student-specific fields.
doc = Document(ROOT/'materials/report_template.docx')
replacements = {
    'Лабораторная работа №X': 'Лабораторная работа №1',
    'Выполнил: И. О. Фамилия': 'Выполнил: Шитов Никита Владиславович',
    'Группа: М8О-3XXБ-2X': 'Группа: М80-314БВ-24',
    'Основы 3D графики': 'Основы 3D-графики. Вариант 7 — тор',
}
for p in doc.paragraphs:
    if p.text in replacements:
        text = replacements[p.text]
        if p.runs:
            p.runs[0].text=text
            for run in p.runs[1:]: run.text=''
        else: p.add_run(text)
for p in list(doc.paragraphs)[37:]: p._element.getparent().remove(p._element)
# Use standard A4 dimensions; retain the cover's original paragraph formatting.
for s in doc.sections:
    s.page_width=Mm(210); s.page_height=Mm(297)
    s.left_margin=Mm(22); s.right_margin=Mm(18)
    s.top_margin=Mm(20); s.bottom_margin=Mm(20)
for page in PAGES:
    doc.add_page_break()
    for kind,text in page:
        if kind.startswith('img'):
            p=doc.add_paragraph(); p.alignment=WD_ALIGN_PARAGRAPH.CENTER
            p.add_run().add_picture(str(REPORT/'screenshots'/text),width=Mm(160 if kind=='imgwide' else 130))
            p.paragraph_format.space_after=Pt(2)
            continue
        p=doc.add_paragraph(); run=p.add_run(text)
        run.font.name='Times New Roman'; run.font.size=Pt(12)
        p.paragraph_format.space_after=Pt(5)
        p.paragraph_format.line_spacing=1.05
        if kind in ('h','s'):
            run.bold=True; run.font.size=Pt(16 if kind=='h' else 13)
            p.paragraph_format.keep_with_next=True
        elif kind=='f':
            run.font.name='DejaVu Sans Mono'; run.font.size=Pt(10)
        elif kind in ('cap','sources'):
            run.font.size=Pt(10)
        else:
            p.alignment=WD_ALIGN_PARAGRAPH.JUSTIFY
            p.paragraph_format.first_line_indent=Mm(8)
doc.save(REPORT/f'{NAME}.docx')

css = '''
@page { size: A4; margin: 20mm 18mm 20mm 22mm;
  @bottom-center { content: counter(page); font: 10pt "Liberation Serif"; } }
@page:first { @bottom-center { content: none; } }
body { font: 12pt "Liberation Serif", "DejaVu Serif", serif; line-height: 1.12; color: #111; }
.page { break-before: page; }
h1 { font-size: 16pt; margin: 0 0 4mm; }
h2 { font-size: 13pt; margin: 4mm 0 2mm; }
p { text-align: justify; text-indent: 8mm; margin: 0 0 3mm; }
pre { font: 10pt "DejaVu Sans Mono"; line-height: 1.2; margin: 2mm 0 3mm; white-space: pre-wrap; }
figure { margin: 0 0 3mm; text-align: center; }
figure img { width: 130mm; }
figure.wide img { width: 160mm; }
.caption { font-size: 10pt; text-indent: 0; text-align: left; margin: 0 0 4mm; }
.sources { font-size: 9pt; text-indent: 0; text-align: left; }
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
<div class="title">Лабораторная работа №1<br>по курсу «Компьютерная графика»</div>
<div class="topic">Основы 3D-графики<br>Вариант 7 — тор</div>
<div class="author">Выполнил: Шитов Никита Владиславович<br>Группа: М80-314БВ-24<br>Преподаватель: В. Д. Бахарев</div>
<div class="year">Москва, 2026</div></div>'''
parts = [f'<!doctype html><html lang="ru"><meta charset="utf-8"><title>{NAME}</title><style>{css}</style><body>', cover]
for page in PAGES:
    parts.append('<div class="page">')
    for kind,text in page:
        text=escape(text)
        if kind in ('h','s'): parts.append(f'<{"h1" if kind=="h" else "h2"}>{text}</{"h1" if kind=="h" else "h2"}>')
        elif kind.startswith('img'): parts.append(f'<figure class="{"wide" if kind=="imgwide" else ""}"><img src="screenshots/{text}"></figure>')
        elif kind=='f': parts.append(f'<pre>{text}</pre>')
        else: parts.append(f'<p class="{"caption" if kind=="cap" else "sources" if kind=="sources" else ""}">{text}</p>')
    parts.append('</div>')
parts.append('</body></html>')
html='\n'.join(parts)
(REPORT/'report.html').write_text(html)
HTML(string=html,base_url=str(REPORT)).write_pdf(REPORT/f'{NAME}.pdf')
print(f'Created {REPORT/NAME}.docx and .pdf')
