Experimento completo: 10 semillas por agente; evaluación sin exploración en 200 episodios nuevos por semilla (media ± desvío entre semillas).

| Agente | Retorno (máx. ≈ 500) | Sobrevive | \|θ\| medio | \|Δduty\| medio | Pasos hasta retorno 450 |
|---|---|---|---|---|---|
| DQN | 493.3 ± 3.9 | 100.0 ± 0.0 % | 0.59 ± 0.24° | 19.6 ± 20.9 | 72724 ± 6478 |
| Double DQN | 487.6 ± 7.7 | 100.0 ± 0.0 % | 0.80 ± 0.32° | 40.8 ± 27.5 | 80797 ± 6028 |
| A2C | 478.3 ± 5.6 | 99.9 ± 0.2 % | 0.31 ± 0.16° | 23.0 ± 5.0 | 166383 ± 28114 |
| PD a mano | 494.4 | 100.0 % | 0.66° | 32.4 | — |

**Discusión**

- **Los tres agentes aprenden a balancear el robot** en las 10 semillas. DQN y Double DQN sobreviven el 100 % de los episodios de evaluación; A2C, el 99.9 % (una caída en 2 000 episodios).
- **DQN es el más eficiente en datos**: llega a un retorno de 450 en ≈ 73 000 pasos, Double DQN en ≈ 81 000 y A2C en ≈ 166 000, con una dispersión entre semillas bastante mayor (Figura 5). DQN y Double DQN reutilizan cada transición muchas veces desde el *replay buffer*; A2C (*on-policy*) usa cada dato una sola vez.
- **Double DQN no mejora a DQN en este problema**: es algo más lento y obtiene un retorno un poco menor y más disperso (487.6 ± 7.7 contra 493.3 ± 3.9). Con 9 acciones y recompensas acotadas en [0, 1] la sobreestimación del máximo es chica, así que hay poco que corregir; una explicación posible de la leve demora es que su objetivo, más conservador, propaga el valor más despacio.
- **A2C deja al robot más derecho** (|θ| medio 0.31° contra 0.59° de DQN) pero lo hace con pulsos de motor a fondo (Figura 6), que la recompensa castiga; por eso su retorno es algo menor. Su curva de entrenamiento queda por debajo porque mientras entrena la política es aleatoria.
- **El PD ajustado a mano rinde igual que el mejor agente** (494.4 contra 493.3). El modelo es casi lineal y el problema es chico; el aporte del aprendizaje por refuerzo es llegar al mismo desempeño **sin conocer la ecuación**, solo con la recompensa.
- En todos los casos queda una **oscilación residual** de unas décimas de grado cerca de la vertical (Figura 6), por la falta de niveles de motor finos (sección 6).

**Inconvenientes y limitaciones**: ver el informe PDF (`informe/desafio1_robot_balanceador.pdf`), secciones 6 y 7.
