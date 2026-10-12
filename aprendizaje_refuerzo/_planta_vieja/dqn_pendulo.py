# ============================================================
# DQN y Double DQN sobre el modelo identificado del robot balanceador
# Aprendizaje por Refuerzo I — Desafío 1
#
# Misma estructura que el TP3 (replay buffer, red objetivo, ε-greedy con
# decaimiento lineal, pérdida de Huber, recorte de gradiente), adaptada a un
# estado de 3 números en vez de imágenes: la red es un MLP, no convolucional.
#
# Correr:  .venv/bin/python dqn_pendulo.py
# En Colab: subir también entorno_pendulo.py (o clonar el repo) antes de correr.
# ============================================================

# ============================================================
# Celda 1: Imports
# ============================================================
import os
import random
import time

import matplotlib
if "COLAB_GPU" not in os.environ and not os.environ.get("DISPLAY"):
    matplotlib.use("Agg")   # sin pantalla: solo guardar las figuras
import matplotlib.pyplot as plt
import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F

from entorno_pendulo import PLANTA_VIEJA, PenduloBalanceadorEnv

SEED = 0
device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
CARPETA_FIGURAS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "figuras")
os.makedirs(CARPETA_FIGURAS, exist_ok=True)
print("Dispositivo:", device)

# ============================================================
# Celda 2: Entorno y parámetros
# ============================================================
PLANTA = PLANTA_VIEJA   # ⚠️ cambiar por la planta nueva cuando K_U esté medido


def make_env():
    return PenduloBalanceadorEnv(planta=PLANTA, acciones="discretas")


_tmp_env = make_env()
N_OBS = _tmp_env.observation_space.shape[0]   # 3: θ, θ̇, acción anterior (normalizados)
N_ACTIONS = _tmp_env.action_space.n           # 9 niveles de duty
print("Observación:", N_OBS, "| Acciones:", N_ACTIONS,
      "| Duty de cada acción:", np.round(_tmp_env.niveles * PLANTA.cap).astype(int))

# --- Hiperparámetros ---
NUM_STEPS = 150_000          # pasos de entorno por agente (cada paso = 10 ms simulados)
GAMMA = 0.99                 # horizonte efectivo ~1/(1-γ) = 100 pasos = 1 s (el robot cae en ~0.3 s)
LR = 5e-4
BATCH_SIZE = 64
BUFFER_SIZE = 50_000
LEARN_START = 2_000          # pasos de experiencia antes de empezar a aprender
TRAIN_EVERY = 1              # una actualización de la red por paso de entorno
TARGET_UPDATE_EVERY = 1_000  # pasos entre copias de la red principal a la objetivo
EPSILON_START = 1.0
EPSILON_END = 0.02
EPSILON_DECAY_STEPS = 50_000
HIDDEN = 64

# ============================================================
# Celda 3: Replay buffer
# ============================================================
class ReplayBuffer:
    def __init__(self, capacity):
        self.capacity = capacity
        self.pos = 0
        self.size = 0
        self.s = np.zeros((capacity, N_OBS), dtype=np.float32)
        self.s2 = np.zeros((capacity, N_OBS), dtype=np.float32)
        self.a = np.zeros(capacity, dtype=np.int64)
        self.r = np.zeros(capacity, dtype=np.float32)
        self.d = np.zeros(capacity, dtype=np.float32)

    def push(self, s, a, r, s2, done):
        self.s[self.pos] = s
        self.s2[self.pos] = s2
        self.a[self.pos] = a
        self.r[self.pos] = r
        self.d[self.pos] = done
        self.pos = (self.pos + 1) % self.capacity
        self.size = min(self.size + 1, self.capacity)

    def sample(self, batch_size):
        idx = np.random.randint(0, self.size, size=batch_size)
        return (
            torch.as_tensor(self.s[idx], device=device),
            torch.as_tensor(self.a[idx], device=device).unsqueeze(1),
            torch.as_tensor(self.r[idx], device=device).unsqueeze(1),
            torch.as_tensor(self.s2[idx], device=device),
            torch.as_tensor(self.d[idx], device=device).unsqueeze(1),
        )

    def __len__(self):
        return self.size

# ============================================================
# Celda 4: Red neuronal
# ============================================================
class QNet(nn.Module):
    """MLP: estado (3) → Q(s, a) para cada una de las 9 acciones."""
    def __init__(self, n_obs, n_actions, hidden=HIDDEN):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(n_obs, hidden), nn.ReLU(),
            nn.Linear(hidden, hidden), nn.ReLU(),
            nn.Linear(hidden, n_actions),
        )

    def forward(self, x):
        return self.net(x)

# ============================================================
# Celda 5: Agente y entrenamiento
# ============================================================
def train_agent(double=False, seed=SEED, num_steps=None, verbose=True):
    """double=False → DQN clásica: el objetivo usa max_a Q_target(s', a).
       double=True  → Double DQN: la red principal ELIGE la acción en s'
                      y la red objetivo la EVALÚA (reduce la sobreestimación)."""
    num_steps = num_steps or NUM_STEPS
    env = make_env()
    random.seed(seed); np.random.seed(seed); torch.manual_seed(seed)
    env.action_space.seed(seed)

    policy_net = QNet(N_OBS, N_ACTIONS).to(device)
    target_net = QNet(N_OBS, N_ACTIONS).to(device)
    target_net.load_state_dict(policy_net.state_dict())
    optimizer = torch.optim.Adam(policy_net.parameters(), lr=LR)
    buffer = ReplayBuffer(BUFFER_SIZE)

    state, _ = env.reset(seed=seed)
    ep_return = 0.0
    log_steps, log_returns = [], []
    t0 = time.time()

    for step in range(1, num_steps + 1):
        # Política de comportamiento ε-greedy con decaimiento lineal
        epsilon = max(EPSILON_END,
                      EPSILON_START - (EPSILON_START - EPSILON_END) * step / EPSILON_DECAY_STEPS)
        if random.random() < epsilon:
            action = random.randrange(N_ACTIONS)
        else:
            with torch.no_grad():
                s_t = torch.as_tensor(state, device=device).unsqueeze(0)
                action = int(policy_net(s_t).argmax(dim=1).item())

        next_state, reward, terminated, truncated, _ = env.step(action)
        done = terminated or truncated
        # Solo 'terminated' (robot caído) corta el bootstrap: el truncamiento
        # a los 5 s es un límite del episodio, no un estado terminal real.
        buffer.push(state, action, reward, next_state, float(terminated))
        state = next_state
        ep_return += reward

        if len(buffer) >= max(BATCH_SIZE, LEARN_START) and step % TRAIN_EVERY == 0:
            s, a, r, s2, d = buffer.sample(BATCH_SIZE)
            q_sa = policy_net(s).gather(1, a)
            with torch.no_grad():
                if double:
                    a2 = policy_net(s2).argmax(dim=1, keepdim=True)
                    q_next = target_net(s2).gather(1, a2)
                else:
                    q_next = target_net(s2).max(dim=1, keepdim=True)[0]
                target = r + GAMMA * q_next * (1.0 - d)
            loss = F.smooth_l1_loss(q_sa, target)
            optimizer.zero_grad()
            loss.backward()
            nn.utils.clip_grad_norm_(policy_net.parameters(), 10.0)
            optimizer.step()

        if step % TARGET_UPDATE_EVERY == 0:
            target_net.load_state_dict(policy_net.state_dict())

        if done:
            log_steps.append(step)
            log_returns.append(ep_return)
            if verbose and len(log_returns) % 50 == 0:
                print(f"  Paso {step:7d} | ε={epsilon:.3f} | Episodio {len(log_returns):4d} | "
                      f"Media (últ. 50): {np.mean(log_returns[-50:]):6.1f}")
            ep_return = 0.0
            state, _ = env.reset()

    if verbose:
        print(f"  Tiempo de entrenamiento: {(time.time() - t0) / 60:.1f} min")
    env.close()
    return policy_net, np.array(log_steps), np.array(log_returns)

# ============================================================
# Celda 6: Evaluación (política greedy, sin exploración)
# ============================================================
def evaluate(net, n_eval=200, seed0=10_000):
    """Episodios con semillas que el entrenamiento no usó. Devuelve el retorno,
    la tasa de supervivencia y dos métricas físicas: inclinación media y
    cuánto conmuta la acción (|Δduty| medio por paso)."""
    eval_env = make_env()
    retornos, sobrevivio, theta_abs, du_abs = [], [], [], []
    for i in range(n_eval):
        state, _ = eval_env.reset(seed=seed0 + i)
        done, total, u_prev = False, 0.0, 0.0
        while not done:
            with torch.no_grad():
                action = int(net(torch.as_tensor(state, device=device).unsqueeze(0)).argmax(dim=1).item())
            state, reward, terminated, truncated, info = eval_env.step(action)
            done = terminated or truncated
            total += reward
            theta_abs.append(abs(info["theta"]))
            du_abs.append(abs(info["u"] - u_prev))
            u_prev = info["u"]
        retornos.append(total)
        sobrevivio.append(not terminated)
    return {
        "retorno": (np.mean(retornos), np.std(retornos)),
        "supervivencia": 100 * np.mean(sobrevivio),
        "theta_medio_deg": np.degrees(np.mean(theta_abs)),
        "du_medio": np.mean(du_abs),
    }


def trayectoria(net, theta0_deg=3.0, theta_dot0=0.0):
    env = make_env()
    state, _ = env.reset(seed=0, options={"theta0": np.radians(theta0_deg), "theta_dot0": theta_dot0})
    t, th, u = [0.0], [np.radians(theta0_deg)], [0.0]
    done = False
    while not done:
        with torch.no_grad():
            action = int(net(torch.as_tensor(state, device=device).unsqueeze(0)).argmax(dim=1).item())
        state, _, terminated, truncated, info = env.step(action)
        done = terminated or truncated
        t.append(t[-1] + env.dt); th.append(info["theta"]); u.append(info["u"])
    return np.array(t), np.degrees(np.array(th)), np.array(u)


def moving_average(x, w=50):
    w = min(w, len(x))
    return np.convolve(x, np.ones(w) / w, mode="valid")

# ============================================================
# Celda 7: Entrenamiento — DQN y Double DQN, 2 semillas cada una
# ============================================================
if __name__ == "__main__":
    SEMILLAS = (0, 1)
    agentes = {"DQN": False, "Double DQN": True}
    resultados = {}
    for nombre, double in agentes.items():
        for semilla in SEMILLAS:
            print(f"\nEntrenando {nombre}, semilla {semilla}...")
            net, steps, rets = train_agent(double=double, seed=semilla)
            resultados[(nombre, semilla)] = (net, steps, rets)

    # ========================================================
    # Celda 8: Curvas de convergencia (retorno vs episodio)
    # ========================================================
    colores = {"DQN": "tab:blue", "Double DQN": "tab:red"}
    fig, axs = plt.subplots(1, 2, figsize=(13, 4.5))
    for (nombre, semilla), (_, steps, rets) in resultados.items():
        ma = moving_average(rets)
        estilo = "-" if semilla == SEMILLAS[0] else "--"
        axs[0].plot(np.arange(len(ma)) + len(rets) - len(ma), ma, estilo,
                    color=colores[nombre], label=f"{nombre} (semilla {semilla})")
        axs[1].plot(steps[len(steps) - len(ma):], ma, estilo, color=colores[nombre])
    axs[0].set_xlabel("Episodio"); axs[0].set_ylabel("Retorno (media móvil de 50)")
    axs[1].set_xlabel("Pasos de entorno"); axs[1].set_ylabel("Retorno (media móvil de 50)")
    for ax in axs:
        ax.axhline(500, color="gray", ls=":", lw=1)
        ax.grid(alpha=0.3)
    axs[0].legend(fontsize=8)
    fig.suptitle(f"DQN vs Double DQN — planta {PLANTA.nombre}")
    plt.tight_layout()
    plt.savefig(os.path.join(CARPETA_FIGURAS, "convergencia_dqn.png"), dpi=150)
    plt.show()

    # ========================================================
    # Celda 9: Evaluación de las políticas finales
    # ========================================================
    print("\nEvaluación greedy, 200 episodios con semillas no vistas:")
    for (nombre, semilla), (net, _, _) in resultados.items():
        ev = evaluate(net)
        print(f"  {nombre:11s} s{semilla} | retorno {ev['retorno'][0]:6.1f} ± {ev['retorno'][1]:5.1f} | "
              f"sobrevive {ev['supervivencia']:5.1f}% | |θ| medio {ev['theta_medio_deg']:.3f}° | "
              f"|Δduty| medio {ev['du_medio']:5.1f}")

    # ========================================================
    # Celda 10: Trayectoria de ejemplo desde 3°
    # ========================================================
    fig, axs = plt.subplots(2, 1, figsize=(10, 6), sharex=True)
    for nombre in agentes:
        net = resultados[(nombre, SEMILLAS[0])][0]
        t, th, u = trayectoria(net)
        axs[0].plot(t, th, color=colores[nombre], label=nombre)
        axs[1].step(t, u, where="post", color=colores[nombre], lw=0.8)
    axs[0].set_ylabel("θ [°]"); axs[0].legend(); axs[0].grid(alpha=0.3)
    axs[1].set_ylabel("duty"); axs[1].set_xlabel("t [s]"); axs[1].grid(alpha=0.3)
    fig.suptitle("Respuesta desde θ₀ = 3°, θ̇₀ = 0 (semilla 0)")
    plt.tight_layout()
    plt.savefig(os.path.join(CARPETA_FIGURAS, "trayectoria_dqn.png"), dpi=150)
    plt.show()
