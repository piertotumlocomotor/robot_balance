# Desafío 1 — Aprendizaje por Refuerzo I (FIUBA 2026)

Un agente aprende a mantener vertical el robot balanceador, en un simulador construido con los
parámetros medidos del robot real (**planta nueva**: 2× BTS7960 + chasis de balsa).
Se comparan **DQN**, **Double DQN** y **A2C** sobre el mismo entorno.

**Abrir en Google Colab** (no requiere instalar nada):
[![Informe ejecutable](https://colab.research.google.com/assets/colab-badge.svg)](https://colab.research.google.com/github/piertotumlocomotor/robot_balance/blob/desafio1-rl/desafio1_profesor.ipynb) `desafio1_profesor.ipynb` ·
[![Guía de estudio](https://colab.research.google.com/assets/colab-badge.svg)](https://colab.research.google.com/github/piertotumlocomotor/robot_balance/blob/develop/aprendizaje_refuerzo/guia_estudio.ipynb) `guia_estudio.ipynb`

## Qué leer, en orden

| Archivo | Para qué |
|---|---|
| `guia_estudio.ipynb` | **Para entender**: el modelo, el MDP y cada algoritmo, con celdas cortas para probar |
| `informe/guia_estudio.pdf` | **Para entender**, en PDF: de Monte Carlo a DQN y A2C, y el MDP del robot paso a paso |
| `informe/desafio1_robot_balanceador.pdf` | **El informe para subir al campus** |
| rama `desafio1-rl` | **Lo que se comparte con el profesor**: solo lo necesario para ejecutar el desafío |
| `desafio1_profesor.ipynb` | **Para entregar**: autocontenido, corre en Google Colab de punta a punta |
| `entorno.py` | El simulador (Gymnasium): física, acciones, recompensa, fin de episodio |
| `agentes.py` | DQN / Double DQN y A2C |
| `experimentos.py` | Entrena 3 agentes × 10 semillas (en paralelo), evalúa y guarda figuras |
| `test_entorno.py` | Verifica el simulador antes de entrenar |
| `construir_notebooks.py` | Regenera los dos notebooks desde los `.py` |
| `informe/` | Generadores de los PDF (`estilo.py`, `generar_informe.py`, `generar_guia.py`, `figuras_explicativas.py`) |
| `figuras/`, `resultados/` | Salidas del experimento |
| `_planta_vieja/` | ⛔ Prototipo con la planta vieja. No forma parte del desafío |

## Correr

```bash
python -m venv .venv && .venv/bin/pip install -r requirements.txt
.venv/bin/python test_entorno.py
.venv/bin/python experimentos.py            # 10 semillas ≈ 45 min con 8 núcleos; --rapido ≈ 4 min
.venv/bin/python construir_notebooks.py
cd informe && ../.venv/bin/python figuras_explicativas.py      # figuras de los PDF
../../.venv/bin/python generar_informe.py && ../../.venv/bin/python generar_guia.py   # reportlab
```

## El supuesto más importante

El torque del motor con el driver nuevo **no se midió** (el ESP32 se dañó antes del ensayo).
Se usa el torque medido con el driver viejo × **1.4** (supuesto medianamente optimista, rango
físico 1.0–1.6) → `K_U ≈ 0.043`, ángulo máximo recuperable ≈ 4.7°. Ver `entorno.py`, sección 1.
