"""
Genera el PDF imprimible del documento de entrega (Desafio Practico, PSO)
a partir de borrador_entrega.md. Estilo compartido con
algoritmos_evolutivos_pso/pdf_teoria/generar_teoria.py.
"""
import os
import hashlib
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (
    SimpleDocTemplate, Paragraph, Spacer, PageBreak, Table, TableStyle,
    Image, HRFlowable, ListFlowable, ListItem
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


story = []

# ============================================================ PORTADA
story.append(Paragraph("Optimización de un controlador PID mediante PSO", st_title))
story.append(Paragraph("Aplicación: péndulo invertido de un robot balanceador real", st_subtitle))
story.append(Spacer(1, 2 * mm))
story.append(Paragraph("<b>Materia:</b> Algoritmos Evolutivos I (2026) — Desafío Práctico", st_meta))
story.append(Paragraph("<b>Técnica:</b> Optimización por Enjambre de Partículas (PSO)", st_meta))
story.append(Paragraph(
    '<b>Repositorio:</b> <link href="https://github.com/piertotumlocomotor/robot_balance" '
    'color="#185fa5">github.com/piertotumlocomotor/robot_balance</link> (rama develop)', st_meta))
story.append(Paragraph("<b>Autor:</b> Cesar Yepez", st_meta))
story.append(Spacer(1, 2 * mm))
story.append(HRFlowable(width="100%", thickness=0.8, color=RULE))
story.append(Spacer(1, 2 * mm))

foto_path = os.path.join(HERE, "robot_foto.png")
if os.path.exists(foto_path):
    story.append(Image(foto_path, width=42 * mm, height=42 * mm * (1280/720)))
    story.append(Paragraph(
        "El robot real sobre el que se midieron todos los parámetros físicos de este trabajo: "
        "tres niveles de madera de aeromodelismo, dos motorreductores con encoder (M1, M2) en "
        "espejo sobre el eje de las ruedas.", st_caption))

# ============================================================ 1. EL PROBLEMA
story.append(Paragraph("1. El problema", st_h1))
story.append(Paragraph(md_inline(
    "Un robot balanceador de dos ruedas es, cerca de su posición vertical, un **péndulo "
    "invertido**: inestable a lazo abierto, requiere un controlador que lo corrija todo el "
    "tiempo. El controlador es un PID clásico — a partir del ángulo medido, calcula qué señal "
    "enviarle a los motores para volver a la vertical. Encontrar buenas ganancias (Kp, Ki, Kd) "
    "a mano es lento y poco sistemático: es optimización en un espacio continuo de 3 "
    "dimensiones sobre una función de costo sin forma cerrada (depende de simular la dinámica "
    "en el tiempo). **PSO** encaja bien: no necesita gradientes, tolera una función de costo "
    "no diferenciable (la nuestra tiene una penalización discontinua), y es simple de "
    "implementar y entender término a término."), st_body))
story.append(Paragraph(md_inline(
    "Este trabajo no usa datos de ejemplo: **todos los parámetros físicos salen de "
    "mediciones directas** sobre un robot real (ESP32, MPU6050, motorreductores con encoder, "
    "driver L298N en modo *brake*). Buena parte del valor está en cómo se midieron esos "
    "parámetros y en los errores descartados en el camino (sección 4)."), st_body))
story.append(Paragraph(
    "<i>Modo brake vs. coast: el L298N (puente H) puede dejar las salidas del motor en alta "
    "impedancia durante el tramo \"apagado\" del PWM (<b>coast</b>, decelera libre) o "
    "cortocircuitadas a masa (<b>brake</b>, frenado activo). Al mismo duty, brake entrega "
    "~2.6× más torque — por eso el modelo usa parámetros medidos en brake.</i>", st_caption))

# ============================================================ 2. MARCO TEORICO
story.append(Paragraph("2. Marco teórico", st_h1))

story.append(Paragraph("2.1 El robot como péndulo invertido — diagrama de cuerpo libre", st_h2))
story.append(Paragraph(md_inline(
    "El robot se modela como un péndulo invertido sobre un eje de ruedas: masa distribuida a "
    "lo largo del cuerpo (motores cerca del pivote, batería arriba), un grado de libertad "
    "relevante — el ángulo θ."), st_body))

dcl_path = os.path.join(HERE, "dcl_pendulo.png")
if os.path.exists(dcl_path):
    story.append(Image(dcl_path, width=68 * mm, height=68 * mm * (1632/1530)))
    story.append(Paragraph("Diagrama de cuerpo libre del péndulo invertido.", st_caption))

story.append(Paragraph(md_inline(
    "El peso `m·g` (en el centro de masa, CG) se descompone, tomando la varilla pivote-CG "
    "como referencia, en **radial** (`m·g·cosθ`, sin brazo de palanca respecto al pivote, no "
    "genera torque) y **tangencial** (`m·g·sinθ`, sí tiene brazo de palanca — produce el "
    "torque desestabilizante `m·g·l·sinθ`). La corrección la aporta `τ_motor`, transmitido al "
    "piso como una fuerza de **rozamiento estático** `f` (estático porque la rueda no patina "
    "— si patinara, no podría transmitir corrección). `N` es la reacción normal."), st_body))
story.append(Paragraph(md_inline(
    "Separando ejes en el contacto rueda-piso: verticalmente hay equilibrio (`ΣFy=0`, "
    "`N≈m·g`), pero horizontalmente no (`ΣFx=f≠0`) — esa fuerza neta acelera al robot sobre "
    "el piso. Es la pista de que el eje de las ruedas **no es, en rigor, un pivote fijo**: se "
    "traslada. Este trabajo no modela esa traslación (licencia 2, abajo) — solo importa el "
    "ángulo — y por eso se puede medir ω₀ colgando el robot de un pivote físicamente fijo "
    "(sección 4.7) y usar ese valor para el robot rodando: es la misma rotación alrededor del "
    "mismo eje."), st_body))

story.append(Paragraph("2.2 Ecuación dinámica y las licencias que se toman al modelarlo", st_h2))
story.append(Paragraph(md_inline(
    "La ecuación no lineal completa incluye sinθ, cosθ y acoplamientos con las ruedas. Se "
    "linealiza en torno a θ=0 (sinθ≈θ) y se colapsa la dinámica de traslación/acoplamiento en "
    "dos constantes medibles sobre el robot real."), st_body))
story.append(Paragraph(md_inline(
    "**De dónde sale.** El análogo rotacional de F=m·a es Στ = J·θ″. Sobre el robot actúan el "
    "torque de la gravedad (`m·g·l·sinθ`, desestabilizante, sección 2.1) y el de reacción del "
    "motor (`τ_motor(u)`, corrector):"), st_body))
story.append(mathimg(r"$J\ddot{\theta} = mgl\sin\theta + \tau_{motor}(u)$"))
story.append(Spacer(1, 1.5*mm))
story.append(Paragraph(md_inline(
    "Linealizando y dividiendo por J: θ″ = (m·g·l/J)·θ + τ_motor(u)/J. El primer término "
    "define ω₀² := m·g·l/J; el segundo agrupa todo lo que depende del actuador en una "
    "constante medida, no derivada: K_U·u_efectivo := τ_motor(u)/J."), st_body))
story.append(Paragraph(md_inline(
    "**Por qué \"ω**₀**\" y no \"una constante k\".** Resolviendo la ecuación libre "
    "(θ″=ω₀²θ) con θ=e^(rt): r²=ω₀², r=±ω₀ — raíces **reales** (a diferencia del péndulo "
    "colgante clásico, θ″=-ω₀²θ, con raíces imaginarias y solución oscilante de período "
    "T=2π/ω₀; es la misma constante m·g·l/J, solo cambia el signo según el lado del "
    "equilibrio — y es exactamente el método usado para medir ω₀: colgar el robot y "
    "cronometrar el período, sección 4.7). La solución θ(t)=A·e^(ω₀t)+B·e^(-ω₀t) diverge "
    "dominada por e^(ω₀t): ω₀ es la tasa de ese crecimiento (en 1/ω₀ s, la desviación se "
    "multiplica por e≈2.72)."), st_body))
story.append(Paragraph(md_inline("Con esas definiciones, la ecuación final:"), st_body))
story.append(mathimg(r"$\ddot{\theta} = \omega_0^2\,\theta + K_U\,u_{\mathrm{efectivo}}$"))
story.append(Spacer(1, 1.5*mm))
story.append(Paragraph(md_inline(
    "**ω**₀ (rad/s): tasa de inestabilidad (medido: 7.4 rad/s). **K_U** (rad/s² por duty): "
    "autoridad del actuador. **u_efectivo** (duty, -160 a 160): comando que efectivamente "
    "llega al motor — `u_pid` (2.4) después de zona muerta y saturación (2.3)."), st_body))
story.append(Paragraph(md_inline("**Licencias explícitas:**"), st_body))
story.append(ListFlowable([
    ListItem(Paragraph(md_inline(
        "**Linealización**, válida hasta ~3-8° (sinθ y θ difieren <0.3%)."), st_li), bulletColor=ACCENT),
    ListItem(Paragraph(md_inline(
        "**Un solo grado de libertad**: el modelo completo tiene θ y la posición sobre el "
        "piso (licencia de la sección 2.1); acá solo importa θ. La inercia rotacional de las "
        "ruedas y la rodadura tampoco se modelan aparte — quedan absorbidas en K_U (licencia 3)."),
        st_li), bulletColor=ACCENT),
    ListItem(Paragraph(md_inline(
        "**K_U agrupa toda la cadena de actuación** en una constante medida de punta a punta "
        "con balanza, evitando propagar error de cada parámetro intermedio."), st_li), bulletColor=ACCENT),
    ListItem(Paragraph(md_inline(
        "**Asimetría entre motores, explícita, no promediada** (sección 4.3) — función "
        "escalón sobre K_U (sección 3.2)."), st_li), bulletColor=ACCENT),
], bulletType="bullet", start="•", leftIndent=12, bulletFontSize=7))

story.append(Paragraph("2.3 El actuador real: zona muerta y saturación", st_h2))
story.append(ListFlowable([
    ListItem(Paragraph(md_inline(
        "**Zona muerta**: el motor no responde por debajo de un umbral de duty (92 en M1, "
        "102 en M2) — medido con balanza en todo el rango, no supuesto (sección 4.2/4.3)."),
        st_li), bulletColor=ACCENT),
    ListItem(Paragraph(md_inline(
        "**Saturación**: cap de seguridad 160/255, de una caracterización térmica del driver "
        "(brake, ventana ~60s antes de riesgo térmico) — no un número elegido para el PSO."),
        st_li), bulletColor=ACCENT),
], bulletType="bullet", start="•", leftIndent=12, bulletFontSize=7))
story.append(Paragraph(md_inline(
    "Si el PSO optimizara contra un actuador ideal, encontraría ganancias que en la "
    "simulación se ven perfectas pero que el motor real jamás ejecuta."), st_body))

story.append(Paragraph("2.4 Control PID", st_h2))
story.append(mathimg(r"$u_{pid}(t) = K_p\,e(t) + K_i\!\int e(t)\,dt + K_d\,\dfrac{de(t)}{dt}$"))
story.append(Spacer(1, 1.5*mm))
story.append(Paragraph(md_inline(
    "con `e(t)=θ(t)`. **Kp** reacciona al error presente, **Ki** elimina el residual "
    "acumulado, **Kd** anticipa y amortigua el sobreimpulso. Encontrar los tres a mano sobre "
    "un sistema con zona muerta y saturación es exactamente lo que se delega en PSO."), st_body))

story.append(Paragraph("2.5 Por qué PSO", st_h2))
story.append(Paragraph(md_inline(
    "PSO optimiza una función de costo de caja negra (4s de dinámica no lineal simulada) sin "
    "necesitar derivada — la alternativa sin gradiente sería una búsqueda de grilla/aleatoria, "
    "mucho menos eficiente en 3D continuas. Cada partícula es un punto (Kp, Ki, Kd); el "
    "enjambre converge combinando inercia, atracción a su mejor histórico y al mejor global "
    "— sin derivar una función que ni siquiera es diferenciable (penalización discontinua, "
    "sección 3.3)."), st_body))

# ============================================================ 3. PSO
story.append(Paragraph("3. PSO — implementación y características", st_h1))
story.append(Paragraph(md_inline(
    "Implementado desde cero en NumPy puro, con la variante de **constricción de "
    "Clerc-Kennedy**:"), st_body))
story.append(mathimg(
    r"$\chi = \dfrac{2}{\left|\,2-\varphi-\sqrt{\varphi^2-4\varphi}\,\right|}, \quad \varphi=\varphi_1+\varphi_2=4.1$",
    r"$v_i(t+1) = \chi\left(v_i(t) + \varphi_1 r_1 (p_i - x_i(t)) + \varphi_2 r_2 (g - x_i(t))\right)$",
    r"$x_i(t+1) = x_i(t) + v_i(t+1)$",
))
story.append(Spacer(1, 1.5*mm))
story.append(Paragraph(md_inline(
    "`i` identifica a la partícula (fija en el tiempo), `t` a la generación — por eso `x_i` "
    "se actualiza a `x_i(t+1)`, no a `x_(i+1)` (otra partícula distinta). `x_i` es la "
    "posición (Kp,Ki,Kd), `v_i` su velocidad, `pbest_i`/`gbest` la mejor posición "
    "propia/del enjambre, `φ1,φ2` los pesos cognitivo/social, `χ` el factor de constricción."), st_body))
story.append(Paragraph(md_inline(
    "**Por qué constricción**: en la variante original (Kennedy y Eberhart, 1995) la "
    "velocidad puede crecer sin límite, disparando partículas fuera del espacio de búsqueda. "
    "La solución clásica es un `Vmax` ajustado a mano por ensayo y error. `χ` resuelve lo "
    "mismo con un análisis matemático de estabilidad, sin ese hiperparámetro extra. Con "
    "`φ=4.1` (estándar de la literatura), `χ≈0.7298`."), st_body))
story.append(Paragraph(md_inline(
    "**Configuración**: 25 partículas, 40 generaciones, semilla fija. Límites: Kp∈[0,3500], "
    "Ki∈[0,3000], Kd∈[0,800] (ampliados durante el desarrollo, sección 4.4). Las 40 "
    "generaciones son iteraciones del optimizador, no tiempo simulado — cada evaluación corre "
    "4s de dinámica (sección 3.3), dos escalas de tiempo distintas."), st_body))

story.append(Paragraph("3.1 Variables del robot real que entran al modelo", st_h2))
story.append(table_md(
    ["Parámetro", "Valor", "Rol"],
    [
        ["ω₀", "7.4 rad/s", "Tasa de inestabilidad"],
        ["K_U", "0.0218 rad/s² por duty", "Autoridad del actuador"],
        ["Zona muerta M1/M2", "92/102 duty", "Umbral de k_u_efectivo"],
        ["MAX_DUTY", "160", "Saturación"],
        ["Escenarios (ángulo)", "1.5°/2.0°/3.0°", "Rango real medido"],
        ["Escenarios (velocidad)", "−0.15/0/+0.15 rad/s", "Acotado por el método de medición"],
    ]))
story.append(Spacer(1, 1.5*mm))
story.append(Paragraph(md_inline(
    "Los ángulos cubren el rango real de perturbación (media 1.98°, peor caso 3.06°, n=22 — "
    "Test 10: parar el robot a mano, sin estímulo externo). Las velocidades salen del mismo "
    "test: solo captura una muestra con giroscopio <0.15 rad/s (\"quieto\"), así que ninguna "
    "muestra real supera esa velocidad en el instante que importa."), st_body))

story.append(Paragraph("3.2 La asimetría entre motores, modelada como dos zonas muertas", st_h2))
story.append(Paragraph(md_inline(
    "M1 responde desde ~92/255, M2 desde ~102/255. Una sola zona muerta ignora la franja de "
    "10 puntos donde uno empuja y el otro no:"), st_body))
story.append(codeblock(
    "def k_u_efectivo(u_abs):\n"
    "    if u_abs < DEAD_ZONE_M1:   return 0.0     # ningun motor responde\n"
    "    elif u_abs < DEAD_ZONE_M2: return PROP_M1  # solo M1 (57.5% del K_U total)\n"
    "    else:                      return 1.0      # los dos motores"))
story.append(Spacer(1, 1.5*mm))
story.append(Paragraph(md_inline(
    "`PROP_M1 = 0.575` sale de la razón de torque M2/M1 medida (~0.74): `1/(1+0.74)`."), st_body))

story.append(Paragraph("3.3 Función de costo", st_h2))
story.append(Paragraph(md_inline(
    "Variante de **ITAE** (Integral of Time-weighted Absolute Error, también llamada "
    "\"aptitud\"/\"fitness\" en la literatura — acá se minimiza, así que \"mejor\" es "
    "\"menor\"), con penalización de 500 si el péndulo termina caído (>30°):"), st_body))
story.append(mathimg(
    r"$\mathrm{costo} = \mathrm{prom.\ escenarios}\left(\sum_t t\,|\theta(t)|\,\Delta t \;+\; 500 \ \mathrm{si}\ |\theta_{final}|>30^\circ\right)$"))
story.append(Spacer(1, 1.5*mm))
story.append(Paragraph(md_inline(
    "Los dos números son deliberadamente holgados, no un ajuste fino: `30°` es ~10× la peor "
    "perturbación real (3.06°) y muy por fuera de donde vale la linealización. `500` domina "
    "el costo de cualquier trayectoria exitosa (ITAE típico 0.03-0.10) por más de 5000×, para "
    "que fallar un escenario nunca compita con ser un poco más lento en los otros ocho. Se "
    "promedia sobre 9 escenarios (3 ángulos × 3 velocidades), no uno solo (motivo en sección "
    "4.1)."), st_body))

story.append(Paragraph("3.4 Resultados", st_h2))
story.append(Paragraph(md_inline(
    "**Ganancias óptimas**: Kp=3500.00, Ki=2297.02, Kd=484.22, costo final 55.5973."), st_body))

conv_path = os.path.join(HERE, "convergencia_pso.png")
if os.path.exists(conv_path):
    story.append(Image(conv_path, width=160 * mm, height=160 * mm * (630/1650)))
    story.append(Paragraph(
        "Convergencia del PSO, en escala lineal (izq., línea roja: piso teórico 500/9≈55.56) "
        "y en escala logarítmica del excedente sobre el piso (der.) — más clara para ver que "
        "los tres saltos de mejora (generación ~1, ~2 y ~7) son reales, no ruido.", st_caption))

story.append(Paragraph(md_inline(
    "La convergencia es casi inmediata (generación ~7) y luego plana, porque el costo tiene "
    "un **piso teórico**. El ITAE nunca es negativo, y el escenario 9 (θ=3°, v=+0.15 rad/s) "
    "paga la penalización de 500 sin importar las ganancias (límite físico del actuador, "
    "sección 4.6) — así que:"), st_body))
story.append(mathimg(
    r"$\mathrm{costo} = \dfrac{\mathrm{ITAE}_1+\cdots+\mathrm{ITAE}_8+\mathrm{ITAE}_9+500}{9} \;\geq\; \dfrac{0+\cdots+0+500}{9} = \dfrac{500}{9} \approx 55.56$"))
story.append(Spacer(1, 1.5*mm))
story.append(Paragraph(md_inline(
    "El óptimo encontrado está a <0,1% de ese piso — casi no queda margen de mejora una vez "
    "resueltos los 8 escenarios fáciles (ITAE entre 0.026 y 0.041 cada uno; el noveno aporta "
    "0.098 + la penalización completa; el promedio da exactamente 55.5973)."), st_body))

story.append(Paragraph(md_inline(
    "**Cómo converge el enjambre, no solo el costo**: instrumentando una copia del algoritmo "
    "que guarda la posición de las 25 partículas por generación (mismo resultado verificado, "
    "55.5973):"), st_body))

swarm_path = os.path.join(HERE, "convergencia_enjambre.png")
if os.path.exists(swarm_path):
    story.append(Image(swarm_path, width=120 * mm, height=120 * mm * (750/1650)))
    story.append(Paragraph(
        "Trayectoria de las 25 partículas, planos (Kp,Ki) y (Ki,Kd). Gris: inicio. Azul: "
        "final. Estrella: gbest.", st_caption))
story.append(Paragraph(md_inline(
    "Las partículas arrancan dispersas por todo el espacio y terminan agrupadas cerca del "
    "óptimo — en (Kp,Ki) migran al borde Kp=3500 (sección 4.5); en (Ki,Kd) se ve el embudo "
    "hacia (Ki≈2300, Kd≈480). Es la misma razón del piso teórico: la superficie de costo es "
    "casi plana lejos de la zona muerta, así que un punto de partida lejano no cuesta mucho "
    "más que uno cercano."), st_body))

resp_path = os.path.join(HERE, "respuesta_pid_optimo.png")
if os.path.exists(resp_path):
    story.append(Image(resp_path, width=110 * mm, height=110 * mm * (600/800)))
    story.append(Paragraph(
        "Respuesta con las ganancias óptimas, θ₀=2° (la media real medida).", st_caption))
story.append(Paragraph(md_inline(
    "Con θ₀=2°, el ángulo decae a 0° en ~3,5-4s. Dos detalles: (1) **no es que el motor "
    "tarde en reaccionar** — con Kp=3500 el comando cruza la zona muerta casi de inmediato, "
    "pero queda oscilando en su borde (patrón *bang-bang*, sección 4.5) en vez de ir a fondo "
    "de escala, así que la corrección promedio es \"a media máquina\"; (2) **el comando se ve "
    "siempre negativo** porque en esta trayectoria puntual θ nunca cruza el cero dentro de "
    "los 4s (`u_motor=-u_pid`) — no es una propiedad general."), st_body))

story.append(Paragraph(md_inline(
    "**Verificación escenario por escenario** (sección 4.6): de 9, **8 estabilizan limpio**. "
    "El que no — θ=3°, v=+0.15 rad/s — combina el peor ángulo medido con una velocidad que ya "
    "empeora esa posición (velocidad positiva con ángulo positivo = ya se está inclinando "
    "más). El controlador solo, con θ=3°, llega a tiempo (queda margen hasta el umbral de "
    "3.65°, sección 4.6); con la velocidad sumada, el ángulo cruza ese umbral *antes* de que "
    "el controlador reaccione, y entra en la misma divergencia exponencial e^(ω₀t) derivada "
    "en 2.2 — sin vuelta atrás (termina en 60.158°, el tope de simulación). El escenario "
    "gemelo con v=−0.15 rad/s sí se resuelve: esa velocidad ya apunta hacia la vertical, a "
    "favor del controlador."), st_body))

story.append(Paragraph("3.5 Repetibilidad entre corridas independientes", st_h2))
story.append(Paragraph(md_inline(
    "Repitiendo el entrenamiento 20 veces con semillas distintas:"), st_body))

box_path = os.path.join(HERE, "boxplot_multiseed.png")
if os.path.exists(box_path):
    story.append(Image(box_path, width=125 * mm, height=125 * mm * (630/1650)))
    story.append(Paragraph(
        "Distribución de Kp, Ki, Kd y costo final sobre 20 corridas independientes.", st_caption))
story.append(Paragraph(md_inline(
    "El costo final es prácticamente idéntico (coef. de variación 0,002%) — consistente con "
    "estar pegado al piso teórico. Kp pega en 3500 en 19/20 corridas (la excepción, 3260, "
    "confirma que no hay óptimo interior — sección 4.5). Ki y Kd sí varían más entre semillas "
    "sin que el costo cambie: la superficie de costo es plana en esas direcciones, hay una "
    "franja ancha de combinaciones casi igual de buenas."), st_body))

# ============================================================ 4. INCONVENIENTES
story.append(Paragraph("4. Inconvenientes encontrados y cómo se resolvieron", st_h1))
story.append(Paragraph(md_inline(
    "El problema no fue \"correr PSO\" (directo), fue construir un modelo cuyos parámetros y "
    "función de costo representaran honestamente al sistema real, y detectar cuándo no lo "
    "hacían."), st_body))

secciones_4 = [
    ("4.1 El costo de una sola condición inicial era engañoso",
     "Evaluar cada partícula con una única condición inicial dejaba el costo dominado por el "
     "ruido caótico de temporización de la zona muerta: cambios de 0.08% en Kd empeoraban el "
     "costo 6.6×. **Solución**: promediar sobre 9 escenarios — la sensibilidad local bajó de "
     "~660% a ~15%."),
    ("4.2 Medir K_U: dos vías fallidas antes de la que funcionó",
     "K_U no se mide con una regla. Inferirlo de la caída de tensión del L298N (multímetro) "
     "falló dos veces por método (asumir 0V en el tramo apagado del PWM; asumir una caída "
     "constante, que tampoco lo era — el driver degrada su salida al calentarse). Se midió "
     "**el torque directo con balanza**: con los dos motores conduciendo a la vez, el torque "
     "de cada uno cae 31-36% respecto de medirlo solo — el robot balancea con los dos activos "
     "siempre, así que ese es el número que corresponde, no el de un motor aislado."),
    ("4.3 La zona muerta no es un número — son dos, distintos",
     "La misma medición reveló que los motores no arrancan al mismo duty. El valor único "
     "usado antes (\"al aire, sin carga\") tampoco correspondía a la condición real, donde "
     "ambos umbrales suben (sección 3.2)."),
    ("4.4 El límite de búsqueda de Kp — y después el de Ki, y el de Kd",
     "K_U más chico que lo asumido sube el umbral teórico Kp > ω₀²/K_U de ~1827 a ~2512 — "
     "por encima del límite viejo del PSO (2000). Al ampliarlo, Kp volvió a pegarse al nuevo "
     "límite; al ampliar el de Ki, Ki encontró óptimo interior pero Kd se pegó al suyo; al "
     "ampliar Kd, los tres se estabilizaron (sección 4.5)."),
]
for titulo, texto in secciones_4:
    story.append(Paragraph(titulo, st_h3))
    story.append(Paragraph(md_inline(texto), st_body))

story.append(Paragraph("4.5 Por qué Kp no tiene óptimo interior en este modelo (y Ki, Kd sí)", st_h3))
story.append(Paragraph(md_inline(
    "Con la zona muerta ocupando gran parte del rango, un error chico produce un comando que "
    "no mueve ningún motor. Mayor Kp cruza antes ese umbral — y como el comando además "
    "satura, subir Kp no tiene costo en este modelo, solo lo acerca a *bang-bang*, óptimo "
    "para ITAE. Un barrido confirma costo monótono decreciente, sin mínimo interior. Que Ki y "
    "Kd sí tengan óptimo interior confirma que el problema es específico de Kp: Ki muy grande "
    "genera sobreimpulso e integral *windup* (ITAE sí lo penaliza), igual Kd con el ruido de "
    "la derivada. **La función de costo está incompleta para Kp, no el resultado está mal** "
    "— un término que penalice la frecuencia de conmutación daría un óptimo con sentido "
    "físico (extensión natural del trabajo)."), st_body))

story.append(Paragraph(
    "4.6 Un escenario no se resuelve — y confirma, por un camino independiente, el mismo "
    "límite medido con la balanza", st_h3))
story.append(Paragraph(md_inline(
    "El escenario más exigente (3.0°, +0.15 rad/s) termina en caída completa sin importar "
    "cuánto se amplíen las ganancias. No es falta de búsqueda: es un límite de autoridad del "
    "actuador. Con el comando saturado, la corrección disponible es K_U·MAX_DUTY≈3.49 "
    "rad/s²; el término desestabilizante a 3° ya es ω₀²·3°≈2.87 rad/s² — margen de 0.62 "
    "rad/s², que se agota en:"), st_body))
story.append(mathimg(r"$\theta_{umbral} = \dfrac{K_U \cdot MAX_{DUTY}}{\omega_0^2} \approx 3.65^\circ$"))
story.append(Spacer(1, 1.5*mm))
story.append(Paragraph(md_inline(
    "Dentro de un 0.3%, es el mismo 3.66° que había cerrado la medición directa de torque con "
    "balanza esa misma noche — dos caminos independientes (simulación vs. torque medido) al "
    "mismo número. No es coincidencia buscada: es la misma física, vista dos veces. El "
    "sistema físico, con este actuador, tiene un límite de perturbación recuperable apenas "
    "por encima del peor caso real medido — margen positivo (~20%) en general, sin margen "
    "solo en la combinación más severa de ángulo y velocidad simultáneos."), st_body))

story.append(Paragraph("4.7 Una lección de método que se repitió tres veces", st_h3))
story.append(Paragraph(md_inline(
    "Medir ω₀ costó tres intentos: los dos primeros, repetibles (dispersión 0.8% y 1.5%), "
    "medían la cantidad equivocada (un péndulo doble sin saberlo; el robot colgando de un "
    "brazo humano en vez de un pivote rígido). Lo que los distinguió del valor correcto no "
    "fue la dispersión, sino un chequeo físico de una línea: L_eq = g·(T/2π)² tiene que ser "
    "del orden del tamaño del robot (26 cm) — solo 7.4 rad/s lo cumple. El mismo patrón "
    "volvió con K_U y con el escenario 9. **La lección: la repetibilidad mide la estabilidad "
    "de un montaje, no la validez de lo que mide** — lo que valida es un chequeo "
    "independiente contra una física que tiene que cerrar."), st_body))

# ============================================================ 5. CONCLUSIONES
story.append(Paragraph("5. Conclusiones", st_h1))
story.append(ListFlowable([
    ListItem(Paragraph(md_inline(
        "Se implementó PSO desde cero (constricción de Clerc-Kennedy) para optimizar un PID "
        "sobre un modelo de péndulo invertido construido enteramente con datos medidos, no "
        "supuestos."), st_li), bulletColor=ACCENT),
    ListItem(Paragraph(md_inline(
        "Construir el modelo fue más largo e instructivo que correr el algoritmo: ω₀, K_U y "
        "la función de costo pasaron por fallas de método identificadas y corregidas con "
        "causa raíz documentada, no solo el número final."), st_li), bulletColor=ACCENT),
    ListItem(Paragraph(md_inline(
        "Las ganancias encontradas (Kp=3500, Ki=2297.02, Kd=484.22) estabilizan 8 de 9 "
        "escenarios de diseño. El que no se resuelve no es un fallo del algoritmo — es un "
        "límite físico de torque, y la simulación lo reproduce de forma independiente casi "
        "exacta (3.65° vs. 3.66° medido), lo que da confianza en que el modelo captura la "
        "física que importa."), st_li), bulletColor=ACCENT),
    ListItem(Paragraph(md_inline(
        "Trabajo futuro: un término de costo que penalice la frecuencia de conmutación (Kp "
        "con óptimo interior físico), y validar contra el robot real cuando el driver se "
        "reemplace por uno con más margen de torque."), st_li), bulletColor=ACCENT),
], bulletType="bullet", start="•", leftIndent=12, bulletFontSize=7))

story.append(Spacer(1, 4*mm))
story.append(HRFlowable(width="100%", thickness=0.6, color=RULE))
story.append(Spacer(1, 2*mm))
story.append(Paragraph(md_inline(
    "Código fuente completo, notebook ejecutable y registro de mediciones físicas: "
    "https://github.com/piertotumlocomotor/robot_balance (rama develop, carpeta "
    "`notebooks/`)."), st_caption))

def build():
    doc = SimpleDocTemplate(
        os.path.join(HERE, "entrega_pso_pendulo.pdf"),
        pagesize=A4,
        leftMargin=16 * mm, rightMargin=16 * mm,
        topMargin=13 * mm, bottomMargin=13 * mm,
        title="Optimización de un controlador PID mediante PSO",
        author="Cesar Yepez",
    )
    doc.build(story)
    print("PDF generado:", os.path.join(HERE, "entrega_pso_pendulo.pdf"))


if __name__ == "__main__":
    build()
