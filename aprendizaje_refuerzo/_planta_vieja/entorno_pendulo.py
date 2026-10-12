"""
Entorno de Gymnasium del robot balanceador: modelo identificado a partir de
parámetros medidos en el robot real.

Dinámica (péndulo invertido linealizado, 1 GDL — la misma del notebook de PSO):

    θ̈ = ω₀²·θ + K_U · k(|u|) · u

    θ    inclinación del cuerpo [rad], 0 = vertical
    u    duty de PWM común a los dos motores, saturado a ±cap
    k()  fracción de K_U disponible según cuántos motores superaron su zona
         muerta: 0 debajo de la de M1, PROP_M1 entre M1 y M2, 1 arriba de M2
         (asimetría M1/M2 medida con balanza)

Convención de signo: u > 0 acelera θ hacia positivo. El agente tiene que
aprender a empujar CONTRA la inclinación.

Lo que este modelo NO tiene (declararlo en el informe): ruedas (posición y
velocidad de rueda), fuerza contraelectromotriz (el torque cae con la
velocidad de la rueda), backlash de la reductora, dinámica eléctrica del
motor. Ruido del IMU y latencia del lazo están como opciones.

Uso:
    env = PenduloBalanceadorEnv(planta=PLANTA_VIEJA, acciones="discretas")
    obs, info = env.reset(seed=0)
    obs, r, terminated, truncated, info = env.step(env.action_space.sample())
"""

from collections import deque
from dataclasses import dataclass, replace

import gymnasium as gym
import numpy as np
from gymnasium import spaces


# ---------------------------------------------------------------------------
# Plantas
# ---------------------------------------------------------------------------

@dataclass(frozen=True)
class Planta:
    nombre: str
    omega0: float          # rad/s — frecuencia natural del péndulo colgante
    k_u: float | None      # rad/s² por unidad de duty, dos motores en brake a la vez
    zona_muerta_m1: float  # duty
    zona_muerta_m2: float  # duty
    prop_m1: float         # fracción de K_U que aporta M1 solo
    cap: float             # duty máximo


# ⛔ PRE — L298N + chasis de contrachapado. Parámetros coherentes entre sí,
# todos medidos sobre la misma planta (registro de pruebas / notebook de PSO). El robot
# ya no existe en esta forma: sirve para desarrollar el entorno y los agentes.
PLANTA_VIEJA = Planta(
    nombre="vieja (L298N, contrachapado)",
    omega0=7.4,
    k_u=0.0218,
    zona_muerta_m1=92.0,
    zona_muerta_m2=102.0,
    prop_m1=0.575,
    cap=160.0,
)

# ✅ POST — 2× BTS7960 + chasis de balsa. ω₀ medido (2026-09-09); K_U, zona
# muerta y cap SIN MEDIR con el driver nuevo. Completar antes de usarla:
#   PLANTA_NUEVA_MEDIDA = replace(PLANTA_NUEVA, k_u=..., zona_muerta_m1=..., ...)
# Los valores de zona muerta y cap de abajo son marcadores, NO mediciones.
PLANTA_NUEVA = Planta(
    nombre="nueva (BTS7960, balsa)",
    omega0=8.0,
    k_u=None,
    zona_muerta_m1=float("nan"),
    zona_muerta_m2=float("nan"),
    prop_m1=float("nan"),
    cap=121.0,
)


# ---------------------------------------------------------------------------
# Entorno
# ---------------------------------------------------------------------------

# Niveles de duty por defecto para acciones discretas: 0 y ± cuatro escalones
# entre la zona muerta y el cap. Por debajo de la zona muerta de M1 no hay
# torque, así que no tiene sentido ofrecer niveles ahí. Se expresan como
# fracción del cap para que valgan en cualquier planta.
LIMITE_OBS = 10.0

NIVELES_DISCRETOS = (-1.0, -0.875, -0.75, -0.625, 0.0, 0.625, 0.75, 0.875, 1.0)


class PenduloBalanceadorEnv(gym.Env):
    metadata = {"render_modes": []}

    def __init__(
        self,
        planta: Planta = PLANTA_VIEJA,
        acciones: str = "discretas",           # "discretas" (DQN) o "continuas" (A2C)
        niveles: tuple = NIVELES_DISCRETOS,    # fracciones del cap, solo para "discretas"
        dt_control: float = 0.01,              # 100 Hz, el lazo del notebook
        subpasos_fisica: int = 10,
        duracion_s: float = 5.0,               # truncamiento del episodio
        theta_falla_deg: float = 10.0,         # terminación: robot caído
        theta0_max_deg: float = 3.06,          # peor caso de colocación medido (Test 10)
        theta_dot0_max: float = 0.15,          # rad/s, umbral de quietud del Test 10
        ruido_theta_deg: float = 0.0,          # σ del ruido de medición sobre θ
        ruido_theta_dot: float = 0.0,          # σ del ruido de medición sobre θ̇ [rad/s]
        latencia_pasos: int = 0,               # ciclos de retardo entre decidir y aplicar u
        peso_u: float = 0.05,                  # penalización de esfuerzo
        peso_du: float = 0.05,                 # penalización de cambio de acción (anti bang-bang)
        aleatorizar: dict | None = None,       # {"omega0": 0.03, "k_u": 0.15} = ±fracción por episodio
        solo_recuperables: bool = True,        # descartar estados iniciales que la física no deja salvar
    ):
        super().__init__()
        if planta.k_u is None or not np.isfinite(planta.zona_muerta_m1):
            raise ValueError(
                f"La planta '{planta.nombre}' tiene parámetros sin medir (K_U / zona muerta). "
                "Completarlos con valores medidos antes de usarla."
            )
        if acciones not in ("discretas", "continuas"):
            raise ValueError("acciones debe ser 'discretas' o 'continuas'")

        self.planta_nominal = planta
        self.planta = planta
        self.modo_acciones = acciones
        self.niveles = np.asarray(niveles, dtype=np.float64)
        self.dt = dt_control
        self.subpasos = subpasos_fisica
        self.dt_fisica = dt_control / subpasos_fisica
        self.max_pasos = int(round(duracion_s / dt_control))
        self.theta_falla = np.radians(theta_falla_deg)
        self.theta0_max = np.radians(theta0_max_deg)
        self.theta_dot0_max = theta_dot0_max
        self.ruido_theta = np.radians(ruido_theta_deg)
        self.ruido_theta_dot = ruido_theta_dot
        self.latencia = latencia_pasos
        self.peso_u = peso_u
        self.peso_du = peso_du
        self.aleatorizar = aleatorizar or {}
        self.solo_recuperables = solo_recuperables

        # Escala de θ̇ para normalizar la observación: la velocidad que alcanza
        # el péndulo libre cayendo desde θ_falla, del orden de ω₀·θ_falla.
        self.escala_theta_dot = planta.omega0 * self.theta_falla

        if acciones == "discretas":
            self.action_space = spaces.Discrete(len(self.niveles))
        else:
            self.action_space = spaces.Box(-1.0, 1.0, shape=(1,), dtype=np.float32)

        # Observación normalizada: [θ/θ_falla, θ̇/escala, u_anterior/cap].
        # u_anterior hace al estado markoviano respecto de la penalización Δu
        # (y ayuda cuando hay latencia).
        # Acotada a ±LIMITE_OBS: con θ_falla como escala, valores mayores solo
        # aparecen en el último paso antes de terminar.
        self.observation_space = spaces.Box(-LIMITE_OBS, LIMITE_OBS, shape=(3,), dtype=np.float32)

    # -- física ------------------------------------------------------------

    def _k_efectivo(self, u_abs: float) -> float:
        p = self.planta
        if u_abs < p.zona_muerta_m1:
            return 0.0
        if u_abs < p.zona_muerta_m2:
            return p.prop_m1
        return 1.0

    def _integrar(self, u: float) -> None:
        p = self.planta
        aceleracion_motor = p.k_u * self._k_efectivo(abs(u)) * u
        for _ in range(self.subpasos):
            theta_ddot = p.omega0**2 * self.theta + aceleracion_motor
            self.theta_dot += theta_ddot * self.dt_fisica   # Euler semi-implícito,
            self.theta += self.theta_dot * self.dt_fisica   # igual que el notebook

    # -- interfaz Gymnasium -------------------------------------------------

    def _accion_a_duty(self, accion) -> float:
        if self.modo_acciones == "discretas":
            fraccion = self.niveles[int(accion)]
        else:
            fraccion = float(np.clip(np.asarray(accion).reshape(-1)[0], -1.0, 1.0))
        return fraccion * self.planta.cap

    def _observar(self) -> np.ndarray:
        theta_med = self.theta + self.np_random.normal(0.0, self.ruido_theta) if self.ruido_theta else self.theta
        theta_dot_med = (self.theta_dot + self.np_random.normal(0.0, self.ruido_theta_dot)
                         if self.ruido_theta_dot else self.theta_dot)
        obs = np.array(
            [theta_med / self.theta_falla,
             theta_dot_med / self.escala_theta_dot,
             self.u_anterior / self.planta.cap],
            dtype=np.float32,
        )
        return np.clip(obs, -LIMITE_OBS, LIMITE_OBS)

    def _sortear_planta(self) -> Planta:
        cambios = {}
        for campo, frac in self.aleatorizar.items():
            nominal = getattr(self.planta_nominal, campo)
            cambios[campo] = nominal * (1.0 + self.np_random.uniform(-frac, frac))
        return replace(self.planta_nominal, **cambios) if cambios else self.planta_nominal

    def es_recuperable(self, theta: float, theta_dot: float) -> bool:
        """Criterio analítico. El modo divergente del sistema lineal es
        x = θ + θ̇/ω₀ (ẋ = ω₀·x + a/ω₀). Con aceleración máxima a_max = K_U·cap,
        solo se puede frenar si |x| < a_max/ω₀² — que es θ_umbral con θ̇ = 0.
        Ignora la zona muerta y el ruido: es una cota optimista."""
        p = self.planta
        modo = theta + theta_dot / p.omega0
        return bool(abs(modo) < p.k_u * p.cap / p.omega0**2)

    def reset(self, *, seed=None, options=None):
        super().reset(seed=seed)
        options = options or {}
        self.planta = self._sortear_planta()

        if "theta0" in options:     # condición inicial fija, para evaluación
            self.theta = float(options["theta0"])
            self.theta_dot = float(options.get("theta_dot0", 0.0))
        else:
            for _ in range(1000):
                signo = self.np_random.choice((-1.0, 1.0))
                self.theta = signo * self.np_random.uniform(0.0, self.theta0_max)
                self.theta_dot = self.np_random.uniform(-self.theta_dot0_max, self.theta_dot0_max)
                if not self.solo_recuperables or self.es_recuperable(self.theta, self.theta_dot):
                    break
            else:
                raise RuntimeError("Ningún estado inicial recuperable: revisar K_U, cap y theta0_max")

        self.paso = 0
        self.u_anterior = 0.0
        self.cola_acciones = deque([0.0] * self.latencia, maxlen=self.latencia + 1)
        return self._observar(), self._info(0.0)

    def step(self, accion):
        u_pedido = self._accion_a_duty(accion)

        # Latencia: se aplica la acción decidida `latencia` pasos atrás.
        self.cola_acciones.append(u_pedido)
        u_aplicado = self.cola_acciones.popleft() if self.latencia else u_pedido

        self._integrar(u_aplicado)
        self.paso += 1

        # Recompensa por paso en [~0, 1]: el retorno de un episodio se lee
        # como "pasos sobrevividos, descontando cuánto se inclinó y cuánto
        # esfuerzo/conmutación usó". Máximo = duracion_s / dt (500).
        cap = self.planta.cap
        recompensa = (1.0
                      - (self.theta / self.theta_falla) ** 2
                      - self.peso_u * (u_pedido / cap) ** 2
                      - self.peso_du * ((u_pedido - self.u_anterior) / (2 * cap)) ** 2)
        self.u_anterior = u_pedido

        terminated = bool(abs(self.theta) > self.theta_falla)
        if terminated:
            recompensa = 0.0
        truncated = bool(self.paso >= self.max_pasos)
        return self._observar(), float(recompensa), terminated, truncated, self._info(u_aplicado)

    def _info(self, u_aplicado: float) -> dict:
        return {"theta": self.theta, "theta_dot": self.theta_dot, "u": u_aplicado,
                "omega0": self.planta.omega0, "k_u": self.planta.k_u,
                "recuperable": self.es_recuperable(self.theta, self.theta_dot)}


# Registro opcional para gym.make("PenduloBalanceador-v0", ...)
gym.register(id="PenduloBalanceador-v0", entry_point=PenduloBalanceadorEnv)
