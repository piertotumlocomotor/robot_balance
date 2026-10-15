"""
Experimento del Desafío 1: DQN vs Double DQN vs A2C sobre el robot balanceador.

Cada agente se entrena con N semillas (en paralelo, un proceso por corrida) y se
evalúa sin exploración.

Correr:   python experimentos.py                 (10 semillas, ≈ 45 min con 8 núcleos)
          python experimentos.py --semillas 3    (≈ 10 min)
          python experimentos.py --rapido        (≈ 3 min, para probar que todo anda)
Salida:   figuras/*.png, resultados/resultados.json, resultados/modelos/*.pt
"""

import argparse
import json
import os
from concurrent.futures import ProcessPoolExecutor, as_completed
from dataclasses import asdict, replace
from multiprocessing import get_context

import matplotlib
matplotlib.use("Agg")                         # solo se guardan PNG (también en Colab)
import matplotlib.pyplot as plt
import numpy as np
import torch

from agentes import (ActorCritico, HiperA2C, HiperDQN, QNet, entrenar_a2c, entrenar_dqn, evaluar,
                     media_movil, politica_a2c, politica_dqn, politica_pd, trayectoria)
from entorno import NIVELES_DUTY, RobotBalanceadorEnv

AQUI = os.path.dirname(os.path.abspath(__file__))
FIG, RES = os.path.join(AQUI, "figuras"), os.path.join(AQUI, "resultados")
COLOR = {"DQN": "tab:blue", "Double DQN": "tab:red", "A2C": "tab:green", "PD a mano": "gray"}
AGENTES = ("DQN", "Double DQN", "A2C")
VENTANA = 50          # episodios de la media móvil
UMBRAL = 450          # retorno que se considera "aprendido" para medir velocidad


def hiperparametros(rapido):
    hp_dqn = HiperDQN(pasos=40_000, eps_pasos_decaimiento=15_000) if rapido else HiperDQN()
    hp_a2c = HiperA2C(pasos=150_000) if rapido else HiperA2C()
    return hp_dqn, hp_a2c


def crear_modelo(nombre):
    n_obs, n_acc = 3, len(NIVELES_DUTY)
    return ActorCritico(n_obs, n_acc) if nombre == "A2C" else QNet(n_obs, n_acc)


def a_politica(nombre, modelo):
    return politica_a2c(modelo) if nombre == "A2C" else politica_dqn(modelo)


def correr(tarea):
    """Una corrida completa (entrenar + evaluar). Se ejecuta en un proceso aparte."""
    nombre, semilla, rapido, n_eval = tarea
    torch.set_num_threads(1)                  # un núcleo por proceso: el paralelismo es entre corridas
    hp_dqn, hp_a2c = hiperparametros(rapido)
    if nombre == "A2C":
        modelo, pasos, rets = entrenar_a2c(RobotBalanceadorEnv, hp_a2c, seed=semilla, verbose=False)
    else:
        hp = replace(hp_dqn, double=(nombre == "Double DQN"))
        modelo, pasos, rets = entrenar_dqn(RobotBalanceadorEnv, hp, seed=semilla, verbose=False)
    ev = evaluar(a_politica(nombre, modelo), RobotBalanceadorEnv, n=n_eval)
    mm = media_movil(rets, VENTANA)
    llega = np.nonzero(mm >= UMBRAL)[0]
    ev["pasos_hasta_umbral"] = float(pasos[VENTANA - 1 + llega[0]]) if len(llega) else float("nan")
    ev["episodios_entrenamiento"] = len(rets)
    estado = {k: v.cpu() for k, v in modelo.state_dict().items()}
    return nombre, semilla, pasos, rets, ev, estado


def graficar_convergencia(corridas, eje, archivo):
    """Media entre semillas (línea) ± 1 desvío estándar (banda) del retorno,
    suavizado con media móvil, contra episodios o contra pasos de entorno."""
    fig, ax = plt.subplots(figsize=(8, 4.5))
    for nombre, lista in corridas.items():
        curvas = []
        for pasos, rets in lista:
            mm = media_movil(rets, VENTANA)
            x = np.arange(VENTANA, len(rets) + 1) if eje == "episodios" else pasos[VENTANA - 1:]
            curvas.append((x, mm))
        grilla = np.linspace(max(x[0] for x, _ in curvas), min(x[-1] for x, _ in curvas), 300)
        ys = np.array([np.interp(grilla, x, y) for x, y in curvas])
        media, desvio = ys.mean(axis=0), ys.std(axis=0)
        ax.fill_between(grilla, media - desvio, np.minimum(media + desvio, 500), color=COLOR[nombre], alpha=0.2)
        ax.plot(grilla, media, color=COLOR[nombre], lw=2.2, label=f"{nombre} ({len(lista)} semillas)")
    ax.axhline(500, color="gray", ls=":", lw=1)
    ax.set_xlabel("Episodio" if eje == "episodios" else "Pasos de entorno (10 ms c/u)")
    ax.set_ylabel(f"Retorno por episodio (media móvil {VENTANA})")
    ax.set_ylim(0, 520); ax.grid(alpha=0.3); ax.legend(loc="lower right")
    ax.set_title(f"Convergencia — retorno vs {eje} (media ± desvío entre semillas)")
    plt.tight_layout(); plt.savefig(os.path.join(FIG, archivo), dpi=150); plt.close()


def graficar_semillas(evaluaciones, archivo):
    """Un punto por semilla: cuánto varía el resultado final según el azar."""
    fig, axs = plt.subplots(1, 2, figsize=(9, 3.6))
    for j, (clave, titulo) in enumerate((("retorno", "Retorno en evaluación"),
                                         ("pasos_hasta_umbral", f"Pasos hasta retorno {UMBRAL}"))):
        for i, nombre in enumerate(AGENTES):
            y = [e[clave] for e in evaluaciones[nombre]]
            x = i + np.random.default_rng(0).uniform(-0.12, 0.12, len(y))
            axs[j].scatter(x, y, color=COLOR[nombre], alpha=0.7, s=28)
            if np.isfinite(y).any():
                axs[j].hlines(np.nanmean(y), i - 0.25, i + 0.25, color="black", lw=1.5)
        axs[j].set_xticks(range(len(AGENTES)), AGENTES); axs[j].set_title(titulo); axs[j].grid(alpha=0.3, axis="y")
    plt.tight_layout(); plt.savefig(os.path.join(FIG, archivo), dpi=150); plt.close()


def main(n_semillas=10, rapido=False, procesos=None):
    os.makedirs(FIG, exist_ok=True)
    os.makedirs(os.path.join(RES, "modelos"), exist_ok=True)
    semillas = list(range(2 if rapido else n_semillas))
    n_eval = 30 if rapido else 200
    nucleos = os.cpu_count() or 2
    procesos = procesos or (nucleos if nucleos <= 4 else nucleos - 1)   # Colab: 2 núcleos, usar los dos

    # 1) Entrenar y evaluar, todas las corridas en paralelo ---------------------
    tareas = [(n, s, rapido, n_eval) for n in AGENTES for s in semillas]
    print(f"{len(tareas)} corridas en {procesos} procesos (cada DQN tarda unos minutos)...", flush=True)
    # El avance se imprime desde este proceso: lo que imprimen los procesos
    # auxiliares no aparece en la celda de Colab.
    with ProcessPoolExecutor(procesos, mp_context=get_context("spawn")) as pool:
        futuros = [pool.submit(correr, t) for t in tareas]
        for hechas, fut in enumerate(as_completed(futuros), 1):
            nombre, semilla, _, _, ev, _ = fut.result()
            print(f"  [{hechas:2d}/{len(tareas)}] {nombre:11s} semilla {semilla:2d}: retorno {ev['retorno']:6.1f}, "
                  f"sobrevive {ev['supervivencia']:5.1f}%", flush=True)
        salidas = [f.result() for f in futuros]               # en el orden original (agente, semilla)

    corridas = {n: [] for n in AGENTES}
    evaluaciones = {n: [] for n in AGENTES}
    estados = {n: [] for n in AGENTES}
    for nombre, semilla, pasos, rets, ev, estado in salidas:      # ya vienen en orden de semilla
        corridas[nombre].append((pasos, rets))
        evaluaciones[nombre].append(ev)
        estados[nombre].append(estado)
        torch.save(estado, os.path.join(RES, "modelos", f"{nombre.replace(' ', '_')}_s{semilla}.pt"))

    politicas = {"PD a mano": politica_pd(RobotBalanceadorEnv())}
    for nombre in AGENTES:            # para graficar trayectorias: la mejor semilla de cada agente
        mejor = int(np.argmax([e["retorno"] for e in evaluaciones[nombre]]))
        modelo = crear_modelo(nombre); modelo.load_state_dict(estados[nombre][mejor])
        politicas[nombre] = a_politica(nombre, modelo)
    evaluaciones["PD a mano"] = [evaluar(politicas["PD a mano"], RobotBalanceadorEnv, n=n_eval)]

    # 2) Tabla: media ± desvío entre semillas ----------------------------------
    print(f"\nEvaluación sin exploración ({n_eval} episodios nuevos por semilla), "
          f"media ± desvío entre {len(semillas)} semillas:")
    tabla = {}
    for nombre, evs in evaluaciones.items():
        fila = {}
        for k in ("retorno", "supervivencia", "theta_medio_deg", "du_medio", "pasos_hasta_umbral"):
            vals = [e[k] for e in evs if k in e and np.isfinite(e[k])]
            fila[k] = (float(np.mean(vals)), float(np.std(vals))) if vals else None
        tabla[nombre] = fila
        f = lambda k, fmt: "—" if fila[k] is None else f"{fila[k][0]:{fmt}} ± {fila[k][1]:{fmt}}"
        print(f"  {nombre:11s} retorno {f('retorno', '.1f'):>14s} | sobrevive {f('supervivencia', '.1f'):>12s}% | "
              f"|θ| {f('theta_medio_deg', '.2f'):>12s}° | |Δduty| {f('du_medio', '.1f'):>12s} | "
              f"pasos hasta {UMBRAL}: {f('pasos_hasta_umbral', '.0f')}")

    # 3) Figuras ----------------------------------------------------------------
    graficar_convergencia(corridas, "episodios", "convergencia_episodios.png")
    graficar_convergencia(corridas, "pasos", "convergencia_pasos.png")
    graficar_semillas(evaluaciones, "dispersion_semillas.png")

    fig, axs = plt.subplots(2, 1, figsize=(9, 6), sharex=True)
    for nombre, pi in politicas.items():
        t, th, u = trayectoria(pi, RobotBalanceadorEnv, theta0_deg=3.0)
        axs[0].plot(t, th, color=COLOR[nombre], label=nombre, lw=1.3)
        axs[1].step(t, u, where="post", color=COLOR[nombre], lw=0.8)
    axs[0].set_ylabel("θ [°]"); axs[1].set_ylabel("duty"); axs[1].set_xlabel("t [s]")
    axs[0].legend(fontsize=8)
    for ax in axs:
        ax.grid(alpha=0.3)
    fig.suptitle("Respuesta desde θ₀ = 3° (mejor semilla de cada agente)")
    plt.tight_layout(); plt.savefig(os.path.join(FIG, "trayectorias.png"), dpi=150); plt.close()

    nombre_json = "resultados_rapido.json" if rapido else "resultados.json"
    with open(os.path.join(RES, nombre_json), "w") as fh:
        json.dump({"semillas": semillas, "n_eval": n_eval, "umbral": UMBRAL,
                   "hp_dqn": asdict(hiperparametros(rapido)[0]), "hp_a2c": asdict(hiperparametros(rapido)[1]),
                   "evaluacion": tabla, "por_semilla": evaluaciones}, fh, indent=1, ensure_ascii=False)
    print("\nFiguras en", FIG)


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--semillas", type=int, default=10)
    ap.add_argument("--rapido", action="store_true")
    ap.add_argument("--procesos", type=int, default=None)
    a = ap.parse_args()
    main(a.semillas, a.rapido, a.procesos)
    # En macOS el proceso auxiliar de multiprocessing a veces no termina y deja
    # colgado el cierre del intérprete; todo ya está guardado, se sale directo.
    os._exit(0)
