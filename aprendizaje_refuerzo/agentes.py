"""
Agentes para el robot balanceador — Aprendizaje por Refuerzo I, Desafío 1.

    DQN / Double DQN   basado en valor, off-policy (replay buffer + red objetivo)
    A2C                actor-crítico, on-policy (n-step returns + GAE, entornos en paralelo)

Todos usan exactamente el mismo MDP (entorno.py): la comparación es entre
algoritmos, no entre problemas.

Estructura de DQN igual a la del TP3 / clase 5; la de A2C igual a la práctica
de la clase 7 (RMSprop, init ortogonal, tanh, GAE opcional).
"""

import random
import time
from dataclasses import dataclass, field

import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F
from torch.distributions import Categorical

# Siempre CPU, aun con GPU disponible: las redes son chicas (≈ 5 000 pesos) y el agente
# decide de a un paso por vez, así que copiar cada dato a la GPU cuesta más que la cuenta.
device = torch.device("cpu")


def fijar_semilla(seed):
    random.seed(seed)
    np.random.seed(seed)
    torch.manual_seed(seed)


def media_movil(x, w=50):
    x = np.asarray(x, dtype=float)
    w = max(1, min(w, len(x)))
    return np.convolve(x, np.ones(w) / w, mode="valid")


# ===========================================================================
# DQN y Double DQN
# ===========================================================================

@dataclass
class HiperDQN:
    pasos: int = 150_000          # pasos de entorno (cada paso = 10 ms simulados)
    gamma: float = 0.99           # horizonte efectivo ~1/(1-γ) = 100 pasos = 1 s
    lr: float = 5e-4
    batch: int = 64
    buffer: int = 50_000
    inicio_aprendizaje: int = 2_000
    copiar_objetivo_cada: int = 1_000
    eps_inicial: float = 1.0
    eps_final: float = 0.02
    eps_pasos_decaimiento: int = 50_000
    oculta: int = 64
    double: bool = False


class ReplayBuffer:
    def __init__(self, capacidad, n_obs):
        self.capacidad, self.pos, self.tam = capacidad, 0, 0
        self.s = np.zeros((capacidad, n_obs), dtype=np.float32)
        self.s2 = np.zeros((capacidad, n_obs), dtype=np.float32)
        self.a = np.zeros(capacidad, dtype=np.int64)
        self.r = np.zeros(capacidad, dtype=np.float32)
        self.d = np.zeros(capacidad, dtype=np.float32)

    def guardar(self, s, a, r, s2, terminal):
        i = self.pos
        self.s[i], self.a[i], self.r[i], self.s2[i], self.d[i] = s, a, r, s2, terminal
        self.pos = (self.pos + 1) % self.capacidad
        self.tam = min(self.tam + 1, self.capacidad)

    def muestrear(self, n):
        idx = np.random.randint(0, self.tam, size=n)
        t = lambda x: torch.as_tensor(x[idx], device=device)
        return t(self.s), t(self.a).unsqueeze(1), t(self.r).unsqueeze(1), t(self.s2), t(self.d).unsqueeze(1)

    def __len__(self):
        return self.tam


class QNet(nn.Module):
    """MLP: estado (3) → Q(s, a) para cada acción."""
    def __init__(self, n_obs, n_acciones, oculta=64):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(n_obs, oculta), nn.ReLU(),
            nn.Linear(oculta, oculta), nn.ReLU(),
            nn.Linear(oculta, n_acciones),
        )

    def forward(self, x):
        return self.net(x)


def objetivo_td(red, red_objetivo, r, s2, d, gamma, double):
    """y = r + γ·(1-terminal)·Q_objetivo(s', a*)
       DQN:        a* = argmax_a Q_objetivo(s', a)   (la misma red elige y evalúa)
       Double DQN: a* = argmax_a Q_red(s', a)        (una elige, la otra evalúa)"""
    with torch.no_grad():
        if double:
            a2 = red(s2).argmax(dim=1, keepdim=True)
            q_sig = red_objetivo(s2).gather(1, a2)
        else:
            q_sig = red_objetivo(s2).max(dim=1, keepdim=True)[0]
        return r + gamma * q_sig * (1.0 - d)


def entrenar_dqn(crear_env, hp: HiperDQN, seed=0, verbose=True):
    """Devuelve (red, pasos_fin_episodio, retornos_por_episodio)."""
    fijar_semilla(seed)
    env = crear_env()
    n_obs, n_acc = env.observation_space.shape[0], env.action_space.n
    red = QNet(n_obs, n_acc, hp.oculta).to(device)
    red_obj = QNet(n_obs, n_acc, hp.oculta).to(device)
    red_obj.load_state_dict(red.state_dict())
    opt = torch.optim.Adam(red.parameters(), lr=hp.lr)
    buffer = ReplayBuffer(hp.buffer, n_obs)

    s, _ = env.reset(seed=seed)
    ret_ep, pasos_fin, retornos = 0.0, [], []
    t0 = time.time()
    for paso in range(1, hp.pasos + 1):
        eps = max(hp.eps_final, hp.eps_inicial - (hp.eps_inicial - hp.eps_final) * paso / hp.eps_pasos_decaimiento)
        if random.random() < eps:
            a = random.randrange(n_acc)
        else:
            with torch.no_grad():
                a = int(red(torch.as_tensor(s, device=device).unsqueeze(0)).argmax(dim=1).item())

        s2, r, terminated, truncated, _ = env.step(a)
        # Solo 'terminated' (robot caído) corta el bootstrap: el truncamiento a
        # los 5 s es un límite del episodio, no un estado terminal del robot.
        buffer.guardar(s, a, r, s2, float(terminated))
        s, ret_ep = s2, ret_ep + r

        if len(buffer) >= max(hp.batch, hp.inicio_aprendizaje):
            bs, ba, br, bs2, bd = buffer.muestrear(hp.batch)
            q_sa = red(bs).gather(1, ba)
            y = objetivo_td(red, red_obj, br, bs2, bd, hp.gamma, hp.double)
            loss = F.smooth_l1_loss(q_sa, y)             # Huber
            opt.zero_grad()
            loss.backward()
            nn.utils.clip_grad_norm_(red.parameters(), 10.0)
            opt.step()

        if paso % hp.copiar_objetivo_cada == 0:
            red_obj.load_state_dict(red.state_dict())

        if terminated or truncated:
            pasos_fin.append(paso)
            retornos.append(ret_ep)
            if verbose and len(retornos) % 100 == 0:
                print(f"  paso {paso:7d} | ε={eps:.3f} | ep {len(retornos):4d} | "
                      f"media últ. 50: {np.mean(retornos[-50:]):6.1f}")
            ret_ep = 0.0
            s, _ = env.reset()
    if verbose:
        print(f"  {'Double DQN' if hp.double else 'DQN'} seed {seed}: {(time.time() - t0) / 60:.1f} min")
    return red, np.array(pasos_fin), np.array(retornos)


def politica_dqn(red):
    def pi(obs):
        with torch.no_grad():
            return int(red(torch.as_tensor(obs, device=device).unsqueeze(0)).argmax(dim=1).item())
    return pi


# ===========================================================================
# A2C (Advantage Actor-Critic)
# ===========================================================================

@dataclass
class HiperA2C:
    pasos: int = 300_000          # pasos de entorno totales (sumando los N entornos)
    n_envs: int = 8               # entornos en paralelo: decorrelacionan el batch (sin replay)
    n_steps: int = 16             # largo del rollout antes de cada actualización
    gamma: float = 0.99
    gae_lambda: float = 0.95      # 1.0 = n-step puro; <1 baja varianza a cambio de sesgo
    lr: float = 7e-4
    ent_coef: float = 0.01        # bonus de entropía: mantiene exploración
    vf_coef: float = 0.5
    max_grad_norm: float = 0.5
    oculta: int = 64
    capas: int = 2


def _mlp(entrada, salida, oculta, capas, ganancia_salida):
    mods, d = [], entrada
    for _ in range(capas):
        lin = nn.Linear(d, oculta)
        nn.init.orthogonal_(lin.weight, np.sqrt(2)); nn.init.zeros_(lin.bias)
        mods += [lin, nn.Tanh()]
        d = oculta
    ult = nn.Linear(d, salida)
    nn.init.orthogonal_(ult.weight, ganancia_salida); nn.init.zeros_(ult.bias)
    return nn.Sequential(*mods, ult)


class ActorCritico(nn.Module):
    """Dos redes separadas: el actor da logits de π(a|s), el crítico estima V(s)."""
    def __init__(self, n_obs, n_acciones, oculta=64, capas=2):
        super().__init__()
        self.actor = _mlp(n_obs, n_acciones, oculta, capas, 0.01)   # política inicial ~uniforme
        self.critico = _mlp(n_obs, 1, oculta, capas, 1.0)

    def dist(self, x):
        return Categorical(logits=self.actor(x))

    def valor(self, x):
        return self.critico(x).squeeze(-1)


def ventajas_gae(r, v, v_ult, no_terminal, gamma, lam):
    """GAE(λ) hacia atrás sobre un rollout [T, N].
       δ_t = r_t + γ·V(s_{t+1})·(1-terminal) − V(s_t)
       A_t = δ_t + γλ·(1-fin_t)·A_{t+1}
    Con λ=1 es el retorno de n pasos menos V(s_t)."""
    T = r.shape[0]
    adv = torch.zeros_like(r)
    ultimo = torch.zeros_like(v_ult)
    for t in reversed(range(T)):
        v_sig = v_ult if t == T - 1 else v[t + 1]
        delta = r[t] + gamma * v_sig * no_terminal[t] - v[t]
        ultimo = delta + gamma * lam * no_terminal[t] * ultimo
        adv[t] = ultimo
    return adv, adv + v


def entrenar_a2c(crear_env, hp: HiperA2C, seed=0, verbose=True):
    """Devuelve (modelo, pasos_fin_episodio, retornos_por_episodio)."""
    fijar_semilla(seed)
    envs = [crear_env() for _ in range(hp.n_envs)]
    n_obs, n_acc = envs[0].observation_space.shape[0], envs[0].action_space.n
    modelo = ActorCritico(n_obs, n_acc, hp.oculta, hp.capas).to(device)
    opt = torch.optim.RMSprop(modelo.parameters(), lr=hp.lr, alpha=0.99, eps=1e-5)

    obs = np.stack([e.reset(seed=seed * 1000 + i)[0] for i, e in enumerate(envs)])
    ret_ep = np.zeros(hp.n_envs)
    pasos_fin, retornos = [], []
    n_updates = hp.pasos // (hp.n_envs * hp.n_steps)
    paso_global, t0 = 0, time.time()

    for upd in range(n_updates):
        for g in opt.param_groups:                       # lr con decaimiento lineal
            g["lr"] = hp.lr * (1 - upd / n_updates)

        b_obs, b_act, b_r, b_nt = [], [], [], []
        for _ in range(hp.n_steps):
            o_t = torch.as_tensor(obs, dtype=torch.float32, device=device)
            with torch.no_grad():
                a = modelo.dist(o_t).sample()
            a_np = a.cpu().numpy()
            sig = np.empty_like(obs)
            r_t = np.zeros(hp.n_envs, dtype=np.float32)
            nt_t = np.ones(hp.n_envs, dtype=np.float32)
            for i, e in enumerate(envs):
                o2, r, term, trunc, _ = e.step(int(a_np[i]))
                ret_ep[i] += r
                if trunc and not term:
                    # Truncamiento por tiempo: el estado no es terminal, se
                    # bootstrapea con V(s_final) dentro de la recompensa.
                    with torch.no_grad():
                        r += hp.gamma * float(modelo.valor(torch.as_tensor(o2, dtype=torch.float32, device=device)))
                if term or trunc:
                    nt_t[i] = 0.0
                    pasos_fin.append(paso_global + i + 1)
                    retornos.append(ret_ep[i])
                    ret_ep[i] = 0.0
                    o2, _ = e.reset()
                r_t[i] = r
                sig[i] = o2
            b_obs.append(o_t); b_act.append(a)
            b_r.append(torch.as_tensor(r_t, device=device)); b_nt.append(torch.as_tensor(nt_t, device=device))
            obs = sig
            paso_global += hp.n_envs

        b_obs = torch.stack(b_obs)                       # [T, N, n_obs]
        b_act, b_r, b_nt = torch.stack(b_act), torch.stack(b_r), torch.stack(b_nt)
        dist = modelo.dist(b_obs)
        v = modelo.valor(b_obs)                           # [T, N]
        with torch.no_grad():
            v_ult = modelo.valor(torch.as_tensor(obs, dtype=torch.float32, device=device))
            adv, ret = ventajas_gae(b_r, v.detach(), v_ult, b_nt, hp.gamma, hp.gae_lambda)

        loss_actor = -(dist.log_prob(b_act) * adv).mean()     # gradiente de política con ventaja
        loss_critico = F.mse_loss(v, ret)
        entropia = dist.entropy().mean()
        loss = loss_actor + hp.vf_coef * loss_critico - hp.ent_coef * entropia
        opt.zero_grad()
        loss.backward()
        nn.utils.clip_grad_norm_(modelo.parameters(), hp.max_grad_norm)
        opt.step()

        if verbose and (upd + 1) % max(1, n_updates // 10) == 0 and retornos:
            print(f"  paso {paso_global:7d} | ep {len(retornos):4d} | media últ. 50: "
                  f"{np.mean(retornos[-50:]):6.1f} | entropía {entropia.item():.2f}")
    if verbose:
        print(f"  A2C seed {seed}: {(time.time() - t0) / 60:.1f} min")
    return modelo, np.array(pasos_fin), np.array(retornos)


def politica_a2c(modelo):
    """Determinista para evaluar: la acción más probable."""
    def pi(obs):
        with torch.no_grad():
            return int(modelo.actor(torch.as_tensor(obs, dtype=torch.float32, device=device)).argmax().item())
    return pi


# ===========================================================================
# Referencia no-RL y evaluación
# ===========================================================================

def politica_pd(env, kp=4.5, kd=3.0):
    """PD a mano sobre la observación normalizada, cuantizado al nivel discreto
    más cercano. No aprende nada: sirve de vara para medir a los agentes."""
    def pi(obs):
        u = -(kp * obs[0] + kd * obs[1])
        return int(np.argmin(np.abs(env.niveles - np.clip(u, -1, 1))))
    return pi


def evaluar(politica, crear_env, n=200, semilla0=10_000):
    """Política fija (sin exploración) en episodios con semillas que el
    entrenamiento no usó."""
    env = crear_env()
    retornos, sobrevive, theta_abs, du_abs = [], [], [], []
    for k in range(n):
        obs, _ = env.reset(seed=semilla0 + k)
        total, u_prev, fin = 0.0, 0.0, False
        while not fin:
            obs, r, term, trunc, info = env.step(politica(obs))
            fin = term or trunc
            total += r
            theta_abs.append(abs(info["theta"]))
            du_abs.append(abs(info["u"] - u_prev)); u_prev = info["u"]
        retornos.append(total)
        sobrevive.append(not term)
    return {"retorno": float(np.mean(retornos)), "retorno_std": float(np.std(retornos)),
            "supervivencia": float(100 * np.mean(sobrevive)),
            "theta_medio_deg": float(np.degrees(np.mean(theta_abs))),
            "du_medio": float(np.mean(du_abs))}


def trayectoria(politica, crear_env, theta0_deg=3.0):
    """θ(t) y u(t) desde una inclinación inicial fija, para graficar."""
    env = crear_env()
    obs, _ = env.reset(seed=0, options={"theta0": np.radians(theta0_deg)})
    t, th, u, fin = [0.0], [theta0_deg], [0.0], False
    while not fin:
        obs, _, term, trunc, info = env.step(politica(obs))
        fin = term or trunc
        t.append(t[-1] + 0.01); th.append(np.degrees(info["theta"])); u.append(info["u"])
    return np.array(t), np.array(th), np.array(u)
