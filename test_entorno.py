"""
Verificaciones del entorno, antes de entrenar nada encima.
Correr:  python test_entorno.py
"""

import numpy as np
from gymnasium.utils.env_checker import check_env

from entorno import CAP, K_U, OMEGA0, THETA_UMBRAL, RobotBalanceadorEnv


def test_api_gymnasium():
    check_env(RobotBalanceadorEnv(), skip_render_check=True)
    print("OK  cumple la API de Gymnasium")


def test_parametros():
    """K_U y θ_umbral coinciden con la cuenta a mano: sen θ = τ_max / (m·g·l)."""
    a_mano = np.degrees(1.4 * (0.0443 + 0.0339) / 160 * CAP / (0.1040 * 9.81))
    assert abs(np.degrees(THETA_UMBRAL) - a_mano) < 0.05
    print(f"OK  K_U = {K_U:.4f} rad/s²/duty, θ_umbral = {np.degrees(THETA_UMBRAL):.2f}° (a mano {a_mano:.2f}°)")


def test_caida_libre():
    """Sin motor, θ(t) = θ₀·cosh(ω₀·t). Valida el integrador y el signo."""
    env = RobotBalanceadorEnv()
    env.reset(seed=0, options={"theta0": np.radians(1.0)})
    for _ in range(30):                                   # 0.3 s con duty 0 (acción 4)
        _, _, _, _, info = env.step(4)
    esperado = np.radians(1.0) * np.cosh(OMEGA0 * 0.3)
    assert abs(info["theta"] - esperado) / esperado < 1e-2
    print(f"OK  caída libre: {np.degrees(info['theta']):.3f}° vs analítico {np.degrees(esperado):.3f}°")


def test_politicas_de_referencia():
    """Resoluble (un PD a mano sobrevive) y no trivial (al azar cae enseguida)."""
    env = RobotBalanceadorEnv()

    def jugar(politica, n=50):
        largos = []
        for s in range(n):
            obs, _ = env.reset(seed=s)
            for paso in range(1, 501):
                obs, _, term, trunc, _ = env.step(politica(obs))
                if term or trunc:
                    break
            largos.append(paso if not term else paso - 500)   # negativo = cayó
        return np.array(largos)

    azar = jugar(lambda o: env.action_space.sample())
    pd = jugar(lambda o: int(np.argmin(np.abs(env.niveles - np.clip(-(4.5 * o[0] + 3.0 * o[1]), -1, 1)))))
    assert (pd == 500).all() and (azar < 0).all()
    print(f"OK  PD a mano: 50/50 episodios completos | al azar: cae siempre, "
          f"en {np.mean(azar + 500):.0f} pasos promedio")


if __name__ == "__main__":
    test_api_gymnasium()
    test_parametros()
    test_caida_libre()
    test_politicas_de_referencia()
