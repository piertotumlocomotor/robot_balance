"""
Genera los dos notebooks a partir de los .py (el código vive en un solo lugar).

    rl1_desafio1_cesaryepez.ipynb   versión de entrega, autocontenida para Colab
    guia_estudio.ipynb        versión para entender los algoritmos, paso a paso

Correr:  python construir_notebooks.py
"""

import json
import os

AQUI = os.path.dirname(os.path.abspath(__file__))
# La entrega vive en una rama propia (solo lo necesario, en la raíz); la guía, en develop.
RAMA_DESAFIO = "desafio1-rl"
REPO = f"https://github.com/piertotumlocomotor/robot_balance/tree/{RAMA_DESAFIO}"
COLAB = "https://colab.research.google.com/github/piertotumlocomotor/robot_balance/blob/"
RUTA_COLAB = {"rl1_desafio1_cesaryepez.ipynb": f"{RAMA_DESAFIO}/",
              "guia_estudio.ipynb": "develop/aprendizaje_refuerzo/"}


def boton_colab(nb):
    return f"[![Abrir en Colab](https://colab.research.google.com/assets/colab-badge.svg)]({COLAB}{RUTA_COLAB[nb]}{nb})"


_contador = [0]


def _nuevo_id():
    _contador[0] += 1
    return f"celda-{_contador[0]:03d}"


def md(texto):
    return {"cell_type": "markdown", "id": _nuevo_id(), "metadata": {}, "source": texto.strip("\n")}


def code(texto, oculto=False):
    meta = {"cellView": "form"} if oculto else {}
    return {"cell_type": "code", "id": _nuevo_id(), "metadata": meta, "execution_count": None, "outputs": [],
            "source": texto.strip("\n")}


def archivo(nombre, oculto=False, titulo=None):
    """Celda que escribe un .py del proyecto en el disco de Colab. Visible:
    %%writefile con el código legible. Oculta: formulario de Colab que lo escribe."""
    with open(os.path.join(AQUI, nombre)) as fh:
        contenido = fh.read()
    if not oculto:
        return code(f"%%writefile {nombre}\n{contenido}")
    return code(f'#@title {titulo or nombre} {{ display-mode: "form" }}\n'
                f"open({nombre!r}, 'w').write({contenido!r})\nprint('escrito', {nombre!r})", oculto=True)


def guardar(nombre, celdas):
    nb = {"cells": celdas, "nbformat": 4, "nbformat_minor": 5,
          "metadata": {"kernelspec": {"name": "python3", "display_name": "Python 3"},
                       "language_info": {"name": "python"}, "colab": {"provenance": []}}}
    with open(os.path.join(AQUI, nombre), "w") as fh:
        json.dump(nb, fh, indent=1, ensure_ascii=False)
    print("escrito", nombre)


INSTALAR = code("%pip install -q gymnasium")

MODELO_MD = r"""
## El modelo del robot

Péndulo invertido sobre dos ruedas, linealizado alrededor de la vertical:

$$J\,\ddot\theta = m g l\,\theta + \tau_{motor}(u) \quad\Longrightarrow\quad \ddot\theta = \omega_0^2\,\theta + K_U\,k(|u|)\,u$$

| Parámetro | Valor | Origen |
|---|---|---|
| $\omega_0$ | 8.0 rad/s | medido: video del robot colgado del eje (registro §7.22) |
| $J$ | 0.0159 kg·m² | medido: dos apoyos + $J = mgl/\omega_0^2$ (§7.23) |
| $m\,l$ | 0.1040 kg·m | medido: dos apoyos sobre balanza (§7.23) |
| $K_U$ | **0.043** rad/s² por unidad de duty | **supuesto** (ver abajo) |
| zona muerta M1 / M2 | 66 / 73 de duty | medida con el driver viejo, escalada igual que $K_U$ |
| tope de duty | 121 | 6 V del motor / 12.6 V de batería |

**Por qué $K_U$ es un supuesto.** El torque del motor con el driver nuevo (BTS7960) no se llegó a medir:
el ESP32 se dañó antes del ensayo. Se parte del torque medido con balanza usando el driver anterior
(L298N, dos motores a la vez): $4.89\times10^{-4}$ N·m por unidad de duty. El BTS7960 no tiene la caída de
~3 V del L298N, por lo que entrega entre 1.0× y 1.6× ese torque. Se adopta **1.4×** (medianamente optimista):

$$K_U = \frac{1.4 \times 4.89\times10^{-4}}{0.0159} \approx 0.043 \qquad\Rightarrow\qquad \theta_{umbral} = \frac{K_U \cdot 121}{\omega_0^2} \approx 4.7^\circ$$

$\theta_{umbral}$ es el máximo ángulo del que el robot puede recuperarse con todo el torque disponible.
"""

MDP_MD = r"""
## El problema como MDP

| Elemento | Definición |
|---|---|
| Estado $s$ | $(\theta,\ \dot\theta,\ u_{anterior})$, normalizados |
| Acción $a$ | 9 niveles de duty: −121, −105, −90, −75, 0, +75, +90, +105, +121 |
| Transición | la ecuación de arriba integrada a 1 ms; el agente decide cada 10 ms (100 Hz) |
| Recompensa | $r = 1 - (\theta/10^\circ)^2 - 0.05\,(u/121)^2 - 0.05\,(\Delta u/242)^2$ por paso |
| Fin del episodio | cae ($\lvert\theta\rvert > 10^\circ$, *terminated*) o pasan 5 s (*truncated*) |
| Estado inicial | $\lvert\theta_0\rvert \le 3.06^\circ$, $\lvert\dot\theta_0\rvert \le 0.15$ rad/s (error de colocación a mano medido) |
| Descuento | $\gamma = 0.99$ (horizonte ≈ 100 pasos = 1 s) |

El retorno máximo es ≈ 500 (500 pasos con recompensa ≈ 1). Un episodio en que el robot cae
rápido suma ≈ 30.
"""

REFERENCIAS_MD = r"""
## Referencias

1. Sutton, R. S. y Barto, A. G. (2018). *Reinforcement Learning: An Introduction* (2.ª ed.). MIT Press. Cap. 6 (Q-learning), 9 (aproximación de funciones) y 13 (actor-crítico).
2. Mnih, V. et al. (2015). Human-level control through deep reinforcement learning. *Nature* 518, 529–533. (DQN)
3. van Hasselt, H., Guez, A. y Silver, D. (2016). Deep Reinforcement Learning with Double Q-learning. *AAAI*. (Double DQN)
4. Mnih, V. et al. (2016). Asynchronous Methods for Deep Reinforcement Learning. *ICML*. (A3C/A2C)
5. Schulman, J. et al. (2016). High-Dimensional Continuous Control Using Generalized Advantage Estimation. *ICLR*. (GAE)
6. Towers, M. et al. (2024). Gymnasium: A Standard Interface for Reinforcement Learning Environments. arXiv:2407.17032.
7. Material de la materia (FIUBA, 2026): clase 5 (DQN), clase 7 (A2C), TP3.
8. Registro de pruebas del robot: `docs/conexiones-registro-pruebas.md` del repositorio (§7.20, §7.22, §7.23, §8.5).
"""


def profesor():
    resultados_md = os.path.join(AQUI, "resultados", "resultados.md")
    resultados = open(resultados_md).read() if os.path.exists(resultados_md) else \
        "_(Correr `experimentos.py` y `construir_notebooks.py` para completar esta sección.)_"
    return [
        md(f"""
{boton_colab("rl1_desafio1_cesaryepez.ipynb")}

# Desafío 1 — Robot balanceador con aprendizaje por refuerzo
**Aprendizaje por Refuerzo I — MIA, FIUBA, 2026** · Repositorio: {REPO}

**Problema.** Un robot de dos ruedas (péndulo invertido) tiene que mantenerse vertical moviendo sus
motores. Se construyó un simulador con los parámetros medidos del robot real y se entrenaron tres
agentes: **DQN**, **Double DQN** y **A2C**, comparados sobre el mismo entorno.

**Cómo correrlo.** *Entorno de ejecución → Ejecutar todas*, con entorno de ejecución **CPU** (la GPU
no acelera estas redes chicas). Con `RAPIDO = True` tarda ≈ 10 min en Colab; con `False` entrena
como en el informe (las semillas se reparten entre los núcleos disponibles).
"""),
        INSTALAR,
        md(MODELO_MD), md(MDP_MD),
        md("### Código del entorno (`entorno.py`)"), archivo("entorno.py"),
        md("Verificación del entorno: API de Gymnasium, caída libre contra la solución analítica, "
           "y que el problema sea resoluble (PD a mano) pero no trivial (política al azar)."),
        archivo("test_entorno.py", oculto=True), code("%run test_entorno.py"),
        md("""
### Agentes (`agentes.py`)
- **DQN**: red $Q(s,a)$, *replay buffer*, red objetivo, ε-greedy, pérdida de Huber.
- **Double DQN**: igual, pero la red principal **elige** la acción de $s'$ y la red objetivo la **evalúa** (función `objetivo_td`).
- **A2C**: actor (política) y crítico ($V(s)$), 8 entornos en paralelo, retornos de 16 pasos con GAE(λ=0.95).
"""),
        archivo("agentes.py"),
        md("### Experimento: 3 agentes × N semillas, en paralelo"), archivo("experimentos.py", oculto=True),
        code("""
RAPIDO = True      # True: 2 semillas cortas (≈ 10 min en Colab). False: entrenamiento completo
N_SEMILLAS = 3     # solo si RAPIDO = False. El informe usa 10 (≈ 45 min con 8 núcleos; en Colab, varias horas)
import experimentos
experimentos.main(n_semillas=N_SEMILLAS, rapido=RAPIDO)
"""),
        code("""
from IPython.display import Image, display
for f in ["convergencia_episodios", "convergencia_pasos", "dispersion_semillas", "trayectorias"]:
    display(Image(f"figuras/{f}.png", width=720))
"""),
        md("## Resultados y discusión\n\n" + resultados),
        md(REFERENCIAS_MD),
    ]


def estudio():
    return [
        md(boton_colab("guia_estudio.ipynb") + r"""

# Guía de estudio — cómo aprende el robot a balancearse

Versión para **entender**, no para entregar. Cada sección es corta y tiene una celda para tocar.
La primera celda prepara los archivos (no hace falta leerla).
"""),
        INSTALAR,
        archivo("entorno.py", oculto=True, titulo="Preparar entorno.py"),
        archivo("agentes.py", oculto=True, titulo="Preparar agentes.py"),
        md(r"""
## 1. El problema en una frase
El robot es un **péndulo invertido**: si está un poco inclinado, la gravedad lo inclina más (es inestable).
La única forma de sostenerlo es mover las ruedas hacia el lado al que cae. Hay que decidir, 100 veces
por segundo, **cuánto y hacia dónde** empujar.

Nadie le dice al agente la ecuación. Solo prueba acciones, recibe una recompensa y aprende.
"""),
        md(MODELO_MD + r"""

**Lectura intuitiva:** $\omega_0^2\theta$ es la gravedad (empuja a caer, más cuanto más inclinado);
$K_U u$ es el motor (empuja de vuelta). Si el robot pasa de ~4.7°, ni con todo el motor se salva.
"""),
        code(r"""
import numpy as np
from entorno import RobotBalanceadorEnv, K_U, THETA_UMBRAL
print(f"K_U = {K_U:.4f}   θ_umbral = {np.degrees(THETA_UMBRAL):.2f}°")

# Sin motor: ¿cuánto tarda en caer desde 1°?
env = RobotBalanceadorEnv()
env.reset(options={"theta0": np.radians(1.0)})
for paso in range(1, 501):
    _, r, cayo, _, info = env.step(4)          # acción 4 = duty 0
    if cayo: break
print(f"Sin hacer nada cae en {paso*10} ms")
"""),
        md(MDP_MD + r"""

**Por qué la recompensa tiene esa forma:** el `1` premia seguir de pie; $-(\theta/10°)^2$ premia estar
derecho; los dos últimos términos castigan gastar motor y cambiar bruscamente (en el robot real
eso calienta el driver y hace vibrar el chasis).

**Por qué el estado incluye $u_{anterior}$:** la recompensa depende de $\Delta u$; sin él, el agente
no podría saber cuánto le va a costar cambiar de acción (el estado no sería *markoviano*).
"""),
        code(r"""
# Jugá un episodio con una política "a mano" (PD) y uno al azar
from agentes import politica_pd, evaluar
pd = politica_pd(env)
azar = lambda obs: env.action_space.sample()
for nombre, pi in [("PD a mano", pd), ("al azar", azar)]:
    ev = evaluar(pi, RobotBalanceadorEnv, n=20)
    print(f"{nombre:10s} retorno {ev['retorno']:6.1f}   sobrevive {ev['supervivencia']:5.1f}%")
"""),
        md(r"""
## 2. De Q-learning a DQN

En el TP2 (Q-learning) había una **tabla** $Q(s,a)$ y se actualizaba con

$$Q(s,a) \leftarrow Q(s,a) + \alpha\,[\,r + \gamma \max_{a'} Q(s',a') - Q(s,a)\,]$$

Acá el estado es **continuo** ($\theta$ puede valer 1.2345°): no entra en una tabla.
**DQN** reemplaza la tabla por una **red neuronal** que recibe $s$ (3 números) y devuelve $Q(s,a)$ para
las 9 acciones. La idea de la actualización es la misma; se agregan tres trucos para que sea estable:

| Truco | Qué hace | Por qué |
|---|---|---|
| ε-greedy | explora al azar con probabilidad ε (1 → 0.02) | sin explorar no descubre qué funciona |
| *Replay buffer* | guarda transiciones y entrena con lotes al azar | pasos seguidos se parecen mucho; mezclar evita sesgos |
| Red objetivo | copia congelada de la red para calcular $r+\gamma \max Q(s',a')$ | si el blanco se mueve en cada paso, el aprendizaje oscila |

En el código: `entrenar_dqn` en `agentes.py`. El corazón es `objetivo_td`.
"""),
        md(r"""
## 3. Double DQN: una línea de diferencia

El $\max$ de DQN **sobreestima** $Q$: si algunas estimaciones tienen error para arriba, el máximo las elige.
Double DQN separa los dos roles:

- DQN: $\;y = r + \gamma\, Q_{obj}(s', \arg\max_{a'} Q_{obj}(s',a'))$ — la misma red elige y evalúa
- Double DQN: $\;y = r + \gamma\, Q_{obj}(s', \arg\max_{a'} Q_{red}(s',a'))$ — una elige, la otra evalúa
"""),
        code(r"""
import torch
from agentes import QNet, objetivo_td
torch.manual_seed(0)
red, red_obj = QNet(3, 9), QNet(3, 9)                 # dos redes sin entrenar
s2 = torch.randn(5, 3); r = torch.ones(5, 1); d = torch.zeros(5, 1)
print("objetivo DQN       :", objetivo_td(red, red_obj, r, s2, d, 0.99, double=False).squeeze().numpy().round(3))
print("objetivo Double DQN:", objetivo_td(red, red_obj, r, s2, d, 0.99, double=True).squeeze().numpy().round(3))
print("→ el de Double DQN nunca es mayor: evalúa con red_obj una acción que no eligió red_obj")
"""),
        md(r"""
## 4. A2C: aprender la política directamente

DQN aprende **valores** y la política sale de elegir el máximo. A2C aprende **dos cosas a la vez**:

- **Actor** $\pi(a|s)$: la probabilidad de cada acción. Es la política.
- **Crítico** $V(s)$: cuánto retorno espera desde $s$.

Después de probar acciones, calcula la **ventaja** $A = (\text{retorno obtenido}) - V(s)$:
"¿me fue mejor o peor de lo que esperaba?". Si $A>0$, sube la probabilidad de esa acción; si $A<0$, la baja.

| | DQN / Double DQN | A2C |
|---|---|---|
| Aprende | $Q(s,a)$ | $\pi(a\mid s)$ y $V(s)$ |
| Datos | *off-policy*: reutiliza experiencia vieja (replay) | *on-policy*: usa solo lo recién jugado |
| Explora con | ε-greedy | el azar propio de $\pi$ (+ bonus de entropía) |
| Para decorrelacionar | replay buffer | 8 entornos en paralelo |

En el código: `entrenar_a2c` y `ventajas_gae` en `agentes.py` (GAE es una forma suavizada de calcular $A$).
"""),
        code(r"""
# Entrenamiento corto de A2C (≈ 20 s) para verlo aprender
from agentes import entrenar_a2c, HiperA2C, politica_a2c
modelo, pasos, retornos = entrenar_a2c(RobotBalanceadorEnv, HiperA2C(pasos=150_000), seed=0)
print(evaluar(politica_a2c(modelo), RobotBalanceadorEnv, n=20))
"""),
        md(r"""
## 5. Cómo leer los resultados

- **Curva de convergencia**: retorno por episodio (media móvil) a lo largo del entrenamiento.
  Empieza en ≈ 30 (cae enseguida) y sube hacia 500 (se sostiene los 5 s).
- **Contra episodios vs contra pasos**: A2C juega muchos episodios cortos al principio (8 entornos), así que
  "por episodio" lo favorece visualmente. **Contra pasos** es la comparación justa de eficiencia.
- **Evaluación sin exploración**: lo que importa al final; se mide en episodios nuevos, con ε = 0 (DQN)
  o eligiendo la acción más probable (A2C).
- **Varias semillas**: el mismo algoritmo puede salir mejor o peor según el azar. Por eso 3 semillas.
- **Límite del modelo**: todo esto vale para el robot *simulado* con $K_U$ supuesto. En el robot real
  faltaría validar $K_U$ y considerar ruido del sensor, demora y las ruedas.
"""),
        md(REFERENCIAS_MD),
    ]


if __name__ == "__main__":
    guardar("rl1_desafio1_cesaryepez.ipynb", profesor())
    guardar("guia_estudio.ipynb", estudio())
