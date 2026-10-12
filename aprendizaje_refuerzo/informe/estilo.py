"""
Estilos y helpers compartidos por los PDF (informe y guía de estudio).
Copiados de informes/generar_pdf.py (entrega de PSO) para mantener el formato.
"""
import os
import hashlib
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (
    SimpleDocTemplate, Paragraph, Spacer, PageBreak, Table, TableStyle,
    Image, HRFlowable, ListFlowable, ListItem, KeepTogether
)
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER, TA_JUSTIFY, TA_LEFT

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
matplotlib.rcParams["mathtext.fontset"] = "cm"

FONT = "ArialUnicode"
FONTB = "ArialUnicodeB"
pdfmetrics.registerFont(TTFont(FONT, "/System/Library/Fonts/Supplemental/Arial Unicode.ttf"))
# Arial Unicode.ttf no tiene variante negrita -- usar Arial Bold.ttf real para
# que <b> en los Paragraph produzca negrita de verdad (antes FONTB apuntaba al
# mismo archivo que FONT, asi que **negrita** en todo el documento no hacia
# nada visualmente). Arial Bold no cubre subindices Unicode (ej. el "0" chico
# de omega0) -- evitar poner esos caracteres puntuales dentro de **negrita**.
pdfmetrics.registerFont(TTFont(FONTB, "/System/Library/Fonts/Supplemental/Arial Bold.ttf"))
pdfmetrics.registerFontFamily(FONT, normal=FONT, bold=FONTB, italic=FONT, boldItalic=FONTB)

HERE = os.path.dirname(os.path.abspath(__file__))
EQDIR = os.path.join(HERE, "eq")
os.makedirs(EQDIR, exist_ok=True)


def _render_tex(tex, fontsize=20):
    """Renderiza una formula LaTeX (subconjunto mathtext de matplotlib, no
    necesita instalacion de LaTeX) a PNG con fondo transparente. Cachea por
    hash del contenido para no re-renderizar en cada build."""
    key = hashlib.sha1(f"{tex}|{fontsize}".encode()).hexdigest()[:16]
    path = os.path.join(EQDIR, f"{key}.png")
    if not os.path.exists(path):
        fig = plt.figure(figsize=(0.01, 0.01))
        fig.text(0, 0, tex, fontsize=fontsize, color="#181c24")
        fig.savefig(path, dpi=400, transparent=True, bbox_inches="tight", pad_inches=0.06)
        plt.close(fig)
    return path

INK = colors.HexColor("#181c24")
DIM = colors.HexColor("#4b5563")
ACCENT = colors.HexColor("#185fa5")
ACCENT2 = colors.HexColor("#993c1d")
RULE = colors.HexColor("#c7ccd6")
NOTEBG = colors.HexColor("#f4f5f8")
# Claro, no oscuro -- este PDF esta pensado para imprimir en blanco y negro,
# un fondo oscuro sale como bloque solido de tinta/tono gris pesado.
CODEBG = colors.HexColor("#eef0f3")
CODEFG = colors.HexColor("#181c24")
CODEBORDER = colors.HexColor("#c7ccd6")

# Tamanos ajustados para que el documento entre en ~7 carillas sin perder
# legibilidad de impresion (no bajar el cuerpo de 8.6pt).
st_title = ParagraphStyle("TitleX", fontName=FONTB, fontSize=17, leading=21,
                           textColor=INK, spaceAfter=3, alignment=TA_LEFT)
st_subtitle = ParagraphStyle("SubtitleX", fontName=FONT, fontSize=11, leading=14,
                              textColor=DIM, spaceAfter=3)
st_meta = ParagraphStyle("Meta", fontName=FONT, fontSize=8.6, leading=12,
                          textColor=DIM, spaceAfter=1)
st_h1 = ParagraphStyle("H1", fontName=FONTB, fontSize=13, leading=16, textColor=ACCENT,
                        spaceBefore=10, spaceAfter=5, keepWithNext=True)
st_h2 = ParagraphStyle("H2", fontName=FONTB, fontSize=10.5, leading=13, textColor=INK,
                        spaceBefore=7, spaceAfter=3, keepWithNext=True)
st_h3 = ParagraphStyle("H3", fontName=FONTB, fontSize=9.6, leading=12, textColor=ACCENT2,
                        spaceBefore=6, spaceAfter=2, keepWithNext=True)
st_body = ParagraphStyle("Body", fontName=FONT, fontSize=8.6, leading=11.6,
                          textColor=INK, alignment=TA_JUSTIFY, spaceAfter=4)
st_caption = ParagraphStyle("Caption", fontName=FONT, fontSize=7.4, leading=9.6,
                             textColor=DIM, alignment=TA_CENTER, spaceAfter=6)
st_li = ParagraphStyle("LI", fontName=FONT, fontSize=8.6, leading=11.6,
                        textColor=INK, alignment=TA_JUSTIFY, spaceAfter=2)
st_code = ParagraphStyle("Code", fontName="Courier", fontSize=7.6, leading=10.4,
                          textColor=CODEFG, alignment=TA_LEFT)


def md_inline(s):
    """Conversion minima de markdown inline a XML de reportlab."""
    import re
    s = s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
    s = re.sub(r"\*\*(.+?)\*\*", r"<b>\1</b>", s)
    s = re.sub(r"(?<!\*)\*(?!\*)(.+?)(?<!\*)\*(?!\*)", r"<i>\1</i>", s)
    s = re.sub(r"`(.+?)`", r'<font face="Courier" size="8.6" color="#993c1d">\1</font>', s)
    s = re.sub(r"\[(.+?)\]\((.+?)\)", r'<link href="\2" color="#185fa5">\1</link>', s)
    return s


def codeblock(text):
    lines = text.rstrip("\n").split("\n")
    data = [[Paragraph((l.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
                         .replace(" ", "&nbsp;")) or "&nbsp;", st_code)]
             for l in lines]
    t = Table(data, colWidths=[178 * mm])
    t.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, -1), CODEBG),
        ("BOX", (0, 0), (-1, -1), 0.5, CODEBORDER),
        ("LEFTPADDING", (0, 0), (-1, -1), 8),
        ("RIGHTPADDING", (0, 0), (-1, -1), 8),
        ("TOPPADDING", (0, 0), (-1, -1), 4),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 4),
    ]))
    return t


def mathimg(*tex_lines, max_width_mm=148, line_height_mm=7.8):
    """Una o mas formulas LaTeX (cada una '$...$', sintaxis mathtext) en su
    propia caja clara, una por fila -- reemplazo de eqn() para lo que es
    matematica real (no codigo Python), evita depender de que la tipografia
    del sistema tenga el glifo exacto (theta con puntos, subindices
    genericos, sumatorias, fracciones)."""
    from PIL import Image as PILImage
    rows = []
    for tex in tex_lines:
        path = _render_tex(tex)
        w_px, h_px = PILImage.open(path).size
        w_mm = (w_px / h_px) * line_height_mm
        if w_mm > max_width_mm:
            w_mm = max_width_mm
            h_mm = w_mm * (h_px / w_px)
        else:
            h_mm = line_height_mm
        rows.append([Image(path, width=w_mm * mm, height=h_mm * mm)])
    t = Table(rows, colWidths=[178 * mm])
    t.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, -1), CODEBG),
        ("BOX", (0, 0), (-1, -1), 0.5, CODEBORDER),
        ("ALIGN", (0, 0), (-1, -1), "CENTER"),
        ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
        ("LEFTPADDING", (0, 0), (-1, -1), 8),
        ("RIGHTPADDING", (0, 0), (-1, -1), 8),
        ("TOPPADDING", (0, 0), (-1, -1), 5),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 5),
    ]))
    return t


def notebox(text):
    p = Paragraph(md_inline(text), st_body)
    t = Table([[p]], colWidths=[178 * mm])
    t.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, -1), NOTEBG),
        ("BOX", (0, 0), (-1, -1), 0.5, RULE),
        ("LEFTPADDING", (0, 0), (-1, -1), 8),
        ("RIGHTPADDING", (0, 0), (-1, -1), 8),
        ("TOPPADDING", (0, 0), (-1, -1), 5),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 5),
    ]))
    return t


def table_md(headers, rows):
    data = [[Paragraph(f"<b>{md_inline(h)}</b>", st_li) for h in headers]] + \
           [[Paragraph(md_inline(c), st_li) for c in row] for row in rows]
    t = Table(data, repeatRows=1)
    t.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#e6f1fb")),
        ("GRID", (0, 0), (-1, -1), 0.4, RULE),
        ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
        ("LEFTPADDING", (0, 0), (-1, -1), 5),
        ("RIGHTPADDING", (0, 0), (-1, -1), 5),
        ("TOPPADDING", (0, 0), (-1, -1), 3),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 3),
    ]))
    return t





def p(story, texto, estilo=None):
    story.append(Paragraph(md_inline(texto), estilo or st_body))


def vinetas(story, items):
    story.append(ListFlowable(
        [ListItem(Paragraph(md_inline(t), st_li), bulletColor=ACCENT) for t in items],
        bulletType="bullet", start="•", leftIndent=12, bulletFontSize=7))


def figura(story, ruta, ancho_mm, epigrafe):
    from PIL import Image as PILImage
    w, h = PILImage.open(ruta).size
    story.append(KeepTogether([Image(ruta, width=ancho_mm * mm, height=ancho_mm * mm * h / w),
                               Paragraph(epigrafe, st_caption)]))


def construir(story, nombre_pdf, titulo):
    salida = os.path.join(HERE, nombre_pdf)
    doc = SimpleDocTemplate(salida, pagesize=A4, leftMargin=16 * mm, rightMargin=16 * mm,
                            topMargin=13 * mm, bottomMargin=13 * mm, title=titulo, author="Cesar Yepez")
    doc.build(story)
    print("PDF generado:", salida)
