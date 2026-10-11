# Checklist de pruebas pre-energización — robot rearmado

Para imprimir y llenar a mano durante la sesión. Es la hoja de trabajo del plan
completo en [`plan-pruebas-pre-energizacion.html`](plan-pruebas-pre-energizacion.html)
(ahí está el detalle, las tablas de verdad y las referencias cruzadas) — esta versión
existe solo para anotar lecturas y tacharlas a medida que se hacen.

**Fecha de la sesión:** ______________  **Hecho por:** ______________

**Estado del robot al empezar:** cableado completo (potencia + señal, 2026-09-12),
Buck nuevo a 5.01V, **nunca energizado con este cableado**. Filas de protoboard:
`docs/conexiones-registro-pruebas.md` §3.1b.

## Por qué existe esta hoja

Este robot ya falló **tres veces** con el mismo síntoma — el firmware reporta todo
bien por Serial y ningún motor se mueve — y las tres veces la causa estaba en la
etapa de potencia, no en el código (pin lógico sin alimentar, sin GND común, switch
en OFF). Además **perdió un Buck, dos capacitores y un fusible** por polaridad
invertida (2026-09-10), y el 2026-09-12 se armó un lazo de masa que solo se detectó
cruzando documentación, no midiendo. El cableado actual es nuevo de punta a punta y
tiene dos cambios que **nunca se probaron energizados**: el reordenamiento
M1→derecha / M2→izquierda del protoboard, y el paso de `RPWM` M1 de GPIO14 a GPIO19.

**Reglas antes de empezar:**

- [ ] Multímetro en modo **Ω**, no en continuidad/beep — el beep dispara con
      cualquier cosa por debajo de ~20-50Ω y no distingue un contacto sano de uno
      marginal (lección 4 del proyecto: eso es justo lo que se busca detectar).
- [ ] No saltear etapas. Van de lo más barato y seguro a lo más riesgoso, y cada
      prueba está pensada para que solo pueda fallar por un motivo — si algo falla,
      ya se sabe qué es sin tener que adivinar entre varias causas.

---

## Etapa 0 — Sin batería, multímetro en Ω

**⛔ BATERÍA DESCONECTADA (XT60 fuera) · SWITCH EN OFF · nada energizado**

### 0.1 — Polaridad del tramo hacia el Buck

> **Por qué:** es la prueba que se saltó el 2026-09-10 y costó un Buck, dos
> capacitores y el fusible. El proyecto no usa código de colores en los cables a
> propósito (no es confiable), así que la única forma válida de confirmar polaridad
> es medir — nunca fiarse del color. Hacerla siempre, sin importar cuántas veces
> haya salido bien antes (lección 11).

Con el XT60 **desenchufado**, medir continuidad desde cada pin del conector de
batería hacia el cable correspondiente en la bornera `IN` del Buck.

| Medición | Esperado | Lectura | OK |
|---|---|---|---|
| XT60 `+` ↔ cable en `IN+` | continuidad | _____________ | [ ] |
| XT60 `−` ↔ cable en `IN−` | continuidad | _____________ | [ ] |

Si falla: están invertidos. Corregir antes de seguir — no avanzar a 0.2.

### 0.2 — Las 6 pull-downs de seguridad

> **Por qué:** son la única protección por hardware si el ESP32 se cuelga o se
> resetea — sin micro secundario en el lazo, no hay nadie más que frene los motores.
> Esta medición valida a la vez que la resistencia está puesta **y** que quedó en la
> fila correcta del protoboard (un error de fila deja la línea sin protección aunque
> la resistencia exista en otro lado).

| Línea | Fila | Esperado en circuito | Lectura | OK |
|---|---|---|---|---|
| `EN` M1 (`R_EN`+`L_EN`) | J10 | ≈5.7–6kΩ | _____________ | [ ] |
| `LPWM` M1 | J8 | ≈7.45kΩ | _____________ | [ ] |
| `RPWM` M1 | J6 | ≈7.45kΩ | _____________ | [ ] |
| `EN` M2 | A9 | ≈5.7–6kΩ | _____________ | [ ] |
| `LPWM` M2 | A7 | ≈7.45kΩ | _____________ | [ ] |
| `RPWM` M2 | A5 | ≈7.45kΩ | _____________ | [ ] |

⚠️ **No se espera ≈10kΩ** (corregido 2026-09-14, registro §7.27). Medido en circuito,
con los drivers conectados, cada entrada del IBT-2 suma ≈26kΩ hacia GND en paralelo:
9.9kΩ ∥ 26kΩ ≈ 7.45kΩ. `EN` tiene dos entradas atadas (≈13kΩ), por eso da menos. La
resistencia suelta mide 9.9kΩ.

Cómo medir: la lectura arranca más baja y sube hasta estabilizarse — anotar el valor
estable, y descargar el nodo (tocar las dos patas con un cable) entre mediciones.

Cómo leer un resultado raro: **abierto** = falta la resistencia, está en la fila
equivocada o no llega a GND. **Estable y claramente menor** = dos pull-downs en
paralelo (≈4.3kΩ en `RPWM`/`LPWM`, ≈3.6kΩ en `EN`).

### 0.3 — Continuidad punta a punta: ESP32 → cada dispositivo

> **Por qué:** mide en el pin del ESP32 y en el pin del dispositivo, nunca en la
> fila del protoboard, porque el punto es detectar un **contacto flojo del
> protoboard** — el mismo tipo de falla que arruinó el enlace UART del ESP8266
> (mover el protoboard sin tocar un cable cambiaba el resultado) y el jumper de VIN
> de §3.2. Cubre los tres grupos que tienen que responder antes de energizar:
> drivers, encoders y MPU6050.

**Antes de medir**: poner las puntas del multímetro en corto, anotar esa lectura y
restarla de cada medición — muchos multímetros tienen ~1-2Ω de offset propio, y sin
restarlo "1.8Ω" no se distingue de "0.8Ω".

**Offset del multímetro (puntas en corto):** _____________ Ω

#### 0.3a — Drivers (6 líneas de señal)

| Pin ESP32 | Pin del módulo | Esperado | Lectura | OK |
|---|---|---|---|---|
| GPIO27 | IBT-2 #1 `R_EN` y `L_EN` | <2Ω a los dos | _____________ | [ ] |
| GPIO19 | IBT-2 #1 `RPWM` | <2Ω | _____________ | [ ] |
| GPIO13 | IBT-2 #1 `LPWM` | <2Ω | _____________ | [ ] |
| GPIO4 | IBT-2 #2 `R_EN` y `L_EN` | <2Ω a los dos | _____________ | [ ] |
| GPIO16 | IBT-2 #2 `RPWM` | <2Ω | _____________ | [ ] |
| GPIO17 | IBT-2 #2 `LPWM` | <2Ω | _____________ | [ ] |
| Riel 5V | `VCC` de cada IBT-2 | <2Ω | _____________ | [ ] |
| Soldadura `−` | `B−` de cada IBT-2 | <2Ω | _____________ | [ ] |

- [ ] **GPIO14 verificado LIBRE** — ningún cable sale de ese pin (saca señal
      activa durante el boot; `RPWM` M1 se movió a GPIO19 por esto).

#### 0.3b — Encoders (4 canales + alimentación)

Medir en el GPIO del ESP32 y en el **extremo del cable** del encoder (mapeo de
colores en §5 del registro).

| Pin ESP32 | Cable del encoder | Esperado | Lectura | OK |
|---|---|---|---|---|
| GPIO32 | M1 · amarillo · canal A | <2Ω | _____________ | [ ] |
| GPIO33 | M1 · verde · canal B | <2Ω | _____________ | [ ] |
| GPIO25 | M2 · amarillo · canal A | <2Ω | _____________ | [ ] |
| GPIO26 | M2 · verde · canal B | <2Ω | _____________ | [ ] |
| 3V3 (f.3/f.12) | azul de los dos encoders | <2Ω | _____________ | [ ] |
| Riel `−` | negro de los dos encoders | <2Ω | _____________ | [ ] |

- [ ] **Chequeo de seguridad**: ningún GPIO de encoder da continuidad a las
      pestañas del motor (rojo/blanco del encoder no van a ningún GPIO — si dieran
      continuidad, al energizar le entrarían 12V a un pin del ESP32).
- [x] **Filas exactas de los 4 canales de encoder anotadas** (trasladadas a §3.1b
      del registro, 2026-10-11, §7.30):
      M1 A → A(18) · M1 B → A(19) · M2 A → A(20) · M2 B → A(21)

#### 0.3c — MPU6050 (GY-521)

| Pin ESP32 | Pin del GY-521 | Esperado | Lectura | OK |
|---|---|---|---|---|
| GPIO21 (f.17-J) | SDA | <2Ω | _____________ | [ ] |
| GPIO22 (f.14-J) | SCL | <2Ω | _____________ | [ ] |
| 3V3 (f.3/f.12) | VCC | <2Ω | _____________ | [ ] |
| Riel `−` | GND | <2Ω | _____________ | [ ] |

> **Por qué el `VCC` importa más que los otros tres**: un GY-521 sin alimentación
> **sigue contestando por I2C** — se alimenta por los diodos de protección de
> SDA/SCL a unos µA, responde `WHO_AM_I` correcto y recién falla en `mpu.begin()`.
> Costó varias horas de diagnóstico (§7.13). Esta continuidad más el voltaje real de
> la Etapa 2 son las dos formas de no repetir ese error.

#### 0.3d — Alimentación del ESP32

| Desde | Hasta | Esperado | Lectura | OK |
|---|---|---|---|---|
| Riel 5V (Buck `OUT+`) | VIN del ESP32 (f.30-B) | <2Ω | _____________ | [ ] |
| Riel `−` | GND del ESP32 — **pin real, fila 25 (A–E)**, entre GPIO12 y GPIO13 | <2Ω (medido 0.4Ω) | _____________ | [ ] |
| Riel `−` | Pin junto a VIN (fila 29) — ⛔ **la serigrafía dice GND y NO es masa** | **abierto** (medido 0.6MΩ) | _____________ | [ ] |

> **Por qué**: las dos ya fallaron una vez cada una — el jumper de VIN sin contacto
> real dio 0.7V al energizar (§3.2), y el GND del ESP32 sin unir al riel lógico dejó
> los motores sin responder mientras el Serial por USB funcionaba perfecto (§7.6b).
> El USB trae su propia referencia de GND — que funcione **no prueba nada** del
> cableado lógico.
>
> ⛔ **Y falló una tercera vez (2026-09-21, registro §7.29):** el jumper de masa estaba
> en un pin que la serigrafía marca GND y **no es masa**. Esta fila lo dejó pasar porque
> medía el jumper, no el pin. **Antes de dar por buena la masa, medir el pin elegido
> contra otro GND de la misma placa (<1Ω) — la serigrafía no alcanza.**

### 0.4 — Ausencia de cruces

> **Por qué:** caza el reordenamiento M1/M2 mal hecho — es el riesgo nuevo de esta
> sesión de cableado — y es el equivalente al chequeo "ENA vs IN1" que ya se usó en
> §3.2. Todos estos pares tienen que ser nodos eléctricamente distintos.

⚠️ **Criterio (corregido 2026-09-14, registro §7.27): un cruce da prácticamente 0Ω
(<~100Ω). Valores en kΩ son normales** — no se espera "abierto", porque todas las
señales llegan a GND por sus pull-downs y las entradas del driver, y encoders/I2C por sus
pull-ups. La columna "Normal" es lo medido el 2026-09-14.

| Par | Normal | Qué detecta si da ~0Ω | Lectura | OK |
|---|---|---|---|---|
| `RPWM` M1 ↔ `LPWM` M1 | 14.9kΩ | Las dos mitades del puente en corto → el motor nunca gira | _____________ | [ ] |
| `RPWM` M2 ↔ `LPWM` M2 | 14.9kΩ | Ídem en M2 | _____________ | [ ] |
| `RPWM` M1 ↔ `RPWM` M2 | 14.9kΩ | Cruce entre lados — el riesgo nuevo del reordenamiento | _____________ | [ ] |
| `EN` M1 ↔ `EN` M2 | 11.9kΩ | Los dos drivers habilitados por el mismo GPIO | _____________ | [ ] |
| Canal A ↔ canal B (mismo encoder, ×2) | 6.6kΩ | Cuadratura imposible: no se detectaría sentido de giro | _____________ | [ ] |
| Canal A de M1 ↔ canal A de M2 | ~6.6kΩ | Cruce entre encoders — las dos ruedas leerían lo mismo | _____________ | [ ] |
| `SDA` ↔ `SCL` | 4.4kΩ | Bus I2C en corto: ningún dispositivo respondería | _____________ | [ ] |
| Cualquier señal ↔ riel 5V | kΩ | Señal pegada a 5V: el driver arrancaría solo | _____________ | [ ] |
| Riel 5V ↔ riel `−` | 1.2kΩ | Corto de alimentación | _____________ | [ ] |
| Riel 5V ↔ riel 3V3 | 24kΩ | 5V entrando al riel de 3.3V: mata el MPU y los encoders | _____________ | [ ] |

⚠️ **Limitación**: en circuito esta prueba solo detecta cruces francos, no fugas de
decenas de kΩ. Las fugas se verifican energizado, en la Etapa 1.1.

### 0.5 — Riel de 3V3: el valor esperado NO es "abierto"

| Medición | Esperado | Lectura | Interpretación si difiere |
|---|---|---|---|
| 3V3 (f.3 o f.12) ↔ riel `−` | ≈135kΩ | _____________ | Si da abierto, el MPU o los encoders no están realmente conectados al 3V3 (ya medido el 2026-09-12, §7.25 — no es fuga anómala, es el MPU+encoders compartiendo el nodo) |
| `EN` del ESP32 (f.13) ↔ riel `−` | abierto | _____________ | Un valor bajo = fila mal identificada (C_en es cerámico aislado) |

### 0.6 — Masa de los drivers

> **Por qué:** confirma que la masa de cada driver llega por el único camino
> decidido (§V1/8.4) y que el segundo camino que causó el lazo de masa del
> 2026-09-12 (§7.26) sigue desconectado.

| Medición | Esperado | Lectura/verificación | OK |
|---|---|---|---|
| `B−` de cada IBT-2 ↔ soldadura `−` | continuidad | _____________ | [ ] |
| `GND` del header desconectado del riel lógico | verificación **visual** | _____________ | [ ] |
| Cables sueltos (`GND` header, `R_IS`, `L_IS`) aislados y con brida | — | _____________ | [ ] |

⚠️ `GND` del header y `B−` son la misma pista dentro del módulo — el header va a dar
continuidad igual aunque esté "desconectado" del riel lógico. Esa fila **no se
puede testear con el multímetro**; confirmar a ojo que el cable está fuera. Aislar
no alcanza para `R_IS`/`L_IS` sueltos — un conductor suelto puede tocar `B+` si el
robot se cae durante el entrenamiento de RL; usar brida.

### 0.7 — Motores y encoders

⚠️ **Criterio corregido 2026-09-16 (registro §7.27).** Lo que detecta esta prueba es un
**abierto** (cable cortado o terminal floja), no un valor exacto. Con un multímetro, un
motor de escobillas quieto no da la resistencia del bobinado: domina el contacto
escobilla–conmutador, que cambia con la posición del rotor y baja de a poco con la
corriente del instrumento. El 3.5Ω histórico no sirve como criterio.

Cómo medir: **motor desconectado de la bornera**, rueda **quieta** — si gira, el motor
genera tensión y la lectura no vale —, esperar 15–30 s a que se asiente.

| Medición | Esperado | Lectura | OK |
|---|---|---|---|
| M1: `M+` ↔ `M−` | finito, de pocos Ω a ~100Ω (medido ~9Ω) | _____________ | [ ] |
| M2: `M+` ↔ `M−` | finito, de pocos Ω a ~100Ω (medido 28–116Ω) | _____________ | [ ] |
| Encoder: `VCC` ↔ `GND` de cada uno | sin corto (medido 26kΩ) | _____________ | [ ] |

Falla = **abierto (OL)** en todas las posiciones del rotor.

---

## Etapa 1 — Primera energización · MOTORES DESCONECTADOS

**⛔ DESCONECTAR `M+`/`M−` DE LAS DOS BORNERAS antes de dar llave**

> **Por qué desconectar los motores:** con los motores fuera, ni una falla total de
> lógica puede mover el robot. Cuesta cuatro tornillos y elimina por completo el
> riesgo de esta etapa — que es la primera vez que este cableado ve 12V.

| Medición | Esperado | Lectura | OK | Nota |
|---|---|---|---|---|
| Switch V+ en ON | — | [ ] | [ ] | Tercera causa histórica del síntoma "no se mueve nada" |
| `B+` IBT-2 #1 vs GND de potencia | ≈12V | _____________ | [ ] | Cubre switch, fusible, batería y soldadura de una sola medición |
| `B+` IBT-2 #2 vs GND de potencia | ≈12V | _____________ | [ ] | Son DOS entradas — medir las dos, no alcanza con una |
| `VCC` IBT-2 #1 | ≈5V | _____________ | [ ] | Sin esto la lógica del driver no conmuta nada aunque el firmware reporte bien (§7.6, tres veces en este proyecto) |
| `VCC` IBT-2 #2 | ≈5V | _____________ | [ ] | ídem |
| Buck `OUT+` bajo carga | 5.01V | _____________ | [ ] | Si cae mucho, el Buck no sostiene el consumo (4.98V era la unidad destruida) |
| VIN del ESP32 (f.30-B) | 4.8–5.0V | _____________ | [ ] | Una lectura intermedia tipo 0.7V = circuito abierto con fuga, no conexión parcial (así falló el jumper de VIN en §3.2) |
| 3V3 del ESP32 (f.3/f.12) | 3.2–3.4V | _____________ | [ ] | Si está bajo, revisar el margen de dropout del regulador interno. ✅ **3.35V (2026-09-21).** El 3.515–3.6V anterior era un desfase: el GND del ESP32 estaba en un pin que no es masa (registro §7.29). **Si vuelve a salir alto, verificar primero la masa del ESP32** |

### 1.1 — La prueba más importante de todas: las pull-downs en condición real

> **Por qué es la más importante:** es lo que garantiza que si el ESP32 se cuelga o
> se resetea, los motores no arrancan solos. La Etapa 0.2 midió la resistencia en
> frío; esta verifica el efecto real con el sistema energizado.

⚠️ **Corregido 2026-09-14 (registro §7.27): medir "~0V" NO prueba que la pull-down
esté.** Una línea flotante, sin pull-down, medida con un multímetro de ~10MΩ también
da ~0V, porque la entrada del multímetro hace de pull-down. Por eso esta prueba tiene
dos partes.

**Parte A — ESP32 sin manejar los pines (Test 12 en modo `z`, o sin firmware de
motores): detecta fugas hacia 5V/3V3.** Una señal que no da ~0V está siendo tirada hacia
arriba por algo. ⚠️ Si el ESP32 tiene cargado un sketch viejo de motores, una línea en
alto puede ser el firmware y no una falla.

| Chequeo | Esperado | Lectura | OK |
|---|---|---|---|
| `EN` M1 vs GND | < 1.1V (medido 0.30V) | _____________ | [ ] |
| `LPWM` M1 vs GND | < 1.1V (medido 0.35V) | _____________ | [ ] |
| `RPWM` M1 vs GND | < 1.1V (medido 0.07V) | _____________ | [ ] |
| `EN` M2 vs GND | < 1.1V (medido 0.40V) | _____________ | [ ] |
| `LPWM` M2 vs GND | < 1.1V (medido 0.40V) | _____________ | [ ] |
| `RPWM` M2 vs GND | < 1.1V (medido 0.40V) | _____________ | [ ] |

1.1V es el umbral bajo mínimo garantizado de `IN`/`INH` del BTS7960 (Infineon 4.4.6,
registro §8.2). Los valores medidos el 2026-09-16 fueron con un sketch desconocido
cargado; repetir con el Test 12 en modo `z`.

**Parte B — GPIO en `INPUT_PULLUP`: prueba que la pull-down está.** El pull-up interno
del ESP32 es débil (decenas de kΩ, valor exacto sin confirmar en datasheet) y pierde
contra la pull-down de 10kΩ. ✅ **Modo `p` del Test 12** (agregado 2026-09-16, compila,
sin flashear): `p` y después el número de línea (1–6). Pone esa línea en `INPUT_PULLUP`
y las otras 5 en bajo, e imprime además la lectura interna (`digitalRead`) — el
multímetro manda.

| Línea | Con pull-down (correcto) | Sin pull-down (falla) | Lectura | OK |
|---|---|---|---|---|
| `EN` M1 | claramente bajo | ~3.3V | _____________ | [ ] |
| `LPWM` M1 | claramente bajo | ~3.3V | _____________ | [ ] |
| `RPWM` M1 | claramente bajo | ~3.3V | _____________ | [ ] |
| `EN` M2 | claramente bajo | ~3.3V | _____________ | [ ] |
| `LPWM` M2 | claramente bajo | ~3.3V | _____________ | [ ] |
| `RPWM` M2 | claramente bajo | ~3.3V | _____________ | [ ] |

| Chequeo | Esperado | Lectura | OK |
|---|---|---|---|
| Temperatura tras 1 min — IBT-2 #1 | frío/tibio | _____________ | [ ] |
| Temperatura tras 1 min — IBT-2 #2 | frío/tibio | _____________ | [ ] |
| Temperatura tras 1 min — Buck | frío/tibio | _____________ | [ ] |

⚠️ **Si una línea da distinto de ~0V en la parte A, o ~3.3V en la parte B, NO SEGUIR** —
la protección por hardware no está funcionando. Cualquier módulo caliente con los
motores desconectados indica corriente que no debería existir: cortar y diagnosticar
antes de continuar.

---

## Etapa 2 — ESP32 corriendo · motores todavía desconectados

**MOTORES AÚN FUERA · solo sensores y verificación de mapeo**

| Prueba | Esperado | Lectura | OK | Por qué |
|---|---|---|---|---|
| `esp_reset_reason()` al arrancar | no `ESP_RST_BROWNOUT` (6) | _____________ | [ ] | Nunca se hizo. Si da brownout, el riel se hunde — explicaría el pendiente del AP que se cae con batería sola |
| Test 0 — scanner I2C | MPU en 0x68 | _____________ | [ ] | Distingue "no hay nadie" de "está en otra dirección" |
| `VCC` del MPU con multímetro | ≈3.3V | _____________ | [ ] | Que conteste por I2C NO prueba que esté alimentado — puede vivir de los diodos de protección a ~µA (§7.13) |
| Test 3 — girar las ruedas a mano | 4 canales cuentan; M1 y M2 dan signo opuesto | _____________ | [ ] | Valida los 4 canales sin energizar motores; el signo opuesto es por diseño (montaje en espejo) |

### 2.1 — Verificación de mapeo de señal, sin mover nada

> **Por qué:** cierra definitivamente el riesgo del reordenamiento M1/M2 y del
> cambio de `RPWM` M1 a GPIO19. Con los motores desconectados no hay consecuencia
> posible aunque el mapeo esté mal, y por la tabla de verdad del BTS7960 una sola
> línea en alto no puede hacer girar nada (`EN` solo = brake; `RPWM`/`LPWM` solo con
> `EN`=0 = stand-by).

Usar [`test/test12_mapeo_senales_esp32/`](../test/test12_mapeo_senales_esp32/test12_mapeo_senales_esp32.ino)
(Serial 115200, menú 1–6 pone una línea en alto a la vez, 0 vuelve al estado seguro).

| Línea comandada | Pin del header donde se espera alto | Lectura | OK |
|---|---|---|---|
| 1 — `EN` M1 | — | _____________ | [ ] |
| 2 — `RPWM` M1 | GPIO19 → `RPWM` de M1, no otro lado | _____________ | [ ] |
| 3 — `LPWM` M1 | — | _____________ | [ ] |
| 4 — `EN` M2 | — | _____________ | [ ] |
| 5 — `RPWM` M2 | — | _____________ | [ ] |
| 6 — `LPWM` M2 | — | _____________ | [ ] |

⚠️ **El modo `z` del Test 12 NO reemplaza a la Etapa 1.1** (corregido 2026-09-14,
registro §7.27). Pone los 6 GPIO en alta impedancia, pero una línea flotante sin
pull-down también mide ~0V con el multímetro. La prueba válida es la parte B de la 1.1
(`INPUT_PULLUP`): modo `p` del mismo sketch.

---

## Etapa 3 — Motores conectados · primer movimiento

**RUEDAS EN EL AIRE · un motor por vez · duty bajo**

⚠️ **El cap de PWM es 121, NO 160.** El 160 se calibró contra los BJT del L298N, que
se comían varios volts. El BTS7960 es MOSFET (R_ON 7–10 mΩ): al mismo duty le llega
bastante más tensión al motor. Arrancar en 121 y no subirlo sin medir τ(u) con
balanza en este driver.

| Prueba | Esperado | Resultado | OK | Nota |
|---|---|---|---|---|
| Test 11 — bring-up BTS7960, M1 solo | gira, sin humo, sin calor | _____________ | [ ] | `#define` corregidos 2026-09-13 (M1_RPWM 14→19) |
| Test 11 — M2 solo | ídem | _____________ | [ ] | |
| Sentido de giro de cada motor | el esperado | _____________ | [ ] | Al recablear a `M+`/`M−` se perdió la corrección de OUT1/OUT2 hecha para M1 — puede hacer falta invertirla de nuevo |
| Signo del encoder vs. sentido comandado | coherente | _____________ | [ ] | Cierra el lazo sensor↔actuador |

**Recién después de esto** (no antes): medir coast vs. brake en el BTS7960 con
balanza (hoy es solo inferencia de datasheet, §8.7 — mueve `K_U` por un factor
~2.7), y después el barrido de `K_U` con los **dos motores en brake conduciendo a la
vez** — la condición real, no un motor aislado. Con eso sale `θ_umbral` vigente.

---

## Notas libres de la sesión

_(anomalías, valores fuera de rango, decisiones tomadas en el momento, filas de
protoboard confirmadas que no estaban anotadas, etc. — para volcar después a
`docs/conexiones-registro-pruebas.md`)_

```




```

---

*Checklist creado 2026-09-14 a partir de [`plan-pruebas-pre-energizacion.html`](plan-pruebas-pre-energizacion.html)
(2026-09-13), y corregido el mismo día con los resultados de la Etapa 0 (registro §7.27):
valores esperados de la 0.2 y la 0.4, y la Etapa 1.1. Filas del protoboard: `docs/conexiones-registro-pruebas.md` §3.1b.
Al cerrar cualquier punto marcado como pendiente acá (p. ej. filas de encoder),
actualizar también el registro y CLAUDE.md — lección 13 del proyecto: una medición
que cierra un punto no está cerrada hasta que su decisión está escrita en todas las
fuentes que lo mencionan.*
