# Desafío 1 — Aprendizaje por Refuerzo I (FIUBA 2026)

Un agente aprende a mantener vertical un robot balanceador de dos ruedas, en un simulador
construido con los parámetros medidos del robot real. Se comparan **DQN**, **Double DQN** y **A2C**
sobre el mismo entorno.

[![Abrir en Colab](https://colab.research.google.com/assets/colab-badge.svg)](https://colab.research.google.com/github/piertotumlocomotor/robot_balance/blob/desafio1-rl/desafio1_profesor.ipynb)

## Archivos

| Archivo | Contenido |
|---|---|
| `desafio1_profesor.ipynb` | Notebook autocontenido: corre en Google Colab de punta a punta |
| `entorno.py` | Simulador (Gymnasium): física, acciones, recompensa, fin de episodio |
| `agentes.py` | DQN / Double DQN y A2C |
| `experimentos.py` | Entrena los 3 agentes con N semillas (en paralelo), evalúa y genera las figuras |
| `test_entorno.py` | Verificación del simulador antes de entrenar |

## Ejecutar

En Colab: botón de arriba → *Entorno de ejecución → Ejecutar todas*.

En local:

```bash
python -m venv .venv && .venv/bin/pip install -r requirements.txt
.venv/bin/python test_entorno.py
.venv/bin/python experimentos.py --rapido          # ≈ 4 min
.venv/bin/python experimentos.py --semillas 10     # experimento del informe
```

## Supuesto principal

El torque del motor con el driver nuevo no se pudo medir. Se usa el torque medido con el driver
anterior × **1.4** (rango físico 1.0–1.6) → `K_U ≈ 0.043`, ángulo máximo recuperable ≈ 4.7°.
Detalle en `entorno.py`, sección 1, y en el informe.
