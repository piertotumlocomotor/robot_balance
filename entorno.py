"""
Entorno de simulación del robot balanceador (planta nueva: 2× BTS7960 + chasis de balsa).

Modelo: péndulo invertido linealizado de 1 grado de libertad.

    J·θ̈ = m·g·l·θ + τ_motor(u)      →      θ̈ = ω₀²·θ + K_U·k(|u|)·u

    θ    inclinación del cuerpo [rad], 0 = vertical
    u    duty de PWM común a los dos motores (−121 … +121)
    k()  fracción del torque disponible según la zona muerta de cada motor

Lo que el modelo NO incluye: posición/velocidad de las ruedas, fuerza
contraelectromotriz, juego de la reductora, dinámica eléctrica del motor.
"""

import gymnasium as gym
import numpy as np
from gymnasium import spaces

# ---------------------------------------------------------------------------
# 1. Parámetros físicos
# ---------------------------------------------------------------------------
# Medidos en la planta nueva (registro de pruebas §7.22 y §7.23)
OMEGA0 = 8.0            # rad/s   frecuencia natural (video del robot colgado)
J = 0.0159              # kg·m²   momento de inercia respecto del eje
M_L = 0.1040            # kg·m    masa × distancia al centro de masa (dos apoyos)

# NO medido: el torque del motor con el driver nuevo (el ESP32 se quemó antes).
# Se parte del torque medido con balanza en la planta vieja, dos motores en
# brake a la vez (§7.20): τ = 0.0443 + 0.0339 N·m a duty 160 → 4.89e-4 N·m/duty.
# El BTS7960 (MOSFET) no tiene la caída de ~3 V del L298N, así que entrega más
# torque al mismo duty: entre 1.0× (pesimista) y 1.6× (optimista).
# SUPUESTO del proyecto: 1.4×, medianamente optimista.
FACTOR_DRIVER = 1.4
TORQUE_POR_DUTY = FACTOR_DRIVER * (0.0443 + 0.0339) / 160      # N·m por unidad de duty
K_U = TORQUE_POR_DUTY / J                                      # ≈ 0.043 rad/s² por unidad de duty

# Zona muerta: duty por debajo del cual el motor no hace fuerza. Medida en la
# planta vieja (92 y 102); como es un umbral de tensión, se escala con el driver.
ZONA_MUERTA_M1 = 92 / FACTOR_DRIVER     # ≈ 66
ZONA_MUERTA_M2 = 102 / FACTOR_DRIVER    # ≈ 73
PROP_M1 = 0.0443 / (0.0443 + 0.0339)    # M1 aporta el 57% del torque (M2 es más débil)
CAP = 121               # duty máximo: 6 V del motor / 12.6 V de batería llena (§8.5)

# Ángulo máximo recuperable con el torque disponible: θ_umbral = K_U·CAP/ω₀² ≈ 4.7°
THETA_UMBRAL = K_U * CAP / OMEGA0**2

# ---------------------------------------------------------------------------
# 2. Definición del MDP
# ---------------------------------------------------------------------------
DT = 0.01               # paso de control: 100 Hz
SUBPASOS = 10           # la física se integra en pasos de 1 ms
DURACION = 5.0          # s por episodio → 500 pasos
THETA_CAIDA = np.radians(10.0)      # más inclinado que esto = robot caído
THETA0_MAX = np.radians(3.06)       # error de colocación a mano medido (Test 10)
THETA_DOT0_MAX = 0.15               # rad/s, ídem

# Acciones: 9 niveles de duty. Todos los no nulos superan la zona muerta.
NIVELES_DUTY = np.array([-121, -105, -90, -75, 0, 75, 90, 105, 121], dtype=np.float64)


class RobotBalanceadorEnv(gym.Env):
    """
    Estado observado (normalizado):  [θ/θ_caída,  θ̇/(ω₀·θ_caída),  u_anterior/CAP]
    Acción:      índice 0..8 de NIVELES_DUTY
    Recompensa:  1 − (θ/θ_caída)² − 0.05·(u/CAP)² − 0.05·(Δu/2CAP)²   por paso; 0 si cae
    Fin:         terminated si |θ| > 10°;  truncated a los 5 s
    """

    def __init__(self, peso_u=0.05, peso_du=0.05):
        super().__init__()
        self.peso_u, self.peso_du = peso_u, peso_du
        self.niveles = NIVELES_DUTY / CAP                 # en fracción del cap
        self.max_pasos = int(round(DURACION / DT))
        self.action_space = spaces.Discrete(len(NIVELES_DUTY))
        self.observation_space = spaces.Box(-10.0, 10.0, shape=(3,), dtype=np.float32)

    # -- física ------------------------------------------------------------
    @staticmethod
    def torque_disponible(u_abs):
        """Fracción de K_U según cuántos motores superaron su zona muerta."""
        if u_abs < ZONA_MUERTA_M1:
            return 0.0
        if u_abs < ZONA_MUERTA_M2:
            return PROP_M1
        return 1.0

    def _integrar(self, u):
        aceleracion_motor = K_U * self.torque_disponible(abs(u)) * u
        dt = DT / SUBPASOS
        for _ in range(SUBPASOS):                     # Euler semi-implícito
            theta_ddot = OMEGA0**2 * self.theta + aceleracion_motor
            self.theta_dot += theta_ddot * dt
            self.theta += self.theta_dot * dt

    @staticmethod
    def es_recuperable(theta, theta_dot):
        """El modo inestable del péndulo es x = θ + θ̇/ω₀. Con el torque máximo
        solo se puede frenar si |x| < θ_umbral (cota optimista: ignora la zona muerta)."""
        return abs(theta + theta_dot / OMEGA0) < THETA_UMBRAL

    # -- interfaz Gymnasium -------------------------------------------------
    def _obs(self):
        return np.clip(np.array([self.theta / THETA_CAIDA,
                                 self.theta_dot / (OMEGA0 * THETA_CAIDA),
                                 self.u_anterior / CAP], dtype=np.float32), -10, 10)

    def reset(self, *, seed=None, options=None):
        super().reset(seed=seed)
        options = options or {}
        if "theta0" in options:                       # condición inicial fija (para graficar)
            self.theta = float(options["theta0"])
            self.theta_dot = float(options.get("theta_dot0", 0.0))
        else:                                         # sorteo: como colocarlo a mano
            while True:
                self.theta = self.np_random.uniform(-THETA0_MAX, THETA0_MAX)
                self.theta_dot = self.np_random.uniform(-THETA_DOT0_MAX, THETA_DOT0_MAX)
                if self.es_recuperable(self.theta, self.theta_dot):
                    break
        self.paso, self.u_anterior = 0, 0.0
        return self._obs(), {}

    def step(self, accion):
        u = NIVELES_DUTY[int(accion)]
        self._integrar(u)
        self.paso += 1

        recompensa = (1.0
                      - (self.theta / THETA_CAIDA) ** 2                       # estar vertical
                      - self.peso_u * (u / CAP) ** 2                          # gastar poca energía
                      - self.peso_du * ((u - self.u_anterior) / (2 * CAP)) ** 2)  # no "vibrar"
        self.u_anterior = u

        terminated = bool(abs(self.theta) > THETA_CAIDA)
        if terminated:
            recompensa = 0.0
        truncated = self.paso >= self.max_pasos
        info = {"theta": self.theta, "theta_dot": self.theta_dot, "u": u}
        return self._obs(), float(recompensa), terminated, truncated, info
