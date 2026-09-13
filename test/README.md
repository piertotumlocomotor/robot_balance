# Tests de validación por subsistema

Sketches de prueba aislada, uno por subsistema. Cada uno valida **una sola cosa**
antes de integrar. El orden importa: no pasar al siguiente sin que el anterior dé
resultados coherentes.

Cableado de referencia: [`../docs/conexiones-registro-pruebas.md`](../docs/conexiones-registro-pruebas.md).
Decisiones de diseño y límites: [`../CLAUDE.md`](../CLAUDE.md).

---

> ⚠️ **Proyecto migrado a un solo ESP32 el 2026-08-04.** Los sketches con sufijo
> `_esp32` son los vigentes. El resto quedó escrito para ESP8266 o AVR y **no
> compila** para la arquitectura actual — se conservan como referencia hasta
> terminar de portar. Ver "Decisiones revertidas" en [`../CLAUDE.md`](../CLAUDE.md).

> ⛔ **Los parámetros y veredictos citados en las descripciones de cada test son de
> su propia época, no del estado actual** (actualizado 2026-09-12). En particular,
> donde se lea `ω₀ = 7.4` (o 2.33), `K_U = 0.0123`, `m·l = 0.1265`, cap de PWM 160, o
> "el robot no puede balancear, falta un factor 2.4 de torque", **está desactualizado**.
> Valores vigentes, todos sobre el chasis de balsa rearmado:
>
> | | Vigente | Fuente |
> |---|---|---|
> | `ω₀` | **8.0 ± 0.1 rad/s** | registro 7.22 (2026-09-09) |
> | `m` / `m·l` / `l` | **1150 g / 0.1040 kg·m / 9.04 cm** | registro 7.23 (2026-09-10) |
> | `J` | **0.0159 kg·m²** | registro 7.23 |
> | Driver | **2× BTS7960 (IBT-2)**, no L298N | CLAUDE.md |
> | Cap de PWM | **121** como punto de partida para el driver nuevo | registro 8.5 |
> | `K_U` / `θ_umbral` | ⏳ **sin valor vigente** — hay que re-medirlos con el driver nuevo | registro 8.6 |
>
> El veredicto "no puede balancear" fue **revertido dos veces** (modo brake, y después
> el Test 10 que midió la perturbación real en 3.06° de peor caso). Ver la sección
> "K_U = 0.0123" de [`../CLAUDE.md`](../CLAUDE.md), que conserva la cadena completa.
>
> ⚠️ **`RPWM` de M1 es GPIO19, no GPIO14** (cambiado por salida activa del 14 en el
> boot). Donde un sketch o una descripción diga `IN1→GPIO14`, es el mapeo del L298N.

## Estado

**Vigentes (ESP32, FQBN `esp32:esp32:nodemcu-32s`):**

| Test | Sketch | Reporte | Compila | Flasheado |
|---|---|---|---|---|
| 1 — MPU6050 | `test1_mpu_esp32/` | USB + WiFi | ✅ | ✅ validado (2026-08-05) |
| 1b — Calibración MPU | *(por portar)* | — | — | — |
| 2 — Motores | `test2_motores_esp32/` | USB | ✅ | ✅ validado (2026-08-05) |
| 3 — Encoders + zona muerta | `test3_encoders_esp32/` | WiFi | ✅ | ✅ validado (2026-08-05) |
| 5 — K_U + zona muerta frío/caliente | `test5_ku_esp32/` | WiFi | ✅ | ⏳ pendiente |

**Históricos (arquitectura ESP8266 + Nano, deprecada):**

| Sketch | Placa | Estado |
|---|---|---|
| `test1_mpu_gy521/` | ESP8266 | Superado por `test1_mpu_esp32/` |
| `test1b_calibracion_mpu/` | ESP8266 | Pendiente de portar |
| `test2_motores_l298n/` | Arduino Nano | Superado por `test2_motores_esp32/` |
| `test4_uart_esp/`, `test4_uart_nano/`, `test4_diag_0x55/` | ambas | ⛔ **Obsoletos** — ya no hay enlace entre chips |

**Resultados que siguen siendo válidos y no hay que repetir**: calibración del
MPU6050 (Test 1/1b) y zona muerta bajo carga (Test 2) — son propiedades del sensor
y de la mecánica, no del microcontrolador.

### Hallazgos (resumen — detalle completo en `../CLAUDE.md`)

- **Test 1 + 1b**: convención de ejes confirmada (Z=arriba, Y=pitch, X=ruedas) y
  calibración de acelerómetro cerrada (bias/escala por eje) — ver "Calibración
  del MPU6050" en `../CLAUDE.md`. Valores todavía no aplicados a ningún firmware.
- **Test 2**: se encontraron y corrigieron **dos** bugs de cableado, ambos con el
  mismo síntoma engañoso (firmware reportando bien, motores sin moverse):
  1. El pin lógico "5V" del L298N no tenía alimentación tras remover el jumper
     permanentemente — ver "Alimentación lógica del L298N" en `../CLAUDE.md`.
  2. Tras migrar al ESP32, faltaba unir el **GND del ESP32** al riel GND lógico
     común. El Serial por USB seguía funcionando porque el USB trae su propia
     referencia de GND, lo que ocultó el problema. Ver "GND común ESP32↔L298N".
     **Lección**: que el Serial funcione no es evidencia de que el cableado
     lógico esté bien.

  También quedó **descartado el riesgo de nivel lógico 3.3V hacia el L298N** — el
  driver responde bien sin level shifter. Y se midió zona muerta por motor
  separado (~duty 85–95 en el aire), pendiente de re-medir con encoders en Test 3.

---

## Test 1 (ESP32) — Reporte del MPU6050 (GY-521)

**Sketch:** `test1_mpu_esp32/` · **FQBN:** `esp32:esp32:nodemcu-32s`

**Cableado:** SDA → GPIO 21, SCL → GPIO 22, VCC → 3V3, GND → GND.

Muestrea el MPU a 50 Hz y reporta por **dos vías a la vez**: USB serie (115200) y
una página web servida por el propio ESP32 en modo Access Point.

> **Cambio respecto de la versión ESP8266**: `Serial` por USB vuelve a estar
> disponible. La versión anterior no podía usarlo porque TX/RX estaban reservados
> para el enlace UART con el Nano. Con un solo micro ese enlace no existe. El USB
> es más cómodo para desarrollo; el AP sigue sirviendo con el robot en movimiento.

**Cómo usarlo:**
1. Alimentar **solo el riel lógico**. No energizar la etapa de potencia: sin nada
   comandando, las entradas del L298N quedan flotantes. (Los pull-down de 10kΩ
   resuelven esto de forma permanente, pero todavía no están instalados.)
2. **Opción A (USB)**: abrir el Monitor Serie a **115200**. Mandar `c` para calibrar
   el giroscopio.
   **Opción B (WiFi)**: conectarse a **`RobotBalance_Test1`** (clave `balance2026`)
   y abrir **http://192.168.4.1**.
3. Con el robot quieto, disparar la calibración del giroscopio y esperar ~5 s.

**Novedad — columna calibrada.** Este port muestra la magnitud de aceleración
**cruda y calibrada** en paralelo, aplicando el bias/escala medidos en el Test 1b
(`valor = (crudo − bias) / escala`). Sirve para cerrar un pendiente abierto: si la
columna calibrada da **~9.81** con el robot quieto y la cruda da ~10.5, valida esos
valores in situ y confirma que se pueden aplicar al firmware de control. Si tampoco
diera ~9.81, hay que revisarlos. Los valores no alimentan ningún cálculo todavía —
solo se muestran.

**Qué esperar (robot quieto):**
- **|Aceleración| ≈ 9.81 m/s²**. Es el chequeo de sanidad más confiable: debe dar
  ese valor sin importar la orientación del módulo. Si da otra cosa, el problema
  es el sensor o el bus I2C, no el montaje.
- El eje del acelerómetro que apunte hacia arriba marca ~9.8; los otros dos, ~0
  (con ruido de ±0.1–0.3, normal en este sensor).
- Tras calibrar, los tres ejes del giroscopio rondando **0.000** rad/s.
- Bias estimado por debajo de |0.05| rad/s.

**Qué anotar:**
- **|Aceleración| calibrada** — si da ~9.81, cierra el pendiente de aplicar
  bias/escala al firmware.
- **σ (desviación estándar)** de cada magnitud, en la tarjeta *Estabilidad*. Mide el
  ruido real del sensor mucho mejor que mirar números pasar. Sirve como línea de
  base: si más adelante σ crece al encender los motores, el ruido es de alimentación
  o de escobillas, no del sensor. **Comparar contra los valores del ESP8266**
  (σ Gx=0.0102, Gy=0.0009, Gz=0.0007 en la corrida del 29/07) para ver si el micro
  nuevo introduce más o menos ruido.
- La convención de ejes **ya está confirmada** (Z arriba, Y pitch, X ruedas) — no
  hace falta re-verificarla, `computePitch()` no requiere cambios.

**Bonus:** este test también estresa la alimentación, y esta vez importa más que
antes: **el ESP32 tiene picos de corriente mayores que el ESP8266** en transmisión
WiFi. Si la placa se reinicia o las lecturas se degradan al levantar el AP, el
problema es el riel de 5V o C4 (que todavía está montado donde iba el ESP8266 —
pendiente reubicarlo), no el MPU.

---

## Test 1b — Calibración de acelerómetro + verificación del eje X

**Placa:** ESP8266 (NodeMCU) · **FQBN:** `esp8266:esp8266:nodemcuv2`

Continuación del Test 1. Surgió de dos hallazgos de esa corrida (2026-07-29):
|Aceleración| = 10.51 m/s² en reposo (debería ser 9.80665, y es estable, no
ruido — sugiere error de escala), y falta confirmar que el eje X del sensor
coincide con el eje de las ruedas del robot.

**Cómo usarlo:**
1. Conectarse a **`RobotBalance_Test1b`** (clave `balance2026`) y abrir
   **http://192.168.4.1**
2. **(A) Calibrar giroscopio** — igual que en el Test 1, hacerlo antes de la
   parte C para no confundir bias con rotación real.
3. **(B) Calibración de acelerómetro** — para cada uno de los 6 botones,
   orientar el eje indicado contra la gravedad (hacia arriba), mantener quieto
   ~1 s y presionar "Capturar". Repetir los 6. La página calcula bias y factor
   de escala por eje automáticamente al completar todos.
   - ⚠️ Si el GY-521 está fijo al chasis y no se puede orientar limpiamente en
     las 6 posiciones, puede hacer falta desconectarlo momentáneamente y
     sostenerlo a mano durante la captura. Si se hace así, revisar que la
     reconexión mantiene SDA=D2/SCL=D1 antes de dar la calibración por buena.
4. **(C) Pureza del eje X** — presionar "Iniciar prueba" y rotar el robot
   *solo* sobre el eje de las ruedas durante los 5 s. La página compara los
   picos de |Giro X| contra |Giro Y| y |Giro Z|.

**Cómo interpretar la calibración de acelerómetro:**
- **Escala ≈ 1.0 en los tres ejes, pero magnitud de reposo sigue alta**: no es
  este el patrón esperado si el error es puramente de escala — revisar si el
  offset (bias) es el que está corriendo el resultado.
- **Escala desviada por igual en los tres ejes** (ej. los tres ~1.07): apunta a
  un error de ganancia global — mismo factor en todo el sensor, más fácil de
  corregir con una única constante.
- **Escala distinta entre ejes**: apunta a un problema específico de un eje o a
  desalineación mecánica del módulo respecto de los ejes que asume el
  software.
- La página marca ⚠️ cualquier eje con escala fuera de ±10% de 1.0.

**Cómo interpretar la prueba de eje X:**
- **Razón |Gx| / max(|Gy|,|Gz|) > 3**: eje confirmado como el eje de las
  ruedas.
- **Razón baja**: la rotación tuvo componente apreciable en otro eje, o la
  convención asumida (X = eje de las ruedas) está mal — repetir con más
  cuidado antes de descartar la convención.
- **Pico de |Gx| < 0.2 rad/s**: probablemente no hubo suficiente rotación real
  durante la ventana — repetir con un movimiento más claro.

---

## Test 2 (ESP32) — Señal PWM a ambos motores vía L298N

**Sketch:** `test2_motores_esp32/` · **FQBN:** `esp32:esp32:nodemcu-32s`

**Cableado (era L298N):** ENA→GPIO27, IN1→GPIO14, IN2→GPIO13 (Motor 1) · ENB→GPIO4, IN3→GPIO16,
IN4→GPIO17 (Motor 2).

Mismas tres fases que la versión Nano: (A) cada motor por separado en ambos
sentidos, (B) ambos motores simultáneos incluyendo giro sobre el eje, (C) barrido
ascendente de duty para el barrido de zona muerta. PWM generado con LEDC
(`ledcAttach`/`ledcWrite` por pin, API del core 3.x — ver "Límite de PWM a los
motores" en `../CLAUDE.md`) en vez de `analogWrite()`, a 8 bits de resolución para
que el tope de 121/255 siga aplicando sin reconvertir escalas.

> **Objetivo principal de esta corrida, más allá de repetir lo ya medido**:
> confirmar el **riesgo abierto #1 de la migración a ESP32** — el Nano manejaba
> IN1–4/ENA/ENB a 5V lógicos, el ESP32 los maneja a 3.3V. El L298N es
> TTL-compatible y muy probablemente funcione igual, pero no está confirmado
> contra datasheet en este proyecto. **Si el sketch reporta las fases por Serial
> pero ningún motor se mueve, es la señal de que hace falta un level shifter** en
> esas 6 líneas.

**Antes de correr:**
- **Levantar el robot** (o tethered, pitch limitado a ±30° como en 7.7); las
  ruedas no deben cargar sin control.
- ⚠️ **Los 4 pull-down de 10kΩ en IN1–4 todavía no están instalados** (ver
  "Seguridad: pull-downs en las entradas del L298N" en `../CLAUDE.md`). Sin ellos,
  un reset o cuelgue del ESP32 a mitad de la corrida deja IN1–4 en alta
  impedancia — estado no necesariamente parado. Mano cerca del switch de la
  batería durante toda la prueba.
- Ciclos cortos hasta reemplazar C1 (hoy 16V sobre un riel de 12.6V — margen
  escaso para los picos de arranque y frenado).
- Solo reporta por **USB** (115200) — no hace falta WiFi para este test, el robot
  no se aleja de la mesa.

**Límite de voltaje aplicado:** duty máximo 121/255 (≈47.6%), que a batería llena
(12.6V) equivale a ~6V promedio en bornes del motor. Se aplica con `constrain()`
dentro de `setMotor()`, no como constante suelta.

**Qué anotar:**
- **Si los motores responden o no** — este es el dato central de la corrida (ver
  recuadro arriba).
- Sentido de giro real de cada rueda respecto del frente del robot (para saber si
  hay que invertir IN1/IN2 o IN3/IN4 por software más adelante).
- Duty al que arranca cada motor en la fase C — referencia ya medida en 7.7
  (~77/255); no hace falta remedirla salvo que se quiera completar el protocolo
  aire/piso de 7.8.
- Si en la fase B (ambos motores) el micro se resetea o los motores titubean: la
  fuente/Buck no sostiene la corriente, y con el ESP32 hay menos margen que con
  el Nano (picos de WiFi, aunque este test no lo use activamente).

---

## Test 3 (ESP32) — Encoders + zona muerta medida por encoder

**Sketch:** `test3_encoders_esp32/` · **FQBN:** `esp32:esp32:nodemcu-32s` ·
**Reporte:** WiFi, AP `RobotBalance_Test3` (clave `balance2026`) → http://192.168.4.1

**Cableado adicional al del Test 2:** canal A/B de cada encoder a GPIO 32/33 (M1) y
25/26 (M2); VCC (azul) → 3V3; GND (negro) → riel lógico. Los 4 canales se configuran
con `INPUT_PULLUP` porque **sigue sin confirmarse si la salida es push-pull o colector
abierto** — el pull-up interno es inofensivo en el primer caso y obligatorio en el
segundo, así que cubre ambos sin tener que resolverlo antes.

**Por qué existe:** la zona muerta del Test 2 (duty 85–95) se midió mirando la rueda
girar. Ese método no distingue "arrancó" de "tembló y se quedó", ni separa la
variabilidad real del motor de la del ojo del operador. El encoder convierte eso en un
umbral objetivo.

**Las tres fases, y por qué ese orden:**

| Fase | Qué hace | Por qué va acá |
|---|---|---|
| **0** | Girar las ruedas **a mano**, sin motores. Conteo en vivo de ambos canales + cálculo de PPR | Valida el instrumento antes de confiar en él. Si algo sale raro en la fase 2, sin esto no se puede distinguir "motor que arranca tarde" de "encoder que no cuenta" |
| **1** | Cada motor a duty 110 en ambos sentidos, midiendo el **signo** del delta del encoder | Verifica objetivamente el sentido de giro — confirma la corrección física de M1 (OUT1/OUT2 intercambiados el 2026-08-05) |
| **2** | Barrido fino (paso 1, desde duty 60) × 5 repeticiones por motor | Zona muerta con min/máx/media/σ en vez de impresión visual |

**⚠️ La fase 0 no se puede saltear.** Además de validar los encoders, mide el **PPR**,
que es un dato abierto desde el inicio del proyecto (`../CLAUDE.md`: "no asumir
resolución/PPR del encoder sin confirmarla") **y del que depende `MOVE_THRESHOLD`** —
la constante que decide cuántos pulsos cuentan como "arrancó". Hoy vale 5 pulsos en
300 ms, elegido sin conocer el PPR. Después de la fase 0, revisarlo: si el PPR es bajo
puede estar descartando giros lentos reales; si es alto, puede contar vibración.

**Antes de correr:**
- **Ruedas en el aire** para las fases 0 y 1 (la 0 requiere girarlas a mano).
- Para la fase 2, **elegir la condición y anotarla**: aire o piso/tethered son
  mediciones distintas y el protocolo de la sección 7.8 pide las dos por separado.
- ⚠️ Si los 4 pull-down de 10kΩ en IN1–4 todavía no están instalados, mano cerca del
  switch de la batería: un cuelgue del ESP32 deja esas entradas en alta impedancia.

**⚠️ Este test levanta el AP de WiFi mientras los motores corren** — primera vez que
ambas cargas coinciden en el ESP32. La página muestra un **contador de arranques**: si
sube a mitad de una fase, la placa se reinició, y eso no es un fallo del sketch sino el
pendiente de C4 (todavía montado donde iba el ESP8266) manifestándose.

**Nota de arquitectura:** el lazo de medición corre en una tarea FreeRTOS separada del
servidor web. No es adorno — es el mismo patrón que va a hacer falta para el PID
(lazo de control fijado a un núcleo, WiFi en el otro), que fue una de las razones para
migrar al ESP32. Conviene tenerlo validado antes de que el robot intente balancear.

---

## Test 5 (ESP32) — K_U y zona muerta, en frío vs. caliente

**Sketch:** `test5_ku_esp32/` · **FQBN:** `esp32:esp32:nodemcu-32s` ·
**Reporte:** WiFi, AP `RobotBalance_Test5` (clave `balance2026`) → http://192.168.4.1

**Cableado:** el mismo del Test 3 (I2C + ambos motores). No requiere encoders.

### Por qué existe

El modelo del péndulo invertido que usa el proyecto de PSO es
`θ'' = ω₀²·θ + K_U·u`. De sus cuatro parámetros, tres están medidos sobre el robot
real (ω₀ = 7.4 rad/s, zona muerta ≈ 85, cap de PWM = 160). **K_U no** — hasta ahora
vale 0.03 porque se eligió a mano, y es el que escala directamente el umbral teórico
de Kp (`Kp > ω₀²/K_U`). Este test lo mide.

⚠️ **Este test quedó superado — primero por el Test 5b y finalmente por el Test 7**
([`test7_torque_balanza_esp32/`](test7_torque_balanza_esp32/test7_torque_balanza_esp32.ino)),
que cerró K_U midiendo el torque directamente como fuerza con una balanza de cocina:
**K_U = 0.0123, ángulo recuperable 2.07° contra un criterio de 5°**. El robot no
puede balancear con estos motores — falta un factor 2.4 de torque. Ver el registro
7.17.

⚠️ **Este test quedó superado por el Test 5b.** Se corrió el 2026-08-14 y no dio un
K_U usable (ver `docs/conexiones-registro-pruebas.md` 7.15). Además fue escrito
cuando ω₀ valía 2.33 rad/s, valor que resultó estar mal por un factor 3.2 — el
umbral real de K_U es **0.030**, no 0.004. Ver 7.12.

### El método

Con el robot **suspendido del eje de las ruedas** y colgando en reposo, el torque de
gravedad en esa posición es cero. Si en ese instante se aplica un escalón de duty
conocido, la ecuación se reduce (mientras θ siga chico) a `θ'' ≈ K_U·u`. La
aceleración angular θ'' se obtiene como la **pendiente de la velocidad angular** del
giroscopio durante los primeros 250 ms, ajustada por mínimos cuadrados.

> ⚠️ **Se usa giro X, no giro Y.** La convención del proyecto dice "Y = pitch"
> refiriéndose al eje del *acelerómetro* que apunta al frente. La **rotación** sobre
> el eje de las ruedas — la que inclina al robot — registra en **giro X** (confirmado
> en el Test 1b: 0.571 rad/s en X vs 0.137/0.041 en Z/Y). Derivar el eje equivocado
> daría un K_U sin sentido.

### Los dos barridos

| Barrido | Cuándo | Para qué |
|---|---|---|
| **Frío** | Motores sin usar | Medición de referencia de K_U y zona muerta |
| **Caliente** | Tras 30 s de movimiento continuo | Testea la hipótesis de que calentar los motores baja la zona muerta |

Correr **primero el frío**, con los motores realmente sin usar. El botón de caliente
mueve los motores 30 s antes de medir, y recalibra el giroscopio después (su bias
deriva con la temperatura).

### Sobre la hipótesis del calentamiento

Es físicamente razonable — la grasa de la reductora es más viscosa en frío. Pero
⚠️ **los datos del Test 3 no la respaldan**: dentro de cada corrida las repeticiones
son secuenciales (la rep 5 está más caliente que la rep 1), y el umbral de M1
*subía* entre repeticiones en 2 de las 3 corridas, no bajaba. Con n=5 y mucho ruido
de cogging eso no la refuta, pero sugiere que si el efecto existe es chico frente a
la variabilidad. Este test lo resuelve con evidencia directa: si las dos curvas se
superponen, no hace falta ninguna regla de "calentar antes de evaluar".

### Antes de correr

- **Suspender el robot del eje de las ruedas**, colgando libre, ruedas sin tocar nada
  (el mismo montaje que se usó para medir ω₀ por video).
- Dejarlo **quieto y sin oscilar** antes de arrancar — la calibración del giroscopio
  lo asume.
- ⚠️ El robot da un tirón en cada escalón: verificar que la suspensión aguante.
- Duración: ~3 min por barrido (7 duties × 3 reps × 8 s de espera entre escalones).

### Qué anotar

- **K_U** (el número grande de la página) — es el que reemplaza el `0.03` asumido en
  `algoritmos_evolutivos_pso/pso_pendulo_invertido.ipynb`.
- **θ recorrido** por escalón: si supera ~5°, en ese punto el término de gravedad
  dejó de ser despreciable durante la ventana y la medición es menos limpia.
- Si K_U frío y caliente difieren de forma apreciable, o si la zona muerta se corre,
  la hipótesis del calentamiento queda confirmada.

---

## Test 2 (histórico) — versión Arduino Nano

**Placa:** Arduino Nano · **FQBN:** `arduino:avr:nano` · Sketch: `test2_motores_l298n/`

⛔ Superado por `test2_motores_esp32/` arriba. Se conserva porque sus resultados de
zona muerta (~77/255, sección 7.7 de `../docs/conexiones-registro-pruebas.md`)
siguen siendo válidos — son propiedad del motor y la mecánica, no del
microcontrolador.

**Nota de flasheo (si hace falta volver a este sketch):** los Nano v3.0 clones
vienen con bootloader viejo (57600 baud) o nuevo (115200). Si el upload falla por
timeout, probar el procesador alternativo en el IDE (*ATmega328P* ↔ *ATmega328P
(Old Bootloader)*). También requiere desconectar J21/J22 (D0/D1 compartían pines
con el enlace UART al ESP8266, arquitectura ya deprecada).

---

## Test 4 — Enlace UART ESP8266 ↔ Nano

**Placas:** las dos a la vez — `test4_uart_esp/` (ESP8266, FQBN `esp8266:esp8266:nodemcuv2`)
y `test4_uart_nano/` (Nano, FQBN `arduino:avr:nano`). A diferencia de los tests
anteriores, este necesita ambos sketches corriendo al mismo tiempo — el enlace en sí
es lo que se prueba.

El ESP manda `PING <n>` cada 1s por UART hardware (9600 baud). El Nano responde
`PONG <n>` con el mismo número, y parpadea un LED en **D5** (con resistencia de
220Ω, LED amarillo) como confirmación visual local — el Nano no tiene forma de
reportar nada más: no tiene WiFi, y D0/D1 (el enlace bajo prueba) son los mismos
pines del USB, así que tampoco puede usar `Serial` para debug durante este test.

**Requiere** el divisor resistivo de la línea Nano TX → ESP RX ya armado y
verificado (ver `../docs/conexiones-registro-pruebas.md` sección 4) — sin él, no
correspondía llegar a este test.

**Antes de flashear el Nano:** desconectar las filas J21/J22 (mismo procedimiento
que el Test 2 — D0/D1 comparten pines con el USB).

**Cómo usarlo:**
1. Flashear ambos sketches (Nano primero, con J21/J22 desconectados; después el
   ESP8266).
2. Reconectar J21/J22.
3. Alimentar ambas placas y conectarse a la red WiFi **`RobotBalance_Test4`**
   (clave `balance2026`), abrir **http://192.168.4.1**.
4. Observar el LED del Nano (D5) — debería parpadear una vez por segundo.

**Cómo interpretar el resultado:**
- **LED parpadea + tasa de éxito >80% en la página**: enlace OK en ambos sentidos,
  Test 4 cerrado.
- **LED parpadea pero tasa de éxito baja/nula**: ESP→Nano funciona (por eso el LED
  reacciona), el problema está en la vuelta Nano→ESP — revisar el divisor
  (continuidad, y voltaje real en el nodo con esta carga específica).
- **LED nunca parpadea**: el problema está en ESP→Nano — revisar el cable directo
  entre TX del ESP8266 y RX del Nano.

**Nota:** el mensaje de arranque del bootloader ROM del ESP8266 (unos bytes a 74880
baud en cada reset, ver Test 1) llega al Nano como ruido una sola vez al encender —
el filtro de `rxBuffer.startsWith("PING ")` del lado Nano lo descarta sin problema.

---

## Compilar sin flashear

Vigentes (ESP32):

```bash
arduino-cli compile --fqbn esp32:esp32:nodemcu-32s test/test1_mpu_esp32
arduino-cli compile --fqbn esp32:esp32:nodemcu-32s test/test2_motores_esp32
arduino-cli compile --fqbn esp32:esp32:nodemcu-32s test/test3_encoders_esp32
arduino-cli compile --fqbn esp32:esp32:nodemcu-32s test/test5_ku_esp32
```

Históricos (arquitectura deprecada, solo si hace falta consultarlos):

```bash
arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 test/test1_mpu_gy521
```

```bash
arduino-cli compile --fqbn arduino:avr:nano test/test2_motores_l298n
```
