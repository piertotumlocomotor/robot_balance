"""
Genera el informe PDF del Desafío 1 (Aprendizaje por Refuerzo I) — versión para el profesor.

Correr (necesita reportlab, que está en el .venv de la raíz del repo):
    ../../.venv/bin/python generar_informe.py
Antes: experimentos.py (figuras y resultados) y figuras_explicativas.py.
"""

import json

from estilo import *   # noqa: F401,F403  (estilos, helpers y reportlab)

FIG = os.path.join(HERE, "..", "figuras")
RES = os.path.join(HERE, "..", "resultados", "resultados.json")
REPO = "https://github.com/piertotumlocomotor/robot_balance/tree/desafio1-rl"

R = json.load(open(RES))
EV, N_SEM, N_EVAL = R["evaluacion"], len(R["semillas"]), R["n_eval"]
TR = json.load(open(os.path.join(HERE, "transiciones.json")))
TEXTOS = json.load(open(os.path.join(HERE, "textos_resultados.json")))   # discusión y conclusiones


def ms(nombre, clave, fmt=".1f"):
    """'media ± desvío' entre semillas, para la tabla de resultados."""
    v = EV[nombre][clave]
    if v is None:
        return "—"
    return f"{v[0]:{fmt}}" if nombre == "PD a mano" else f"{v[0]:{fmt}} ± {v[1]:{fmt}}"


story = []
P = lambda t: p(story, t)

# ------------------------------------------------------------ portada
story.append(Paragraph("Balanceo de un robot de dos ruedas con aprendizaje por refuerzo", st_title))
story.append(Paragraph("DQN, Double DQN y A2C sobre un simulador construido con mediciones del robot real", st_subtitle))
story.append(Spacer(1, 2 * mm))
story.append(Paragraph("<b>Materia:</b> Aprendizaje por Refuerzo I (2026) — Desafío Nº 1 — MIA, FIUBA", st_meta))
story.append(Paragraph("<b>Técnicas:</b> Deep Q-Network, Double DQN, Advantage Actor-Critic (A2C)", st_meta))
story.append(Paragraph("<b>Autor:</b> Cesar Yepez", st_meta))
story.append(Paragraph(f'<b>Repositorio:</b> <link href="{REPO}" color="#185fa5">{REPO}</link>', st_meta))
story.append(Spacer(1, 2 * mm))
story.append(HRFlowable(width="100%", thickness=0.8, color=RULE))

# ------------------------------------------------------------ 1. problema
story.append(Paragraph("1. El problema", st_h1))
P("Un robot balanceador de dos ruedas es un **péndulo invertido**: su equilibrio vertical es inestable "
  "y, si nadie actúa, cae en menos de medio segundo. Para sostenerlo, un controlador tiene que decidir "
  "100 veces por segundo cuánto y hacia dónde mover los motores, a partir de la inclinación que mide "
  "el sensor (IMU).")
P("El robot existe físicamente (ESP32, dos motorreductores JGB37-520B, dos drivers BTS7960, MPU6050). "
  "El objetivo es que un agente **aprenda** esa política sin que se le dé la ecuación del sistema, solo "
  "una señal de recompensa, y comparar tres algoritmos vistos en la materia sobre el mismo problema. "
  "Como el aprendizaje por prueba y error sobre el robot real lo rompería, se entrena sobre un "
  "**simulador construido con los parámetros medidos** del robot.")

# ------------------------------------------------------------ 2. modelo
story.append(Paragraph("2. Modelo físico y simulador", st_h1))
story.append(Paragraph("2.1 Ecuación del movimiento", st_h2))
figura(story, os.path.join(HERE, "dcl.png"), 52,
       "Figura 1 — Diagrama de cuerpo libre. El cuerpo gira alrededor del eje de las ruedas.")
P("Tomando momentos respecto del eje de las ruedas y linealizando para ángulos chicos (sen θ ≈ θ):")
story.append(mathimg(r"$J\,\ddot{\theta} = m g l\,\theta + \tau_{motor}(u) \quad\Rightarrow\quad "
                     r"\ddot{\theta} = \omega_0^2\,\theta + K_U\,k(|u|)\,u$"))
story.append(Spacer(1, 1.5 * mm))
P("La aceleración angular tiene dos términos que compiten:")
vinetas(story, [
    "**La gravedad** (término ω₀²·θ). Empuja a caer, y más cuanto más inclinado está (ω₀² = m·g·l / J). Por "
    "eso el equilibrio es inestable.",
    "**Los motores** (término K_U · k(|u|) · u, que es τ_motor / J). Se lee de derecha a izquierda: **u** es el duty "
    "de PWM aplicado a los dos motores (−121 a +121; el signo es el sentido de giro); **k(|u|)** es la "
    "fracción de torque disponible por la zona muerta: vale 0 si |u| < 66 (ningún motor vence su "
    "fricción interna), 0.57 si 66 ≤ |u| < 73 (solo empuja M1) y 1 si |u| ≥ 73 (empujan los dos); "
    "**K_U** convierte duty en aceleración angular (rad/s² por unidad de duty) con los dos motores activos.",
])

story.append(Paragraph("2.2 Parámetros", st_h2))
story.append(table_md(
    ["Parámetro", "Valor", "Origen"],
    [["ω₀ (frecuencia natural)", "8.0 rad/s", "medido: video del robot colgado del eje"],
     ["J (momento de inercia)", "0.0159 kg·m²", "medido: masa y centro de masa (dos apoyos), J = m·g·l/ω₀²"],
     ["m·l", "0.1040 kg·m", "medido: dos apoyos sobre balanza"],
     ["K_U", "**0.043** rad/s² por duty", "**supuesto** (ver 2.3)"],
     ["zona muerta M1 / M2", "66 / 73 de duty", "medida con el driver anterior, escalada igual que K_U"],
     ["tope de duty", "121 / 255", "6 V nominales del motor sobre 12.6 V de batería"]]))
story.append(Spacer(1, 2 * mm))

story.append(Paragraph("2.3 El parámetro que no se pudo medir: K_U", st_h2))
P("El torque de los motores con el driver nuevo (BTS7960) iba a medirse con balanza, pero el "
  "microcontrolador se dañó por un cortocircuito antes del ensayo. K_U se construyó así:")
story.append(mathimg(r"$K_U = f \cdot \dfrac{\tau_{M1} + \tau_{M2}}{u_{ensayo} \cdot J} = "
                     r"1.4 \cdot \dfrac{0.0443 + 0.0339}{160 \cdot 0.0159} \approx 0.043 "
                     r"\ \mathrm{rad/s^2\ por\ duty}$", line_height_mm=11))
story.append(Spacer(1, 1.5 * mm))
story.append(table_md(
    ["Término", "Valor", "De dónde sale"],
    [["τ_M1, τ_M2", "0.0443 y 0.0339 N·m", "torque de cada motor medido con balanza, con el driver anterior "
      "(L298N), los dos motores encendidos a la vez"],
     ["u_ensayo", "160 de duty", "duty al que se midieron esos torques"],
     ["J", "0.0159 kg·m²", "inercia del robot actual (2.2): pasa de torque a aceleración angular"],
     ["f (factor de driver)", "**1.4** (supuesto)", "cuánto más torque da el driver nuevo al mismo duty"]]))
story.append(Spacer(1, 1.5 * mm))
P("**Por qué f está entre 1.0 y 1.6.** El torque de un motor frenado es proporcional a la tensión que "
  "le llega. El L298N usa transistores bipolares que pierden ≈ 3 V: a duty 160 debía entregar "
  "160/255 · 12.4 V ≈ 7.8 V y el motor recibía ≈ 5 V (estimado por la velocidad de giro). El BTS7960 "
  "usa MOSFETs (≈ 10 mΩ) y prácticamente no pierde tensión. Entonces f = 1.0 es el caso pesimista "
  "(rinde igual que el viejo) y f ≈ 7.8/5 ≈ 1.6 el optimista (llega toda la tensión). Se adoptó **1.4**, "
  "medianamente optimista, dejando margen para pérdidas en cables y batería. La zona muerta medida "
  "con el driver viejo (92 y 102 de duty) se divide por el mismo factor, porque también es un umbral "
  "de tensión.")
story.append(mathimg(r"$\theta_{umbral} = \dfrac{K_U \cdot 121}{\omega_0^2} \approx 4.7^\circ$", line_height_mm=11))
story.append(Spacer(1, 1.5 * mm))
P("θ_umbral es la inclinación máxima de la que el robot puede recuperarse aun con todo el motor (más "
  "allá, la gravedad gana). Es el número que más depende del supuesto y debe validarse en el robot real.")

story.append(Paragraph("2.4 Simulador y su verificación", st_h2))
P("Se implementó como entorno de **Gymnasium** (`entorno.py`). La física se integra con Euler "
  "semi-implícito en pasos de 1 ms y el agente decide cada 10 ms. Antes de entrenar se hicieron tres "
  "verificaciones (`test_entorno.py`), porque un error en el simulador se confunde fácilmente con un "
  "agente que aprende mal:")
vinetas(story, [
    "**(a) Cumple la interfaz de Gymnasium.** La función `check_env` de la biblioteca comprueba que "
    "`reset` y `step` devuelven lo que deben, con los tipos y tamaños declarados, y que con la misma "
    "semilla se repite el episodio. Garantiza que cualquier algoritmo puede usar el entorno.",
    "**(b) La física coincide con la solución exacta.** Sin motor, la ecuación tiene solución "
    "analítica θ(t) = θ₀·cosh(ω₀·t). Partiendo de 1°, a los 0.3 s el simulador da 5.58° y la fórmula "
    "5.56° (error 0.4%): el integrador y el signo de la gravedad están bien.",
    "**(c) El problema es resoluble pero no trivial.** Una política al azar cae siempre (en ≈ 35 pasos, "
    "0.35 s) y un controlador PD ajustado a mano sostiene el robot en 50 de 50 episodios. Si el azar "
    "sobreviviera, no habría nada que aprender; si el PD cayera, el problema sería imposible o habría un error.",
])
P("**El controlador PD de referencia** aplica u = −(K_p·θ + K_d·dθ/dt): un término proporcional a la "
  "inclinación (como un resorte que la devuelve a la vertical) y otro a la velocidad con que cambia "
  "(como un amortiguador que evita pasarse). Con K_p ≈ 54 de duty por grado y K_d ≈ 260 de duty por "
  "rad/s, elegidas a mano, y el resultado redondeado al nivel de duty disponible más cercano. No "
  "aprende nada: sirve de vara para medir a los agentes.")

# ------------------------------------------------------------ 3. MDP
story.append(Paragraph("3. Formulación como proceso de decisión de Markov (MDP)", st_h1))
P("Cada 10 ms el agente mira el estado del robot, elige cuánto motor aplicar, el simulador avanza "
  "10 ms y le devuelve el estado nuevo y una recompensa (Figura 2). Un **episodio** es un intento de "
  "balanceo: empieza con el robot apoyado a mano y termina cuando cae o cuando pasan 5 s.")
figura(story, os.path.join(HERE, "bucle.png"), 128, "Figura 2 — El bucle agente-entorno en este problema.")
story.append(table_md(
    ["Elemento", "Definición"],
    [["Estado s", "θ (inclinación, rad), dθ/dt (velocidad angular, rad/s) y u anterior (último duty aplicado)"],
     ["Acciones a", "9 niveles de duty: −121, −105, −90, −75, 0, +75, +90, +105, +121"],
     ["Recompensa r", "1 − (θ/10°)² − 0.05·(u/121)² − 0.05·(Δu/242)² por paso; 0 si cae"],
     ["Fin del episodio", "cae: |θ| > 10° (terminal)  ·  pasan 5 s = 500 pasos (truncado, no terminal)"],
     ["Estado inicial", "θ₀ al azar entre −3.06° y +3.06°; dθ/dt₀ al azar entre −0.15 y +0.15 rad/s"],
     ["Descuento γ", "0.99"]]))
story.append(Spacer(1, 1.5 * mm))
story.append(Paragraph("Decisiones de diseño", st_h3))
vinetas(story, [
    "**Estado normalizado.** Antes de entrar a la red, cada variable se divide por una escala típica: θ "
    "por 10°, dθ/dt por ω₀·10° ≈ 1.4 rad/s y u por 121. Así las tres quedan en el orden de −1 a 1. Sin "
    "eso, θ (≈ 0.05 rad) y u (≈ 100) diferirían en más de mil veces, y la red aprendería casi solo de la "
    "variable más grande. El u anterior se incluye porque la recompensa castiga cambiarlo (Δu): sin él "
    "el agente no podría saber cuánto le cuesta cada acción y el estado no sería markoviano.",
    "**Acciones discretas.** DQN calcula un valor Q para cada acción y elige el máximo, así que necesita "
    "una lista finita de acciones. Se usaron 9 niveles y los mismos para A2C, para que la comparación "
    "sea sobre el mismo problema. El menor nivel distinto de cero es 75 porque por debajo de 66–73 el "
    "motor no se mueve (zona muerta).",
    "**Recompensa.** El **1** premia cada paso de pie (sobrevivir es lo principal). **−(θ/10°)²** premia "
    "estar derecho: cuesta poco cerca de la vertical y ≈ 1 cerca de los 10° (anula el premio de seguir "
    "de pie). Los términos con **0.05** castigan, con poco peso, gastar motor y conmutarlo bruscamente, "
    "que en el robot real calienta el driver y hace vibrar el chasis. Caer da 0 y termina el episodio.",
    "**Estado inicial = la colocación a mano.** Es el error de apoyar el robot \"lo más vertical que el "
    "ojo permite\": se midió 22 veces, registrando la inclinación cuando el robot estaba quieto "
    "(velocidad angular menor a 0.15 rad/s). Dio en promedio 1.98° y como máximo 3.06°. Por eso cada "
    "episodio arranca con un ángulo al azar de hasta ±3.06° **y** una velocidad angular al azar de hasta "
    "±0.15 rad/s: son dos variables distintas, de ahí las unidades distintas. Se descartan los pocos "
    "estados de los que no se puede salir ni con todo el motor.",
    "**Descuento γ = 0.99.** El agente maximiza G = r₁ + γ·r₂ + γ²·r₃ + …: las recompensas no se "
    "achican, pero **pesan menos cuanto más lejos están**. Una recompensa a 100 pasos (1 s) pesa "
    "0.99¹⁰⁰ ≈ 0.37 y una a 500 pasos, 0.007. El agente \"mira\" aproximadamente el próximo segundo, "
    "suficiente porque sin control el robot cae en 0.4 s.",
])
P("**Escala del retorno.** Los gráficos muestran el retorno **sin descuento**: la suma de las "
  "recompensas del episodio. Como cada paso vale como máximo 1 y hay 500 pasos, el máximo es ≈ 500. Una "
  "política al azar cae en ≈ 35 pasos sumando ≈ 0.85 por paso (el término de inclinación crece mientras "
  "cae), es decir ≈ 30.")
story.append(Paragraph("Ejemplo: los primeros pasos de un episodio (DQN entrenada, desde θ = 3°)", st_h3))
story.append(table_md(
    ["t", "θ", "dθ/dt", "acción (duty)", "recompensa", "Q(s, a elegida)", "Q(s, duty 0)"],
    [[f"{f['t_ms']} ms", f"{f['theta_deg']:.2f}°", f"{f['theta_dot']:+.3f} rad/s", f"{f['duty']:+.0f}",
      f"{f['r']:.3f}", f"{f['q_elegida']:.1f}", f"{f['q_duty0']:.1f}"] for f in TR[:6]]))
story.append(Spacer(1, 1.5 * mm))
P(TEXTOS["ejemplo_transiciones"])

# ------------------------------------------------------------ 4. agentes
story.append(Paragraph("4. Agentes", st_h1))
P("Los tres agentes usan exactamente el mismo entorno; la comparación es entre algoritmos. Las "
  "estructuras siguen las implementaciones de la materia (clase 5 y TP3 para DQN, clase 7 para A2C).")

story.append(Paragraph("4.1 DQN y Double DQN", st_h2))
P("DQN extiende Q-learning a estados continuos: reemplaza la tabla Q(s, a) por una red neuronal. La "
  "red tiene **3 entradas** (las variables del estado), **9 salidas** (un valor Q por acción, así una "
  "sola pasada da los nueve y se elige el mayor) y **dos capas ocultas de 64 neuronas** con ReLU, unos "
  "5 000 parámetros. Es el tamaño habitual para problemas de control con pocas variables (el usado en "
  "clase para CartPole); no se ajustó. El entrenamiento usa:")
vinetas(story, [
    "**Replay buffer**: una memoria con las últimas 50 000 transiciones (s, a, r, s', cayó). En cada paso "
    "se entrena con 64 transiciones elegidas al azar de esa memoria. Dos motivos: pasos consecutivos son "
    "casi iguales y entrenar con ellos en orden sesga a la red; y cada experiencia se reutiliza muchas veces.",
    "**Red objetivo**: una copia congelada de la red (se actualiza cada 1 000 pasos) para calcular el "
    "objetivo y. Si se usara la misma red, el blanco se movería con cada actualización y el aprendizaje oscilaría.",
    "**Exploración ε-greedy**: con probabilidad ε (de 1 a 0.02 en 50 000 pasos) elige una acción al azar.",
])
story.append(KeepTogether(mathimg(
    r"$\mathrm{DQN:}\;\; y = r + \gamma\,(1-d)\,\max_{a'} Q_{obj}(s',a')$",
    r"$\mathrm{Double\ DQN:}\;\; y = r + \gamma\,(1-d)\,Q_{obj}\left(s',\ \mathrm{argmax}_{a'}\,Q(s',a')\right)$")))
story.append(Spacer(1, 1.5 * mm))
P("La red se ajusta para que Q(s, a) se acerque a y (pérdida de Huber). El máximo de DQN tiende a "
  "**sobreestimar** Q, porque elige justamente las estimaciones con error positivo. Double DQN separa "
  "los roles: la red principal elige la acción y la red objetivo la evalúa. d vale 1 solo si el robot "
  "**cayó**: el corte a los 5 s no es un estado terminal y ahí se sigue estimando el futuro.")
story.append(codeblock(
    "def objetivo_td(red, red_objetivo, r, s2, d, gamma, double):\n"
    "    with torch.no_grad():\n"
    "        if double:\n"
    "            a2 = red(s2).argmax(dim=1, keepdim=True)        # la red principal elige\n"
    "            q_sig = red_objetivo(s2).gather(1, a2)          # la objetivo evalúa\n"
    "        else:\n"
    "            q_sig = red_objetivo(s2).max(dim=1, keepdim=True)[0]\n"
    "        return r + gamma * q_sig * (1.0 - d)"))

story.append(Paragraph("4.2 A2C (Advantage Actor-Critic)", st_h2))
P("A2C aprende la política directamente. Un **actor** π(a|s) da la probabilidad de cada acción y un "
  "**crítico** V(s) estima el retorno esperado; los dos son redes 3→64→64 (tanh), con 9 salidas el "
  "actor y 1 el crítico. Se juegan 16 pasos en 8 entornos en paralelo (en lugar de replay buffer, para "
  "no entrenar con datos correlacionados) y se calcula la **ventaja** A: cuánto mejor resultó la acción "
  "respecto de lo que el crítico esperaba, con GAE (λ = 0.95). La pérdida combina tres términos:")
story.append(mathimg(
    r"$L = -\,\overline{\log \pi(a|s)\,A} \;+\; 0.5\,\overline{(V(s) - G)^2} \;-\; 0.01\,\overline{H[\pi(\cdot|s)]}$"))
story.append(Spacer(1, 1.5 * mm))
P("El primero sube la probabilidad de las acciones con A > 0; el segundo ajusta al crítico contra el "
  "retorno G; el tercero (entropía) evita que la política deje de explorar demasiado pronto. "
  "Optimizador RMSprop, tasa de aprendizaje 7·10⁻⁴ con decaimiento lineal, 300 000 pasos.")
story.append(codeblock(
    "adv, ret = ventajas_gae(b_r, v.detach(), v_ult, b_nt, hp.gamma, hp.gae_lambda)\n"
    "loss_actor = -(dist.log_prob(b_act) * adv).mean()\n"
    "loss_critico = F.mse_loss(v, ret)\n"
    "loss = loss_actor + hp.vf_coef * loss_critico - hp.ent_coef * dist.entropy().mean()"))

# ------------------------------------------------------------ 5. resultados
story.append(Paragraph("5. Resultados", st_h1))
P(f"Cada agente se entrenó con **{N_SEM} semillas** (distinta inicialización de la red y distinto azar "
  "en la exploración). Las curvas muestran la media móvil de 50 episodios del retorno: la línea es el "
  "promedio entre semillas y la banda, ± un desvío estándar.")
figura(story, os.path.join(FIG, "convergencia_episodios.png"), 125,
       "Figura 3 — Convergencia: retorno en función de los episodios.")
figura(story, os.path.join(FIG, "convergencia_pasos.png"), 125,
       "Figura 4 — Convergencia: retorno en función de los pasos de entorno (comparación de eficiencia).")
P("A2C juega muchos más episodios (cortos, en 8 entornos), así que contra episodios aparece "
  "desplazado; **contra pasos de entorno** está la comparación justa de cuánta experiencia necesita "
  "cada algoritmo.")
P(f"Evaluación final **sin exploración**, en {N_EVAL} episodios nuevos por semilla. Media ± desvío "
  f"entre las {N_SEM} semillas; el PD es la referencia que no aprende:")
filas = [[n, ms(n, "retorno"), ms(n, "supervivencia") + " %", ms(n, "theta_medio_deg", ".2f") + "°",
          ms(n, "du_medio"), ms(n, "pasos_hasta_umbral", ".0f")]
         for n in ("DQN", "Double DQN", "A2C", "PD a mano")]
story.append(KeepTogether(table_md(["Agente", "Retorno", "Sobrevive", "|θ| medio", "|Δduty| medio",
                                    f"Pasos hasta retorno {R['umbral']}"], filas)))
story.append(Spacer(1, 2 * mm))
figura(story, os.path.join(FIG, "dispersion_semillas.png"), 118,
       "Figura 5 — Un punto por semilla: retorno final y experiencia necesaria. La raya es la media.")
figura(story, os.path.join(FIG, "trayectorias.png"), 112,
       "Figura 6 — Respuesta desde θ₀ = 3° (mejor semilla de cada agente): inclinación y duty aplicado.")
story.append(Paragraph("Discusión", st_h3))
vinetas(story, TEXTOS["discusion"])

# ------------------------------------------------------------ 6. inconvenientes
story.append(Paragraph("6. Inconvenientes encontrados y cómo se resolvieron", st_h1))
for titulo, texto in [
    ("Un parámetro físico sin medir",
     "El microcontrolador se dañó antes de medir el torque con el driver nuevo. Se reutilizó la "
     "medición hecha con el driver anterior, se acotó físicamente cuánto puede mejorar (1.0× a 1.6×) "
     "y se adoptó explícitamente 1.4× (sección 2.3), dejándolo como supuesto declarado."),
    ("Episodios imposibles de ganar",
     "Con estados iniciales al azar, algunos no se pueden salvar ni con todo el motor, y castigar al "
     "agente por eso solo agrega ruido. Se filtran con un criterio analítico: la combinación "
     "θ + (dθ/dt)/ω₀ (el modo que diverge) tiene que ser menor que θ_umbral."),
    ("Fin por tiempo no es caída",
     "Si el corte a los 5 s se tratara como estado terminal, el agente aprendería que al final del "
     "episodio \"no hay futuro\". DQN guarda solo la caída como terminal en el buffer; A2C suma "
     "γ·V(s_final) a la última recompensa del episodio truncado."),
    ("Oscilación residual cerca de 0°",
     "Como el nivel de motor más chico (duty 75) ya supera la zona muerta, no hay empujes finos y el "
     "robot queda pulsando cerca de la vertical (Figura 6). Se dejó así porque refleja el actuador "
     "real; la mejora natural es agregar niveles más finos o pasar a acción continua."),
    ("Variabilidad entre semillas",
     "Con pocas semillas, una diferencia entre algoritmos puede ser solo azar. Se pasó de 3 a "
     f"{N_SEM} semillas, corridas en paralelo, y se reporta media ± desvío."),
]:
    story.append(Paragraph(titulo, st_h3))
    P(texto)

# ------------------------------------------------------------ 7. conclusiones
story.append(Paragraph("7. Conclusiones y trabajo futuro", st_h1))
vinetas(story, TEXTOS["conclusiones"])

story.append(Paragraph("Referencias", st_h1))
for ref in [
    "Sutton, R. S. y Barto, A. G. (2018). *Reinforcement Learning: An Introduction* (2.ª ed.). MIT Press.",
    "Mnih, V. et al. (2015). Human-level control through deep reinforcement learning. *Nature*, 518, 529–533.",
    "van Hasselt, H., Guez, A. y Silver, D. (2016). Deep Reinforcement Learning with Double Q-learning. *AAAI*.",
    "Mnih, V. et al. (2016). Asynchronous Methods for Deep Reinforcement Learning. *ICML*.",
    "Schulman, J. et al. (2016). High-Dimensional Continuous Control Using Generalized Advantage Estimation. *ICLR*.",
    "Towers, M. et al. (2024). Gymnasium: A Standard Interface for Reinforcement Learning Environments. arXiv:2407.17032.",
    "Material de la materia Aprendizaje por Refuerzo I, FIUBA (2026): clases 5 (DQN) y 7 (A2C), TP3.",
]:
    story.append(Paragraph(md_inline(ref), st_li))

story.append(Spacer(1, 3 * mm))
story.append(HRFlowable(width="100%", thickness=0.6, color=RULE))
story.append(Spacer(1, 2 * mm))
COLAB = ("https://colab.research.google.com/github/piertotumlocomotor/robot_balance/blob/desafio1-rl/"
         "rl1_desafio1_cesaryepez.ipynb")
story.append(Paragraph(
    f'Código fuente: <link href="{REPO}" color="#185fa5">{REPO}</link><br/>'
    f'Notebook ejecutable en Google Colab: <link href="{COLAB}" color="#185fa5">{COLAB}</link>', st_caption))

if __name__ == "__main__":
    construir(story, "desafio1_robot_balanceador.pdf",
              "Balanceo de un robot de dos ruedas con aprendizaje por refuerzo")
