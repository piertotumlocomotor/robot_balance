# Cómo correr el notebook

```bash
cd notebooks
python3 -m venv .venv
source .venv/bin/activate        # Windows: .venv\Scripts\activate
pip install -r requirements.txt
jupyter lab pso_pendulo_invertido.ipynb
```

Corre de punta a punta (`Run > Run All Cells`) sin datos externos — todos los parámetros están
en el propio notebook, medidos sobre el robot real (ver la celda de historial de correcciones
al principio). El entrenamiento completo de PSO tarda unos 40 segundos.

Solo dos dependencias reales: `numpy` y `matplotlib`. `jupyterlab` e `ipykernel` son para poder
abrirlo y ejecutarlo de forma interactiva.
