"""
Figuras explicativas de los PDF (no son resultados del experimento):
    dcl.png               diagrama de cuerpo libre del robot
    bucle.png             el bucle agente-entorno con las variables de este problema
    gamma.png             cuánto pesa una recompensa futura según γ
    mapa_algoritmos.png   de Monte Carlo a DQN y A2C: qué problema resuelve cada paso
    transiciones.json     un episodio real de la DQN entrenada, paso a paso

Correr con el venv de aprendizaje_refuerzo (necesita torch y el modelo entrenado):
    ../.venv/bin/python figuras_explicativas.py
"""

import json
import os
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Arc, Circle, FancyArrowPatch, FancyBboxPatch

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(AQUI))
plt.rcParams.update({"font.size": 15, "mathtext.fontset": "cm"})
AZUL, ROJO, VERDE, GRIS, NARANJA = "#185fa5", "#993c1d", "#0f7a4a", "#6b7280", "#c2410c"


def guardar(fig, nombre):
    fig.savefig(os.path.join(AQUI, nombre), dpi=200, bbox_inches="tight", pad_inches=0.08)
    plt.close(fig)
    print("escrita", nombre)


def flecha(ax, a, b, color, **kw):
    ax.add_patch(FancyArrowPatch(a, b, arrowstyle="-|>", mutation_scale=22, lw=2.4, color=color, **kw))


def dcl():
    fig, ax = plt.subplots(figsize=(5.6, 6.2))
    th = np.radians(18)
    eje = np.array([0.0, 0.0])
    tope = eje + 3.2 * np.array([np.sin(th), np.cos(th)])
    cg = eje + 1.6 * np.array([np.sin(th), np.cos(th)])
    ax.plot([-2.2, 2.6], [-0.42, -0.42], color="black", lw=1.5)                     # piso
    ax.plot([0, 0], [-0.2, 3.5], ls="--", color=GRIS, lw=1.3)                        # vertical
    ax.add_patch(Circle(eje, 0.42, fill=False, lw=2.5, color="black"))               # rueda
    ax.plot(*zip(eje, tope), color=AZUL, lw=9, solid_capstyle="round")               # cuerpo
    ax.add_patch(Circle(cg, 0.09, color=VERDE, zorder=5))
    ax.text(cg[0] + 0.2, cg[1] + 0.05, "CG", color=VERDE, fontweight="bold")
    flecha(ax, cg, cg + [0, -1.25], ROJO)                                            # peso
    ax.text(cg[0] + 0.15, cg[1] - 1.15, r"$m\,g$", color=ROJO, fontsize=19)
    ax.add_patch(Arc(eje, 2.2, 2.2, theta1=90 - np.degrees(th), theta2=90, color=VERDE, lw=2))
    ax.text(0.08, 1.2, r"$\theta$", color=VERDE, fontsize=21)
    perp = 0.42 * np.array([-np.cos(th), np.sin(th)])                               # cota l, a la izquierda
    ax.annotate("", xy=cg + perp, xytext=eje + perp,
                arrowprops=dict(arrowstyle="<->", color=GRIS, lw=1.5))
    medio = (eje + cg) / 2 + 1.6 * perp
    ax.text(medio[0], medio[1], r"$l$", color=GRIS, fontsize=19)
    ax.add_patch(Arc(eje, 1.25, 1.25, theta1=200, theta2=340, color=NARANJA, lw=2.4))
    flecha(ax, (0.55, -0.24), (0.6, -0.15), NARANJA)
    ax.text(0.75, -0.95, r"$\tau_{motor}(u)$", color=NARANJA, fontsize=18)
    ax.text(-2.15, 3.15, "vertical", color=GRIS, fontsize=13)
    ax.text(-2.15, -1.25, "eje de las ruedas = pivote", color=GRIS, fontsize=13)
    ax.set_xlim(-2.3, 2.7); ax.set_ylim(-1.4, 3.7); ax.set_aspect("equal"); ax.axis("off")
    guardar(fig, "dcl.png")


def caja(ax, xy, w, h, texto, color, fs=14):
    ax.add_patch(FancyBboxPatch((xy[0] - w / 2, xy[1] - h / 2), w, h, boxstyle="round,pad=0.08",
                                fc="white", ec=color, lw=2.2))
    ax.text(*xy, texto, ha="center", va="center", fontsize=fs, color="#181c24")


def bucle():
    fig, ax = plt.subplots(figsize=(10, 4.2))
    caja(ax, (1.6, 2), 2.8, 1.5, "AGENTE\n(red neuronal)\nelige la acción", AZUL, 15)
    caja(ax, (9.4, 2), 3.6, 1.5, "ENTORNO\n(simulador del robot)\nθ̈ = ω₀²θ + K_U·k(|u|)·u", VERDE, 14)
    flecha(ax, (3.15, 2.45), (7.45, 2.45), AZUL)
    ax.text(5.3, 2.75, "acción  aₜ\nduty u ∈ {−121 … +121}", ha="center", fontsize=12.5, color=AZUL)
    flecha(ax, (7.45, 1.55), (3.15, 1.55), VERDE)
    ax.text(5.3, 0.6, "estado  sₜ₊₁ = (θ, dθ/dt, u anterior)\nrecompensa  rₜ₊₁", ha="center",
            fontsize=12.5, color=VERDE)
    ax.text(5.5, 3.75, "se repite cada 10 ms, hasta que el robot cae o pasan 5 s",
            ha="center", fontsize=12.5, color=GRIS)
    ax.set_xlim(0, 11.4); ax.set_ylim(0.2, 4); ax.axis("off")
    guardar(fig, "bucle.png")


def gamma():
    k = np.arange(0, 501)
    fig, ax = plt.subplots(figsize=(8, 3.6))
    for g, c in ((0.9, GRIS), (0.99, AZUL), (0.999, VERDE)):
        ax.plot(k, g ** k, color=c, lw=2.2, label=f"γ = {g}")
    for kk in (100, 500):
        ax.plot(kk, 0.99 ** kk, "o", color=AZUL)
        ax.annotate(f"0.99^{kk} = {0.99 ** kk:.3f}", (kk, 0.99 ** kk), xytext=(12, 14),
                    textcoords="offset points", fontsize=12, color=AZUL)
    ax.set_xlabel("pasos hacia el futuro  k   (1 paso = 10 ms)")
    ax.set_ylabel("peso  γᵏ")
    ax.grid(alpha=0.3); ax.legend(fontsize=12)
    guardar(fig, "gamma.png")


def mapa_algoritmos():
    fig, ax = plt.subplots(figsize=(12.5, 6.2))
    pos = {"MC": (2, 4.6), "TD": (7, 4.6), "DQN": (12, 4.6),
           "RF": (2, 1.4), "A2C": (7, 1.4), "DDQN": (12, 1.4)}
    textos = {
        "MC": "Monte Carlo (+ muestreo\npor importancia) · tabla\nespera el final del episodio",
        "TD": "SARSA / Q-learning\ntabla · aprende en cada paso\n(bootstrapping)",
        "DQN": "DQN\nQ-learning con red neuronal\nadmite estados continuos",
        "RF": "REINFORCE\naprende la política π directo\n(retornos de Monte Carlo)",
        "A2C": "A2C (actor-crítico)\nREINFORCE + crítico V(s)\ncon bootstrapping",
        "DDQN": "Double DQN\ncorrige la sobreestimación\ndel máximo",
    }
    colores = {"MC": GRIS, "TD": GRIS, "RF": GRIS, "DQN": AZUL, "DDQN": ROJO, "A2C": VERDE}
    for k in pos:
        caja(ax, pos[k], 3.7, 1.5, textos[k], colores[k], 12.5)
    rot = dict(fontsize=11.5, color=ROJO, style="italic", ha="center")
    flecha(ax, (3.95, 4.6), (5.05, 4.6), "#181c24"); ax.text(4.5, 5.65, "no esperar al\nfinal del episodio", **rot)
    flecha(ax, (8.95, 4.6), (10.05, 4.6), "#181c24"); ax.text(9.5, 5.65, "estado continuo:\nno entra en tabla", **rot)
    flecha(ax, (3.95, 1.4), (5.05, 1.4), "#181c24"); ax.text(4.5, 0.45, "ruidoso: un crítico\nbaja la varianza", va="top", **rot)
    flecha(ax, (2, 3.75), (2, 2.25), "#181c24"); ax.text(2.15, 2.85, "aprender la política,\nno los valores", **{**rot, "ha": "left"})
    flecha(ax, (12, 3.75), (12, 2.25), "#181c24"); ax.text(12.15, 2.85, "el máximo\nsobreestima", **{**rot, "ha": "left"})
    ax.text(7, 6.35, "Gris: visto antes en la materia (TP1, TP2)   ·   Color: usado en este trabajo",
            ha="center", fontsize=12, color=GRIS)
    ax.set_xlim(-0.1, 14.3); ax.set_ylim(-0.6, 6.7); ax.axis("off")
    guardar(fig, "mapa_algoritmos.png")


def transiciones(n_pasos=8):
    """Un episodio de la DQN entrenada desde θ₀ = 3°: estado, acción, recompensa y Q."""
    import torch
    from agentes import QNet
    from entorno import CAP, NIVELES_DUTY, OMEGA0, THETA_CAIDA, RobotBalanceadorEnv
    red = QNet(3, len(NIVELES_DUTY))
    red.load_state_dict(torch.load(os.path.join(AQUI, "..", "resultados", "modelos", "DQN_s0.pt")))
    env = RobotBalanceadorEnv()
    obs, _ = env.reset(seed=0, options={"theta0": np.radians(3.0)})
    filas = []
    for t in range(n_pasos):
        with torch.no_grad():
            q = red(torch.as_tensor(obs).unsqueeze(0))[0].numpy()
        a = int(q.argmax())
        theta, theta_dot = env.theta, env.theta_dot
        obs2, r, term, trunc, info = env.step(a)
        filas.append({"t_ms": t * 10, "theta_deg": float(np.degrees(theta)), "theta_dot": float(theta_dot),
                      "obs": [float(x) for x in obs], "duty": float(NIVELES_DUTY[a]), "r": float(r),
                      "q_elegida": float(q[a]), "q_duty0": float(q[4])})
        obs = obs2
    with open(os.path.join(AQUI, "transiciones.json"), "w") as fh:
        json.dump(filas, fh, indent=1)
    print("escrito transiciones.json")
    for f in filas:
        print(f)


if __name__ == "__main__":
    dcl(); bucle(); gamma(); mapa_algoritmos()
    if os.path.exists(os.path.join(AQUI, "..", "resultados", "modelos", "DQN_s0.pt")):
        transiciones()
