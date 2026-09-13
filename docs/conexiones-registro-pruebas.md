# Robot Balanceador — Conexiones y Registro de Pruebas

Este archivo reúne, en un solo lugar, **todas las conexiones ya definidas** y **todos los valores obtenidos durante las pruebas físicas de validación**, para consulta rápida sin recorrer todo el historial de decisiones.

> ⚠️ **Migrado a arquitectura de un solo ESP32 el 2026-08-04.** Las secciones de
> cableado (1–6) describen la arquitectura vigente. El registro de pruebas (7)
> conserva mediciones hechas con la arquitectura anterior (ESP8266 + Arduino Nano);
> se indica en cada caso cuáles siguen siendo válidas y cuáles quedaron obsoletas.
> El porqué del cambio está en `../CLAUDE.md`, sección "Decisiones revertidas".

---

## 1. Topología de alimentación

⚠️ **Actualizado 2026-08-29 — migración L298N → 2× BTS7960 (IBT-2).** El L298N
manejaba los dos motores con **una sola** entrada de potencia; cada IBT-2 es un
puente H completo para **un** motor, así que ahora hay **dos** entradas de potencia
independientes. Ver 8 para el detalle del driver nuevo.

⛔ **Cambio de topología, 2026-09-03 — rearmado del chasis.** `CLAUDE.md` documenta como decisión
cerrada "nodo único en el nivel superior (bornera V+ y bornera GND **dedicadas**, sin compartir tramo
con ninguna carga)". Esa bornera dedicada **no existe en el armado actual**. La topología real, aclarada
por el usuario, es en **dos etapas**, no una:

1. **Soldadura (empalme mecánico + estaño)** — el nodo estrella real. Ahí confluyen: batería (posterior
   al switch), `B+` de M1 y `B+` de M2 (mismo esquema del lado GND con `B−`). De esa unión sale **un
   solo cable combinado** por lado.
2. **Bornera `IN+`/`IN−` del Buck** — recibe **dos conductores**, no cuatro: el cable combinado de la
   soldadura, y la pata de C1 (ver 6), puestos ahí por separado.

No es una reapertura de la decisión original por gusto: es la arquitectura con la que el robot está
armado hoy, confirmada por el usuario. El diagrama de abajo refleja las dos etapas.

```
LiPo 3S (11.1V, hasta 12.6V full)
      │
   Fusible 10A ── Switch
      │
      ├───────────────┐
      │               │
      ▼               ▼
 SOLDADURA (+)    SOLDADURA (−)  ← nodo estrella real
 (empalme de      (empalme de
  estaño)          estaño)
   │   ▲   ▲          │   ▲   ▲
   │   │   │          │   │   │
   │  IBT-2 IBT-2     │  IBT-2 IBT-2
   │  M1 B+ M2 B+     │  M1 B− M2 B−
   ▼                  ▼
 cable combinado    cable combinado
   │                  │
   ▼                  ▼
BORNERA IN+ DEL BUCK   BORNERA IN− DEL BUCK
   │  ▲ (C1 +)         │  ▲ (C1 −)
   ▼                   ▼
Buck OUT+ (5V) ──────────┴─── Buck OUT− (GND lógico)
   │                                     │
   ▼                                     ▼
Riel 5V lógico protoboard         Riel GND lógico protoboard
(ESP32, VCC de ambos IBT-2)       (mismo grupo)
```

**Reglas actualizadas:**
- El nodo estrella real es la **soldadura**, no la bornera del Buck. La bornera del Buck es un punto
  aguas abajo de la soldadura, que recibe el cable combinado más la pata de C1 — dos conductores, no
  cuatro. ✅ **Confirmado por el usuario el 2026-09-12 y ya reflejado en las tres fuentes**: `CLAUDE.md`
  ("Decisiones de arquitectura cerradas"), `plano-potencia.html` (§ árbol de potencia y tabla punto a
  punto) y esta sección. No es una desviación sin documentar.
- ✅ **Conflicto de capacidad de la bornera — CERRADO, no abierto.** La preocupación original (8.3) asumía
  que la bornera del Buck tendría que alojar 4 conductores (batería + Buck + 2× `B+`/`B−`). En la
  práctica solo aloja 2 (cable combinado + C1), y **entró** — confirmado por la instalación real de C1,
  no por cálculo. Sigue siendo una bornera más chica que la dedicada que se había planeado originalmente,
  así que no forzar un tercer conductor ahí sin volver a evaluar.
- Buck y drivers ya no se alimentan de forma independiente en el sentido original (bornera separada por
  carga) — comparten la soldadura como origen común, y el Buck recibe su alimentación de potencia por un
  cable propio desde ahí, igual que cada driver. Sigue valiendo que el Buck no debe **alimentar** `B+`
  de los motores (no debe haber corriente de motor circulando por dentro del Buck).
- GND de potencia y GND lógico solo se unen en la soldadura — ningún cable hace de "paso" entre dos
  cargas distintas.
- El GND lógico no necesita cable de retorno adicional al nodo estrella: el Buck XL4016 (no aislado) ya
  une IN− y OUT− internamente en su propia placa.
- ⚠️ **Nota de mantenimiento, no bloqueante:** una soldadura de 4 cables sin alivio de tensión mecánico
  es candidata a fisurarse con vibración o con caídas del robot durante el entrenamiento de RL. Si no
  tiene ya algo que la sostenga (brida, termocontraíble anclado a la estructura), vale la pena sumarlo.

---

## 2. Conexiones de potencia — detalle

**Vigente desde 2026-08-29 (2× BTS7960 / IBT-2).** ✅ **Actualizado 2026-09-12: la
potencia está armada y el Buck nuevo verificado.** El estado "nada cableado todavía"
que tenía esta sección quedó obsoleto con el rearmado del chasis (2026-09-03) y la
sesión de cableado del 2026-09-12 — ver 3.1b para la señal y 7.26 para el lazo de
masa detectado y corregido.

⚠️ **El origen ya no es una bornera dedicada, es la soldadura** (nodo estrella real,
ver 1 y `plano-potencia.html`). La tabla está corregida acá abajo; decía "Bornera V+"
/ "Bornera GND" por arrastre de la topología vieja.

| Origen | Destino | Notas |
|---|---|---|
| Soldadura + (nodo estrella) | Buck IN+ | Vía el **cable combinado**, único conductor de la soldadura que llega a esa bornera (junto con la pata de C1) |
| Soldadura + (nodo estrella) | IBT-2 M1 `B+` | Rama directa desde la soldadura |
| Soldadura + (nodo estrella) | IBT-2 M2 `B+` | Ídem |
| Soldadura − (nodo estrella) | Buck IN− | Vía el cable combinado del lado GND |
| Soldadura − (nodo estrella) | IBT-2 M1 `B−` | ✅ Cableado. `B−` **es** el GND del módulo (8.4 cerrada) — este cable establece toda la referencia de masa del driver |
| Soldadura − (nodo estrella) | IBT-2 M2 `B−` | ✅ Ídem |
| Buck OUT+ | Riel 5V lógico | ✅ Unidad nueva tras el incidente de 7.24 — verificada y calibrada en **5.01V** (2026-09-12). **El 4.98V histórico era de la unidad destruida**, no usarlo como referencia |
| Buck OUT− | Riel GND lógico | Común interno con IN− |
| Riel 5V lógico | IBT-2 M1 `VCC` | ✅ Cableado 2026-09-12. ⚠️ Sin este cable la lógica del driver no tiene alimentación y no conmuta ninguna salida — mismo bug que costó 7.6 con el L298N |
| Riel 5V lógico | IBT-2 M2 `VCC` | ✅ Ídem |
| ~~Riel GND lógico → IBT-2 `GND` (header), ×2~~ | ⛔ **NO cablear** | **Resuelto por V1 (2026-09-10): `B−` y `GND` del header son el mismo nodo.** Llevar los dos arma lazo de masa. Se cableó por error el 2026-09-12 y se retiró — ver 7.26. La referencia común la da `B−` → soldadura |
| Riel 5V lógico | ESP32 pin `5V` (VIN) | Alimentación del micro — fila 30 (B) en el protoboard rearmado (3.1b) |
| IBT-2 M1 `M+` | Motor 1 (rojo) | Motor derecho |
| IBT-2 M1 `M−` | Motor 1 (blanco) | Motor derecho |
| IBT-2 M2 `M+` | Motor 2 (rojo) | Motor izquierdo |
| IBT-2 M2 `M−` | Motor 2 (blanco) | Motor izquierdo |

⚠️ **El sentido de giro se valida en banco, no se asume.** Con el L298N hubo que
intercambiar OUT1/OUT2 de M1 físicamente. Al recablear a `M+`/`M−` esa corrección
se pierde — hay que repetir la Fase 1 del Test 3 y, si hace falta, intercambiar de
nuevo. Recordar además que M1 y M2 están montados en espejo: dan signo opuesto de
encoder para el mismo comando (ver 5).

**Histórico L298N** (deprecado 2026-08-29): OUT1/OUT2 → Motor 1, OUT3/OUT4 → Motor 2,
VIN+ único desde bornera V+, pin lógico "5V" desde el Buck, jumper de 5V interno
removido de forma permanente (ver 7.2).

---

## 3. Conexiones — ESP32 (NodeMCU ESP-32S, 38 pines)

Único microcontrolador del lazo de control. Chip USB-serie CP2102, conector USB-C.

| GPIO | Etiqueta placa | Función |
|---|---|---|
| 21 | `P21` | I2C SDA → GY-521 |
| 22 | `P22` | I2C SCL → GY-521 |
| 32 | `P32` | Encoder M1, canal A |
| 33 | `P33` | Encoder M1, canal B |
| 25 | `P25` | Encoder M2, canal A |
| 26 | `P26` | Encoder M2, canal B |
| 27 | `P27` | `R_EN`+`L_EN` Motor 1 (atados) — era ENA |
| **19** | `P19` | `RPWM` Motor 1 — era IN1. ⚠️ **Movido de `P14` a `P19` (2026-09-12)**: GPIO14 tiene salida activa en el boot — ver abajo |
| 13 | `P13` | `LPWM` Motor 1 — era IN2 |
| 4 | `P4` | `R_EN`+`L_EN` Motor 2 (atados) — era ENB |
| 16 | `P16` | `RPWM` Motor 2 — era IN3 |
| 17 | `P17` | `LPWM` Motor 2 — era IN4 |
| — | `3V3` | VCC GY-521 + VCC ambos encoders |
| — | `GND` | GND lógico común |
| — | `5V` | Entrada 5V desde riel lógico (Buck) |

**Total: 12 GPIO usados.** Libres: 5, 18, 23 (full-featured) + **14** (libre, pero a evitar para motores) + 34, 35, SVP(36), SVN(39) (solo entrada). TX/RX (GPIO 1/3) reservados para debug USB.

### Pines a evitar en esta placa

| Etiqueta | GPIO | Motivo |
|---|---|---|
| `CLK, SD0, SD1, SD2, SD3, CMD` | 6–11 | **Nunca usar** — flash SPI interna, usarlos cuelga el chip |
| `SVP, SVN, P34, P35` | 36, 39, 34, 35 | Solo entrada — sin salida ni pull-up/pull-down interno |
| `P0, P2, P12, P15` | 0, 2, 12, 15 | Bootstrapping. GPIO12 en alto al arrancar puede impedir el boot |
| `P5` | 5 | Emite pulso al bootear — no usar para nada que mueva un motor |
| `P14` | 14 | **Salida activa durante el boot** — reportado consistentemente (junto con 1, 3, 5 y 15), **no en el datasheet oficial de Espressif**: sospecha fuerte, no dato cerrado. Era `RPWM` M1; **se movió a `P19` el 2026-09-12** porque mover un jumper no cuesta nada. Análisis completo en `plano-conexiones-esp32.html` §3 |

**Debug:** por USB serie (`Serial`). El reporte por WiFi sigue disponible y es útil con el robot en movimiento, pero ya no es obligatorio — no hay enlace entre chips que proteja TX/RX.

### 3.1 Zona de conexión del protoboard ESP32 (filas 20-30) — cableado real, confirmado 2026-08-05

Reemplaza la asignación de filas propuesta originalmente (nunca cableada). Detalle
visual completo en `obsoleto/plano-protoboard-esp32.html` (⛔ deprecado). Notación: columna+fila donde
aterriza cada cable (columnas A-E y F-J son tiras independientes; dentro de una
misma tira, cualquier columna de esa fila es el mismo nodo).

| Señal | Fila ESP32 (pin) | Fila zona de conexión | Nota |
|---|---|---|---|
| SDA (P21) | 14 (C) | 23 (C) | → GY-521 SDA |
| SCL (P22) | 17 (A) | 24 (A) | → GY-521 SCL |
| ENB (P4) | 7 (E) | 27 (E) | → L298N ENB |
| IN4 (P17) | 8 (D) | 28 (D) | → L298N IN4. Pull-down 10kΩ: A28 → GND lógico (directo, sin fila) |
| IN3 (P16) | 9 (C) | 29 (C) | → L298N IN3. Pull-down 10kΩ: A29 → GND lógico (directo, sin fila) |
| IN2 (P13) | 5 (G) | 21 (G) | → L298N IN2. Pull-down 10kΩ: I21 → GND lógico (directo, sin fila) |
| IN1 (P14) | 8 (H) | 22 (H) | → L298N IN1. Pull-down 10kΩ: I22 → GND lógico (directo, sin fila) |
| ENA (P27) | 9 (H) | 23 (I) | → L298N ENA |
| Encoder M2 canal B (P26) | 10 (F) | 24 (F) | |
| Encoder M2 canal A (P25) | 11 (H) | 25 (H) | |
| Encoder M1 canal B (P33) | 12 (F) | 26 (F) | |
| Encoder M1 canal A (P32) | 13 (J) | 27 (F) | |
| 5V (entrada) | 1 (F/J) | 30 (F/G) | Jumper corto directo riel lógico(+)→VIN en J1 (ver 3.2). Puente adicional 30→28 alimenta la pata + de C4 |
| C4 pata + | — | 28 (F) | Alimentada por el puente desde la fila 30 |
| C4 pata − | — | directo a GND lógico | Sin fila numerada |
| 3V3 | 19 (G) | 29 (G) | → GY-521 VCC + Encoder M1 VCC + Encoder M2 VCC |

⚠️ **Corregido durante el cableado**: un borrador intermedio tuvo 5V (F30) y GND
(J30) en la misma fila — como F y J son la misma tira de 5 agujeros, eso era un
corto directo, no dos nodos distintos. Se corrigió antes de energizar: la fila 30
quedó solo con 5V, y el retorno GND de C4 pasó a ir directo al cable de GND lógico.

### 3.1b Rearmado del protoboard (2026-09-12) — ⏳ en curso, reemplaza parcialmente a 3.1

**3.1 describía el protoboard anterior al rearmado del chasis (2026-09-03) y a este
segundo rearmado del protoboard en sí** — ambos hechos, no solo el primero. Lo que
sigue es lo confirmado fila por fila en esta sesión; falta terminar de cablear señal
de motores (`RPWM`/`LPWM`/`R_EN`+`L_EN`) y las filas exactas de cada canal de
encoder, así que esta tabla **no reemplaza completa a 3.1 todavía** — solo las filas
que ya están confirmadas abajo.

| Señal | Fila (columna) | Nodo / destino |
|---|---|---|
| GND ESP32 | 1 (A) | Riel "−" del protoboard (GND lógico común) |
| 3V3 (GY-521 + 2 encoders) | 3 (A) | Unida por jumper a fila 12 — mismo nodo de 3V3 |
| 3V3 (pin del ESP32) + C6 | 12 (A) | Unida por jumper a fila 3. C6 → riel "−" |
| `EN` (reset ESP32) + C_en | 13 | Fila consecutiva a la 12 (confirma serigrafía de fábrica `3V3`→`EN`). C_en → riel "−" |
| VIN | 30 (B) | Riel de 5V del Buck |
| SDA (GPIO21) | 17 (J) | → GY-521 SDA |
| SCL (GPIO22) | 14 (J) | → GY-521 SCL |
| Encoders (los 4 canales) | columna A | Filas exactas sin especificar todavía |
| `VCC` de cada BTS7960 | — | Riel de 5V del Buck (mismo que VIN) |
| `GND` de cada BTS7960 (header de señal) | — | Riel "−" (GND lógico común) — ⏳ **pendiente medir continuidad `B−`↔`GND` del header en cada módulo antes de confirmar esta unión como definitiva** (ver 8.4) |

**Señal de motores — reordenada 2026-09-12: M1 al lado derecho (F–J), M2 al lado
izquierdo (A–E) del protoboard**, para simplificar el tendido físico de cables hacia
cada driver. Los GPIO no cambian (siguen siendo los de la tabla de la sección 3);
lo que cambia es de qué lado del canal central sale cada cable, usando jumpers
cortos donde hace falta cruzar de un lado al otro.

| Señal | Fila (columna) | Pull-down (10kΩ) |
|---|---|---|
| `R_EN`+`L_EN` M1, atados (GPIO27) | 10 (H, I — mismo nodo, tira F–J) | J10 → riel "−" |
| `LPWM` M1 (GPIO13) | 8 (I) | J8 → riel "−" |
| `RPWM` M1 (**GPIO19**, no 14) | 6 (I) | J6 → riel "−" |
| `R_EN`+`L_EN` M2, atados (GPIO4) | 9 (C, B — mismo nodo, tira A–E) | A9 → riel "−" |
| `LPWM` M2 (GPIO17) | 7 (B) | A7 → riel "−" |
| `RPWM` M2 (GPIO16) | 5 (B) | A5 → riel "−" |

⚠️ **Las pull-downs de `EN` quedaron soldadas sobre el protoboard, no en el header
de cada IBT-2 como establecía la decisión original** (sección 8.4 / CLAUDE.md) —
cambio deliberado por dificultad práctica de soldar en el header. **Consecuencia
aceptada, no un pendiente reabierto**: estas 2 pull-downs nuevas protegen "desde
el protoboard hacia adelante", igual que las 4 viejas — si el cable entre el
protoboard y el pin `EN` del driver se afloja, ese tramo puntual queda sin
protección por hardware. Sigue siendo mejor que no tener pull-down, simplemente no
cierra el "último tramo" que la ubicación en el header hubiera cerrado.

✅ **Las pull-downs en `RPWM`/`LPWM` no son una adición nueva**: son las mismas 4
pull-downs históricas del L298N (protegían `IN1`–`IN4`), que quedan en esas líneas
porque la migración L298N→BTS7960 reusó los mismos GPIO y solo cambió a qué pin del
módulo llega cada cable. Documentado en `plano-conexiones-esp32.html` y coherente
con "conservar igual las 4 viejas" del CLAUDE.md. Total: 6 pull-downs (4 viejas +
2 nuevas), como estaba previsto desde el principio — no hace falta remover nada.

Verificaciones con multímetro ya hechas sobre este armado: ver 7.25 (C6/C_en, sin
corto). **Falta**: QC de continuidad completo estilo 3.2 sobre todo este armado
nuevo antes de la primera energización, y la medición `B−`↔`GND` de cada driver.

---

### 3.2 QC de continuidad y primera energización — ✅ aprobado (2026-08-05)

Chequeo completo antes de energizar, con el sistema apagado (multímetro):

| Chequeo | Resultado |
|---|---|
| 5V vs GND lógico | Sin continuidad ✅ |
| 3V3 vs GND lógico | Sin continuidad ✅ |
| Polaridad de C4 | Respetada ✅ |
| Resistencias pull-down (I21, I22, A28, A29 vs GND) | ~9.9kΩ las 4 ✅ (nominal 10kΩ) |
| Puentes de señal (muestra: SDA, Encoder M1 canal A, IN1) | Continuidad ✅ |
| ENA vs IN1 (deben ser nodos distintos) | Sin continuidad ✅ |

**Primera energización — un problema encontrado y corregido**: al medir voltaje en
la fila 1 (VIN del ESP32) con el sistema ya energizado (Buck confirmado a 4.98V en
sus propios terminales), la lectura ahí daba solo **0.7V** — señal de circuito
abierto (probable fuga/diodo de protección captada por la alta impedancia del
multímetro, no una conexión real). Se aisló por bisección desde la fuente: bornera
del nodo estrella (12.32V, ok) → salida del Buck (4.98V, ok) → fila 1 (0.7V, **acá
está el corte**). El jumper entre la bornera externa (donde llegan los 5V del Buck)
y la fila 1 del protoboard no estaba haciendo contacto real.

**Fix**: jumper corto directo nuevo entre el riel lógico (+) y el pin VIN del ESP32
(columna J, fila 1), reemplazando el tramo problemático.

**Verificado tras el fix**: VIN = **4.8V**, fila 29 (3V3) = **3.4V** — ambos dentro
de rango sano (la pequeña caída respecto a los 4.98V/3.3V de referencia es
resistencia de cables/conectores, no un problema). C4 sigue con su pata + en fila
28 (alimentada por la cadena fila 1 → 30 → 28, ya validada) y su pata − directa a
GND lógico, sin cambios respecto a 3.1.

⚠️ **Lección que aplica a futuras conexiones de alimentación en este protoboard**:
una lectura de voltaje "rara" (ni 0V limpio ni el valor esperado, algo intermedio
como este 0.7V) es más probable que sea un circuito abierto con fuga parásita que
una conexión "parcialmente buena" — priorizar el chequeo de continuidad del tramo
sospechoso antes de asumir que el problema es otra cosa (regulador, carga, etc.).

---

## 4. Seguridad — pull-downs en las entradas del driver

✅ **CERRADO 2026-09-12 — las 6 pull-downs están instaladas** sobre el protoboard
rearmado. Filas en 3.1b. El texto de abajo conserva el fundamento (sigue vigente) y
el estado anterior como registro.

⛔ **El problema que esto resolvió (planteado 2026-08-29).** Las 4 pull-downs
originales protegen los pines que en el L298N eran IN1–IN4. Con el BTS7960 esos
mismos GPIO pasan a ser `RPWM`/`LPWM`, y **el pin que corta el puente es `EN`**, que
hasta el 2026-09-12 **no tenía pull-down**. Tal como estaba, la protección por
hardware ante un cuelgue del ESP32 **no cubría el caso que importa**.

Fundamento (datasheet oficial Infineon BTS7960, tabla de verdad 4.4.5): con
`INH`/`EN` en bajo, **ambos MOSFET quedan apagados sin importar el estado de `IN`**
("Stand-by mode"). Textual, 4.4.1: *"To deactivate both switches, the INH pin has to
be set to low."* O sea: la pull-down en `EN` es necesaria **y suficiente**; las de
`RPWM`/`LPWM` por sí solas no apagan nada.

| Componente | Valor | Conexión | Estado |
|---|---|---|---|
| 4× resistencia | 10kΩ (marrón-negro-naranja-dorado) | Una por cada `RPWM`/`LPWM` (ex IN1–IN4) → riel "−" | ✅ **Reinstaladas 2026-09-12** en el protoboard rearmado: J8 (`LPWM` M1), J6 (`RPWM` M1), A7 (`LPWM` M2), A5 (`RPWM` M2). Antes del rearmado estaban en filas 21/22/28/29 |
| **2× resistencia** | **10kΩ** | **Una por cada `EN` (nodo `R_EN`+`L_EN` atados) → riel "−"** | ✅ **Instaladas 2026-09-12**: J10 (M1), A9 (M2) |

✅ **Conservar las 4 existentes**: con `EN` en bajo el puente ya está apagado, pero
dejar `RPWM`/`LPWM` definidos en bajo no cuesta nada y evita entradas flotantes
durante el boot. Defensa en profundidad, no redundancia inútil.

✅ **Desbloqueadas por V2 (2026-09-10, `plan-cableado-senales.html`)**: se midió si el
módulo IBT-2 trae pull-up interno en `EN`, porque un pull-up resistivo habría armado
un divisor con el pull-down de 10kΩ dejando el pin en ~2.5V en vez de un bajo real.
Resultado en los dos módulos: **no es un pull-up resistivo** — `R_EN` y `L_EN` dan
~1.6MΩ en un sentido y `OL` en el otro, patrón de diodo/clamp ESD hacia `VCC`. El
pull-down de 10kΩ es seguro.

⚠️ **Ubicación: protoboard, no header.** La decisión previa era soldar las 2 nuevas
directo en el header de cada IBT-2 para cubrir el tramo de cable protoboard→driver
que las 4 viejas no cubren. Se desestimó por dificultad práctica de soldar ahí. Las 6
protegen "desde el protoboard hacia adelante" — limitación conocida y aceptada, no un
pendiente abierto.

⚠️ El datasheet (6.2, "Layout Considerations") recomienda además **resistencia serie
de ~10kΩ en cada línea digital** de entrada, contra picos inducidos. Verificar si el
módulo IBT-2 ya las trae en placa antes de agregarlas — hay resistencias SMD visibles
junto al header.

⚠️ **Ubicación real distinta a la recomendación original**: se instalaron en el
protoboard del ESP32 (filas 21, 22, 28, 29 — ver 3.1), no en el header del L298N
como se había sugerido. Siguen siendo muy superiores a no tener pull-down, pero la
protección cubre "desde el protoboard del ESP32 hacia adelante" — si el cable entre
esa fila y el pin IN del L298N se afloja, ese tramo puntual queda sin cubrir. Si en
algún momento se quiere cerrar ese último tramo, moverlas al header del L298N lo
resolvería.

Cuando el ESP32 se resetea o cuelga, sus GPIO quedan en alta impedancia. Los
pull-down garantizan **por hardware** que las entradas del L298N caigan a nivel bajo
y los motores se detengan, sin depender de software. Cubre además las entradas
flotantes cuando el micro no las maneja y cualquier glitch de GPIO durante el boot.

---

## 5. Motor y encoders

**Motorreductor: JGB37-520B, 6V, 793 RPM** (confirmado 2026-07-30). La lectura previa de "160RPM, 12V" correspondía a una foto de un motor similar pero distinto al instalado — descartada. 793 RPM es nominal a 6V sin carga; no asumir que se sostiene bajo carga real (ver 7.7).

### Encoders — mapeo de colores (confirmado por datasheet + medición)

| Color | Función | Conectado a |
|---|---|---|
| Rojo | Motor power + | Comparte pestaña con cable grueso de potencia del motor |
| Blanco | Motor power − | Comparte pestaña con cable grueso de potencia del motor |
| Negro | Encoder GND | Riel GND lógico |
| Azul | Encoder VCC | Riel 3V3 (desde ESP32) |
| Amarillo | Canal A | GPIO 32 (M1) / GPIO 25 (M2) |
| Verde | Canal B | GPIO 33 (M1) / GPIO 26 (M2) |

**Ambos canales conectados** desde la migración a ESP32. La limitación anterior a un solo canal era por escasez de pines del ESP8266. Tener A y B habilita detección de **sentido** de giro, no solo de velocidad.

⚠️ **Sin confirmar**: si la salida de los encoders es push-pull o colector abierto. Por eso están asignados a pines full-featured (con pull-up interno disponible) y no a los de solo-entrada, que no lo tienen. Si se confirma que son push-pull, se podrían mover a 34/35/SVP/SVN y liberar 4 pines.

✅ **PPR confirmado (2026-08-05, Test 3 Fase 0)**: 69.1 pulsos/vuelta (decodificación 1x) en ambos motores. Ver 7.11. Datasheet exacto del motor sigue sin cargarse, pero el dato que importaba (PPR) ya no es una suposición.

---

## 6. Capacitores del sistema

⚠️ **Reemplazada 2026-09-03 — rearmado del chasis.** El armado físico del protoboard se rehizo desde
cero y en el proceso **se perdieron todos los electrolíticos** (C1, C4, C5). Lo único que sobrevivió
son C2/C3, porque están soldados directo en las pestañas de cada motor, no en el protoboard. La tabla
de abajo es el inventario real a esta fecha, no el que había antes del rearmado.

**Stock disponible para reconstruir (confirmado 2026-09-03):** electrolíticos **1000µF/16V**,
**1000µF/50V** y **100µF/25V**, y cerámicos marcados **104** — código EIA, 10×10⁴ pF = **100nF**, el
valor que hace falta para todo el desacople de alta frecuencia de esta sección. También hay marcados
**101** (100pF) — demasiado chicos para esto, no confundir con el 104.

**Criterio de reparto del stock**: 16V donde el nodo es de ~5V (margen 3.2×, de sobra); en el nodo
estrella (~12.6V llena) el 100µF/25V calza con el objetivo ya fijado en este proyecto de subir C1 a
"25V/35V" — con el de 16V el margen quedaría en 1.27×, insuficiente. El 1000µF/50V queda libre como
reserva (por ejemplo, bulk adicional en `B+`/`B−` de algún driver si en algún momento aparece evidencia
de caída de bus real — hoy no la hay, ver 7.20).

Detalle completo del razonamiento (por qué duplicar bulk antes no resolvió los reinicios, la sospecha
del margen de *dropout* del LDO de 3.3V, y dónde ubicar cada capacitor en el protoboard) en
[`plano-conexiones-esp32.html`](plano-conexiones-esp32.html); versión de una sola hoja para tener a
mano mientras se cablea en [`plano-imprimible-conexiones-esp32.html`](plano-imprimible-conexiones-esp32.html).

| ID | Valor | Ubicación | Propósito / estado |
|---|---|---|---|
| C1 | 100µF, **25V** | **Bornera `IN+`/`IN−` del Buck**, como conductor separado junto al cable combinado que viene de la soldadura (nodo estrella real, ver 1) — no en la soldadura misma | ⛔ **Instalado 2026-09-03, dañado en el incidente de polaridad invertida (2026-09-10, ver 7.24) — reemplazo con repuesto en stock, pendiente.** Amortigua picos de arranque/frenado del tramo batería→fusible→switch. Menos capacidad que el 1000µF original, pero cada IBT-2 ya trae 330µF de fábrica pegado a su `B+`/`B−` (660µF entre los dos) — ese es el reservorio que importa en la conmutación; C1 aguas abajo de la soldadura es secundario. El de 25V calza con el objetivo ya fijado más abajo en esta sección. La bornera del Buck solo tuvo que alojar 2 conductores (cable combinado + C1), no 4 — cierra en la práctica el conflicto de 1/8.3 |
| Cbus | 330µF, en placa | `B+`/`B−` de cada IBT-2 | ✅ **De fábrica**, uno por módulo (electrolítico negro visible) — no depende del rearmado del chasis, verificar igual que siga soldado. No hace falta bulk externo por driver para arrancar; si aparece caída de bus con carga real, ahí sí sumar uno externo directo sobre `B+`/`B−` |
| C2 | 100nF cerámico | Pestañas M1/M2, Motor 1 | ✅ **Sobrevivió el rearmado** — soldado al motor, no al protoboard. Supresión de ruido de escobillas |
| C3 | 100nF cerámico | Pestañas M1/M2, Motor 2 | ✅ **Sobrevivió el rearmado**, ídem C2 |
| C4 | 1000µF, **16V** (reservado el de 50V para C1) | Protoboard ESP32, **fila 30**: columna A = pata +, columna B = cable a `VIN` del ESP32, columna C = jumper en U desde el carril de 5V — las tres son la misma tira, mismo nodo. Pata − al riel GND del protoboard | ✅ **Instalado (2026-09-03).** Junto con C5, define si hay algo de bulk en el riel del ESP32. ⏳ **Pendiente verificar con multímetro** continuidad A↔C y GND del protoboard ↔ GND lógico común antes de dar la ubicación por completamente cerrada (misma lección de 3.2 y 7.6b) |
| C5 | 1000µF, **50V** | Terminales de salida del Buck (5V), antes de dividirse hacia los 2 IBT-2 y hacia el riel del ESP32 | ⛔ **Reinstalado 2026-09-03, sospechado tras el incidente de polaridad invertida (2026-09-10, ver 7.24)** — sin hinchazón visible pero en el camino directo de la falla; reemplazo por precaución con repuesto en stock, pendiente. Es más tensión de la que este nodo de 5V necesita, pero no daña, solo ocupa más lugar |
| C6 | 104 (100nF cerámico) | **Fila 12 del protoboard rearmado** (nodo 3V3 del pin del ESP32, unido por jumper a la fila 3 donde entran GY-521 + 2 encoders) ↔ riel "−" | ✅ **Instalado (2026-09-12)**, sobre el armado nuevo del protoboard — reemplaza la ubicación propuesta (fila 25/26) de `plano-conexiones-esp32.html` §4.4, escrita para el protoboard anterior al rearmado. Verificado con multímetro (ver 7.25): 135kΩ fila 12↔"−", sin corto. |
| C7 | 100nF cerámico | Carril 5V/GND del Nano | ✅ **Cerrado (2026-08-05): sin función** — el Nano no se usa. Estado físico tras el rearmado sin confirmar, pero irrelevante: no protege nada del lazo actual |
| C9 *(nuevo)* | 104 (100nF cerámico) | Fila 30-A, **soldado junto con C4** (mismo empalme: pata a pata, + con + y − con −) | ✅ **Instalado (2026-09-06).** Filtra lo que C4/C5 no alcanzan por la resistencia parásita del cableado entre ambos |
| C<sub>en</sub> *(nuevo)* | 104 (100nF cerámico) — **no más** | **Fila 13 del protoboard rearmado** (pin `EN`/reset del ESP32, fila consecutiva a la del 3V3 — confirma la serigrafía de fábrica de la NodeMCU-32S) ↔ riel "−" | ✅ **Instalado (2026-09-12)**, sobre el armado nuevo del protoboard — reemplaza la ubicación propuesta (H18/H20) de `plano-conexiones-esp32.html` §4.4, escrita para el protoboard anterior al rearmado. Ataca directo el reset espurio por ruido. Verificado con multímetro (ver 7.25): abierto/sin lectura fila 13↔"−", sin corto. Un valor mayor a 100nF puede romper el auto-reset de la carga por USB — respetado |
| C10 / C11 *(nuevos)* | 104 (100nF cerámico) ×2 | Soldados directo en el header de señal de cada IBT-2, entre `VCC` y `GND` — no en el protoboard del ESP32 | ✅ **Instalados (2026-09-06)**, uno por driver. Los MOSFET del BTS7960 conmutan más rápido que los BJT del L298N: más di/dt inyectado al mismo riel de 5V que alimenta al ESP32 |

⚠️ No hay cerámico de 10µF en stock — se evaluó un C8 (10µF, en paralelo con C6) como opción opcional
y quedó descartado por falta de ese valor, sin pérdida grave.

---

## 7. Registro de pruebas y valores obtenidos

> ## 🚧 Frontera PRE / POST dentro de esta sección
>
> El rearmado del robot (chasis 2026-09-03, protoboard 2026-09-12) y el cambio de driver
> (L298N → 2× BTS7960, 2026-08-29) parten este registro en dos. Ver el bloque "FRONTERA"
> al principio de `../CLAUDE.md`.
>
> | Subsecciones | Época | Cómo usarlas |
> |---|---|---|
> | **7.1 – 7.21** | ⛔ **PRE** — L298N + chasis de contrachapado | **Solo para repasar errores y método.** Ningún valor de acá sirve como cantidad para el robot nuevo: `ω₀`=7.4, `m·l`=0.1265, `J`=0.0227, `K_U`=0.0123/0.0326/0.0218, `θ_umbral`≈3.65°, cap de PWM 160, todos los umbrales de duty. |
> | **7.22 – 7.26** | ✅ **POST** — balsa + BTS7960 | Valores vigentes: `ω₀`=8.0 (7.22), `m`/`m·l`/`l`/`J` (7.23), C6/C_en (7.25), lazo de masa corregido (7.26). |
>
> **Excepciones que sobreviven a las dos épocas** (propiedades del sensor o del método, no
> de la planta): **7.5** y **7.10** (calibración y ejes del MPU6050), **7.11** (PPR 69.1 y
> sentido de giro de los encoders), **7.13** (lección de alimentación parásita por I2C),
> **7.17** (el *método* de balanza y de dos apoyos — es el que hay que reusar para `K_U`),
> y las lecciones de método de 7.12, 7.19 y 7.20.


### 7.1 Validación de motores y encoder (antes de energizar el driver) — ✅ vigente

| Prueba | Método | Resultado | Interpretación |
|---|---|---|---|
| Resistencia M1↔M2 (bobinado del motor) | Multímetro, modo Ω | **3.5Ω** | Normal — resistencia de bobinado, sin cortocircuito |
| Continuidad del capacitor cerámico retirado | Multímetro, modo continuidad | Sin continuidad | Capacitor sano, no era causa de falsa lectura |
| Voltaje VCC del encoder alimentado a 3.3V | Multímetro, modo voltaje DC | **3.28V** | Dentro de rango esperado |
| Canal A (amarillo) al girar el eje a mano | Multímetro, modo voltaje DC | Cambia de valor | Canal funcional |
| Canal B (verde) al girar el eje a mano | Multímetro, modo voltaje DC | Cambia de valor | Canal funcional — **ahora sí se usa** (ESP32 tiene pines de sobra) |

### 7.2 Validación del jumper de 5V del L298N — ✅ vigente

| Prueba | Resultado | Interpretación |
|---|---|---|
| Continuidad regulador→salida 5V con jumper removido | Sin continuidad | Confirmado: era el jumper correcto. Removido de forma **permanente**. |

### 7.3 Chequeo de continuidad global del sistema (sin batería) — ✅ vigente

| Prueba | Método | Resultado | Interpretación |
|---|---|---|---|
| V+ ↔ GND, sistema completo | Multímetro, modo Ω | **2.5MΩ** | Sin evidencia de corto — consistente con fuga normal de electrolíticos en paralelo |
| V+ ↔ GND, sin el Buck conectado | Multímetro, modo Ω | **1MΩ** | Confirma que la fuga es de los capacitores (C1 principalmente), no un corto real |

### 7.4 Calibración del Buck XL4016 — ⛔ SUPERADO: esta unidad fue destruida (7.24)

⛔ **Los 4.98V de esta sección son de la unidad vieja, destruida por polaridad
invertida el 2026-09-10.** La unidad vigente está **calibrada en 5.01V**
(2026-09-12, ver 7.24). No usar el 4.98V como referencia en ningún cálculo ni
como criterio de "está bien calibrado".

| Prueba | Condición | Resultado (unidad vieja) |
|---|---|---|
| Voltaje OUT+/OUT− | Sin carga, recién ajustado | **4.98V** |
| Voltaje OUT+/OUT− | Con carga real (micro + GY-521 conectados) | **4.98V** (sin caída) |

⚠️ Medido con la carga del ESP8266, y sobre un módulo que ya no existe. **Repetir
el barrido de carga con el módulo nuevo y con el ESP32**, que tiene picos de
corriente mayores — sigue pendiente, y ahora además sobre hardware distinto.

### 7.5 Test 1 / Test 1b — MPU6050: ejes y calibración — ✅ vigente

Propiedades del sensor, **no cambian con el cambio de microcontrolador.**

**Test 1 (robot quieto, a nivel):**

| Magnitud | Resultado | Interpretación |
|---|---|---|
| Acel X/Y/Z | −0.22 / −0.03 / 10.51 m/s² | Z domina en reposo a nivel → Z = eje "arriba" |
| \|Aceleración\| | 10.51 m/s² (σ=0.016, estable) | Desvío de +7.1% vs 9.80665 — no es ruido, dispara la calibración |
| Pitch estimado | −0.1° | Coherente con robot a nivel |
| σ Giro X/Y/Z | 0.0102 / 0.0009 / 0.0007 | Bajo y comparable entre ejes → giroscopio sano |

**Test 1b — Pureza del eje X** (ventana de 5s, robot rotado a mano sobre el eje de las ruedas):

| Eje | Pico \|giro\| (rad/s) |
|---|---|
| X | 0.571 |
| Z | 0.137 |
| Y | 0.041 |

Razón X/max(Y,Z) = 4.2x → **X confirmado como eje de las ruedas.**

**Convención de ejes resultante**: **Z = arriba, Y = inclinación/pitch (frente-atrás), X = eje de las ruedas.**

**Test 1b — Calibración de acelerómetro** (6 capturas arriba/abajo por eje):

| Eje | Captura "arriba" (m/s²) | Captura "abajo" (m/s²) | Bias = (arriba+abajo)/2 | Escala = (arriba−abajo)/(2×9.80665) |
|---|---|---|---|---|
| X | 10.12 | −9.59 | **0.265** | **1.005** |
| Y | 9.86 | −9.76 | **0.050** | **1.000** |
| Z | 10.51 | −9.39 | **0.560** | **1.015** |

**Conclusión:** no hay error de escala global — los 3 ejes dentro de ±1.5%. El 10.51 m/s² del Test 1 se explica casi enteramente por el bias de Z: 9.80665×1.015+0.56≈10.51.

⚠️ Estos valores **no están aplicados en el firmware de control todavía** (Kalman/PID) — pendiente decidir dónde (candidato: antes de alimentar el Kalman). ✅ Sí están validados en hardware ESP32 real, ver 7.10.

### 7.6 Test 2 — Motores no responden pese a comandos correctos — ✅ vigente

| Prueba | Método | Resultado | Interpretación |
|---|---|---|---|
| Micro ejecutando el sketch | Monitor Serie | Reporta todas las fases correctamente | Firmware funcionando — el problema no está ahí |
| Motores durante la ejecución | Observación directa | No se mueven, ninguno de los dos | Falla aislada en la etapa L298N |
| Pin "5V" del L298N vs GND | Multímetro, modo voltaje DC | **0V** | **Causa raíz**: lógica interna del L298N sin alimentación |

**Diagnóstico**: con el jumper de 5V removido permanentemente, el pin "5V" del L298N pasa a ser una entrada que necesita alimentación externa para energizar la lógica interna (la que interpreta IN1–IN4/ENA/ENB). VIN+ tenía tensión, pero sin lógica alimentada el chip nunca conmuta las salidas.

**Fix aplicado**: cable desde el riel de 5V lógico (Buck OUT+) al pin "5V" del L298N. Verificado: con el cable puesto, los motores responden.

### 7.7 Test 2 — Zona muerta bajo carga real — ✅ vigente

**Método**: robot tethered (cuerda que limita el pitch a ±30°), ruedas apoyadas con carga real sobre alfombra. Barrido ascendente de duty.

| Prueba | Resultado | Interpretación |
|---|---|---|
| `TEST_DUTY=70` (≈3.46V) | Respuesta nula o marginal, inconsistente | Duty justo por debajo del umbral real bajo carga |
| Barrido, observación de patinaje | Sin patinaje visible | Descarta problema de tracción con la alfombra |
| Duty al que empieza a responder | **~3.8V** (duty ≈ 77/255) | **Zona muerta real bajo carga** |

**Conclusión**: no es problema de tracción. La alfombra no es la superficie final, así que no dispara cambios de diseño. El umbral de ~77/255 deja **37% de margen** hasta el cap de seguridad (121/255).

⚠️ Primera medición real de zona muerta, dato de referencia para la compensación en el PID. No aplicado en firmware. **M1 y M2 no se midieron por separado.**

### 7.8 Zona muerta v2 — en curso, medida en ESP32 con ruedas en el aire (2026-08-05)

La corrida de 7.7 midió M1+M2 juntos, solo en el piso, con el Nano. Repetido ahora
con `test/test2_motores_esp32/`, ruedas en el aire (sin carga). **Protocolo**:
correr dos veces, anotando cada corrida aparte:

| Condición | M1 | M2 |
|---|---|---|
| Ruedas en el aire (sin carga) | **duty 90–95** (~4.3–4.6V reales) | **duty 85–95** (~4.1–4.6V reales) |
| Ruedas en el piso, tethered (carga real) | *pendiente* | *pendiente* |

**Condiciones de la medición**: batería a **12.3V** en el momento del test (vs. los
12.6V que asume `dutyToVolts()` en el firmware — diferencia de ~2.4%, corrección
aplicada arriba). Voltajes leídos directo del Serial (`~X.XX V` calculado, no
multímetro), convertidos al valor real de batería.

⚠️ **Variabilidad entre ciclos, no es error de medición**: en corridas repetidas, a
veces ambos motores arrancaban juntos a duty 95, otras veces M1 arrancaba a 90 y M2
a 85. Es esperable en un motor DC con reductora — depende de en qué posición quedó
el rotor/engranajes al detenerse (cogging torque distinto por posición) y de si el
motor ya estaba tibio del ciclo anterior. El rango 85–95 es el dato útil, no un
valor puntual.

⚠️ **Discrepancia sin explicar todavía**: 85–95/255 es notablemente más alto que el
~77/255 de la medición combinada de 7.7 (Nano, ruedas en el piso). No se puede
atribuir todavía a una sola causa — candidatos: (a) la condición cambió, aire vs.
piso, con menos fricción de rodadura pero también sin el peso del chasis
precargando los rodamientos; (b) medir cada motor solo, sin que el otro tire de la
misma fuente, cambia la dinámica de arranque; (c) desgaste/lubricación distinta
desde julio. **Repetir la fila "piso, tethered" con este mismo firmware permitirá
aislar (a)** — si el piso da un valor similar a 77/255, el aire es la explicación;
si sigue por encima de 85, hay algo más adicional al efecto aire/piso.

La comparación aire/piso separa cuánto del umbral es fricción de rodadura real vs.
fricción de rodamientos.

⚠️ **Actualización 7.11**: la medición por encoder (Test 3) da umbrales más bajos
(60–99/116) que esta medición visual (85–95). Es consistente, no contradictorio —
el encoder detecta el arranque antes de que sea visible a ojo. Ver 7.11 para el
dato más preciso; esta sección queda como referencia histórica del método visual.

### 7.9 Test 4 — Enlace UART Nano→ESP8266 — ⛔ OBSOLETO (arquitectura deprecada)

**Este test ya no aplica**: con un solo ESP32 no hay enlace entre chips. Se conserva
el registro por su valor metodológico — el diagnóstico consumió ~4 sesiones y sus
lecciones siguen siendo válidas.

**Síntoma**: dirección ESP→Nano funcionaba al 100% (LED confirmaba cada PING). Dirección Nano→ESP (a través del divisor resistivo 1kΩ/2kΩ) recibía **0 bytes** en la mayoría de las corridas, pese a que topología, resistencias y voltaje estático medían bien repetidamente.

| Prueba | Resultado |
|---|---|
| `0x55` continuo, 408s, divisor original | 0 bytes — ningún byte, no era corrupción |
| Loopback TX↔RX propio del ESP8266 | 405/405 bytes — el ESP8266 en sí estaba sano |
| R1 medida aislada (fuera del circuito) | **0.98kΩ** — sana |
| R2+R3 soldadas, medidas aisladas | **1.98kΩ** — sanas |
| Nodo conectado por error a TX del ESP8266 (no RX) | Encontrado y corregido |
| `0x55` continuo, divisor reconstruido y topología corregida, sin mover nada | 0 bytes |
| Mismo test, **tras mover el protoboard del ESP8266** (sin tocar cables) | 32 bytes, último byte `0xFF` |
| Mismo test, poco después, sin cambios deliberados | 653 bytes, byte corrupto |

**Diagnóstico final**: contacto físico marginal, no error de diseño. Que el resultado cambiara al *mover la placa* (sin tocar ningún cable) y que los bytes recibidos fueran casi-correctos pero corruptos (`0xD5`, `0xFF` en vez de `0x55`) es la firma de un contacto mecánico intermitente. Sospecha final sin confirmar: la masa entre los dos protoboards.

**Lecciones que siguen aplicando a cualquier cableado del proyecto:**
1. Una medición estática correcta (resistencia, voltaje en reposo) **no garantiza** que una señal dinámica pase. El multímetro presiona el contacto y usa corriente mínima; una señal que conmuta miles de veces por segundo no perdona un contacto marginal.
2. Sacar y reinsertar un módulo entero del protoboard para flashearlo reintroduce riesgo de mal contacto en **todos** sus pines cada vez. Preferible desconectar solo los cables puntuales necesarios.
3. Instrumentar el firmware para distinguir "no llega nada" de "llega corrupto" (el contador de bytes crudos) fue lo que permitió descartar hipótesis en vez de adivinar.

### 7.10 Test 1 en ESP32 — validación de bias/escala y ejes en hardware real — ✅ vigente (2026-08-05)

Primera corrida del Test 1 portado (`test/test1_mpu_esp32/`), flasheado y corriendo sobre el ESP32 real (no el banco de pruebas original del ESP8266). Objetivo: confirmar que los valores de bias/escala de 7.5 siguen siendo válidos en el hardware nuevo, y reconfirmar la convención de ejes rotando el robot a mano por sus 3 ejes.

**Método**: robot rotado a mano por turnos, gravedad alineada primero con X, luego con Z, luego con Y, dejando que las estadísticas de sesión se acumularan sin resetear entre orientaciones (ver nota abajo).

| Hora | Eje alineado con gravedad | \|Acel\| cruda | \|Acel\| calibrada | Pitch | Interpretación |
|---|---|---|---|---|---|
| 9:41 | X (−9.60/−9.81 calib) | 9.63 | **9.82** | −0.4° | X confirmado — no es la orientación normal de operación |
| 9:46 | Z (10.46/9.75 calib) | 10.46 | **9.76** | −1.7° | **Orientación normal** — Z=arriba, coincide con la convención documentada |
| 9:48 | Y (9.56/9.51 calib) | 9.90 | **9.81** | 74.9° | Y confirmado como eje de pitch — pitch grande al inclinar sobre Y, como se espera |

**Conclusión**: calibrada da 9.76–9.82 m/s² en las 3 orientaciones (ideal: 9.80665) — **confirma que el bias/escala de 7.5 no eran específicos del banco de pruebas del ESP8266**, siguen siendo válidos en el ESP32. Convención de ejes (Z=arriba, Y=pitch, X=ruedas) reconfirmada en hardware real.

⚠️ **Nota metodológica**: las estadísticas de sesión (mín/máx/σ) no se resetearon entre las 3 orientaciones, así que los rangos amplios que se ven en pantalla (p. ej. \|Acel\| cruda entre 0.12 y 18.35, giro Z hasta 4.365 rad/s) son producto de **mover el robot a mano entre capturas**, no ruido del sensor en reposo. Para un piso de ruido limpio (útil al ajustar el Kalman), presionar "Reiniciar estadísticas" después de asentarse en cada orientación y esperar unos segundos antes de leer σ — no se hizo en esta corrida.

Bias de giroscopio recalibrado en cada captura, consistente entre sí (X≈−0.066 a −0.067, Y≈0.0120–0.0121, Z≈0.0003–0.0005) — coherente con estar todo dentro de la misma sesión sin apagar el micro.

---

### 7.11 Test 3 — Encoders: PPR, sentido de giro y zona muerta v3 — ✅ vigente (2026-08-05)

Primera corrida de `test/test3_encoders_esp32/`, ruedas en el aire. Cierra el PPR
(pendiente desde el inicio del proyecto) y reemplaza la zona muerta visual de 7.7/7.8
por una medida objetivamente por encoder.

**Fase 0 — PPR** (10 vueltas a mano, decodificación 1x):

| Motor | PPR (pulsos/vuelta) |
|---|---|
| M1 | 69.1 |
| M2 | 69.1 |

Coincide entre ambos motores — misma reductora/encoder en los dos. Cierra
"Datasheet exacto del motor nunca cargado — no asumir resolución/PPR" (sección 5).

**Fase 1 — sentido de giro**: M1 FWD=+219/+229 (dos corridas), M2 FWD=−195/−209 —
signos opuestos entre motores, confirmado **por diseño, no por error**: M1 y M2
están montados como espejo en los dos costados del chasis, así que giran en
sentidos rotacionales opuestos para producir el mismo avance neto del robot.
**Confirmado físicamente** (el usuario observó ambas ruedas empujando en el mismo
sentido durante la prueba). ⚠️ **Implicación para el firmware de control**: el
lazo de PID va a necesitar invertir el signo de uno de los dos motores al combinar
sus velocidades — no se puede sumar el conteo crudo de encoder de M1 y M2
directamente.

**Fase 2 — zona muerta por encoder** (barrido fino, `RAMP_START=60`, 5 repeticiones ×
2 corridas válidas, ruedas en el aire):

| Motor | Min | Max | Media | σ | n válido |
|---|---|---|---|---|---|
| M1 | 60 | 99 | 85.2 | ~15.4 | 15 |
| M2 | 60 | 116 | 83.9 | ~19.3 | 10 |

⚠️ **Una corrida de M2 descartada**: dio 5/5 repeticiones en exactamente duty 60
(σ=0.00) — estadísticamente inverosímil para fricción mecánica real (se esperaría
algo de dispersión). Candidato principal: backlash/juego de la reductora generando
pulsos falsos al primer escalón probado, sin rotación sostenida real. No se
reprodujo en la corrida siguiente (M2 dio 76–115 con varianza normal) — apoya que
fue un artefacto puntual, no el comportamiento real de M2.

⚠️ **Efecto de piso sin resolver del todo**: varias repeticiones tocaron
exactamente `RAMP_START=60` (el primer escalón del barrido) — para esas, el
umbral real podría ser más bajo, sin medir. Repetir con `RAMP_START` más bajo
(ej. 20-30) daría el piso real si hace falta más precisión.

**Pendiente**: repetir esta Fase 2 con ruedas en el piso/tethered para completar
el protocolo aire/piso de 7.8.

### 7.12 Medición de ω₀ del chasis viejo — ⛔ SUPERADO por 7.22 (2026-08-15, 7.4 rad/s)

> ⛔ **El valor vigente es ω₀ = 8.0 rad/s, ver 7.22 (2026-09-09).** Los 7.4 de acá
> **no están mal**: son del chasis viejo de contrachapado, de 26 cm, que ya no
> existe. El rearmado a balsa mide 22 cm y es más liviano. Todo el método y las
> lecciones de esta sección siguen valiendo — 7.22 los reusa y los extiende.

**Valor de esta sección: ω₀ = 7.4 rad/s** (T ≈ 0.845 s), medido sobre el montaje de
brackets rígidos. Es el **tercer** intento y el primero que resiste una
verificación física independiente. Reemplaza dos mediciones anteriores, ambas
inválidas: 4.07 rad/s (2026-08-10) y **2.33 rad/s (2026-08-13)**.

⚠️ El valor viejo de 2.33 rad/s circuló por todo el proyecto durante dos días y
alimentó el modelo de PSO. Como **K_U escala con ω₀²**, ese error valía un
factor 10 en todo lo que dependía de él.

#### Método vigente — pivote rígido + análisis de video

Robot colgado del eje de las ruedas mediante dos brackets de chapa atornillados a
una viga fija. La cupla hexagonal de rueda queda unida al bracket por un bulón
**flojo**, que trabaja como pasador: el cuerpo pivota libremente alrededor del eje
de las ruedas y la reductora no se retro-acciona. Cuerda de seguridad holgada, sin
tomar carga.

5 sueltas grabadas a 30 fps. El rastreo se hizo por **sustracción de fondo**
(mediana temporal por píxel), no por diferencia entre cuadros consecutivos: la
diferencia entre cuadros mide velocidad y da señal **cero justo en los extremos**,
que son los instantes que se necesitan. Los extremos se refinaron con una parábola
por 3 puntos para llegar a resolución sub-cuadro.

Se midieron **períodos completos** (extremo → extremo del mismo signo) y no
semi-períodos: el período completo no se ve afectado por una asimetría del punto de
equilibrio (un cable que tira para un lado alarga un semi-período y acorta el otro).

| Variante del análisis | T | ω₀ |
|---|---|---|
| ROI completo, corte de amplitud 8 px (n=13) | 0.834 s | 7.53 rad/s |
| ROI completo, corte 12 px (n=8, σ 1.6%) | 0.846 s | 7.43 rad/s |
| **ROI independiente, solo cuerpo superior** | 0.855 s | 7.35 rad/s |

- **Valor adoptado: ω₀ = 7.4 rad/s** (T ≈ 0.845 s)
- El tercer renglón es la verificación que importa: rastreando **solo el cuerpo del
  robot** y excluyendo la cuerda que cuelga, el resultado no cambia. Descarta que
  se estuviera midiendo el cable.

**Amortiguamiento**: la amplitud decae de forma **lineal** y la oscilación frena de
golpe. Es la firma de fricción **seca** (el bulón de acero en el agujero de chapa),
no viscosa. Conviene saberlo: la fricción de Coulomb **no corre la frecuencia de
oscilación**, así que no hay corrección de amortiguamiento que aplicar. El efecto
de amplitud grande (~26° al inicio) predice +1.3% y queda dentro del ruido.

#### Verificación física independiente — el test que cierra el caso

Longitud del cuerpo medida: **26 cm del eje de los motores a la batería** (el punto
más alejado del pivote).

**Predicción a priori**: una barra uniforme de 26 cm pivotada en un extremo tiene
`L_eq = 2L/3 = 17.3 cm`, y por lo tanto `T = 2π·√(L_eq/g) = 0.835 s → ω₀ = 7.52
rad/s`. Sale de un solo número, sin tocar el video. **Cae dentro del rango medido,
con 2% de error.** El robot se comporta casi exactamente como una barra uniforme de
su propia longitud: los motores pesados justo sobre el pivote tiran `L_eq` para
abajo, la batería en la punta la tira para arriba, y se compensan.

**Prueba inversa** — con `L_eq = (k_cg² + l²)/l` y `k_cg = L/√12 = 7.5 cm`, cada
valor de T exige un CG distinto:

| T | ω₀ | CG que exigiría (l) | ¿Posible? |
|---|---|---|---|
| **0.845 s** (esta medición) | **7.4** | **13.6 cm** = 52% del cuerpo | ✅ |
| 2.699 s (2026-08-13) | 2.33 | 180.7 cm, o bien 0.31 cm | ⛔ |
| 1.544 s (2026-08-10) | 4.07 | 58.3 cm, o bien 0.97 cm | ⛔ |

Las dos mediciones viejas exigen un CG **fuera del robot**, o un robot equilibrado
a **3 mm del eje de las ruedas**. Un cuerpo balanceado a 3 mm casi no tiene torque
restitutivo: colgaría sin orientación preferida. En el video vuelve con decisión a
un equilibrio bien definido. Quedan descartadas por las dos ramas.

Herramientas del análisis en `algoritmos_evolutivos_pso/video_omega0/`; videos
fuente en `experiments/` (`WhatsApp Video 2026-08-15 at 1.18–1.19 PM`).

⚠️ **Limitación de los clips**: entre el 30% y el 80% de cada clip es el robot
sostenido con el hilo antes de soltar. Quedaron 2–3 períodos libres por clip.
Alcanzó porque el período resultó corto. Si se repite, arrancar la grabación justo
antes de soltar: con este período, 15 s de caída libre dan ~17 períodos y bajarían
el error por debajo del 1%.

#### ⛔ Intento descartado (2026-08-13): ω₀ = 2.33 rad/s — INVÁLIDO

**Qué se hizo**: robot suspendido del eje de las ruedas pero **sostenido a mano**,
no sobre un pivote rígido. 3 sueltas analizadas por video, T medio = 2.699 s
(σ ≈ 1.5%) → ω₀ = 2.33 rad/s.

**Por qué es inválido**: T = 2.699 s corresponde a un péndulo simple equivalente de
**1.81 m**. El robot mide 26 cm. Lo que osciló no fue el robot alrededor de su eje
de ruedas, sino un sistema mucho más largo — presumiblemente el brazo de quien lo
sostenía, con el robot colgando de él.

⚠️ **Lección metodológica**: es la **segunda vez seguida** que una medición de ω₀
con dispersión baja resulta ser de la cantidad equivocada (la primera fue la de
4.07 rad/s, abajo). La σ de 1.5% se volvió a leer como señal de calidad. La
conclusión práctica: **la repetibilidad no valida nada, hay que validar contra la
física.** El chequeo de `L_eq` cuesta una línea de aritmética y habría descartado
las dos mediciones en el día, sin repetir ningún experimento:

> `L_eq = g·(T/2π)²` tiene que dar del orden del tamaño del cuerpo.
> Si da metros para un robot de 26 cm, no se midió el robot.

**Aplicar este chequeo a cualquier medición futura de ω₀ antes de darla por buena.**

#### ⛔ Intento descartado (2026-08-10): ω₀ = 4.07 rad/s — INVÁLIDO

**Qué se hizo**: se ató una cuerda al eje de los motores, con el otro extremo
anclado a ~40 cm **fuera del robot**, y se cronometraron 7 repeticiones de 10
oscilaciones (media 15.439 s → T = 1.544 s → ω₀ = 4.07 rad/s).

**Por qué es inválido**: ese montaje no es un péndulo simple oscilando sobre el eje
de las ruedas — es un sistema de **dos péndulos acoplados** (la cuerda oscilando
desde su anclaje externo, más el robot oscilando y rotando en su punta). El período
medido corresponde a ese sistema compuesto, no a la magnitud que se buscaba. El
chequeo de `L_eq` también lo habría detectado: 1.544 s → 59 cm, más del doble del
cuerpo.

⚠️ **Lección metodológica**: la baja dispersión entre repeticiones (0.8%) se
interpretó originalmente como señal de que la medición era buena. Fue un error de
razonamiento: **una medición muy repetible de la cantidad equivocada sigue siendo
la cantidad equivocada**. La consistencia solo indica que el montaje era estable,
no que midiera lo correcto. El bamboleo observado —atribuido entonces a un pivote
"no perfectamente rígido"— era en realidad el segundo grado de libertad del sistema
acoplado, es decir, la evidencia del problema real.

#### Consecuencias del valor nuevo

`ω₀²` pasa de 5.43 a **54.8** — un factor 10 sobre todo lo que depende de él:

- **El umbral de K_U se multiplica por 10.** Para recuperarse de 5° con duty 160:
  `K_U ≥ θ·ω₀²/u_max = 0.0873 × 54.8/160 = 0.030`, no 0.0030. El 0.03 que el
  notebook asumía a ojo resulta estar justo en el límite.
- **La cota que dejó el Test 5 (K_U ≈ 0.001) queda 30× corta.** Esa medición era
  inválida y probablemente subestima mucho, pero la vara subió un orden de
  magnitud: medir K_U pasa de "confirmar un margen" a **decidir si el robot puede
  balancear con estos motores**.
- **El robot es un péndulo corto.** L_eq = 17 cm: se comporta como una regla, no
  como un palo de escoba. Constante de divergencia `1/ω₀ = 0.135 s` — de 1° a 10°
  en ~0.31 s. El lazo de control tiene que ser bastante más rápido de lo que
  sugería el modelo viejo.
- **Las ganancias PID que optimizó el PSO son de otra planta** — el notebook corría
  sobre ω₀ = 2.33.

**Uso**: el valor vigente (7.4 rad/s) alimenta el modelo simulado del péndulo
invertido para la optimización por PSO del proyecto de Algoritmos Evolutivos I.

### 7.13 Sesión de diagnóstico 2026-08-14 — el AP no levantaba y los motores no giraban

Sesión larga de diagnóstico. Se registra la cadena completa porque varias hipótesis
intermedias fueron **incorrectas** y saber cuáles se descartaron evita repetirlas.

**Síntoma inicial**: el AP WiFi del Test 5 no aparecía. Con USB parecía funcionar,
con batería no.

**Causa raíz final**: el GY-521 no estaba alimentado (VCC desconectado durante las
mediciones). Sin MPU, `mpu.begin()` falla y el sketch queda en su `while(1)` de
error **antes** de llegar a `WiFi.softAP()` — por eso nunca había AP.

#### ⭐ Lección principal: alimentación parásita por el bus I2C

**Un módulo I2C sin VCC puede seguir contestando en el bus.** La corriente entra por
los diodos de protección ESD de los pines SDA/SCL (que están en alto por los
pull-ups) y alimenta el riel interno del chip a ~1-2.7V con unos pocos µA.

Firma del problema, tal como se vio acá:

| Observación | Explicación |
|---|---|
| LED del módulo apagado | Un LED pide mA; la vía parásita da µA |
| `WHO_AM_I` responde correcto (0x68) | Leer un registro estático consume µA |
| `mpu.begin()` falla | Hace reset y despierta giro/acelerómetro — consumo real |
| Escaneo I2C con fantasmas en 0x06/0x07 | Direcciones reservadas: nadie puede vivir ahí. Es el chip semi-alimentado contestando cualquier cosa |
| Riel 3V3 en ~1V | Fila flotante levantada solo por la inyección parásita |

⚠️ **"El sensor contesta por I2C" NO prueba que esté alimentado.** Verificar VCC con
multímetro, o mirar el LED del módulo. Es hermana de la lección ya registrada en
7.9 (que el Serial/USB funcione no prueba que el resto del cableado esté bien).

#### Hipótesis descartadas durante el camino

| Hipótesis | Por qué se descartó |
|---|---|
| Capacitor nuevo en corto / polaridad invertida | Se retiró y se probó con otro: mediciones idénticas |
| Corto entre 5V y 3V3 en el protoboard | Medido: 0.63MΩ. Sin corto |
| Corto franco 5V↔GND | Nunca bajó de ~360Ω. Un corto real da pocos ohms |
| L298N cargando el riel lógico | Se desconectó su pin "5V": sin cambio (373Ω) |
| Contacto marginal en el bus I2C | El patrón era **perfectamente reproducible**, no aleatorio. Un contacto marginal varía al azar |
| Pin 3V3 del ESP32 sin contacto con fila 19 | Con batería la misma fila da 3.49V constantes |

#### Sobre alimentar por USB solo

Con USB como única fuente el riel se derrumba (VIN 2.2V, 3V3 ~1V) porque el USB
intenta energizar **hacia atrás** la salida apagada del Buck (XL4016 back-driven +
C5 de 1000µF colgando). Explica también los ~360Ω medidos entre 5V y GND, que no
son un corto sino el camino pasivo del Buck.

- ✅ **Configuración correcta para debug**: **batería + USB simultáneos**. La batería
  alimenta (Buck a 4.98V), el USB es solo cable de datos para el Serial.
- ⚠️ Si hace falta USB solo, desconectar antes el jumper del riel de 5V del Buck al
  VIN del ESP32.

#### Herramienta nueva: `test/test0_i2c_scanner/`

Scanner I2C crudo (sin librería Adafruit). Barre las 127 direcciones a 100 kHz y a
400 kHz, lee `WHO_AM_I` en 0x68/0x69 e informa el nivel de reposo del bus. Distingue
"no hay nadie" de "está en otra dirección" de "contesta a una velocidad y no a la
otra" — cosas que el error genérico de `mpu.begin()` no separa.

Resultado final con todo bien alimentado: **~50 iteraciones seguidas, 0x68 a las dos
velocidades, `WHO_AM_I` = 0x68 siempre, sin fantasmas.** Bus sano.

#### Pendiente abierto

⏳ **El AP se cae al quitar el USB** (batería sola). Es el síntoma original de la
sesión y **sigue sin resolverse**. Hipótesis viva: el Buck no sostiene los picos de
corriente del radio WiFi del ESP32. Se sumó un segundo capacitor de 1000µF/50V en
paralelo con C4 (misma fila 28, tira F-J) y no alcanzó.

### 7.14 Test 6 — Control de velocidad por encoder y revisión del cap de PWM ✅ (2026-08-14)

Sketch: **[`test/test6_velocidad_esp32/`](../test/test6_velocidad_esp32/test6_velocidad_esp32.ino)**.
Dos modos: diagnóstico en lazo abierto (un motor por vez) y control de velocidad en
lazo cerrado (PI por motor, realimentado por encoder, sin filtro de fusión).

#### ✅ Por qué los motores no giraban: el switch estaba en OFF

En el Modo 1 las corridas 1 y 2 dieron **cero pulsos en ambos motores**; las
corridas 3 y 4 anduvieron. **Causa confirmada: el switch de V+ no estaba
encendido.** El switch está en línea con V+ antes del nodo estrella, así que la
etapa de potencia del L298N no tenía tensión, mientras el ESP32 corría alimentado
por USB y reportaba todo con normalidad.

⚠️ **Tercera aparición del mismo modo de falla en este proyecto:**

| # | Causa | Síntoma |
|---|---|---|
| 1 | Pin lógico "5V" del L298N sin alimentar | Firmware reporta bien, motores quietos |
| 2 | Sin GND común ESP32↔L298N | Firmware reporta bien, motores quietos |
| 3 | Switch de V+ en OFF | Firmware reporta bien, motores quietos |

Las tres agravadas por lo mismo: **el USB alimenta el micro y da la falsa sensación
de que el sistema está energizado.** El Serial funcionando no dice nada sobre la
etapa de potencia.

#### ⭐ Checklist obligatorio antes de cualquier prueba que mueva motores

Cuesta 30 segundos y habría evitado las tres fallas de arriba:

1. **Switch de V+ en ON.** (Falla #3)
2. **VIN+ del L298N mide ~12V** contra GND de potencia. Cubre switch, fusible,
   batería y nodo estrella de una sola medición.
3. **Pin lógico "5V" del L298N mide ~5V.** (Falla #1)
4. **Continuidad entre un GND del ESP32 y el GND lógico común.** (Falla #2)

Si los cuatro pasan y los motores igual no giran, recién ahí el problema es de
firmware, del driver o del motor.

#### ⭐ El cap de PWM de 121 estaba mal calculado

El cap histórico salía de `6.0V / 12.6V = 47.6%` → 121/255, asumiendo que al motor le
llega todo el voltaje del bus. **No es así**: el L298N usa BJTs y cae varios volts.

Velocidad medida por encoder, ruedas en el aire, y voltaje equivalente inferido de
`RPM / 793 × 6V` (793 RPM nominales a 6V sin carga):

| duty | M1 RPM | V equiv. M1 | M2 RPM | V equiv. M2 |
|---|---|---|---|---|
| 121 | 339.7 | **2.57V** | 252.9 | 1.91V |
| 140 | 479.7 | 3.63V | 363.6 | 2.75V |
| 160 | 655.6 | **4.96V** | 518.8 | 3.93V |

**A duty 121 el motor recibía el equivalente a ~2.6V, no 6V.** El cap conservador
dejaba afuera más de la mitad del rango útil — esta es la explicación de por qué los
motores "nunca se vieron rápidos".

⚠️ **Esos ~3V de diferencia NO son todos caída del L298N**: incluyen también la
fricción de la reductora, que consume voltaje antes de producir giro. **Pendiente**:
medir con multímetro en DC directamente entre las pestañas del motor girando a duty
160 para separar ambas contribuciones. Hasta entonces, el cap extendido de 160 está
justificado por inferencia, no por medición directa.

#### ⭐ Asimetría entre M1 y M2 — relevante para el firmware de balanceo

Lazo cerrado, ambos motores sosteniendo la misma consigna:

| Consigna | M1 (duty) | M2 (duty) | Δ duty |
|---|---|---|---|
| 500 RPM | 504 (134) | 504 (149) | +15 |
| −500 RPM | −504 (129) | −504 (153) | +24 |
| −200 RPM | −191 (84) | −191 (109) | +25 |

**M2 necesita 15-25 puntos más de duty que M1 para la misma velocidad**, y su zona
muerta es mayor (M2 no arranca hasta duty ~90; M1 arranca en 60-70, variando entre
corridas por cogging). En lazo abierto el robot se iría de costado. El firmware de
balanceo necesita compensar esto, o directamente usar un lazo de velocidad por rueda.

✅ El lazo cerrado funciona en ambos sentidos — la inversión de signo de M2
(`SIGNO_ENC2 = -1`) está correctamente puesta.

#### Resolución de medición del lazo

Todas las lecturas caen en cuentas enteras de pulsos: a 200 RPM con ventana de 50 ms,
**un pulso vale 17.4 RPM**. Por eso el lazo muestra 191 en vez de 200 (11 pulsos vs.
los 11.5 que harían falta). **No es error del controlador, es cuantización.** Si
alguna vez hace falta más precisión: alargar la ventana o medir tiempo entre pulsos.
Para balancear no hace falta — ahí importa la rapidez de respuesta.

#### Estructura de topes de seguridad

| Capa | Valor | Quién la mueve |
|---|---|---|
| Tope absoluto | 170 | Nadie — ninguna ruta del código lo cruza |
| Tope activo | 121 por defecto, 160 desde la UI | El usuario, solo con motores parados |
| Corte por sobrevelocidad | 850 RPM | Automático |
| Reversión del cap extendido | 15 s sostenidos sobre 121 → vuelve a 121 | Automático |
| Corte por motor trabado | 1.5 s con duty ≥ zona muerta sin pulsos | Automático |
| Apagado por tiempo | 60 s de corrida | Automático |

Los cortes por trabado dispararon correctamente durante las corridas en que los
motores no giraban.

### 7.15 Test 5 — Intento de medición de K_U ⛔ sin resultado usable (2026-08-14)

Corrido con el robot **sostenido a mano**, ruedas al aire. **No produjo un K_U
confiable.**

#### Por qué los datos no sirven

- **Sin tendencia monotónica con el duty.** Frío, promedio de 3 reps: duty 70 → 0.019,
  80 → 0.040, 90 → 0.052, 100 → 0.056, 110 → 0.107, **121 → −0.093** rad/s². El punto
  de mayor duty da negativo.
- **Dispersión del tamaño de la señal.** Frío duty 121: −0.059, +0.065, −0.285.
- **Signos alternando al azar** → ruido del giroscopio, no acoplamiento físico.
- Los puntos con θ recorrido grande (0.61-0.85°) son los más extremos — el robot
  todavía oscilaba y se midió el péndulo, no el motor.

#### Tres fallas de método, en orden de importancia

1. **Sostenido a mano**: la mano no solo agrega ruido, **absorbe el torque de
   reacción** que es justamente lo que se quiere medir. Explica los θ de ~0.05°.
2. **Ruedas al aire**: el torque de reacción existe solo mientras la rueda acelera
   (unos cientos de ms). Después queda girando libre y la reacción cae casi a cero.
   No hay aceleración angular sostenida que medir.
3. **Estimador equivocado**: recta por mínimos cuadrados sobre 250 ms, que asume
   aceleración constante. Ante un transitorio que sube y baja da cualquier cosa,
   incluso signo negativo.

#### Lo que sí se obtuvo

**1. Cota: K_U ≈ 0.001, no 0.03.** Tomando los θ limpios a duty 121 (~0.2°), la
aceleración implícita es ~0.11 rad/s² → K_U ≈ 0.0009. El valor asumido en el notebook
de PSO (0.03) está **~30x sobreestimado**. ⚠️ Es una medición al aire y con montaje
blando: vale como *cota inferior* del caso real con ruedas en el piso.

**2. ✅ Hipótesis del calentamiento — DESCARTADA.** Frío vs. caliente, mismos duties
(rad/s²): 0.019/0.010, 0.040/−0.005, 0.052/0.043, 0.056/0.062, 0.107/0.026. Se
superponen por completo. Coincide con lo que ya insinuaban los datos del Test 3.
**No hace falta ninguna regla de "calentar los motores antes de evaluar".**

#### Criterio para el próximo intento

Ángulo máximo recuperable: `θ_max = K_U · u_max / ω₀²`, con ω₀² = 5.43.

| K_U | u_max = 121 | u_max = 160 |
|---|---|---|
| 0.03 (asumido) | 38° | 50° |
| 0.004 | 5.1° | 6.8° |
| 0.001 (cota medida) | 1.3° | 1.7° |

**K_U ≳ 0.004 es el umbral para recuperarse de una inclinación de 5°.** Sirve como
criterio de pasa/no pasa para la próxima medición.

#### Cómo medirlo mejor — sketch ya modificado (2026-08-14)

**Pendiente del usuario**: montaje rígido. Colgar del eje de las ruedas de una barra
fija, o abrazaderas del eje a un borde de mesa. **Nada blando** (mano, cuerda larga)
entre el eje y el punto de anclaje. ⚠️ Con anclaje rígido el robot oscila **más** que
sostenido a mano (la mano amortiguaba), así que esperar a que se quede quieto antes
de cada barrido pasa a importar más, no menos.

**Ya hecho en `test/test5_ku_esp32/`**:

| Cambio | Antes | Ahora | Por qué |
|---|---|---|---|
| Estimador | pendiente por mínimos cuadrados | **pico de ω y su instante**: `K_U = (ω_pico/t_pico)/duty` | La pendiente asume aceleración constante; el transitorio sube y baja |
| Duties | 60,70,80,90,100,110,121 | **70,100,110,121,135,150,160** | Se sacó la zona muerta (M2 no arranca hasta ~90). Queda el 70 como **control nulo** |
| Repeticiones | 3 | **5** | Con esta dispersión, n=3 no promediaba nada |
| Cap de PWM | 121 | **160** | K_U es una pendiente: se estima mejor sobre un rango ancho (ver 7.14) |
| Ventana | 250 ms | **500 ms** | El transitorio de arranque dura más |
| Traza cruda | — | **volcado decimado a duty 121, rep 1** | Ver la forma real de la respuesta en vez de suponerla |

Se conserva el estimador viejo en paralelo (columna `pend_vieja` en el Serial) para
poder comparar ambos sobre los mismos datos.

**Cómo leer el resultado**: lo que importa no es un valor puntual sino si **K_U es
parecido en todos los duties**. Si lo es, el modelo lineal se sostiene. El duty 70
debe dar ruido — esa es su función.

**Si vuelve a salir ruidoso**: cortar y pasar al análisis de sensibilidad en el
notebook de PSO (correr el PSO con K_U en un rango y mostrar cómo cambian las
ganancias óptimas). Es más honesto que un número inventado y responde directo a lo
que el enunciado pide en "inconvenientes encontrados".

**A futuro, fuera del plazo de entrega**: medirlo con **ruedas en el piso y el robot
atado** — es la única configuración donde el acoplamiento que el modelo necesita
existe de verdad. Al aire solo se mide la reacción inercial de acelerar la rueda.

### 7.16 Test 5b — K_U por desviación estática ⛔ SUPERADO por el Test 7 (2026-08-15)

> ⛔ **Este resultado ya no es el vigente.** El Test 7 (7.17) cerró K_U en **0.0123**
> midiendo el torque como fuerza, sin pasar por el bracket de chapa cuya flexión
> inflaba esta medición ~1.8×. Se conserva el registro completo porque el método y
> sus modos de falla siguen siendo la explicación de por qué hizo falta el Test 7.

**Resultado: K_U entre 0.022 y 0.042. Ángulo recuperable entre 3.7° y 7.0°.
Criterio: 5°.** El robot queda **al límite, con el criterio dentro del intervalo**.

Es un salto de dos órdenes frente al Test 5, que dejaba K_U ≈ 0.001. Pero no es
un número cerrado: las dos lecturas difieren en un factor 2 por una razón física
que este banco no puede resolver.

#### Montaje

El mismo soporte de brackets del video de ω₀, con el bulón **apretado**: la cupla
hexagonal queda abrazada contra la chapa y el eje de salida del motor pasa a estar
anclado al mundo. Marcas testigo de fibrón cruzando cupla↔eje y cupla↔chapa, en
los dos lados.

⚠️ **El criterio de aceptación "empujar el cuerpo y que no se mueva" es
INCORRECTO** — se usó al principio y llevó a diagnosticar mal el montaje. Con el
eje anclado, la única forma que tiene el cuerpo de rotar es **retro-accionando la
reductora**, y ese es justamente el mecanismo por el que el motor lo mueve durante
la medición. Que el cuerpo se mueva al empujarlo es normal y necesario.

Lo que hay que mirar es **quién** se mueve:

| Observación | Significa |
|---|---|
| Marcas quietas, el cuerpo se frena en menos de un ciclo, pesado | Gira la reductora. Correcto. |
| Oscila varios ciclos, liviano | Pivota sobre el bulón flojo. No está anclado. |
| Marcas corridas | Patinó. Dato inválido. |

El test bueno no pasa por la mano: **aplicar duty sostenido y ver si el cuerpo se
queda desviado**. Es el modo `v` del sketch, 20 s, y es la medición en versión
cruda.

#### Las dos mediciones confiables

Ambas son directas, repetidas en dos sesiones independientes, sin ajuste de por
medio. Los ángulos se reconstruyen contra la referencia de la primera corrida
(cada `v` re-referencia, por eso las lecturas crudas parecen inconsistentes).

| | Valor | Evidencia |
|---|---|---|
| **Desviación sostenida a duty 160** | **3.72°** | FWD −3.73° / REV +3.71°: simetría de 0.02°. Estable ±0.03° durante 4 s. |
| **Fricción (ángulo estacionado)** | **3.29°** | −3.25° y +3.33° según la dirección de la que venga |

Piso de ruido con motor apagado: **0.02°**. Relación señal/ruido ≈ 350.

**El ángulo estacionado mide la fricción directamente**, y es el hallazgo de método
de esta sesión. Al apagar el motor, el cuerpo se relaja y se clava donde
gravedad = fricción:

```
ω₀²·sin(θ_estacionado) = τ_fricción / J
```

O sea que θ_estacionado **es** la fricción, en las mismas unidades que todo lo
demás. No hay que estimarla ni modelarla.

#### La ambigüedad que no se resolvió

En el equilibrio bajo carga el motor pelea contra gravedad **y** contra fricción:

```
K_U·u = ω₀²·( sin θ_medido + sin θ_fricción )
```

| Lectura | Qué es | K_U | θ recuperable | vs criterio |
|---|---|---|---|---|
| **Neta** | Lo que queda después de la fricción | 0.022 | 3.7° | 0.74× |
| **Bruta** | Torque del motor antes de descontarla | 0.042 | 7.0° | 1.40× |

Cuál corresponde depende de si la fricción se opone al movimiento de recuperación
durante el balanceo real:

- **A favor de la bruta**: lo que mató los duties bajos fue la fricción de
  **arranque**, que existe solo porque el cuerpo parte del reposo. Balanceando, la
  rueda prácticamente nunca está quieta y ese despegue no se paga en cada
  corrección.
- **A favor de la neta**: con el eje trabado el motor trabaja casi en stall y
  entrega más torque del que va a entregar con la rueda girando.

#### Por qué el barrido no pudo cerrar el número

```
duty:      70     100    115    130    145
theta_eq:  0.034  0.142  0.129  0.282  1.652
```

Plano hasta 130 y recién despega en 145. **No es una recta con offset: es un
umbral de despegue en duty ≈ 140.** Por debajo, el motor no vence la fricción
estática y no hay nada que medir. La ventana útil (140–160) es demasiado angosta
para una pendiente.

Se intentó romper la fricción con **dither** (±15 puntos de duty a 12.5 Hz
sostenido durante la medición). No funcionó, y no podía: para despegar desde duty
100 haría falta ±40, y el tope absoluto de 160 no lo permite. A duty 145 con
dither dio 1.65°, contra 1.93° a duty 143 sin dither — sin diferencia.

⚠️ **Los dos veredictos automáticos del sketch fueron inválidos, por la misma
causa**: ajustar una recta a un umbral.

| Corrida | Veredicto impreso | Por qué era falso |
|---|---|---|
| Sin dither | PASA, 7.28° | El control nulo (duty 70) daba 3.56°, más que las mediciones reales |
| Con dither | NO PASA, 1.72° | Extrapola 1.72° a duty 160, **por debajo de los 3.72° medidos directamente ahí** |

Una extrapolación que queda por debajo de una medición directa del mismo punto es
la firma de un ajuste mal condicionado.

#### Bugs del sketch encontrados y corregidos

- **Signo**: el criterio comparaba `ku >= kuMin` sin `abs()`. Como `s` sale
  negativo (convención de hacia dónde inclina FWD, no física), informó "NO PASA"
  con un |K_U| que era casi el doble del umbral.
- **Envoltorio en `derivaReposo`**: el robot cuelga boca abajo, con el reposo en
  ~178°, pegado al borde de ±180°. Un max-min sobre grados crudos informó 359.39°
  de deriva donde en realidad eran 2.21°.
- **Criterio sin zona muerta**: usaba `K_U·u_max/ω₀²`, que asume que todo el duty
  produce torque. La fricción se come hasta la ordenada al origen (medido: 105–118
  de 160). El veredicto tiene que salir del ángulo extrapolado, que ya la descuenta.
- **Ventana de 500 ms del Test 5**: con el eje anclado, el pico de velocidad
  angular cae en `t = π/(2ω₀) = 0.21 s` con ω₀ = 7.4 — dentro de la ventana, pero
  el método estático evita derivar el giroscopio por completo.

#### Dato de diseño, no ruido de banco

**La fricción equivale a ~105–118 puntos de duty de los 160 disponibles.** El
notebook de PSO modela una zona muerta de 85, medida con las ruedas al aire y sin
carga. Bajo carga real es bastante mayor, y el lazo de control va a enfrentar esa
misma zona muerta. Hay que corregirlo en el modelo aunque K_U termine pasando.

### 7.17 Test 7 — τ(u) con balanza ✅ **K_U CERRADO: el robot no pasa** (2026-08-16)

**Resultado: K_U = 0.0123, ángulo recuperable 2.07°, criterio 5°. Falta un factor
2.42 de torque.** Cierra el parámetro que quedó abierto en 7.16.

#### Por qué esta medición y no otra

El banco colgante (7.16) dejó K_U entre 0.022 y 0.042 por dos ambigüedades que no
podía resolver, y esta medición elimina las dos:

1. **Neta vs. bruta.** Colgado, el motor pelea contra gravedad Y contra la fricción
   de la reductora, y no se pueden separar. Acá la fuerza se mide directamente
   contra un plato apoyado en la mesa: no pasa por la reductora ni por el bracket.
2. **Umbral de despegue.** Colgado, por debajo de duty ~140 el cuerpo no se movía y
   4 de 7 puntos eran ruido. Acá **nada tiene que moverse**, así que se mide en todo
   el rango y sale una pendiente bien condicionada.

#### Montaje

Chasis amarrado a la mesa con la rueda sobresaliendo del borde. Hilo inextensible
tangente al borde de la rueda (**r = 35 mm**), bajando vertical hasta una pesa de
~860 g apoyada sobre la balanza. El motor enrolla el hilo y aliviana la pesa:

```
F = lectura_reposo − lectura_con_motor        τ = F · r
```

- La pesa es necesaria **por geometría, no por rango**: un hilo solo puede tirar
  hacia la rueda, o sea hacia arriba. Le da al motor algo que levantar.
- Balanza de 1 g a 10 kg. Con fuerzas de ~70 gf, la resolución da mejor de 1.5%.
- **Un motor por vez**, pulsos de 3.5 s (stall) con 12 s de descanso.

⚠️ **La línea de base se toma antes de CADA pulso.** En la primera corrida se usó
una referencia única y la lectura en reposo derivó ~10 g durante el barrido — el
hilo se reacomoda, la balanza deriva. Ese corrimiento se le sumaba entero a F, y
pesaba sobre todo en los duties bajos, que son los que definen el umbral del ajuste.

#### Resultados — 8 corridas (4 por motor)

| | M1 | M2 |
|---|---|---|
| Fuerza a duty 160 | **72.0 ± 3.5 gf** (±4.8%) | **58.3 ± 6.8 gf** (±11.7%) |
| Umbral (ordenada al origen) | **91.6 ± 1.1** de duty | **101.6 ± 2.5** de duty |
| Pendiente | 3.45e-4 N·m/duty | 2.97e-4 N·m/duty |

```
τ_total (duty 160) = (72.0 + 58.3) gf × 0.035 m = 0.0447 N·m
```

**Asimetría M1/M2 confirmada por un camino independiente** — sin lazo de control,
sin encoders, sin velocidad. M2 entrega el 81% del torque de M1 y arranca 10 puntos
de duty más tarde. Coincide en sentido con el Test 6 (7.14), que la había medido
con lazo cerrado de velocidad. **Es propiedad de los motores, no del lazo.** M2
además es menos repetible (±12% vs ±5%), coherente con más fricción interna.

#### El hallazgo de método: se separó fricción de zona muerta

| Medición | Umbral (duty) | Qué incluye |
|---|---|---|
| Test 3, rueda al aire (7.11) | ~85 | Zona muerta eléctrica |
| **Test 7, balanza** | **91.6 / 101.6** | Zona muerta eléctrica |
| Test 5b, banco colgante (7.16) | 105–118 | Eléctrica **+ fricción de reductora** |

Como acá nada se mueve, el umbral que aparece es solo el eléctrico. **La diferencia
contra el banco colgante es la fricción de la reductora**, ahora medida en vez de
estimada: ~15–25 puntos de duty.

#### Medición de m y m·l

> ⛔ **Chasis VIEJO — superado por 7.23 (2026-09-10).** Sobre el chasis rearmado:
> `m·l = 0.1040 kg·m`, `l = 9.04 cm`, `J = 0.0159 kg·m²`. El método de acá (dos
> apoyos, pivote libre) es el mismo que se reusó.

Robot horizontal, apoyo fijo bajo el eje de las ruedas (el propio bracket, con el
bulón **flojo** para que sea pivote y no empotramiento), extremo de la batería sobre
la balanza.

| Cantidad | Valor | Detalle |
|---|---|---|
| m | **1038 g** | Robot completo en la balanza |
| Lectura del apoyo lejano | **486.7 ± 2.1 g** | 20 lecturas, σ = 9.4 g (1.9%) |
| D | 26 cm | Eje de motores → extremo de la batería |
| **m·l** | **0.1265 kg·m** | = lectura × D |
| **l** | **12.2 cm** | = m·l / m — 47% del cuerpo |
| J | 0.0227 kg·m² | = m·g·l / ω₀² |

⚠️ **El apoyo del eje tiene que ser un pivote libre.** Con el bulón apretado el
apoyo transmite momento, el sistema queda estáticamente indeterminado, la balanza
lee de menos y θmax sale falsamente grande. Verificación: levantar el extremo libre
y soltarlo — tiene que pivotar y volver a apoyar solo.

✅ **`l` = 12.2 cm es la TERCERA confirmación independiente de ω₀ = 7.4 rad/s**, esta
vez desde masa y geometría sin tocar el video. El radio de giro que implica es
8.4 cm, contra 7.5 cm de una barra uniforme de 26 cm — un poco más, exactamente lo
que corresponde a un cuerpo con los motores en un extremo y la batería en el otro.

#### El resultado

```
sin θmax = τ_total / (m·g·l) = 0.0447 / 1.241 = 0.0360   →   θmax = 2.07°
K_U = τ_total / (J · u_max) = 0.0447 / (0.0227 × 160) = 0.0123
```

| | Medido | Criterio | |
|---|---|---|---|
| Ángulo recuperable | **2.07°** | 5.0° | 0.41× |
| K_U | **0.0123** | 0.030 | 0.41× |
| Torque a duty 160 | **0.0447 N·m** | 0.108 N·m | **falta 2.42×** |

El veredicto es robusto frente al error en `D`: para llegar a 5° haría falta
`D = 10.7 cm` en vez de 26. No hay error de medición que cubra esa diferencia.

#### Reemplaza al resultado de 7.16

El banco colgante daba 3.72° neto y 7.02° bruto; esto da 2.07°. Factor 1.8 contra el
neto, 3.4 contra el bruto. **Se le cree a la balanza:**

- El banco colgante mide el ángulo **a través del bracket de chapa**, y la torsión de
  esa chapa el IMU la lee como inclinación del cuerpo. Era la limitación anotada
  desde que se armó ese test. La balanza no pasa por el bracket.
- Descartado que sea al revés: en el banco colgante corrían **los dos motores a la
  vez**, o sea el doble de corriente y más caída en Buck y L298N. Si algo, ahí cada
  motor entregaba menos torque, no más.

#### Palancas evaluadas para cerrar el déficit

| Palanca | Ganancia | Contra |
|---|---|---|
| **Reducción del motor** | hasta 3× | Menos velocidad de rueda |
| Cap de PWM (160 → más) | ~1.5× | Excede los 6V del motor; térmico en stall |
| Bajar `m·l` | ~1.3× | Sube ω₀; exige rediseñar el chasis |

**Solo la reducción alcanza sola.** ~330 RPM en vez de 793 da 2.42×; la velocidad de
rueda cae de 2.9 a 1.2 m/s, que para un balanceador sobra de lejos.

⚠️ **Mover la batería no sirve.** Pesa **123 g**: el 12% de la masa y el 25% de `m·l`.
Llevarla al eje da +29% (θmax 2.07° → 2.66°) y baja el déficit de 2.42× a 1.88× — o
sea que **igual hace falta cambiar el motor, solo cambia cuál**. Y sube ω₀ de 7.4 a
8.2 rad/s, quitando un 10% del tiempo de reacción. No compensa el desarme más la
sesión de video para re-medir ω₀.

El problema es que la masa está **distribuida**: los motores son lo más pesado pero
están sobre el pivote y aportan ~6% de `m·l`; el 68% restante son maderas, varillas
roscadas, L298N, Buck y protoboard, repartidos con un brazo promedio de ~17 cm. No
hay una sola pieza que mover.

### 7.18 Tests 8 y 8b — caída del L298N por multímetro ⛔ **la vía no sirvió** (2026-08-17)

**Resultado: ningún número utilizable sobre la caída del L298N.** Se registra en
detalle porque el valor de la sesión está en *por qué* falló — para que nadie
reintente el mismo camino.

Lo que sí quedó firme, y lo que se descartó, está separado más abajo. **El resultado
del Test 7 (déficit 2.42×) no fue tocado por nada de esto**: sigue siendo la única
medición de torque válida, porque fue directa.

#### La pregunta

El Test 7 (7.17) cerró que faltan 2.42× de torque. La pregunta que decide qué
comprar es de dónde sale ese déficit: si el L298N se come varios volts, cambiar el
driver recupera una fracción grande sin tocar los motores; si entrega casi todo el
bus, el déficit es mecánico y la única salida es la reducción.

#### Modelo del Test 8 — falso

```
V_medido = (duty/255) · (V_bus − V_caida)
```

Asume que en el tramo OFF del PWM el motor ve 0 V. **Se descarta con los propios
datos**: dos duties sobre la misma condición (M1, rueda trabada) tienen que dar la
misma caída y no la dan.

| duty | V medido | Caída implícita |
|---|---|---|
| 150 | 1.8 V | 9.08 V |
| 160 | 2.3 V | 8.45 V |

0.6 V de diferencia entre dos duties separados 4% — error sistemático del modelo, no
dispersión.

#### Modelo del Test 8b — también falso

Sin asumir nada sobre el tramo OFF, `V_medido = frac·V_on + (1−frac)·V_off` es una
recta en `frac`, y `V_off` sale del ajuste en vez de asumirse. Se corrieron 4
barridos (M1 y M2, dos repeticiones cada uno, duties 60/90/120/150).

**R² entre 0.77 y 0.83 en los cuatro, con residuos en U.** Y el ajuste está mal
condicionado: con los 9 puntos de M1 da `V_off = −0.60 V` y caída 9.10 V; usando
solo los tres duties altos da `V_off = −3.08 V` y caída 6.76 V. Que la respuesta
cambie tanto según qué puntos entren significa que los datos no determinan los
parámetros.

⚠️ **El patrón de residuos en U reprodujo en los 4 barridos, pero eso NO es la
prueba** — los cuatro usaron el mismo orden de duty (60, 90, 120, 150), así que
cualquier efecto dependiente del orden (calentamiento acumulado) reproduce el mismo
patrón. **Faltó randomizar el orden.** La conclusión se sostiene por la
inconsistencia entre duties, no por la reproducibilidad del patrón.

#### El hallazgo que explica todo: se estaba midiendo en modo coast

En todos los sketches del proyecto el PWM va sobre **ENA/ENB** con los IN fijos.
Cuando ENA baja, las dos salidas del puente quedan en **alta impedancia** y la
corriente inductiva del motor vuelve al bus por los diodos: las pestañas quedan a
tensión **negativa** durante el tramo OFF. Eso es *fast decay* (coast), y es el
`V_off` negativo que aparece en los cuatro ajustes.

Además `V_off` **no es constante**: depende de cuánta corriente había, o sea del
duty. Por eso ninguna recta ajusta.

⚠️ **`V_off` lo fija el modo de decaimiento, NO la tecnología del transistor.** Un
puente de MOSFETs cableado igual tendría el mismo `V_off` negativo. Cambiar de
driver reduce la caída en conducción, no este término. El Test 8b imprime una cota
optimista basada en esa física equivocada — ver la advertencia en su cabecera.

#### La medición es un blanco móvil

La lectura del multímetro **arranca alta y baja durante la ventana**: el L298N
degrada su propia salida al calentarse. Se descartó que fuera la fuente — el bus,
monitoreado en vivo en VIN+ con un segundo multímetro durante el pulso, sólo cayó de
**12.12 V a 12.07 V**.

Consecuencias:

1. Ningún voltaje en stall de esta sesión es régimen permanente: son puntos
   arbitrarios de una curva que baja. **Ahí está la dispersión de ~0.4 V**, no en la
   resolución del instrumento.
2. Los Tests 8/8b usaron pulsos de 8 s; el Test 7 usaba 3.5 s con 12 s de descanso.
   **Sus números no son comparables con los del Test 7.**
3. Es un problema operativo, no sólo de medición: el robot perdería torque justo
   cuando más lo usa.

#### Hipótesis levantada y descartada — resistencia del cableado

Se midió ~1Ω entre la bornera OUT del L298N y las pestañas del motor y se tomó como
contacto marginal (el cable tiene empalmes soldados). **Era el offset del
multímetro.**

| Instrumento | Offset (puntas juntas) | OUT1 bruto → neto | OUT2 bruto → neto |
|---|---|---|---|
| De lápiz | 0.1 Ω | 1.0 → 0.9 Ω | 0.4 → 0.3 Ω |
| El otro | **1.0 Ω** | 1.0 → **0.0** | 1.0 → **0.0** |

Lo que lo delató: **dos cables independientes dando idéntico valor** es mucho más
compatible con el piso del instrumento que con dos defectos coincidentes.

Y aunque fuera real, **no explicaba nada**: para justificar la caída medida harían
falta ~7 A, y el L298N está especificado en 2 A por canal (3 A de pico) — entraría
en protección casi de inmediato.

⚠️ **Verificar el cero del instrumento antes de construir cualquier hipótesis sobre
una lectura chica.** Un multímetro puede tener ~1Ω de offset y el modo continuidad
("beep") dispara con cualquier cosa por debajo de ~20-50Ω, o sea que no distingue
nada a esta escala.

#### Dato lateral, sin peso

M2 mide más tensión que M1 en stall al mismo duty (2.25 V vs 1.75 V a duty 150) y da
menos torque (81%, 7.17). Si se toma literal implicaría `R_m(M2) ≈ 1.6 × R_m(M1)`, o
sea una explicación **eléctrica** de la asimetría M1/M2 en vez de mecánica.

⚠️ **Es una conjetura floja, no un resultado**: cruza tensiones de duty 150 con
torques de duty 160, sobre lecturas inestables, y asume igual constante de torque en
ambos motores. Barata de testear midiendo `R_m` (óhmetro con offset restado,
promediando varias posiciones de rotor), pero no asignarle peso hasta entonces.

#### Balance de la sesión

**Firme:**

- El bus no se hunde bajo carga (12.12 → 12.07 V). Descarta batería, fusible, switch
  y nodo estrella como fuente de la caída.
- El PWM del proyecto está en ENABLE, o sea modo coast. Hecho del código.
- El L298N degrada su salida al calentarse. Cualitativo pero inequívoco.
- Los modelos de los Tests 8 y 8b son ambos falsos.

**No establecido:** cuánto cae realmente el L298N, y por lo tanto cuánto torque
recuperaría un driver de MOSFETs.

#### Lección de método

El CLAUDE.md ya registra dos veces el patrón de medir con dispersión baja la
cantidad equivocada (ω₀ y K_U, dos veces cada uno). Esta sesión agrega dos variantes:

1. **Un modelo que ajusta no valida nada — hay que mirar los residuos.** Y si el
   ajuste cambia mucho según qué puntos entren, los datos no determinan los
   parámetros.
2. **Verificar el cero del instrumento** antes de construir una hipótesis encima.

Y una restricción concreta: **cualquier medición eléctrica en stall sobre este
driver mide un blanco móvil.** No es problema de instrumento ni de operador.

#### Qué se hace en su lugar

Medir el torque **directo con la balanza**, igual que el Test 7 — que es exactamente
lo que cerró K_U cuando la cadena inferencial había fallado dos veces. Ver
[`test9_decay_torque_esp32/`](../test/test9_decay_torque_esp32/test9_decay_torque_esp32.ino):
compara COAST contra BRAKE al mismo duty, y **la razón entre modos es la ganancia**,
sin multímetro y sin modelo.

### 7.19 Test 9 — COAST vs BRAKE ✅ **EL ROBOT PASA EL CRITERIO** (2026-08-17)

**Resultado: pasar de coast a brake multiplica el torque por 2.67 sin tocar el
hardware.** El ángulo recuperable sube de 2.05° a **5.47°**, contra un criterio de
5.0°: **margen del 9.4%.** Revierte el veredicto del Test 7 (7.17) sin invalidar su
medición — lo que faltaba era una palanca de firmware, no torque.

⚠️ **Con una condición abierta**: el resultado se midió a **duty 160 en brake**, que
es el tope de medición. El cap operativo real lo tiene que fijar un ensayo térmico de
ciclo continuo, y si termina siendo bastante menor, el margen desaparece (a duty 127
la proyección da ~4.3°, que no pasa).

Sketch: [`test9_decay_torque_esp32/`](../test/test9_decay_torque_esp32/test9_decay_torque_esp32.ino)

#### De dónde salió la hipótesis

Del fracaso del Test 8b (7.18). Los cuatro ajustes daban `V_off` negativo, y eso es
la firma de **fast decay (coast)**: el PWM va sobre ENA/ENB, así que al apagar, las
dos salidas del puente quedan en alta impedancia y la corriente inductiva vuelve al
bus por los diodos.

```
COAST   EN = PWM,  IN_a = 1,    IN_b = 0     <- lo que tenía el proyecto
BRAKE   EN = 1,    IN_a = PWM,  IN_b = 0
```

En BRAKE, durante el OFF quedan `IN_a = IN_b = 0` con `EN = 1`: ambas salidas a
masa, **motor cortocircuitado**. La corriente circula en un lazo de tensión casi
nula y decae lento, en vez de ir contra los 12 V del bus. Sube la corriente media y
con ella el torque.

#### Método

Idéntico al Test 7 (balanza, hilo tangente, `r = 35 mm`, pesa de ~900 g), con su
mismo protocolo térmico: **pulsos de 3.5 s con 12 s de descanso forzado**, línea de
base antes de cada pulso. La pesa cambió de 860 a 900 g y **no afecta**: `F` es
diferencial contra la línea de base tomada en cada pulso.

Modos alternados (C, B, C, B…) para que cualquier deriva térmica afecte a los dos
por igual.

#### ⚠️ El radio es del MONTAJE, no del robot: 33 mm, no 35

El Test 7 asumía `r = 35 mm` (borde de la rueda). Medido con calibre el 2026-08-17
con el hilo ajustado: **33 mm**. Son **6% de error directo sobre τ**, porque
`τ = F · r`.

El sketch quedó con el radio **configurable por Serial (`r`)** y con 33 mm por
defecto. **Medirlo con calibre y cargarlo en cada remontaje.** Todos los τ impresos
antes de ese cambio están 6% altos.

#### Resultados finales — duty 160, montaje firme, r = 33 mm

| | COAST | BRAKE | Ganancia |
|---|---|---|---|
| **M1** | 81.67 gf (n=6) | **213.4 gf** (n=5, ±2%) | **2.61×** |
| **M2** | 55.3 gf (n=7, ±37%) | **152.2 gf** (n=5, ±4%) | **2.75×** |

```
τ_M1 = 0.0691 N·m      τ_M2 = 0.0493 N·m      τ_total = 0.1184 N·m
```

| | COAST (hoy) | **BRAKE** | Criterio |
|---|---|---|---|
| τ total | 0.0443 N·m | **0.1184 N·m** | 0.1082 N·m |
| θ recuperable | 2.05° | **5.47°** | 5.0° |
| K_U | 0.0121 | **0.0326** | 0.030 |
| Veredicto | ⛔ | ✅ **pasa, margen 9.4%** | — |

#### Tres consistencias internas que sostienen el resultado

1. **El coast de hoy reproduce el Test 7 al 1%**: 0.0443 N·m contra 0.0447 N·m
   (θ 2.05° contra 2.07°). **El Test 7 era correcto** — su medición queda validada,
   no refutada. Lo que cambia es que existía una palanca de firmware sin usar.
2. **Las dos ganancias brake/coast coinciden**: 2.61× en M1 y 2.75× en M2. Tienen que
   coincidir, porque el modo de decaimiento es una propiedad del puente, no del
   motor. Es el chequeo que detectó que las mediciones intermedias estaban mal.
3. **La asimetría M2/M1 da igual en los dos modos**: 0.68 en coast, 0.71 en brake.
   También tiene que ser así.

#### La saga del montaje — tres versiones del mismo rig, tres escalas distintas

Buena parte de la sesión se fue en descubrir que **el banco no era reproducible**, y
vale la pena el detalle porque el modo de falla es sutil.

| Versión del montaje | Síntoma |
|---|---|
| **1. Hilo tangente sin fijar** (igual al Test 7) | Brake NO MONÓTONO: duty 127 → 165 gf, duty 140 → 132, duty 155 → 159. El torque no puede bajar al subir el duty. |
| **2. Hilo fijado** | Monótono, pero M1 en brake a 160 daba 185 gf |
| **3. Hilo firme** | M1 en brake a 160 da **213.4 gf** — **15% más** |

**La clave del diagnóstico**: entre la versión 2 y la 3, el **coast no se movió nada**
(81.67 gf en ambas) y el **brake subió 15%**. Eso localiza el problema: a 80 gf la
firmeza del amarre no importa, a 210 gf sí. **El hilo cedía sólo bajo carga alta.**

⚠️ **Consecuencia de método: las mediciones de coast NO detectan este problema.** Un
banco puede estar validado contra el Test 7 en coast y aun así estar 15% corrido en
brake. Cualquier comparación entre modos tiene que hacerse **sin tocar el montaje**.

⚠️ **M2 quedó con el amarre menos firme**: su coast a duty 160 dio ±37% de dispersión
(34 a 75 gf) contra ±2% de M1. El promedio (55.3) es consistente con el Test 7
(58.3), pero la dispersión es señal de que ese lado sigue cediendo de forma
intermitente. **Antes de cualquier medición futura de M2, revisar ese amarre.**

#### Controles que salieron bien

- **Sin deriva térmica** en las 4 corridas seguidas de M2 en brake (92 → 100 → 101 →
  96): ninguna tendencia descendente. Descarta que el número de brake esté inflado
  por medir en frío.
- **Térmica al tacto**: el L298N apenas entibia con este protocolo. ⚠️ Pero es ciclo
  de trabajo ~22% (3.5 s cada ~16 s); balanceando la carga térmica es ~4.5× mayor.
  **No es evidencia de que sea seguro en operación continua.**
- **BRAKE es mucho más repetible que COAST** (±2.5% contra ±33% en M1). Esperable:
  coast a esos duty trabaja pegado a la zona muerta, donde domina el cogging.
  Consecuencia práctica: **las mediciones en coast necesitan n alto; las de brake no.**

#### ⛔ Dos hallazgos intermedios RETRACTADOS

Se anotaron durante la sesión y las mediciones finales los desmintieron. Se dejan
registrados para que no reaparezcan.

**1. "La asimetría M1/M2 empeora en brake (60% contra 81%)."** ⛔ **Falso.** Era
artefacto del montaje sin fijar. Con el montaje firme, M2/M1 da **0.68 en coast y
0.71 en brake** — igual en ambos modos, como corresponde. Esto además **le quita el
apoyo principal a la conjetura de 7.18** de que `R_m(M2) > R_m(M1)`: esa conjetura se
sostenía justamente en la asimetría diferencial, que no existe.

**2. "La pendiente de brake no es consistente (~1.0 gf/duty entre 100 y 120, ~3.6
entre 120 y 127)."** ⛔ **Falso.** Con el montaje firme la curva es regular. El salto
era el hilo cediendo bajo carga alta.

**3. "La primera corrida de cada configuración lee bajo por asentamiento del hilo."**
⛔ Refutada dentro de la misma sesión: los 4 puntos nuevos de M2 (92, 100, 101, 96)
mostraron que el supuesto primer-punto-bajo (93) encajaba perfecto y el outlier era
el otro (114).

⚠️ **Las tres tenían la misma forma**: una regularidad aparente en datos tomados con
un banco que cedía. Ninguna sobrevivió al arreglo mecánico.

#### Cap de BRAKE: subido de 127 a 160 sólo para medir (2026-08-17)

El 127 salía de `127/255 × 12.1 V ≈ 6.0 V`, el nominal del motor — pero ese cálculo
**ignora la caída del driver**, así que la tensión real a duty 127 está por debajo
del nominal y el tope era más conservador de lo necesario.

⚠️ **Arriba de duty 127 en brake el motor pasa su nominal de 6 V.** Aceptable para
pulsos de medición de 3.5 s con descanso; **no** es permiso para operar ahí.

⚠️ **El cap operativo del firmware es una decisión aparte y más conservadora**, y hay
que fijarla con un ensayo térmico de ciclo continuo. Subir el tope para medir no
autoriza subirlo para operar.

#### Lo que falta — y es lo que decide si el resultado se sostiene

**1. Ensayo térmico de ciclo continuo, para fijar el cap operativo.** Es el pendiente
crítico. El 5.47° se midió a **duty 160 en brake**, con pulsos de 3.5 s cada ~16 s
(ciclo ~22%). Balanceando, la carga térmica es ~4.5× mayor. Proyectando la pendiente
hacia abajo:

| cap operativo en brake | θ recuperable | |
|---|---|---|
| 160 (medido) | **5.47°** | ✅ |
| ~127 (el conservador original) | ~4.3° | ⛔ |

**El margen del 9.4% vive entero en la zona que este test declaró "sólo para
medir".** Hasta que el ensayo térmico fije el cap real, el veredicto de "pasa" es
condicional.

Al tacto, el L298N quedó **fresco** después de decenas de corridas en brake a duty
160 — buena señal, pero con ciclo 22%, no es evidencia de operación continua.

**2. Batería**: todo se midió a **11.95 V**. Con el pack lleno (12.6 V) habría ~5%
más de margen; descargado, menos.

**3. El amarre de M2** (ver arriba, ±37% de dispersión en coast).

#### Consecuencia de diseño

**Cambiar la reducción de los motores deja de ser necesario**, condicionado al ensayo
térmico. La decisión que bloqueaba el proyecto desde el 16/08 queda en suspenso, no
descartada: si el cap operativo termina cerca de 127, vuelve a hacer falta.

### 7.20 Test 9 (continuación) — carga SIMULTÁNEA de los dos motores revierte 7.19 (2026-08-18)

**Resultado: con los dos motores conduciendo a la vez —la condición real de
balanceo, no la de 7.19— el torque cae 31-36% y el ángulo recuperable baja de 5.47°
a 3.6°.** No pasa contra el criterio de 5.0°. Se agregaron dos modos al sketch
([`test9_decay_torque_esp32/`](../test/test9_decay_torque_esp32/test9_decay_torque_esp32.ino)):
`e` (sostenido continuo) y `w` (intermitente, ciclo ON/OFF).

⚠️ **Este hallazgo es EMPÍRICO, no una extrapolación.** Se midió directo con la
balanza, dos motores, dos corridas completas. No confundirlo con "límite térmico
teórico" — es un dato, igual de firme que el resto de 7.19.

#### Por qué hacía falta esto

Todo 7.19 midió **un motor a la vez** — el otro quedaba sin conducir mientras se
medía. Eso aísla bien cada motor, pero no es como el robot va a operar: balanceando,
los dos motores del L298N conducen simultáneamente. Faltaba saber si eso cambia algo.

#### Ensayo sostenido (`e`) — motor 1, duty continuo, dos motores

| duty | resultado |
|---|---|
| 100 | Torque se **estabilizó eléctricamente** en ~195 s (86→72 gf, −16% total desde frío), pero **siguió calentando físicamente** hasta volverse intocable a los ~300 s — cortado por tacto, no por la lectura. |
| 130 | Cortado por calor (sin tiempo registrado). |
| 145 | Cortado por calor a los **60 s** — mucho más rápido que a 100. |

⚠️ **Lección de seguridad**: el torque estabilizándose NO significa que la
temperatura del paquete se estabilizó. La masa térmica del disipador/carcasa tiene
una constante de tiempo mucho más lenta que la del semiconductor — por eso el
protocolo corta por tacto, no por F. Se cumplió como corresponde.

#### Ensayo intermitente (`w`) — 0.5 s ON / 2 s OFF (20%), duty 160, dos motores

| Motor | Dual (hoy) | Un motor (7.19) | Caída |
|---|---|---|---|
| M1 | 136.7 gf (n=6, 30-180s, cortado por calor) | 213.4 gf | **−36%** |
| M2 | 104.8 gf (n=9, completó 270s) | 152.2 gf | **−31%** |

Ninguno de los dos muestra tendencia de caída con el tiempo (M1: 138→142→138→134→
132→136; M2: 113→112→99→106→106→100→96→101→110) — **la caída ya está presente en la
primera lectura**, no se acumula. Es la firma de un efecto de carga simultánea, no
de calentamiento progresivo.

**Consistencia que lo respalda**: `M2/M1 dual = 104.8/136.7 = 0.77`, cae justo en el
rango ya establecido por dos caminos independientes (0.68 coast, 0.71 brake
single-motor — ambos en 7.19). Que la asimetría se sostenga en esta tercera
condición es la clase de chequeo que este proyecto viene usando para confiar en un
número.

**V_bus casi no se movió**: 12.23 → 11.93 V (−2.3%), muy poco para explicar una
caída de torque del 31-36% por simple hundimiento de batería/cableado (eso
necesitaría una caída de V proporcional). **Descarta la hipótesis simple de sag de
alimentación.** Apunta a algo interno del L298N cuando sus dos puentes conducen a la
vez (posible acoplamiento entre canales) — **no confirmado, es hipótesis.**

#### Resultado bajo condición real

```
τ_M1_dual = 0.0443 N·m      τ_M2_dual = 0.0339 N·m      τ_total_dual = 0.0782 N·m
```

| | 7.19 (un motor) | **7.20 (dos motores)** | Criterio 5.0° | Criterio 3.0° |
|---|---|---|---|---|
| θ recuperable | 5.47° | **3.6°** | ⛔ no pasa | ✅ pasa (~20% margen) |

#### Criterio bajado de 5.0° a 3.0° (decisión del usuario, 2026-08-18)

⚠️ **No es una re-derivación física — es una decisión explícita, documentada como
tal.** El criterio de 5° debería salir de cuánto necesita tolerar el robot ante una
perturbación real (golpe, piso irregular, error de arranque), no ajustarse para que
empate con el torque disponible — eso sería razonamiento circular. **La justificación
de por qué 3.0° alcanza contra las perturbaciones reales esperadas queda pendiente.**
Hasta que se documente esa justificación, tratar 3.0° como criterio de trabajo
provisorio, no como un valor cerrado.

#### Próxima palanca: driver de MOSFETs independientes — hipótesis, no plan confirmado

Si la caída del 31-36% es interna al L298N compartido (apoyado por el V_bus estable),
un driver con canales independientes (TB6612FNG, DRV8871 ×2, etc.) podría recuperarla.
**No comprar nada todavía** — la hipótesis no está aislada del resto de causas
posibles, y el mismo proyecto ya se equivocó una vez suponiendo qué arreglaría un
cambio de driver (Test 8b, ver 7.18).

#### Lo que falta

- Aislar el mecanismo: ¿es interno al chip, o hay algo más en el camino que solo
  aparece con los dos canales activos?
- `e` y `w` a duty 100 en dos motores no completaron sin cortar por calor —
  el ciclo 20% tampoco demostró ser sostenible indefinidamente para M1 (cortado a
  180s). M2 completó 270s pero sin confirmar más allá de eso.
- Repetir con batería llena (todo esto se midió a ~12.2-12.35V, no a 12.6V).

### 7.21 De dónde sale el criterio de 5° — y por qué no hay que inventar el 3° (2026-08-18)

**Ninguno de los dos números (5° ni 3°) tiene una derivación física en este
proyecto.** Rastreado hasta la fuente: `THETA0 = np.radians(5.0)` en
[`pso_pendulo_invertido.ipynb`](../algoritmos_evolutivos_pso/pso_pendulo_invertido.ipynb),
la perturbación inicial de la simulación — no una medición ni una especificación de
diseño. El mismo notebook usa además `ESCENARIOS` con ángulos **2°, 5°, 8°** (cruzados
con tres velocidades) para robustecer el costo del PSO contra el ruido de temporización
de la zona muerta. 5° es simplemente el del medio de esa terna.

No se encontró en ningún archivo del proyecto (notebook, PDF teórico, registro) una
justificación de por qué 5° específicamente, ligada a una perturbación real esperada
(golpe, desnivel del piso, error de colocación).

⚠️ **Consecuencia**: bajar el criterio a 3.0° (7.20) no está peor fundado que el 5.0°
original — pero elegirlo *después* de ver que el hardware da 3.6° sigue siendo débil
metodológicamente, con o sin ese antecedente.

**Alternativa propuesta, sin inventar ningún número nuevo**: usar los tres ángulos que
ya estaban en `ESCENARIOS` *antes* de conocerse el resultado de hoy, y reportar contra
los tres en vez de un solo pass/fail:

```
θ recuperable medido (7.20): 3.6°

vs 2°  -> PASA (margen 80%)
vs 5°  -> NO pasa (28% corto)
vs 8°  -> NO pasa (55% corto)
```

Describe el envolvente real de operación sin maquillarlo: el robot se recupera de una
perturbación chica, no de una moderada ni de una fuerte.

#### El camino que sí cierra la pregunta: medir la perturbación real

El banco de pruebas real del usuario es "parar el robot lo más vertical posible a
mano, activar, sin ningún estímulo externo" — o sea que la perturbación que hay que
tolerar es el **error de colocación humano**, no un empujón deliberado a un ángulo
elegido. Eso se puede medir directo, hoy, sin firmware de balanceo.

**Sketch nuevo**: [`test10_pitch_colocacion_esp32/`](../test/test10_pitch_colocacion_esp32/test10_pitch_colocacion_esp32.ino).
Detecta automáticamente cuándo el robot quedó quieto (por magnitud de giroscopio) y
captura el pitch en ese instante (puro del acelerómetro, con el bias/escala del Test
1b ya aplicados) — el usuario no necesita mirar la pantalla mientras sostiene el
robot, solo repetir el intento ~10 veces. Al final, `l` da media, desvío, mínimo y
máximo — ese rango **es** el criterio de diseño real para este banco de pruebas,
sin necesidad de elegir entre 3° y 5° a ojo.

⏳ **Pendiente de correr.** El resultado se compara directo contra el 3.6° de 7.20.

#### Candidatos de driver (sin decidir — falta medir `R_m` primero)

Evaluados para reemplazar el L298N, condicionado a aislar el mecanismo de la caída
por carga simultánea (7.20):

| Driver | Corriente cont./pico | Nota |
|---|---|---|
| **DRV8871 (×2, uno por motor)** | 3.6 A / 6.5 A | Favorito: son dos chips separados, no un die compartiendo dos puentes como el L298N — si el mecanismo es interno al chip, esto lo resuelve por diseño. Protección térmica y limitación de corriente integradas. |
| **TB6612FNG** | 1.2 A / 3.2 A por canal | Un solo módulo, dos canales, bajo RDSon. Riesgo: si el stall real supera ~1.2-2 A por motor, queda corto. |
| **BTS7960** | hasta 43 A | Muy sobredimensionado, pero barato y con disipador propio. Opción de respaldo si `R_m` da una sorpresa. |

⚠️ **Ninguno se elige sin medir `R_m` primero** (pendiente desde 7.18, óhmetro con
offset restado) — da la corriente de stall real, que decide si el TB6612FNG alcanza
o hace falta ir directo al DRV8871.

---

### 7.22 ω₀ del chasis rearmado — ✅ cerrado (2026-09-09, valor vigente: 8.0 rad/s)

**Valor vigente: ω₀ = 8.0 ± 0.1 rad/s** (T₀ = 0.785–0.790 s), sobre el chasis de
balsa rearmado, colgado del eje de los motores.

⛔ **Reemplaza a los 7.4 rad/s de 7.12.** Ese valor **no estaba mal** — era de otra
planta: el chasis viejo de contrachapado, de 26 cm. El nuevo mide **22 cm** (eje de
giro al extremo estructural, medido con cinta) y es más liviano, así que un ω₀ más
alto es lo esperado.

Análisis, scripts y datos: [`algoritmos_evolutivos_pso/video_omega0/2026-09-09_rearmado/`](../algoritmos_evolutivos_pso/video_omega0/2026-09-09_rearmado/README.md).
Videos fuente: `experiments/w0_videos/` (6 clips).

#### Los seis clips

| clip | archivo | condición | n períodos | amplitud | T medio | T₀ (A→0) | ω₀ | L_eq |
|---|---|---|---|---|---|---|---|---|
| A | 6.26 PM | sin fondo | 26 | 36–50° | 0.8252 ± 0.0034 | 0.7936 | 7.92 | 15.6 cm |
| B | 6.44 PM | sin fondo | 96 | 0.6–41° | 0.8000 ± 0.0022 | 0.7841 | 8.01 | 15.3 cm |
| C | 8.39 PM | fondo blanco | 12 | 10–22° | 0.8076 ± 0.0066 | 0.7767 | 8.09 | 15.0 cm |
| **D** | **8.41 PM** | **fondo blanco** | **109** | **0.5–46°** | **0.7993 ± 0.0014** | **0.7887 ± 0.0011** | **7.97** | **15.5 cm** |
| E | 8.49:33 | nivel 13° | 23 | 10–19° | 0.8008 ± 0.0027 | 0.8038 | 7.82 | 16.0 cm |
| F | 8.49:45 | nivel 13° | 25 | 6–18° | 0.7983 ± 0.0020 | 0.7936 | 7.92 | 15.6 cm |

**Dispersión entre clips: 1.2%.** El clip D es el mejor material: fondo blanco,
55 s, 109 períodos, amplitudes de 46° a 0.5°.

**Dos estimadores con supuestos distintos, 0.7% de diferencia:**

```
extrapolando T contra A^2 a amplitud cero   T0 = 0.7901 s  ->  w0 = 7.95 rad/s
promediando solo los periodos con A < 10    T0 = 0.7844 s  ->  w0 = 8.01 rad/s
```

El segundo no extrapola nada: a 10° la corrección no lineal teórica es 0.19%.
Son 74 períodos de los clips B, D y F.

#### Chequeos — todos pasan

1. **`L_eq = g(T/2π)²`**: 15.0–16.0 cm contra un cuerpo de 22 cm, banda admisible
   11–22 cm. Es el chequeo de 7.12, el que habría descartado el mismo día las dos
   mediciones falsas.
2. **Barra uniforme de 22 cm**: `L_eq = 2L/3 = 14.7 cm → ω₀ = 8.18 rad/s`, desde un
   solo número y sin tocar el video. Medido 7.95–8.01: **3% de error**. La razón
   `L_eq/L = 0.70` contra 0.667 de la barra ideal.
3. **Seis clips independientes** dentro del 1.2%.
4. **Cuerpo rígido** (clip D): recalculando θ(t) solo con los píxeles a r > 0.6R y
   solo con los de r < 0.6R, el período tiene que ser el mismo. **0.6 ms de
   diferencia contra 6.4 ms de 3σ.** Es lo que descarta que el bracket esté
   flexionando en vez de pivotar — el modo de falla de 7.12.
5. **Dos observables geométricamente independientes** (clip D): eje principal por
   PCA y ángulo del centroide respecto del eje de giro. Sostenido antes de soltar
   44.58° vs 45.68°; en reposo 3.05° vs 3.08°; período 0.7897 vs 0.7993 s.
6. **Submuestreo a 15 fps** y **sentido de cruce**: ≤0.3 ms de diferencia.

#### Amortiguamiento: fricción SECA, igual que en 7.12

Clip D: amplitud lineal en t (Coulomb) R² = 0.968, contra exponencial (viscosa)
R² = 0.903. Los otros cinco dan lo mismo. **Coulomb no corre la frecuencia**, así
que no hay corrección que aplicar.

⚠️ Consecuencia: un péndulo con fricción seca **se detiene en cualquier punto de
una banda muerta alrededor de la vertical, no en la vertical exacta**. El reposo no
es un cero angular confiable.

#### El efecto de amplitud finita, medido

`T(A) = T₀(1 + A²/16 + …)`. Se ve sin ajustar nada, ordenando los clips por
amplitud media: A (~43°) → 0.8252 s; D (~23°) → 0.7993; B (~21°) → 0.8000;
C (~16°) → 0.8076; E (~14°) → 0.8008; F (~12°) → 0.7983.

Con amplitudes de 40-50° el sesgo llega al 3-5%, más grande que la dispersión
estadística. **7.12 no aplicó esta corrección** (estimó +1.3% a 26° y lo dejó
dentro del ruido).

⚠️ La **pendiente** `dT/dA²` sale entre 1.4× y 7× la teórica según el clip —
inestable. El **intercepto** `T₀` no depende de eso (A=0 sigue siendo A=0) y es el
que se usa. Es la razón de dar ±0.1 y no el ±0.01 estadístico del clip D.

#### ✅ Escala angular del video — cerrado (2026-09-10): sobre-lee 23-36%

Los clips E y F se soltaron desde **13° medidos con nivel digital**, apoyado
directo sobre el robot — sin cámara de por medio. El video, con la máscara y el
pivote estimados, midió una amplitud de suelta mayor:

| | video (sostenido − equilibrio) | física (nivel, sin cámara) | video / física |
|---|---|---|---|
| E | 16.51° − 1.22° = **15.29°** | | 1.23× (+23%) |
| F | 15.62° − (−1.25°) = **16.87°** | | 1.36× (+36%) |

**Medición de cierre (2026-09-10)**: el mismo nivel digital, sobre el mismo
ensamble, con el robot colgando libre en reposo, sin tocarlo, sin cámara: **0.6°**.
Da la amplitud de suelta física real por camino directo, sin pasar por el video:

```
amplitud fisica real = 13° - 0.6° = 12.4°
```

Contra eso, el video sobre-lee **23% en E y 36% en F** (tabla arriba). Es
consistente con lo esperado: el eje de giro se tomó a ojo de la imagen — el
ajuste por mínimos cuadrados sobre la trayectoria del centroide no convergió
(residuos de 19-22 px, ver más abajo) — y un pivote corrido infla el ángulo
aparente del blob visible.

⚠️ **23% contra 36% es más dispersión de la que un factor de escala fijo (mismo
ensamble, mismo pivote supuesto) debería dar.** Puede ser precisión al fijar los
13° a mano en cada suelta, o que el error de pivote no sea perfectamente
constante entre clips. No se le asigna más peso sin una tercera medición.

- **ω₀ NO se corrige por esto y sigue en pie.** La extrapolación usa el intercepto
  en A=0, que es el mismo punto sin importar la escala del eje de ángulos — un
  error de escala constante mueve la pendiente medida, no el intercepto.
- **De hecho esto explica, en parte, la pendiente `dT/dA²` sobre-medida** (sección
  anterior): si el video sobre-lee la amplitud, el error de escala por sí solo
  debería haber hecho la pendiente medida **menor** a la teórica, no mayor. Que
  haya salido al revés confirma que el exceso de pendiente es otro efecto —
  degradación de la máscara en ángulos grandes — y no este error de escala. Son
  dos problemas distintos.
- **`θ_umbral = K_U·MAX_DUTY/ω₀²` NO depende de esta escala tampoco** — se
  calcula con `K_U` medido por balanza (protocolo propio, sin pasar por este
  video) y `ω₀` (que ya no depende de la escala, ver arriba). La escala angular
  del video solo importaría si en el futuro se quisiera validar una recuperación
  real filmándola con este mismo pipeline — no bloquea nada de lo que falta para
  cerrar la planta.

#### Método — qué cambió respecto de 7.12

- **PTS real de cada cuadro**, leído con `ffprobe`. Los clips de WhatsApp corren a
  29.94–29.98 fps, no 30, y tienen cuadros perdidos (dt de 0.0666 s donde debería
  haber 0.0333). Contar índices y dividir por 30 mete sesgo sistemático en T.
- **Cruces por el equilibrio, no extremos** — al revés que 7.12. Acá la máscara se
  degrada justo en los extremos de amplitud grande (cuerpo más inclinado, más
  borroso). En los cruces la señal tiene pendiente máxima. Se usan períodos completos
  entre cruces del mismo sentido: un error de línea de base corre los dos cruces casi
  lo mismo y se cancela en la resta.
- **Observable = ángulo del centroide respecto del eje de giro**, no el eje principal
  por PCA. Con la máscara fragmentada el PCA es ruido entre ±90°. El ángulo respecto
  del eje tolera la fragmentación por un motivo físico: en un cuerpo rígido que gira
  alrededor de un eje, todos los puntos comparten la coordenada angular.
- **El fondo blanco funcionó**: el área mediana de la máscara sube de ~5 000 px a
  ~13 500 px y el PCA vuelve a servir, lo que habilita el chequeo 5.
- ⚠️ **La cinta blanca con marca roja todavía no rindió.** Contra el fondo blanco
  tiene casi el mismo brillo que la sábana; contra el fondo desordenado el umbral por
  brillo se derrama a la pared (máscaras de 120 000–160 000 px contra los ~30 000 del
  robot). Lo que la haría rendir: **3 o 4 marcas gruesas y saturadas (rojo/azul)
  separadas varios centímetros** — se rastrea cada una como blob de color y se ajusta
  la recta que las une; funciona con cualquier fondo y cada marca es un punto material
  fijo, que es lo que hoy falta para ajustar el eje de giro desde los datos.
- ⛔ **El centro de rotación sigue sin poder ajustarse desde los datos**: el círculo
  ajustado a la trayectoria del centroide da residuos de 19-22 px sobre radios de
  50-96 px, porque el centroide del fragmento visible no es un punto material fijo.
  El eje de giro se tomó de la imagen, y esa es la fuente más probable del problema de
  escala angular.

#### ⛔ El cronómetro no es una alternativa mejor (evaluado 2026-09-09)

10 períodos duran 7.84 s. Para igualar la precisión del video (1.2%) habría que
medirlos con error menor a **89 ms**, contra los ~100 ms de componente aleatorio de
un start-stop humano. Con ~10 repeticiones bajaría a ~0.4% y sí mejoraría el número
— pero:

- **No puede corregir la amplitud finita.** Está medido acá mismo: T va de 0.798 s a
  12° hasta 0.825 s a 43°. Soltando desde 40° da un valor 3% sesgado sin forma de
  saberlo.
- **No detecta un montaje malo**, que es exactamente lo que invalidó los 2.33 y 4.07
  rad/s — los dos tenían dispersión de 1.5% y 0.8%. Precisos y falsos.

Si se quiere igual como confirmación independiente: soltar desde ~15°, cronometrar
**20** períodos (≈15.7 s) y contar **al pasar por la vertical**, no en los extremos.
Debería dar 15.5–16.0 s.

#### Consecuencias

- **ω₀² pasa de 54.8 a 63.2 (+15%).** `θ_umbral = K_U·MAX_DUTY/ω₀²` baja un 15% a
  igual torque.
- ⚠️ **No propagar este número solo.** `K_U` también cambia con el chasis nuevo
  (menos masa, otro `J`): hay que re-medir `m` y `m·l` por el método de dos apoyos
  (7.17) y recalcular. Eso además da `l`, y con ω₀ fija `J = m·g·l/ω₀²`.
- Las ganancias del PSO (`Kp=3500, Ki=2297.02, Kd=484.22`) son de la planta vieja.

---

### 7.23 m·l y J del chasis rearmado — ✅ cerrado (2026-09-10)

**Valor vigente: `m·l = 0.1040 kg·m`, `l = 9.04 cm`, `J = 0.0159 kg·m²`.**
Reemplaza a los valores de 7.17 (`m·l = 0.1265 kg·m`, `l = 12.2 cm`,
`J = 0.0227 kg·m²`) — igual que con ω₀ (7.22), esos valores no estaban mal, eran
de otra planta (chasis viejo, contrachapado, 26 cm).

**Método**: el de 7.17 (dos apoyos, pivote libre), reusando el mismo bracket/bulón
flojo del banco de ω₀, ahora fijo a la mesa en vez de a una viga — el robot queda
inclinado, con el extremo lejano apoyado sobre un lápiz (contacto de línea) que
descansa en la balanza. **Confirmado: ruedas sacadas, tanto acá como en los videos
de ω₀** — `m` y `ω₀` describen el mismo sistema físico, condición necesaria para
combinarlos en `J`.

#### Datos

| Cantidad | Valor | Detalle |
|---|---|---|
| `m` | **1150 g** | robot completo, sin ruedas, balanza directa |
| Lectura apoyo lejano, `D=20.5cm` | **507.2 ± 8.7 g** | media de 5 tomas: 494, 515, 503, 511, 513 (±1.7%) |
| `D` | 20.5 cm | pivote → punto de contacto (lápiz), horizontal |
| **`m·l`** | **0.1040 kg·m** | = 0.5072 kg × 0.205 m |
| **`l`** | **9.04 cm** | = m·l / m — 41% del cuerpo (`L=22cm`, ver 7.22) |
| **`J`** | **0.0159 kg·m²** | = m·g·l / ω₀² (ω₀=8.0, ver 7.22) |
| radio de giro `√(J/m)` | 11.8 cm | contra 12.7 cm ideal de barra uniforme (`L/√3`) — 93% |

#### El chequeo que casi descarta esta medición: `N·D` tiene que ser constante

Primer barrido, variando `D` a propósito: **522g a 22cm, 877g a 17cm, 997g a
10cm.** Por equilibrio de momentos sobre un pivote libre, `N·D = m·g·l` tiene que
dar el **mismo valor sin importar dónde** apoye el extremo lejano — no depende de
`D`. No daba: `m·l` calculado de esos tres puntos sale **0.1148, 0.1491, 0.0997
kg·m** — hasta 50% de dispersión.

**Causa más probable**: a `D` chico el robot queda casi vertical, y algo más
—además del pivote y el punto de apoyo— empieza a tocar la mesa (una rueda, un
cable, el borde de un nivel). Deja de ser un sistema de dos apoyos limpio y la
ecuación de momentos ya no aplica. Se descartaron los puntos de 17 y 10cm; se
repitió solo cerca de 20-22cm, donde el robot queda más acostado y el riesgo de un
tercer contacto es menor.

⚠️ **Un punto suelto a `D=22cm` (522g) sigue sin explicarse**: da `m·l=0.1148
kg·m`, 10% más alto que el resultado adoptado. Fue una sola toma, sin las
repeticiones ni el mismo cuidado que las 5 de 20.5cm — **no se promedió con el
resto**, para no esconder una discrepancia real detrás de un promedio. No bloquea
el resultado (la muestra de 20.5cm tiene 5 tomas consistentes al ±1.7%), pero
queda como cabo suelto si alguna vez hace falta repetir.

#### Chequeo de cordura

Barra uniforme de 22cm predice radio de giro `L/√3 ≈ 12.7 cm`. Medido: **11.8 cm,
93% del ideal** — algo más concentrado hacia el pivote que una barra uniforme,
coherente con los motores montados casi sobre el eje (misma dirección que el
chasis viejo, que daba 98.6% del ideal — acá un poco más marcado, ver más abajo el
aviso sobre ensamble parcial).

⚠️ **No necesariamente es el estado final del robot.** Si al momento de esta
medición faltaban componentes por montar (drivers, cableado completo de potencia),
`l` y `J` describen el ensamble parcial actual, no la configuración final de
operación — re-verificar si el armado cambia de forma apreciable antes de dar el
número por definitivo para firmware.

#### Consecuencias

- **`J` cierra.** Es lo que hacía falta para `θ_umbral = K_U·MAX_DUTY/ω₀²` — pero
  `K_U` sigue sin remedirse con el driver nuevo. ⚠️ Ya no lo bloquea el cableado:
  **los BTS7960 quedaron cableados el 2026-09-12** (ver 3.1b); falta correr la
  medición con balanza. `θ_umbral` sigue **sin número vigente** hasta entonces.
- `J` bajó de 0.0227 (chasis viejo) a **0.0159 kg·m², −30%**. Con `ω₀²` +15%
  (7.22) y `m` +11% (1150g contra 1038g), la caída viene sobre todo de `l`
  (9.04cm contra 12.2cm, −26%).

---

### 7.24 Incidente — Buck destruido por polaridad invertida (2026-09-10)

**Contexto**: sesión de cableado de señales de los drivers BTS7960 (plan en
`docs/plan-cableado-senales.html`). V1 (`B−`↔`GND` del header) y V2 (¿pull-up
interno en `EN`?) de la Sección 0 de ese plan ya estaban cerradas, en los dos
módulos. Al pasar a V3 (confirmar potencia armada, primera energización del día)
ocurrió el incidente.

**Síntoma**: al energizar — **solo el nodo estrella, sin riel de 5V/3.3V hacia
ESP32/MPU/encoders todavía conectado** — salió humo y un olor característico de
la zona de la bornera `IN` del Buck. El fusible de 10A **no saltó** (pero quedó
dañado — ver el Fix). Sin calor ni humo persistente al revisar después; batería
sin hinchazón.

**Causa raíz**: cableado invertido en la entrada del Buck — el cable hacia la
bornera `IN−` llevaba en realidad el `+` de la batería, y el que iba a `IN+`
llevaba el `−`/tierra. Hasta 12.6V al revés de lo que el módulo espera en su
entrada.

⚠️ **"No saltó el fusible" no prueba que no pasó corriente peligrosa.** El
camino de falla (un componente fallando en corto) puede limitar o cortar la
corriente antes de que el fusible llegue a reaccionar.

#### Diagnóstico, en el orden que se hizo

1. **Inspección visual de `C1` y del capacitor de salida (1000µF/50V)**: sin
   hinchazón en ninguno de los dos. ⚠️ No es garantía suficiente — hubo humo
   real, y un electrolítico puede degradarse internamente sin hincharse de
   forma visible.
2. **Resistencia `B+`↔`B−` en cada IBT-2**, módulos desconectados: **500kΩ y
   1MΩ** — del orden de la fuga normal del capacitor de bulk de 330µF de
   fábrica en cada módulo, no un corto (que daría unos pocos ohms o menos).
   **Los dos drivers quedan descartados de daño** — consistente con que la
   inversión de polaridad fue específica del tramo Buck, sin tocar el nodo
   estrella ni el cableado hacia `B+`/`B−` de cada driver.
3. **Continuidad `IN−`↔`OUT+` en el Buck viejo**, ya desconectado de todo: **da
   continuidad.** En un Buck no aislado, `IN−` y `OUT−` están unidos
   internamente por diseño (documentado desde el principio de este proyecto),
   pero `IN−` a `OUT+` **nunca** debería darla. Confirma falla interna real, no
   artefacto de la medición — **el módulo queda descartado sin ambigüedad**,
   diagnóstico cerrado.
4. **ESP32/MPU/encoders**: sin alimentación en el momento del incidente —
   descartados de cualquier revisión, no había forma de que les llegara nada
   anormal.

#### Fix — ✅ ejecutado 2026-09-12

Buck reemplazado por una unidad nueva, verificada y **calibrada en 5.01V** (ese
es el valor de referencia vigente; el 4.98V era de la unidad destruida). `C1`
(100µF/25V) y el capacitor de salida (1000µF/50V) se reemplazaron **por
precaución**, con los repuestos en stock, aunque ninguno mostraba daño visible —
los dos estuvieron en el camino directo de la tensión invertida, y son las
piezas más baratas de todo el sistema como para apostar a que "parecen estar
bien" alcanza.

⚠️ **El fusible de 10A también quedó dañado y hubo que reemplazarlo** (dato
incorporado 2026-09-12). Matiza el "no saltó" del síntoma: el fusible **sí se
vio afectado**, pero no abrió el circuito a tiempo para salvar al Buck. Refuerza
la lección ya anotada: un fusible puede degradarse sin abrir limpiamente, así
que **ni "no saltó" ni "está puesto" son evidencia de que no pasó corriente
peligrosa**. Tras un incidente de este tipo el fusible entra en la lista de
piezas a verificar o cambiar, no se asume sano.

#### Estado de los pendientes que dejó el incidente

| # | Pendiente | Estado |
|---|---|---|
| 1 | Verificar polaridad/continuidad del tramo hacia el Buck nuevo antes de conectarlo | ✅ hecho 2026-09-12, cumple lo esperado |
| 2 | Reemplazar `C1` y el capacitor de salida | ✅ hecho 2026-09-12 |
| 3 | Probar el Buck nuevo y confirmar su salida | ✅ hecho 2026-09-12 — **5.01V** |
| 4 | **Repetir V3** (potencia armada: `B+` ~12V y `VCC` ~5V en cada driver) | ⏳ **pendiente — lo único que queda de este bloque** |
| 5 | Re-calibrar el Buck nuevo, no asumir 4.98V | ✅ hecho — 5.01V |
| 6 | Reemplazar el fusible de 10A dañado | ✅ hecho 2026-09-12 |

---

### 7.25 C6 y C_en instalados sobre el protoboard rearmado — ✅ cerrado (2026-09-12)

**Contexto**: el protoboard del ESP32 se está rearmando desde cero (aparte del
rearmado del chasis de 7.22/7.23) — la ubicación de C6/C_en propuesta en
`plano-conexiones-esp32.html` §4.4 (filas 25/26 y H18/H20) estaba escrita sobre
el protoboard **anterior** a este rearmado y queda superada solo para estos dos
componentes. Se relevó el armado real fila por fila, sin asumir nada del plano
viejo.

**Armado confirmado**:

| Fila | Nodo | Contenido |
|---|---|---|
| 3 | 3V3 | GY-521 + 2 encoders (GND de ambos directo al riel "−") |
| 5 | — | vacía (se había considerado como parte del nodo 3V3, descartada) |
| 12 | 3V3 | Pin `3V3` del ESP32, unido por jumper a la fila 3. **C6** entre esta fila y el riel "−" |
| 13 | `EN` | Pin `EN` (reset) del ESP32 — consecutivo a la fila 12, confirma la serigrafía de fábrica (`3V3` seguido de `EN`) de la NodeMCU-32S. **C_en** entre esta fila y el riel "−" |
| 30-B | `VIN` | Cable a `VIN` del ESP32 (referencia, sin cambios en esta sesión) |

Riel "−" = riel GND lógico común del protoboard, el mismo que recibe el `GND`/`OUT−`
del Buck — no un GND aparte (ver sección 1 y el bug de "GND común ESP32↔L298N").

**Verificación con multímetro (modo Ω, no continuidad) antes de energizar**:

| Medición | Resultado | Interpretación |
|---|---|---|
| Fila 12 ↔ "−" | **135kΩ** | Sin corto (un corto real da unos pocos Ω). No es el valor aislado de C6: la fila 12 comparte nodo con la fila 3, así que la lectura incluye pull-ups I2C del GY-521 y/o caminos de fuga por diodos de protección del MPU6050/ESP32 — un cerámico sano y solo en ese nodo daría mucho más alto. |
| Fila 13 ↔ "−" | **Abierto / sin lectura** | Sin corto. Acá sí es la lectura "limpia": la fila del `EN` no tiene ningún otro dispositivo colgando, solo C_en — un cerámico sano y aislado se comporta como circuito abierto en DC. |

Ninguna de las dos dio un corto franco (0Ω o unos pocos Ω), que era el único
resultado que hubiera bloqueado seguir. **C6 y C_en quedan instalados y
verificados**, pendiente la prueba de energización real del sistema completo.

---

### 7.26 Lazo de masa por `GND` del header — detectado por auditoría de documentación, no por medición (2026-09-12)

**Qué pasó**: durante el recableado del protoboard se conectó el pin `GND` del header
de señal de cada IBT-2 al riel "−" lógico, **con `B−` de cada driver ya cableado a la
soldadura**. Como V1 (2026-09-10) había medido que `B−` y `GND` del header son el
mismo nodo dentro del módulo, eso armó dos caminos de masa en paralelo entre el riel
lógico y el nodo estrella: uno por el Buck (no aislado, `IN−`↔`OUT−` internos) y otro
por el `GND` del header → `B−` → soldadura. **Lazo de masa**, justo lo que la
topología estrella del proyecto existe para evitar.

**Fix**: se retiró el cable `GND` del header → riel "−". Los cables que quedan
conectados del lado del header (`GND`, `R_IS`, `L_IS`) quedan aislados en su punta
libre.

⚠️ **`R_IS`/`L_IS` abiertos es el estado correcto**, no una omisión: son salidas de
sensado de corriente, y sin resistencia a GND simplemente no hay sensado (ver 8.1,
"opcional, sin cablear"). No hay riesgo eléctrico en dejarlos flotando.

⚠️ **Nota mecánica, no eléctrica**: un conductor conectado de un lado y libre del
otro, aunque esté aislado en la punta, sigue siendo un cable suelto dentro de un robot
que va a caerse repetidamente durante el entrenamiento de RL. Si el aislante se zafa y
la punta del `GND` del header toca `B+` o una pestaña de motor, entra corriente de
potencia por la masa lógica. **Sujetarlos con brida a la estructura, o retirarlos del
header si no se van a usar pronto.**

#### La lección: no alcanza con medir y cerrar — la *decisión* tiene que propagarse

Esto **no** se detectó midiendo. Se detectó cruzando las fuentes de verdad entre sí.
V1 estaba correctamente medida y correctamente decidida, pero su resultado vivía en un
solo archivo:

| Fuente | Tenía el resultado de V1 |
|---|---|
| `plan-cableado-senales.html` | ✅ sí, completo |
| `conexiones-registro-pruebas.md` 8.4 | ⛔ no — seguía como "⚠️ A MEDIR" |
| `conexiones-registro-pruebas.md` 2 (tabla) | ⛔ no — seguía listando el cable como "condicionado" |
| `CLAUDE.md` (pendientes) | ⛔ no — seguía como `[ ]` sin marcar |
| `plano-potencia.html` | ⛔ no — seguía como "⚠️ Condicionado a §4 — puede ser lazo de masa" |

Al recablear se consultó el estado "condicionado/pendiente" de 4 de esas 5 fuentes, y
se resolvió la ambigüedad cableando ambos GND — que es la opción que V1 había
descartado explícitamente. **El proyecto ya tenía la regla de consultar la
documentación antes de asumir; el agujero era que la documentación se contradecía a sí
misma.**

Regla que queda: **una medición que cierra un punto abierto no está cerrada hasta que
su decisión está escrita en todas las fuentes que mencionan ese punto.** Un resultado
registrado en un solo archivo, mientras los otros siguen diciendo "a medir", es peor
que no tenerlo: genera la falsa sensación de que el punto está resuelto, mientras quien
cablea lee "pendiente" y decide por su cuenta.

---

## 8. Próximo paso

**Estado al 2026-08-05.** Ya cerrado: Test 1 (MPU, 7.10), Test 2 (motores — riesgo de
3.3V hacia el L298N descartado, bug de GND encontrado y corregido), sentido de giro de
M1 corregido físicamente intercambiando OUT1/OUT2. Protoboard del ESP32 cableado
completo (sección 3.1): I2C, encoders, control de motores, pull-downs, C4 y C5
instalados.

**Test 3 corrido (ruedas en el aire)** — ver 7.11: PPR confirmado (69.1), sentido de
giro validado (M1/M2 opuestos por diseño, esperable por montaje en espejo), zona
muerta por encoder medida (M1 85.2 media, M2 83.9 media, con una corrida de M2
descartada por anómala).

**Lo que sigue, en orden:**

1. **Instalar C6** (100nF, desacople del riel 3V3) si aparece espacio — diferido,
   no crítico. Ver sección 6.
2. **Correr Test 3 Fase 2 en piso/tethered** — falta esta condición para completar
   el protocolo aire/piso de 7.8/7.11.
3. **Portar Test 1b** a ESP32 (calibración del MPU) — el único test que queda sin migrar.
4. Repetir 7.4 (Buck bajo carga) con el hardware nuevo y el ESP32 transmitiendo por WiFi.
5. Decidir si el software de control necesita invertir el signo de M1 o M2 al combinar
   velocidades (hallazgo de la Fase 1, ver 7.11) — anotarlo cuando se escriba el PID.

✅ **`MOVE_THRESHOLD` evaluado con el PPR ya conocido**: 5 pulsos sobre 69.1 PPR en
300ms equivale a ~7% de vuelta (~14.5 RPM) — umbral razonablemente sensible, no hace
falta subirlo por ahora. La corrida anómala de M2 en 7.11 (5/5 en el mismo duty) no
parece deberse a que el umbral sea demasiado bajo, sino a backlash mecánico
generando pulsos sin rotación sostenida — si vuelve a repetirse, ahí sí reconsiderar
`MOVE_THRESHOLD` o agregar una segunda ventana de confirmación.

**ω₀ medido** (2026-08-15, ver 7.12): **7.4 rad/s** por análisis de video sobre pivote rígido. Alimenta directamente el modelo simulado para la optimización por PSO del proyecto de Algoritmos Evolutivos I. ⚠️ **Dos mediciones anteriores quedaron descartadas por montaje inválido: 4.07 rad/s (10/08) y 2.33 rad/s (13/08).** Ambas tenían dispersión baja y aun así medían la cantidad equivocada. El chequeo que las detecta —`L_eq = g·(T/2π)²` tiene que dar del orden del tamaño del robot— está en 7.12 y hay que aplicarlo a cualquier medición futura de ω₀.

### 8.1 Estado al 2026-08-14 — reemplaza la lista de arriba

**Cerrado en esta sesión** (ver 7.13, 7.14, 7.15):

- Falla del AP: diagnosticada y resuelta (GY-521 sin VCC). Lección de alimentación
  parásita por I2C documentada en 7.13.
- Falla de los motores: **el switch de V+ estaba en OFF** — tercera aparición del
  mismo modo de falla. Checklist preventivo en 7.14.
- Cap de PWM: el 121 estaba mal calculado. Rango extendido a 160 con topes de
  seguridad en capas (7.14).
- Asimetría M1/M2 cuantificada: M2 necesita +15 a +25 de duty (7.14).
- Control de velocidad en lazo cerrado por encoder: funciona en ambos sentidos.
- Hipótesis del calentamiento de motores: **descartada con evidencia** (7.15).

**Abierto, en orden de prioridad:**

1. **Medir K_U con montaje rígido** — ver el criterio de pasa/no pasa y el método
   corregido en 7.15. Es el único parámetro del modelo de PSO todavía sin medir.
2. **El AP se cae con batería sola** (sin USB) — ver 7.13. Hipótesis viva: picos de
   corriente del radio WiFi que el Buck no sostiene.
3. **Medir voltaje directo en las pestañas del motor a duty 160** para separar la
   caída del L298N de la fricción de la reductora (7.14).
4. Correr Test 3 Fase 2 en piso/tethered — falta esa condición (7.8/7.11).
5. Instalar C6 (100nF, desacople 3V3) si aparece espacio — diferido, no crítico.
6. Portar Test 1b a ESP32 — único test sin migrar.
7. Reemplazar C1 por uno de 25V/35V antes de pruebas prolongadas a máxima carga.

### 8.2 Estado al 2026-08-17 — reemplaza la lista de 8.1

**Cerrado desde entonces:**

- **K_U medido y cerrado** (7.17, Test 7): 0.0123, θ recuperable 2.07° contra un
  criterio de 5°. **Falta un factor 2.42 de torque.** Era el punto 1 de 8.1.
- **La vía de medir la caída del L298N con multímetro: descartada** (7.18, Tests 8 y
  8b). Era el punto 3 de 8.1. No produjo ningún número utilizable, y **no hay que
  reintentarla** — el driver degrada su salida al calentarse, así que toda medición
  eléctrica en stall mide un blanco móvil.

**Reformulado:**

- El punto 3 de 8.1 ("medir voltaje en las pestañas a duty 160") existía para cerrar
  la revisión del cap de PWM. **Era el instrumento equivocado para esa pregunta**: el
  Test 7 ya mide τ(u) directo y muestra que sigue creciendo lineal en 160, sin
  saturación, que es exactamente lo que hacía falta saber. La tensión nunca fue
  necesaria para eso.

**Abierto, en orden de prioridad:**

1. 🎯 **ENSAYO TÉRMICO DE CICLO CONTINUO en BRAKE, para fijar el cap operativo.**
   Es lo que decide si el 5.47° de 7.19 se sostiene o no: el margen del 9.4% existe
   sólo a duty 160, y a duty 127 la proyección da ~4.3° (no pasa). Todo lo demás del
   proyecto depende de este número.
2. **Portar el modo BRAKE a los sketches de control** (`EN=1`, PWM sobre los IN).
   Es el cambio de firmware que vale 2.67× de torque. Ver 7.19.
3. **El AP se cae con batería sola** (sin USB) — sigue abierto desde 7.13. Hipótesis
   viva: picos de corriente del radio WiFi que el Buck no sostiene. C4b (1000µF/50V)
   no alcanzó.
4. **Medir `R_m` de ambos motores** — barato, testea la conjetura de 7.18 sobre la
   asimetría M1/M2 y da la corriente de stall, que le falta al modelo de PSO.
   Óhmetro con offset restado, promediando varias posiciones de rotor.
5. **Corregir el modelo del notebook de PSO**: `K_U = 0.03` asumido (real 0.0123),
   `DEAD_ZONE = 85` medida al aire (real 92 en M1, 102 en M2), asimetría M1/M2 no
   modelada, y ganancias vigentes optimizadas con ω₀ = 2.33 (real 7.4).
6. Correr Test 3 Fase 2 en piso/tethered — falta esa condición (7.8/7.11).
7. Instalar C6 (100nF, desacople 3V3) si aparece espacio — diferido, no crítico.
8. Portar Test 1b a ESP32 — único test sin migrar.
9. Reemplazar C1 por uno de 25V/35V antes de pruebas prolongadas a máxima carga.

⚠️ **Todo lo que sea escribir el lazo de control o re-optimizar por PSO sigue
bloqueado detrás del punto 2**: no tiene sentido sintonizar una planta que todavía no
se sabe si puede balancear.

---

## 8. Driver BTS7960 / IBT-2 — cableado y verificación de consistencia (2026-08-29)

Reemplaza al L298N. **Decisión tomada**: 2× módulo IBT-2 (cada uno con 2 chips
BTS7960B formando un puente H completo) — **un módulo por motor**. Cierra el pendiente
"elegir driver de reemplazo"; el chasis N3 ya se cortó con dos huecos de 50×50mm para
estos módulos.

✅ **Actualizado 2026-09-12: el cableado está hecho** (potencia y señal — filas
vigentes en 3.1b). Lo que sigue se escribió como plan, con el robot desarmado para el
rediseño de chasis; se conserva porque separa lo confirmado por datasheet de lo que
falta verificar. **Estado de sus puntos abiertos:** 8.3 capacidad de borneras ✅
cerrada, 8.4 masa `B−`/`GND` ✅ cerrada, **8.7 coast-vs-brake ⏳ sigue abierto** (es
inferencia de datasheet, sin pasar por la balanza).

### 8.1 Pinout completo

**Señal — reusa los mismos 6 GPIO que manejaban el L298N.** Cambia solo a qué pin del
módulo llega cada cable.

⛔ **Las filas de esta tabla son del protoboard VIEJO (3.1) y ya no valen.** Cuando se
escribió esto (2026-08-29) se afirmaba que "no hay que tocar ninguna fila del
protoboard" — cierto entonces, falso ahora: el protoboard se rearmó por completo el
2026-09-12, con M1 reubicado al lado derecho (F–J) y M2 al izquierdo (A–E). **Las
filas vigentes están en 3.1b**; los GPIO no cambiaron.

| ESP32 | Fila (3.1 — ⛔ vieja, ver 3.1b) | L298N (antes) | IBT-2 (ahora) |
|---|---|---|---|
| GPIO27 | 23 (I) | ENA | `R_EN` + `L_EN` M1, atados juntos |
| ~~GPIO14~~ → **GPIO19** | 22 (H) → ver 3.1b | IN1 | `RPWM` M1 — ⚠️ **movido a GPIO19** (salida en boot del 14) |
| GPIO13 | 21 (G) | IN2 | `LPWM` M1 |
| GPIO4 | 27 (E) | ENB | `R_EN` + `L_EN` M2, atados juntos |
| GPIO16 | 29 (C) | IN3 | `RPWM` M2 |
| GPIO17 | 28 (D) | IN4 | `LPWM` M2 |

Atar `R_EN`+`L_EN` a un solo GPIO es la práctica que recomienda el propio fabricante
del módulo; no hay necesidad identificada de habilitarlos por separado.

**Potencia y lógica**: ver la tabla de la sección 2.

**Opcional, sin cablear**: `R_IS`/`L_IS` de cada módulo (sensado de corriente). Atar
los dos IS de un módulo juntos da una señal analógica por motor → **GPIO34 (M1) y
GPIO35 (M2)**.

✅ **Consistencia verificada**: 34/35 son ADC1. En el ESP32 **los pines de ADC2 no
funcionan mientras el WiFi está activo**, y este proyecto reporta por AP — usar ADC2
habría dado lecturas rotas justo en las pruebas con el robot en movimiento. Los otros
libres full-featured (18, 19, 23) no tienen ADC. 34/35 es la única opción correcta.

Factor de conversión del datasheet oficial: `k_ILIS = I_L / I_IS = 8500` típico. Con
1kΩ a GND en el pin IS: `V_IS = (I_L / 8.5) V`. ⚠️ Verificar con multímetro si el
módulo ya trae esa resistencia antes de asumir su valor.

### 8.2 Nivel lógico 3.3V — ✅ CONFIRMADO POR DATASHEET, no hace falta medirlo

Datasheet oficial Infineon BTS7960 Rev 1.1, **4.4.6, pág. 20** (`docs/BTS7960.pdf`):

| Parámetro | Símbolo | típ. | máx. |
|---|---|---|---|
| Nivel alto `INH`, `IN` | V_INH(H) / V_IN(H) | 1.75 / 1.6 V | **2.15 / 2.0 V** |
| Nivel bajo `INH`, `IN` | V_INH(L) / V_IN(L) | 1.4 V | (mín 1.1 V) |

**Son valores absolutos, NO referenciados a V_S.** El peor caso de umbral alto es
2.15V; los 3.3V del ESP32 quedan **1.15V por encima**. Margen de sobra.

4.4.1, textual: *"The control inputs IN and INH consist of TTL/CMOS compatible schmitt
triggers... No external driver is needed. The BTS7960 can be interfaced directly to a
microcontroller."*

⚠️ Diferencia con el L298N: ahí la compatibilidad con 3.3V **se verificó en banco**
(7.6) porque no había garantía de fábrica. Acá el fabricante la garantiza en el propio
IC — es el único punto de esta migración que **no** requiere validación empírica.

### 8.3 ✅ CERRADO (2026-09-03): resuelto por empalme/pigtail, opción 2 de abajo

⛔ ~~Capacidad confirmada físicamente: **3 conductores por lado** (1 entrada + 2 salidas).~~ — esa
capacidad era de la bornera dedicada que se había planeado y nunca se construyó. Superado por el
rearmado del chasis: ver 1 para la topología real de dos etapas (soldadura → bornera del Buck).

| | Salidas necesarias | Total con la entrada | ¿Entra? |
|---|---|---|---|
| L298N (antes) | Buck IN+, L298N VIN+ | 3 | ✅ justo |
| 2× IBT-2, bornera dedicada (plan original) | Buck IN+, `B+` M1, `B+` M2 | **4** | ⛔ **no** |
| 2× IBT-2, real (soldadura + bornera Buck, 2026-09-03) | cable combinado + C1 | **2** | ✅ **entró** |

Se resolvió con la opción 2 de la lista original (empalme/pigtail), no con la 1. El usuario soldó
batería + `B+` M1 + `B+` M2 (y del lado GND, batería + `B−` M1 + `B−` M2) en un empalme mecánico
separado, del que sale un solo cable combinado por lado hacia la bornera del Buck. Esa bornera solo
tiene que alojar ese cable combinado más la pata de C1 — 2 conductores, no 4. El punto de estrella real
quedó documentado como la soldadura, no la bornera, siguiendo la advertencia que ya estaba anotada acá
abajo.

Ídem del lado GND. Opciones que se habían evaluado, de mejor a peor (registro histórico):

1. **Bornera de más posiciones** en el nodo estrella. Preserva la topología estrella
   intacta, que es una decisión de arquitectura cerrada del proyecto. Era la recomendada, pero no la
   que terminó implementándose.
2. ✅ **Empalme/pigtail — la que se usó.** Un solo cable grueso combinado hacia la bornera del Buck,
   desde un nodo de unión soldado de batería + los dos `B+`. Documentado en 1: el punto de estrella
   real es la soldadura, no la bornera.
3. ⛔ **Puentear `B+` de un driver al otro en cadena.** Viola la regla "ningún cable
   hace de paso entre dos cargas distintas": el consumo de un motor pasaría por el
   terminal del otro. Justamente lo que la estrella existe para evitar, y con dos
   motores conduciendo a la vez —el caso que ya dio problemas en 7.20— es el peor
   momento para introducirlo.

### 8.4 ✅ CERRADO (medido 2026-09-10): `B−` y `GND` del header SON el mismo nodo

**Resultado de V1 (`plan-cableado-senales.html`, Sección 0): dan continuidad en los
dos módulos.** Se confirmó además, por separado, que `GND`↔`VCC` del header **no** da
continuidad — o sea que el cerámico C10/C11 soldado ahí no puenteó nada.

**Decisión que se desprende, vigente:**

```
cablear SOLO  B− → soldadura (nodo estrella de potencia)
NO cablear    GND del header → riel GND lógico
```

La referencia común entre ESP32 y driver ya queda establecida por la estrella: el
riel lógico llega a la soldadura a través del Buck (no aislado, une `IN−` y `OUT−`
internamente), y el driver llega por su `B−`. Llevar también el `GND` del header al
riel lógico agrega un **segundo camino en paralelo** = lazo de masa.

⚠️ **Esto se violó en el banco el 2026-09-12 y se corrigió** — ver 7.26. El resultado
de V1 había quedado registrado **solo** en `plan-cableado-senales.html`, nunca llegó a
esta sección, ni a `CLAUDE.md`, ni a `plano-potencia.html`, así que al recablear se
volvió a hacer lo que V1 había descartado.

⏳ **Sigue pendiente, distinto de esto**: el punto 4 del checklist (continuidad
ESP32↔driver de punta a punta) antes de energizar — el bug de 7.6b costó una sesión.

---

#### Registro original del planteo (antes de medirlo)

El datasheet, 6.2: *"The BTS7960 has no separate pin for power ground and logic
ground."* **El chip tiene un solo GND.** Es muy probable que en el módulo IBT-2 el pin
`GND` del header de señal y el terminal `B−` de potencia sean **la misma pista**.

Si lo son, cablear los dos (uno al riel GND lógico, otro al nodo estrella de potencia)
crea un **lazo de masa** en paralelo — exactamente lo que la topología estrella del
proyecto existe para evitar.

**Medición**: con el módulo desconectado, continuidad entre el pin `GND` del header y
el terminal `B−`.

- **Si dan continuidad** (esperado): cablear **solo `B−` al nodo estrella de potencia**
  y no llevar el `GND` del header al riel lógico — la referencia común ya queda
  establecida por la estrella. Verificar igual continuidad ESP32↔driver antes de
  energizar (el bug de 7.6b costó una sesión entera).
- **Si están separados**: cablear ambos como dice la tabla de la sección 2.

⚠️ **No asumir el resultado.** Es medición de 30 segundos y decide la topología de
masa de todo el robot.

### 8.5 Cap de PWM: el 160 del L298N NO se hereda

El tope de 160 se calibró contra un driver de BJTs que **se comía varios volts**. El
BTS7960 es MOSFET con R_ON del orden de 7–10 mΩ (datasheet 4.2.1): esa caída
prácticamente desaparece, así que **el mismo duty entrega bastante más tensión real al
motor**.

```
MAX_DUTY = 121      (6.0V nominal del motor / 12.6V batería llena × 255)
```

Es el mismo 121 del primer cálculo del L298N. Ahí resultó demasiado conservador
porque la cuenta ignoraba la caída del driver; **acá no hay esa caída que compense el
error, así que el número sí es el límite real.**

⚠️ Subirlo es una decisión aparte, después de medir τ(u) con balanza en este driver.

### 8.6 ⚠️ Todo el presupuesto de torque queda sin validar

`K_U = 0.0218`, `θ_umbral ≈ 3.65°` y las ganancias del PSO se midieron **con el L298N
y con el chasis viejo**. La migración cambia **las dos cosas a la vez**: driver nuevo
(más torque disponible al mismo duty) y chasis nuevo (menor `m·l`, mayor `ω₀`).

Ambos cambios empujan `θmax` **hacia arriba**, así que no hay riesgo de que el robot
quede peor — pero **ningún número del presupuesto actual sigue siendo válido como
cantidad**. Repetir, en este orden:

| Paso | Estado |
|---|---|
| `ω₀` (colgado del eje) | ✅ **8.0 rad/s**, medido 2026-09-09 (7.22) |
| `m` y `m·l` (dos apoyos, pivote libre) | ✅ **1150 g / 0.1040 kg·m → l=9.04cm, J=0.0159 kg·m²**, medido 2026-09-10 (7.23) |
| `τ(u)` con balanza (Test 7/9, **dos motores en brake a la vez**) | ⏳ **pendiente — es lo único que bloquea `θ_umbral`** |
| Re-correr el PSO | ⏳ pendiente, depende del anterior |

⚠️ Aplicar el chequeo `L_eq = g·(T/2π)²` a la medición de ω₀ — ya descartó dos
mediciones falsas en este proyecto.

### 8.7 Hipótesis abierta: el BTS7960 podría hacer BRAKE de fábrica

Con `EN=1`, `RPWM=PWM`, `LPWM=0`: según la tabla de verdad, `IN=0` mantiene encendido
el MOSFET inferior de esa mitad. Durante el tramo OFF de `RPWM`, **ambos terminales
del motor quedan atados a GND** — definición de brake, no de coast.

Si se confirma, el BTS7960 daría **de entrada** la ganancia de 2.67× que en el L298N
hubo que ganar moviendo el PWM de ENABLE a IN (7.19).

⚠️⚠️ **Es una inferencia del datasheet, NO una medición.** Este proyecto ya se equivocó
dos veces con ω₀ y dos con K_U por confiarle a una cadena de razonamiento lo que sólo
cierra la balanza. **No usar ni coast ni brake como supuesto en ningún cálculo** hasta
medirlo con el método del Test 9.

### 8.8 Checklist antes de la primera energización

Extiende el de 7.14 al hardware nuevo:

1. Switch de V+ en **ON**.
2. `B+` de **cada** módulo mide ~12V contra GND de potencia (son dos entradas ahora,
   no una — medir las dos).
3. `VCC` de **cada** módulo mide ~5V. Sin esto la lógica del driver no conmuta nada,
   aunque el firmware reporte todo bien (bug de 7.6, tres veces en este proyecto).
4. Continuidad entre un `GND` del ESP32 y el GND del driver (resuelto según 8.4).
5. **Las 2 pull-downs nuevas en `EN` instaladas** (sección 4).
6. Sentido de giro validado por encoder antes de subir el duty (Fase 1 del Test 3).
