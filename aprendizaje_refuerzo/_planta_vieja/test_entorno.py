"""
Verificaciones del entorno. Correr con:
    aprendizaje_refuerzo/.venv/bin/python aprendizaje_refuerzo/test_entorno.py

No son tests de los agentes: prueban que el entorno implemente la física que
dice implementar, antes de entrenar nada encima.
"""

import numpy as np
from gymnasium.utils.env_checker import check_env

from entorno_pendulo import PLANTA_NUEVA, PLANTA_VIEJA, PenduloBalanceadorEnv


def test_api_gymnasium():
    for modo in ("discretas", "continuas"):
        check_env(PenduloBalanceadorEnv(acciones=modo), skip_render_check=True)
    print("OK  API de Gymnasium (discretas y continuas)")


def test_planta_nueva_sin_medir_se_rechaza():
    try:
        PenduloBalanceadorEnv(planta=PLANTA_NUEVA)
    except ValueError:
        print("OK  la planta nueva sin K_U medido se rechaza")
        return
    raise AssertionError("la planta nueva sin K_U no debería instanciarse")


def test_caida_libre_contra_solucion_analitica():
    """Sin acción, θ(t) = θ0·cosh(ω₀t) para θ̇0 = 0. Valida integrador y signo."""
    env = PenduloBalanceadorEnv(acciones="continuas", theta_falla_deg=90.0)
    theta0 = np.radians(1.0)
    env.reset(seed=0, options={"theta0": theta0})
    for _ in range(30):                       # 0.30 s
        _, _, _, _, info = env.step(np.array([0.0], dtype=np.float32))
    esperado = theta0 * np.cosh(PLANTA_VIEJA.omega0 * 0.30)
    error = abs(info["theta"] - esperado) / esperado
    # Euler semi-implícito con dt=1ms: ~0.4% a 0.3s, muy por debajo del ±1.2% de ω₀ medido
    assert error < 1e-2, f"error relativo {error:.2e}"
    print(f"OK  caída libre: θ(0.3s) = {np.degrees(info['theta']):.3f}°, "
          f"analítico {np.degrees(esperado):.3f}° (error {error:.1e})")


def test_zona_muerta():
    """Duty por debajo de la zona muerta de M1 no produce torque: igual a acción 0."""
    finales = []
    for frac in (0.0, 80.0 / PLANTA_VIEJA.cap):
        env = PenduloBalanceadorEnv(acciones="continuas")
        env.reset(seed=0, options={"theta0": np.radians(1.0)})
        for _ in range(10):
            _, _, _, _, info = env.step(np.array([-frac], dtype=np.float32))
        finales.append(info["theta"])
    assert finales[0] == finales[1]
    print("OK  duty 80 (< zona muerta de M1, 92) no hace nada")


def test_equivalencia_con_notebook():
    """Mismo controlador (PD sobre el estado real) en el entorno y en una copia
    literal del integrador del notebook: las trayectorias tienen que coincidir.
    Verificación de la implementación, no comparación entre controladores."""
    p = PLANTA_VIEJA
    kp, kd = 4000.0, 370.0
    theta0 = np.radians(2.0)

    def k_ef(u_abs):
        return 0.0 if u_abs < p.zona_muerta_m1 else (p.prop_m1 if u_abs < p.zona_muerta_m2 else 1.0)

    th, thd, ref = theta0, 0.0, []
    for _ in range(200):
        u = np.clip(-(kp * th + kd * thd), -p.cap, p.cap)
        for _ in range(10):
            thdd = p.omega0**2 * th + p.k_u * k_ef(abs(u)) * u
            thd += thdd * 0.001
            th += thd * 0.001
        ref.append(th)

    env = PenduloBalanceadorEnv(acciones="continuas")
    env.reset(seed=0, options={"theta0": theta0})
    th_env, thd_env, traj = theta0, 0.0, []
    for _ in range(200):
        u = np.clip(-(kp * th_env + kd * thd_env), -p.cap, p.cap)
        _, _, _, _, info = env.step(np.array([u / p.cap], dtype=np.float64))
        th_env, thd_env = info["theta"], info["theta_dot"]
        traj.append(th_env)

    diff = np.max(np.abs(np.array(ref) - np.array(traj)))
    assert diff < 1e-9, f"diferencia máxima {diff:.2e} rad"
    print(f"OK  trayectoria idéntica al integrador del notebook (máx dif {diff:.1e} rad)")


def retorno_medio(politica, n=50, **kwargs):
    env = PenduloBalanceadorEnv(**kwargs)
    retornos, largos = [], []
    for semilla in range(n):
        obs, _ = env.reset(seed=semilla)
        total, terminado, truncado, pasos = 0.0, False, False, 0
        while not (terminado or truncado):
            obs, r, terminado, truncado, _ = env.step(politica(obs, env))
            total += r
            pasos += 1
        retornos.append(total)
        largos.append(pasos)
    return np.mean(retornos), np.mean(largos)


def test_politicas_de_referencia():
    """El entorno tiene que ser resoluble y no trivial: una política aleatoria
    cae, una PD a mano sobrevive el episodio completo."""
    def aleatoria(obs, env):
        return env.action_space.sample()

    def nula(obs, env):
        return 4                                       # índice del duty 0

    def pd_discreta(obs, env):
        # obs normalizada; elige el nivel más cercano a -(kp·θ + kd·θ̇)
        u = -(4.5 * obs[0] + 3.0 * obs[1])
        return int(np.argmin(np.abs(env.niveles - np.clip(u, -1, 1))))

    for nombre, pol in (("aleatoria", aleatoria), ("nula", nula), ("PD a mano", pd_discreta)):
        ret, largo = retorno_medio(pol)
        print(f"    {nombre:10s} retorno medio {ret:7.1f}   pasos {largo:6.1f} / 500")

    ret_pd, largo_pd = retorno_medio(pd_discreta)
    ret_al, _ = retorno_medio(aleatoria)
    assert largo_pd == 500, "la PD de referencia debería sobrevivir todos los episodios"
    assert ret_al < 0.5 * ret_pd
    print("OK  políticas de referencia: el problema es resoluble y no trivial")


def test_caidas_de_la_pd_son_solo_las_irrecuperables():
    """Sin filtrar estados iniciales, la PD tiene que caer exactamente en los
    que el criterio analítico declara irrecuperables (límite de torque), no en
    otros: si cae en uno recuperable, el problema es del entorno o de la PD."""
    env = PenduloBalanceadorEnv(solo_recuperables=False)
    cayo_recuperable, irrecuperables, caidas = 0, 0, 0
    for semilla in range(500):
        obs, info = env.reset(seed=semilla)
        recuperable = info["recuperable"]
        terminado = truncado = False
        while not (terminado or truncado):
            u = -(4.5 * obs[0] + 3.0 * obs[1])
            obs, _, terminado, truncado, _ = env.step(int(np.argmin(np.abs(env.niveles - np.clip(u, -1, 1)))))
        irrecuperables += not recuperable
        caidas += terminado
        cayo_recuperable += terminado and recuperable
    assert cayo_recuperable == 0
    print(f"OK  sin filtro: {caidas} caídas en 500, las {irrecuperables} irrecuperables por física "
          f"({100 * irrecuperables / 500:.1f}% de la distribución del Test 10)")


if __name__ == "__main__":
    test_api_gymnasium()
    test_planta_nueva_sin_medir_se_rechaza()
    test_caida_libre_contra_solucion_analitica()
    test_zona_muerta()
    test_equivalencia_con_notebook()
    test_politicas_de_referencia()
    test_caidas_de_la_pd_son_solo_las_irrecuperables()
