# ⛔ Planta VIEJA (L298N + chasis de contrachapado) — solo referencia

Primer prototipo del entorno y del DQN, hecho con los parámetros de la planta vieja
(ω₀ = 7.4 rad/s, K_U = 0.0218, cap 160). **No es parte del Desafío 1**: el proyecto
usa solo la planta nueva (carpeta de arriba).

De acá se rescató **una sola cosa**: el torque por unidad de duty medido con balanza
(registro §7.20), que es el punto de partida para estimar K_U de la planta nueva.

Es autocontenido: `python dqn_pendulo.py` usa el `entorno_pendulo.py` de esta carpeta.
