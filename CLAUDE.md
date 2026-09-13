# Robot Balanceador — CLAUDE.md

## 🚧 FRONTERA: robot VIEJO vs. robot NUEVO (establecida 2026-09-12)

**El robot se rearmó.** Este archivo describe **dos plantas distintas**, y confundirlas
ya causó errores reales. La regla, sin excepciones:

> **PRE (planta vieja)** = L298N + chasis de contrachapado de 3 niveles.
> **Es documentación DEPRECADA.** Se usa **solo para repasar errores y experiencias**,
> nunca para planificar, cablear, construir ni calcular.
>
> **POST (planta nueva)** = 2× BTS7960 (IBT-2) + chasis de balsa de 4 niveles.
> **Es sobre esta que se desarrolla el robot.** Todo número que se use en un cálculo
> tiene que salir de acá.

La frontera es **de qué robot habla el documento**, no su fecha: los planos de diseño del
robot nuevo se escribieron entre el 2026-08-24 y el 2026-08-29, antes del rearmado
físico, y son POST.

### Parámetros — la tabla que hay que consultar antes de calcular

| Parámetro | ⛔ PRE (NO usar) | ✅ POST (vigente) |
|---|---|---|
| Driver | L298N (BJT) | **2× BTS7960 / IBT-2** (MOSFET) |
| Chasis | Contrachapado, 3 niveles, ~26cm | **Balsa, 4 niveles, 76×150×10mm, ~22cm** |
| `ω₀` | 7.4 rad/s (y 2.33 / 4.07, falsas) | **8.0 ± 0.1 rad/s** (7.22) |
| `m` | 1038 g | **1150 g** sin ruedas (7.23) |
| `m·l` | 0.1265 kg·m | **0.1040 kg·m** (7.23) |
| `l` | 12.2 cm | **9.04 cm** (7.23) |
| `J` | 0.0227 kg·m² | **0.0159 kg·m²** (7.23) |
| `K_U` | 0.0123 / 0.0326 / 0.0218 | ⏳ **SIN MEDIR** — falta balanza con el driver nuevo |
| `θ_umbral` | ≈3.65° | ⏳ **SIN NÚMERO** — depende de `K_U` |
| Cap de PWM | 160 (tope 170) | **121** como punto de partida (8.5), a re-derivar |
| Ganancias PID | Kp=3500, Ki=2297.02, Kd=484.22 | ⏳ re-correr el PSO con los valores nuevos |
| Buck | 4.98V (unidad destruida) | **5.01V** (unidad nueva, 2026-09-12) |
| `RPWM` M1 | GPIO14 | **GPIO19** (el 14 tiene salida en el boot) |
| Filas del protoboard | §3.1 (filas 20–30) | **§3.1b** del registro |

⚠️ **`θmax` con el `m·l` medido y el τ viejo: ≈4.44°** — es un **piso**, porque τ=0.079 N·m
se midió con el L298N. Sube cuando se mida `K_U` con los BTS7960.

### Qué sobrevive a las dos épocas (propiedades del hardware, no de la planta)

Calibración del MPU6050 (bias, escala, ejes) · PPR de encoders 69.1 y mapeo de colores ·
motor JGB37-520B 6V/793RPM · asimetría M1/M2 (M2 ≈ 81% del torque de M1) · restricciones
de GPIO del ESP32 · datos de datasheet del BTS7960 · **las 13 lecciones de método**.

### Dónde vive cada cosa

- **POST / vigente**: este archivo, `docs/conexiones-registro-pruebas.md` (§1–§6, §8, y
  §7.22–§7.26), y los planos de `docs/` — potencia, señales, conexiones, plan de
  cableado, distribución de niveles, plantillas, ensayo de `m·l`.
- **PRE / deprecado**: **[`docs/obsoleto/`](docs/obsoleto/README.md)** (con su propio
  README explicando la frontera), más las secciones de este archivo marcadas ⛔ y
  §7.1–§7.21 del registro.

---

## Qué es esto

Robot de balanceo (péndulo invertido de 2 ruedas), construido en **4 niveles de balsa (76×150×10mm cada uno)** unidos por varillas roscadas. Plataforma de prueba para:
- Filtro de Kalman (fusión de datos del MPU6050/GY-521).
- Controlador PID.
- Optimización de ganancias PID mediante PSO (Particle Swarm Optimization).

## Arquitectura de control (migrada a ESP32 el 2026-08-04)

**Un solo microcontrolador: ESP32 (NodeMCU ESP-32S, 38 pines).** Corre todo:
lectura del IMU por I2C, filtro de fusión, lazo PID, PSO, lectura de encoders y
generación directa del PWM hacia los **2× BTS7960** (era el L298N hasta el
2026-08-29). No hay micro secundario en el lazo.

- **Debug por USB serie** — ya no hay enlace UART entre chips que proteja los pines
  TX/RX, así que `Serial` vuelve a estar disponible para desarrollo. El reporte por
  WiFi sigue siendo útil para pruebas con el robot en movimiento, pero ya no es
  obligatorio.
- **Dos núcleos**: el lazo de control puede fijarse a un núcleo y el stack WiFi al
  otro. Resuelve estructuralmente el problema de convivencia tiempo-real/WiFi que
  tenía el ESP8266 de un solo núcleo.
- **FPU por hardware**: Kalman y PSO en punto flotante corren mucho más rápido que
  en el ESP8266 (que emulaba float por software).

> **El Arduino Nano queda montado en un flanco del chasis pero fuera del lazo de
> control** — no ejecuta nada, no recibe comandos. Se conserva físicamente por si
> hiciera falta revertir o reutilizarlo, no como parte activa del sistema.

## Perfil del usuario

Ingeniero, entusiasta de electrónica con conocimiento intermedio. Explicaciones técnicas directas están bien, sin simplificar en exceso — pero siempre justificando el "por qué" de cada recomendación.

## Reglas para Claude

- **Ser riguroso y crítico, no complaciente.** Si algo propuesto no es buena práctica, decirlo explícitamente con su justificación. No asumir que una idea es correcta solo porque el usuario la propuso.
- **No inventar pinouts, valores ni datos de hardware.** Si falta un dato de un componente no confirmado en este archivo, pedir verificación con multímetro o datasheet antes de asumir.
- **Nada de LaTeX en archivos `.md` ni en el chat** — no hay renderizador. Usar texto plano o Unicode (ω₀, ±, ≈). LaTeX solo en HTML que cargue MathJax.
- Este archivo es la fuente de verdad del proyecto. Si el estado del repo (código, carpetas) parece contradecirlo, señalarlo y preguntar antes de asumir o de editar este archivo unilateralmente.

## Archivos de referencia

- **[`docs/conexiones-registro-pruebas.md`](docs/conexiones-registro-pruebas.md)** — mapeo pin a pin y registro de todas las pruebas físicas con sus valores medidos. **Consultar antes de asumir cualquier pin o valor.** Su **sección 8** (2026-08-29) tiene el plan completo de cableado del driver nuevo (2× BTS7960) con los puntos abiertos marcados.
- **[`docs/BTS7960.pdf`](docs/BTS7960.pdf)** — datasheet **oficial de Infineon** del chip BTS7960B (Rev 1.1). Es la fuente de los umbrales lógicos (4.4.6), la tabla de verdad (4.4.5) y las recomendaciones de layout (6.2). **[`docs/bts7960_datasheet.pdf`](docs/bts7960_datasheet.pdf)** es la ficha del módulo IBT-2 (HW-039), menos rigurosa — ante discrepancia, gana el de Infineon.
- **[`test/`](test/)** — sketches de validación por subsistema, con [`test/README.md`](test/README.md) como guía. ⚠️ **Todos los sketches actuales están escritos para ESP8266 o AVR y requieren migración a ESP32** — ver "Estado de los tests tras la migración".
- **[`docs/teoria-control-balanceo-pso.html`](docs/teoria-control-balanceo-pso.html)** — documento HTML autocontenido: teoría del péndulo invertido, fusión de sensores, PID, sintonía, función de costo y PSO, con conexión explícita a los datos reales del proyecto. ⚠️ Su §0 y §7 describen la arquitectura ESP8266+Nano — **desactualizados tras la migración**, pendiente revisarlos.

- **[`docs/pinout-esp32.html`](docs/pinout-esp32.html)** — creado 2026-08-04. Diagrama visual de las dos filas de la NodeMCU ESP-32S, reconstruido a partir de foto de la placa, con cada pin marcado por categoría (usado en el proyecto / bootstrapping / flash interna — nunca tocar / solo entrada / libre). Pensado para consultar desde el celular mientras se cablea, cuando la placa está en el protoboard y se pierde referencia visual de la serigrafía.


- **[`docs/plano-potencia.html`](docs/plano-potencia.html)** — creado 2026-08-29. **El plano de potencia vigente.** Solo alimentación, sin señales de control: árbol batería→fusible→switch→nodo estrella→{Buck, 2× IBT-2}, riel lógico, tabla de conexiones punto a punto, capacitores y el checklist de primera energización adaptado a dos drivers. ✅ Actualizado 2026-09-12: los dos puntos que antes bloqueaban el cableado están cerrados (capacidad de borneras §3 por empalme, masa `B−`/`GND` §4 por V1 — el `GND` del header **no** se cablea al riel lógico). Apto para imprimir (A4) y para consultar del celular mientras se cablea.

- **[`docs/plano-senales.html`](docs/plano-senales.html)** — creado 2026-08-29. **El plano de señales vigente**, complemento de `plano-potencia.html`. Mapa GPIO↔protoboard↔destino para drivers/I2C/encoders, la tabla de verdad del BTS7960 que justifica mover las pull-down a `EN`, detalle de montaje de esas pull-down, LEDC/PWM y el estado (sin confirmar) de coast vs. brake en este driver. Junto con `plano-potencia.html`, reemplaza por completo a `docs/obsoleto/plano-electrico.html`.

- ⛔ **[`docs/obsoleto/plano-electrico.html`](docs/obsoleto/plano-electrico.html)** — creado 2026-08-05, **movido a `docs/obsoleto/` y marcado obsoleto 2026-08-29**. Describía alimentación + señales + pull-downs, todo con arquitectura L298N — ya cubierto por `plano-potencia.html` + `plano-senales.html`. Se conserva como referencia histórica (toda la caracterización de torque vigente se midió con este driver), no usar para cablear.


- **[`docs/plano-distribucion-niveles.html`](docs/plano-distribucion-niveles.html)** — creado 2026-08-24, reescrito 2026-08-29 (drivers en N3 propio), corregido 2026-09-12 a "4 niveles". Plantas a escala de los 4 niveles (N1-N4, todos ya cortados a 76×150mm), corte vertical del stack y masa/margen proyectados. Ver "Drivers a nivel propio" más abajo.

- **[`docs/plantilla-imprimible-niveles.html`](docs/plantilla-imprimible-niveles.html)** (+ `.pdf`) — creado 2026-08-29. **La plantilla vigente para marcar/perforar.** Escala real 1:1, los 4 niveles (N1-N4) en 2 páginas A4 apaisadas (2 niveles por hoja), minimalista a propósito — solo la línea de calibración de 50mm y las etiquetas necesarias para identificar cada componente. Reemplaza a las tres plantillas de abajo.

- ⛔ **[`docs/obsoleto/`](docs/obsoleto/README.md)** — **carpeta de documentación DEPRECADA (planta vieja).** Tiene su propio README con la frontera PRE/POST y la tabla de qué reemplaza a qué. Contiene: `plano-electrico.html` (potencia+señales con L298N), `plano-protoboard-esp32.html` y `plano-imprimible-esp32.html` (el protoboard de la era L298N, rehecho dos veces desde entonces), `plantilla-imprimible-nivel1-completa.html` / `plantilla-imprimible-nivel1.html` / `plantilla-imprimible-soporte-motor.html` (+ PDFs — arquitecturas de N1 superadas el 2026-08-28) y `reporte_01.html` (instrumental inercial en ESP8266). **Solo para repasar errores, nunca para cablear, construir ni calcular.**

- **[`docs/ensayo-ml-dos-apoyos.html`](docs/ensayo-ml-dos-apoyos.html)** — creado 2026-09-10. Banco de medición de `m·l` por dos apoyos sobre el **chasis rearmado**, con el procedimiento y los chequeos de validez. Complementa §7.17 (método) y §7.23 (resultado) del registro.

### Proyecto de Algoritmos Evolutivos I (entrega 23/08/2026)

Entregable de cursada, **no** firmware del robot. Usa los datos medidos del robot
real (ω₀, K_U, zona muerta, cap de PWM) para optimizar ganancias PID por PSO en
Python. ⚠️ **La ubicación cambió el 2026-08-19/20** — ya no vive todo bajo
`algoritmos_evolutivos_pso/`, quedó repartido en tres carpetas:

- **[`notebooks/pso_pendulo_invertido.ipynb`](notebooks/pso_pendulo_invertido.ipynb)** — el entregable principal: modelo simulado del péndulo con las no-linealidades reales del actuador (zona muerta asimétrica por motor, saturación, ω₀=7.4, K_U=0.0218 medido con los dos motores en brake conduciendo a la vez), PSO desde cero (constricción de Clerc-Kennedy), gráficos de convergencia. Corre de punta a punta (~40s). Tiene su propio [`notebooks/README.md`](notebooks/README.md) y `requirements.txt` — venv independiente del `.venv/` de la raíz.
  - ⚠️ **Sus parámetros son de la planta VIEJA (2026-09-09).** `ω₀=7.4` quedó superado por **8.0** (chasis rearmado, ver más abajo) y `K_U=0.0218` se midió con el L298N y el chasis de contrachapado. Como entregable de cursada está cerrado; **para el firmware hay que re-correrlo** con los parámetros nuevos. ✅ `ω₀=8.0`, `m=1150g`, `m·l=0.1040 kg·m` y `J=0.0159 kg·m²` **ya están medidos** (2026-09-09/10); **lo único que falta para poder re-correrlo es `K_U` con el driver nuevo**.
  - **Ganancias resultantes: `Kp=3500` (clavado en el límite superior del espacio de búsqueda), `Ki=2297.02`, `Kd=484.22`.** Ver "Corregir el modelo del notebook de PSO" en pendientes — no son para cargar directo en firmware sin más.
- **[`algoritmos_evolutivos_pso/`](algoritmos_evolutivos_pso/)** — lo que quedó ahí: `pdf_teoria/teoria_pendulo_pid_pso.pdf` (16 pág., guía teórica) y `video_omega0/` (medición de ω₀ por video). Sumó `pso_explicacion_profunda.html`, sin revisar todavía.
  - **[`video_omega0/2026-09-09_rearmado/`](algoritmos_evolutivos_pso/video_omega0/2026-09-09_rearmado/README.md)** — ⭐ **el análisis de ω₀ vigente** (8.0 rad/s, chasis rearmado). Pipeline reproducible en numpy puro: `rastrear.py` (segmentación y θ(t)), `periodo.py` (cruces y extrapolación a amplitud cero), `verificar.py` (chequeos de cuerpo rígido y muestreo), `overlay.py` (contact sheets y video anotado). Videos fuente en `experiments/w0_videos/`.
- **[`informes/`](informes/)** — nueva (2026-08-19/20): `borrador_entrega.md` + `entrega_pso_pendulo.pdf` (generado por `generar_pdf.py`), con las gráficas de convergencia, el diagrama de cuerpo libre y la foto del robot. **Parece ser el PDF de entrega que faltaba** — a diferencia del documento teórico de 16 páginas, este sí sigue la estructura problema/enfoque/inconvenientes + URL de repositorio que pide el enunciado (`docs/uba-2026-mia-ae1-dp1.pdf`). ⚠️ No verificado línea a línea contra el enunciado — confirmar antes de dar la entrega por cerrada.

## Inventario de hardware

### ESP32 — NodeMCU ESP-32S (38 pines), único micro del lazo de control

Chip USB-serie **CP2102** (puede requerir driver CP210x en macOS). Conector USB-C.

**Pines asignados** (ver `docs/conexiones-registro-pruebas.md` sección 3):

| Función | GPIO |
|---|---|
| I2C SDA → GY-521 | 21 |
| I2C SCL → GY-521 | 22 |
| Encoder M1 canal A | 32 |
| Encoder M1 canal B | 33 |
| Encoder M2 canal A | 25 |
| Encoder M2 canal B | 26 |
| `R_EN`+`L_EN` M1 (atados) — era ENA | 27 |
| `RPWM` M1 — era IN1 | **19** (⚠️ **no 14** — ver nota) |
| `LPWM` M1 — era IN2 | 13 |
| `R_EN`+`L_EN` M2 (atados) — era ENB | 4 |
| `RPWM` M2 — era IN3 | 16 |
| `LPWM` M2 — era IN4 | 17 |

⚠️ **Remapeados 2026-08-29 al migrar L298N → 2× BTS7960.** Son **los mismos 6 GPIO
y las mismas filas del protoboard** — cambia solo a qué pin del módulo llega cada
cable. Detalle completo en `docs/conexiones-registro-pruebas.md` sección 8.

⚠️ **`RPWM` M1 está en GPIO19, NO en GPIO14** (corregido 2026-09-12). El 14 figuraba
en esta tabla por arrastre del mapeo del L298N, pero **GPIO14 se reporta de forma
consistente como pin con salida activa durante el boot** (junto con 1, 3, 5 y 15) — cae
bajo la misma regla que ya tiene GPIO5 en este proyecto: *no usarlo para nada que mueva
un motor*. El cambio a GPIO19 está analizado en `docs/plano-conexiones-esp32.html` §3 y
**ya está implementado en el cableado**. ⚠️ No figura en el datasheet oficial de
Espressif: es sospecha fuerte y bien reportada, no dato cerrado — pero GPIO19 no cuesta
nada y elimina el riesgo.

**Total: 12 GPIO.** Libres tras la asignación: 5, 18, 23 (full-featured) + **14** (libre
pero a evitar para motores) + 34, 35, SVP(36), SVN(39) (solo entrada). TX/RX (1/3)
reservados para debug USB.

**Pines a evitar en esta placa:**

| Pines | Motivo |
|---|---|
| CLK, SD0, SD1, SD2, SD3, CMD (GPIO 6–11) | **Nunca usar** — flash SPI interna. |
| SVP (36), SVN (39), 34, 35 | Solo entrada. Sin pull-up/pull-down interno. |
| 0, 2, 12, 15 | Bootstrapping. GPIO12 en alto al arrancar puede impedir el boot. |
| 5 | Emite un pulso al bootear — no usar para nada que mueva un motor. |
| **14** | **Salida activa durante el boot** (reportado, no en el datasheet oficial). Era `RPWM` M1 y **se movió a GPIO19 por esto**. Queda libre, pero no usarlo para motores. |

⚠️ **Los encoders NO están en los pines de solo-entrada a propósito**: no está
confirmado si su salida es push-pull o colector abierto. Si fuera colector abierto
necesitaría pull-up, y esos pines no lo tienen interno. Como sobran pines, se
priorizó seguridad sobre elegancia. Si se confirma que son push-pull, se pueden
mover y liberar 4 pines full-featured.

### Resto del hardware

- **MPU6050 (GY-521)**: alimentado a **3.3V** desde el pin 3V3 del ESP32.
- **2× BTS7960 (módulo IBT-2)** — ✅ **driver elegido, decidido 2026-08-29.** Cierra el pendiente que estaba abierto entre BTS7960 / DRV8871 / TB6612FNG. Cada módulo es un puente H completo (2 chips BTS7960B) para **un** motor: dos chips separados por construcción, que es justamente lo que ataca la hipótesis de 7.20 (la caída del 31-36% con carga simultánea sería interna al L298N, que tiene los dos puentes en un mismo die). El chasis N3 ya está cortado con dos huecos de 50×50mm para estos módulos.
  - ✅ **Lógica 3.3V confirmada por datasheet oficial** (Infineon, 4.4.6): umbral alto de `IN`/`INH` es **2.15V máx, valor absoluto** (no referenciado a V_S). Los 3.3V del ESP32 quedan 1.15V por encima. *"No external driver is needed."* — no hace falta level shifter ni validación en banco para este punto.
  - Terminal de potencia: `B−` `B+` `M+` `M−`. Header de señal: `VCC` `R_IS` `R_EN` `RPWM` / `GND` `L_IS` `L_EN` `LPWM`.
  - **Trae 330µF de bulk en placa por módulo** — no hace falta agregar bulk externo por driver.
  - ✅ **Cableado 2026-09-12** (potencia y señal): `B+`/`B−` de cada módulo a la soldadura/nodo estrella, `VCC` al riel de 5V, y las 6 líneas de señal con sus pull-downs — M1 al lado derecho del protoboard (F–J), M2 al izquierdo (A–E). Filas en `docs/conexiones-registro-pruebas.md` 3.1b. ⛔ **El `GND` del header NO se cablea al riel lógico** (es el mismo nodo que `B−`, sería lazo de masa — ver 8.4 y 7.26).
  - De los 3 puntos que estaban abiertos en la sección 8 del registro: capacidad de borneras ✅ cerrada (2026-09-03, por empalme), masa `B−`/`GND` ✅ cerrada (2026-09-10, V1), **coast-vs-brake sigue abierto** — es inferencia de datasheet, sin medir con balanza (8.7).
- **L298N** — ⛔ **deprecado 2026-08-29**, reemplazado por los BTS7960. Se conserva el registro porque toda la caracterización de torque vigente (`K_U`, `θ_umbral`, ganancias del PSO) se midió con él. Jumper de 5V removido permanentemente. Su pin lógico "5V" se alimenta desde el riel del Buck (ver "Alimentación lógica del L298N"). OUT1/OUT2 → Motor 1 (derecho), OUT3/OUT4 → Motor 2 (izquierdo).
  - ⚠️ Usa BJTs no MOSFETs → caída de tensión por canal, menor eficiencia, más calor.
  - ✅ **Verificado tras la migración (2026-08-05)**: el L298N responde correctamente a las señales de 3.3V del ESP32 (IN1–IN4/ENA/ENB) — no hace falta level shifter. El riesgo se consideraba abierto porque el Nano manejaba esas líneas a 5V; quedó cerrado con `test2_motores_esp32/` corriendo con ambos motores respondiendo.
- **Motorreductor JGB37-520B, 6V, 793 RPM** (cerrado, 2026-07-30): la lectura original de "160RPM, 12V" correspondía a una foto de un motor similar pero distinto al instalado — descartada.
  - **Ambos canales de encoder (A y B) por motor ahora conectados** — la limitación anterior a un solo canal era por escasez de pines del ESP8266, ya no aplica. Habilita detección de sentido de giro, no solo de velocidad.
  - ✅ **PPR confirmado (2026-08-05, Test 3 Fase 0)**: 69.1 pulsos/vuelta (decodificación 1x), igual en ambos motores. Datasheet exacto del motor sigue sin cargarse, pero el PPR ya no es una suposición. 793 RPM es nominal a 6V sin carga.
  - ✅ **Sentido de giro entre M1 y M2**: confirmado por diseño que dan signo opuesto de encoder para el mismo comando FWD — están montados en espejo en los dos costados del chasis. **El firmware de control va a necesitar invertir el signo de uno de los dos motores** al combinar velocidades.
- **Arduino Nano v3.0**: montado en un flanco del chasis, **fuera del lazo de control**. No ejecuta nada. Conservado por si hiciera falta revertir o reutilizarlo.
- **Batería LiPo Cecicebb 2200mAh 3S (11.1V), 35C**, conector XT60. Fusible 10A + switch en línea con V+ antes del nodo estrella. ⚠️ **El fusible se reemplazó el 2026-09-12**: quedó dañado en el incidente de polaridad invertida del 2026-09-10.
  - Medida el 2026-08-04 en 10.80V (celdas 3.50/3.60/3.70V) — descargada y desbalanceada. ✅ **Recargada**: medida de nuevo el 2026-08-05 en **12.32V** en el pack completo, rango sano para 3S. Sin desglose por celda en esta medición — no confirmar "balanceada" hasta volver a medir celda por celda.
- **Convertidor DC-DC Buck XL4016**: ✅ **unidad vigente calibrada en 5.01V** en OUT+/OUT− (2026-09-12). El **4.98V** histórico era de la unidad destruida el 2026-09-10 — no usarlo como referencia. Alimenta solo el riel lógico. No aislado — une IN− y OUT− internamente.
  - ⚠️ Corriente máxima y rango de entrada exactos sin confirmar por datasheet del módulo.
  - ⚠️ **El ESP32 tiene picos de corriente mayores que el ESP8266** en transmisión WiFi. Verificar que no haya reinicios bajo carga y revisar C4.
  - ⛔ **La unidad calibrada en 4.98V quedó DESTRUIDA el 2026-09-10** (polaridad invertida en su entrada — ver "Buck destruido por polaridad invertida" más abajo). ✅ **Reemplazada por una unidad nueva, verificada y calibrada en 5.01V el 2026-09-12**, junto con el reemplazo de `C1`, del capacitor de salida y **del fusible de 10A**. ⚠️ **El 4.98V ya no es la referencia** — era de la unidad destruida.

## Límite de PWM a los motores (revisado 2026-08-14)

> ⛔ **PRE — planta vieja (L298N).** El cap de **160/170** de esta sección es del L298N.
> Con los BTS7960 el punto de partida es **121** (registro 8.5) y hay que re-derivarlo por
> criterio térmico. ✅ Lo que sí sigue vigente: la API de LEDC del core 3.x, al final.

⚠️ **El cap histórico de 121 estaba mal calculado.** Se conserva como valor por
defecto conservador, pero ya no es "el" límite correcto.

**El cálculo original**: batería 3S llena da hasta **12.6V**, motor rated a **6V** →
duty máximo = 6.0/12.6 = **47.6%** → 121/255. Referencia 12.6V fijo (peor caso
físico), no medición dinámica — no hay divisor resistivo de batería en el inventario.

**Por qué está mal**: asume que al motor le llega todo el voltaje del bus. El L298N
usa BJTs y cae varios volts. Medido por encoder (Test 6, ver
`docs/conexiones-registro-pruebas.md` 7.14), infiriendo voltaje de `RPM/793 × 6V`:

| duty | M1 RPM | V equivalente |
|---|---|---|
| 121 | 339.7 | **2.57V** |
| 160 | 655.6 | **4.96V** |

**A duty 121 el motor recibía ~2.6V, no 6V** — más de la mitad del rango útil
desperdiciado. Explica por qué los motores nunca se vieron rápidos.

⚠️ Esos ~3V de diferencia **no son todos del L298N**: incluyen la fricción de la
reductora. **Pendiente**: medir con multímetro directamente en las pestañas del motor
girando, para separar ambas contribuciones. Hasta entonces el cap de 160 está
justificado por inferencia, no por medición directa.

**Estructura vigente de topes** (implementada en `test6_velocidad_esp32/`): tope
absoluto 170 que ninguna ruta del código cruza, tope activo ajustable 121→160 solo
con motores parados, más cortes automáticos por sobrevelocidad (850 RPM), motor
trabado (1.5 s sin pulsos con duty alto), reversión del cap extendido tras 15 s
sostenidos, y apagado por tiempo (60 s).

- Debe estar como constante explícita en cualquier sketch que mueva motores, no como valor mágico.
- ⚠️ **En ESP32 no existe `analogWrite()` como en AVR** — el PWM se genera con el periférico LEDC. **Configurar LEDC a 8 bits de resolución** para que el cap de 121 siga siendo directamente aplicable sin reconvertir escalas.
- **API correcta para el core instalado (esp32:esp32 3.3.11), verificada por compilación 2026-08-04:**
  ```cpp
  ledcAttach(pin, frecuenciaHz, resolucionBits);  // ej: ledcAttach(27, 20000, 8)
  ledcWrite(pin, duty);                            // duty 0..255 con 8 bits
  ```
  ⚠️ **No usar** `ledcSetup()` + `ledcAttachPin()` + `ledcWrite(canal, ...)` — es la API vieja (core 2.x), que aparece en la mayoría de los tutoriales pero **no existe en el core 3.x**. La nueva trabaja por pin, no por canal.

## Calibración del MPU6050 (cerrada — sigue vigente tras la migración)

Son propiedades del sensor, no del microcontrolador: **no cambian con el cambio de
micro**. Detalle de capturas en `docs/conexiones-registro-pruebas.md` sección 7.5.

- **Convención de ejes** (robot en orientación normal de operación):
  - **Z** = arriba (contra la gravedad).
  - **Y** = eje de inclinación / pitch (frente-atrás).
  - **X** = eje de las ruedas (lateral). Confirmado por rotación: pico de giro 0.571 rad/s en X vs 0.137/0.041 en Z/Y (razón 4.2x).
- **Bias del acelerómetro** (m/s²): X=0.265, Y=0.050, Z=0.560.
- **Escala del acelerómetro** (1.0=ideal): X=1.005, Y=1.000, Z=1.015.
  - Los 3 ejes dentro de ±1.5% — **no hay error de escala global**. El 10.51 m/s² del Test 1 se explica casi enteramente por el bias de Z: 9.80665×1.015+0.56≈10.51.
- ✅ **Validados en hardware ESP32 real (2026-08-05)**: con bias/escala aplicados, la magnitud calibrada dio 9.76–9.82 m/s² en reposo sobre 3 orientaciones distintas (gravedad alineada con X, Y y Z por turno) — confirma que los valores no eran específicos del banco de pruebas original. Detalle en `docs/conexiones-registro-pruebas.md` sección 7.10.
- ⚠️ **Todavía no aplicados en el firmware de control** (Kalman/PID) — por ahora solo se muestran en el reporte del Test 1. Pendiente decidir dónde aplicarlos (candidato natural: corrección antes de alimentar el Kalman).
- Bias del giroscopio: se recalibra en cada arranque (varía con temperatura), por eso todos los tests de MPU incluyen su rutina de calibración al vuelo.

## Decisiones de arquitectura cerradas (no reabrir sin razón nueva)

- **Un solo micro (ESP32)** para todo el lazo de control. Ver "Decisiones revertidas" para el porqué del cambio.
- Masa en configuración estrella: un único punto donde confluyen batería, `B+`/`B−` de cada driver y la entrada del Buck; GND de potencia y GND lógico se unen únicamente ahí.
  - ⛔ **Actualizado 2026-09-03 (rearmado del chasis) — cambia el lugar físico, no el principio.** La versión original de esta decisión (hasta 2026-08-29) exigía una **bornera dedicada**, separada de toda carga. En el rearmado esa bornera dedicada se reemplazó por una **soldadura**: batería (post-switch) + `B+`/`B−` de M1 y de M2 se unen ahí por empalme mecánico y estaño — **esa soldadura es el nodo estrella real**, no la bornera del Buck. De la soldadura sale un solo cable combinado por lado hacia la **bornera de entrada (`IN+`/`IN−`) del Buck XL4016**, que recibe ese cable combinado más la pata de C1 — 2 conductores, no 4. Confirmado por el usuario, no una desviación no documentada. ✅ El conflicto de capacidad que esto parecía agravar quedó **cerrado**: la bornera del Buck nunca tuvo que alojar más de 2 conductores. Detalle en `docs/conexiones-registro-pruebas.md` §1 y §8.3, y en `docs/plano-potencia.html`.
- El Buck no debe **alimentar** `B+`/`B−` de los motores (no debe circular corriente de motor por dentro del Buck) — sigue valiendo aunque el punto de unión física haya cambiado. Esta regla no cubre el pin lógico `VCC` de cada IBT-2 (antes "5V" del L298N), que sí se alimenta desde el Buck (consumo despreciable).
- ⛔ **El `GND` del header de señal de cada IBT-2 NO se cablea al riel GND lógico** (cerrado 2026-09-10 por medición V1, reafirmado 2026-09-12 tras violarlo en el banco). En el módulo, `B−` y el `GND` del header **son la misma pista** — medido, dan continuidad en los dos módulos. La referencia común ya la da la estrella: el riel lógico llega a la soldadura por el Buck (no aislado, `IN−`↔`OUT−` internos) y el driver por su `B−`. Cablear los dos agrega un segundo camino en paralelo = **lazo de masa**. Detalle en `docs/conexiones-registro-pruebas.md` 8.4 y 7.26.
- Sin código de colores en los cables — identificación confiable es la fila del protoboard.

## Seguridad: pull-downs en las entradas del driver (6 instaladas, 2026-09-12)

> ✅ **Estado vigente (2026-09-12)**: **6 pull-downs de 10kΩ** sobre el protoboard
> rearmado — 4 en `RPWM`/`LPWM` (J8, J6, A7, A5) y **2 en las líneas `EN`** (J10 para
> M1, A9 para M2), que son las que realmente cortan el puente en el BTS7960. Filas y
> detalle en `docs/conexiones-registro-pruebas.md` 3.1b y sección 4.
>
> ⚠️ Quedaron en el **protoboard**, no soldadas en el header de cada IBT-2 como se
> había decidido — se desestimó por dificultad práctica. Consecuencia aceptada: el
> tramo de cable protoboard→pin del driver no está cubierto.
>
> El texto de abajo es el fundamento original (escrito para el L298N) y sigue valiendo
> conceptualmente: lo que cambió con el BTS7960 es **cuál** pin hay que aterrizar
> (`EN`, no `RPWM`/`LPWM` — ver sección 4 del registro).

**Contexto**: con la arquitectura anterior, el Nano actuaba como capa de seguridad
independiente — si el ESP se colgaba, el Nano lo detectaba por timeout y frenaba los
motores por su cuenta. **Con un solo chip esa capa se pierde**: si el ESP32 se
cuelga o se resetea, no hay nadie que apague nada.

**Mitigación**: resistencias de **pull-down de 10kΩ** desde IN1, IN2, IN3, IN4 hacia
GND. Cuando el ESP32 se resetea o cuelga, sus GPIO quedan en alta impedancia; los
pull-down garantizan que las entradas del L298N caigan a nivel bajo **por hardware**,
deteniendo los motores sin depender de que ningún software lo ordene.

Resuelve tres cosas de una vez:
1. Motores detenidos garantizados ante cuelgue o reset del ESP32.
2. El problema ya documentado de entradas flotantes del L298N cuando el micro no las maneja.
3. Cualquier glitch de los GPIO durante el arranque del ESP32.

✅ **Instaladas (2026-08-05)** — detalle de filas en `docs/conexiones-registro-pruebas.md` sección 3.1 y 4.

⚠️ **Ubicación real distinta a la recomendación original**: quedaron en el
protoboard del ESP32 (filas 21, 22, 28, 29), no en el header del L298N. Protegen
"desde el protoboard del ESP32 hacia adelante" — si el cable entre esa fila y el
pin IN del L298N se afloja, ese tramo puntual no está cubierto. Sigue siendo mucho
mejor que no tener pull-down; moverlas al header del L298N cerraría ese último
tramo si alguna vez hace falta.

## Alimentación lógica del L298N (cerrado — bug encontrado y corregido)

> ⛔ **PRE — hardware ya retirado.** ✅ **La lección sigue siendo crítica y se trasladó al
> BTS7960**: el pin `VCC` de cada IBT-2 cumple el mismo rol que el "5V" del L298N — sin él
> la lógica del driver no conmuta nada aunque el firmware reporte todo bien.

**Síntoma original**: el micro ejecutaba y reportaba todas las fases correctamente, pero ningún motor se movía.

**Causa raíz**: con el jumper de 5V removido permanentemente, el pin "5V" del L298N pasa a ser una **entrada** que necesita alimentación externa para energizar la lógica interna del chip (la que interpreta IN1–IN4/ENA/ENB). No había ningún cable hacia ese pin — medido: **0V**. El chip nunca tenía energía para su lógica, por eso las señales no producían movimiento aunque VIN+ sí tuviera los 12.6V.

**Fix**: cable desde el riel de 5V lógico (salida del Buck) hasta el pin "5V" del L298N. No requiere GND adicional — ya es común vía nodo estrella. Consumo de pocos mA.

**Verificado**: con el cable puesto, los motores responden.

## GND común ESP32↔L298N (cerrado — segundo bug del mismo tipo, encontrado y corregido)

> ⛔ **PRE — hardware ya retirado**, pero ✅ **la lección es de las más importantes del
> proyecto** y aplica igual hoy: que el Serial por USB funcione no prueba nada sobre el
> cableado lógico, porque el USB trae su propia referencia de GND.

**Síntoma** (2026-08-05, primera corrida de `test2_motores_esp32/`): igual que el
bug anterior — el firmware reportaba las 3 fases por Serial USB correctamente,
pero ningún motor se movía. Los pines "5V" y VIN+ del L298N medían bien (4.98V y
12.38V respectivamente), así que no era el mismo bug de alimentación lógica.

**Causa raíz**: el pin `5V` (VIN) del ESP32 estaba conectado al riel lógico del
Buck, pero **ningún pin `GND` del ESP32 estaba unido al riel GND lógico común**.
El monitor Serie por USB seguía funcionando con normalidad porque usa el GND del
propio cable USB como referencia — eso ocultó el problema, dando la falsa
impresión de que "el micro está bien, el problema es otra cosa". Pero las señales
de IN1–IN4/ENA/ENB sí necesitan compartir referencia de GND con el L298N para que
un "alto" de 3.3V del ESP32 signifique algo del lado del driver; sin esa
referencia común, el L298N no interpretaba las señales como válidas.

**Fix**: cable directo desde un pin `GND` del ESP32 al riel GND lógico común
(el mismo que ya comparten el MPU6050 y los encoders, unido en el nodo estrella
al GND de potencia del L298N).

**Verificado**: con el cable puesto, ambos motores responden a las señales del
ESP32 a 3.3V — confirma que el riesgo de nivel lógico 3.3V vs. 5V (ver "L298N"
arriba) **no era el problema real** en este caso.

⚠️ **Lección que aplica a cualquier módulo nuevo que se agregue al lazo**: que el
Serial/USB funcione **no es evidencia** de que el resto del cableado lógico esté
bien — el USB trae su propia referencia de GND independiente del circuito de
potencia. Antes de descartar hardware por "el micro está bien", confirmar
continuidad de GND entre el micro y cualquier periférico que no comparta la
alimentación por el mismo cable.

## Checklist antes de cualquier prueba que mueva motores (2026-08-14)

**Tres veces** en este proyecto el mismo síntoma —"el firmware reporta todas las
fases bien y ningún motor se mueve"— costó horas de diagnóstico: (1) pin lógico "5V"
del L298N sin alimentar, (2) sin GND común ESP32↔L298N, (3) switch de V+ en OFF.

Las tres agravadas por lo mismo: **el USB alimenta el micro y da la falsa sensación
de que el sistema está energizado.** El Serial funcionando no dice nada sobre la
etapa de potencia.

Cuesta 30 segundos y las habría evitado todas:

1. **Switch de V+ en ON.**
2. **La entrada de potencia del driver mide ~12V** contra GND de potencia — cubre
   switch, fusible, batería y nodo estrella de una sola medición.
   ⚠️ **Con los BTS7960 son DOS entradas (`B+` de cada módulo), no una** — medir
   las dos, no alcanza con una.
3. **El pin lógico del driver mide ~5V** (era "5V" en el L298N, es `VCC` en cada
   IBT-2 — también son dos ahora).
4. **Continuidad entre un GND del ESP32 y el GND lógico común.**
5. **Las pull-downs de seguridad están en los pines correctos** — con el BTS7960 son
   las líneas `EN`, no `RPWM`/`LPWM` (ver pendientes).

Si los cuatro pasan y los motores igual no giran, recién ahí el problema es de
firmware, del driver o del motor.

## ⛔ Buck destruido por polaridad invertida (incidente 2026-09-10) — corregido

**Síntoma**: primera energización del sistema tras cerrar V1/V2 del plan de
cableado (ver `docs/plan-cableado-senales.html`) — solo alimentación al nodo
estrella, **sin** riel de 5V/3.3V hacia ESP32/MPU/encoders todavía. Salió humo y
un olor característico de la zona de la bornera `IN` del Buck. Fusible de 10A
**no saltó**. Sin calor ni humo persistente al revisar después.

**Causa raíz**: el cable que llega a la bornera `IN−` del Buck tenía en realidad
el `+` de la batería, y el que llega a `IN+` tenía el `−`/tierra — polaridad
invertida en la entrada del Buck, hasta 12.6V al revés de lo que el módulo
espera.

⚠️ **El fusible sin saltar no es evidencia de que no pasó corriente peligrosa.**
El camino de falla (un componente semiconductor fallando en corto) puede limitar
o interrumpir la corriente antes de que el fusible reaccione — no usar "no saltó
el fusible" como criterio de que todo está bien.

**Diagnóstico, en orden**:
1. Inspección visual de `C1` (bornera `IN`) y del capacitor de salida (bornera
   `OUT`, 1000µF/50V): **sin hinchazón visible en ninguno de los dos.** ⚠️ No
   alcanza como garantía — hubo humo real, y un capacitor puede degradarse
   internamente sin hincharse de forma visible.
2. Resistencia `B+`↔`B−` en cada IBT-2 (con todo desconectado): **500kΩ y
   1MΩ** — del orden de la fuga normal del capacitor de bulk de 330µF de cada
   módulo (fábrica), no un corto. **Los dos drivers quedan descartados de daño**
   — consistente con que la inversión fue específica del tramo del Buck, no del
   nodo estrella ni del cableado hacia los drivers.
3. Continuidad `IN−`↔`OUT+` en el Buck viejo, ya desconectado: **da continuidad**.
   En un Buck no aislado, `IN−` y `OUT−` deben estar unidos internamente (por
   diseño), pero `IN−` a `OUT+` **nunca** debería darla — confirma falla interna
   real, no artefacto de medición. **El módulo queda descartado sin ambigüedad.**
4. ESP32/MPU/encoders: **nunca tuvieron alimentación en el momento del
   incidente** — quedan descartados de cualquier revisión, no pudo llegarles
   nada anormal.

**Fix** (ejecutado 2026-09-12): Buck reemplazado por una unidad nueva, **calibrada en
5.01V** y verificada antes de reintegrarla. `C1` (100µF/25V) y el capacitor de salida
(1000µF/50V) se reemplazaron **por precaución**, aunque no mostraban daño visible —
ambos estuvieron en el camino directo de la tensión invertida, y son las piezas más
baratas de todo el sistema como para apostar a que "parecen estar bien" alcanza.

⚠️ **El fusible de 10A también hubo que reemplazarlo: quedó dañado** (confirmado
2026-09-12). Eso matiza el "no saltó" de arriba — el fusible **sí se vio afectado por
la falla**, pero no interrumpió el circuito a tiempo para salvar al Buck. O sea que la
lección 12 se refuerza en vez de debilitarse: un fusible puede degradarse sin abrir
limpiamente, así que **ni "no saltó" ni "el fusible está puesto" dicen nada sobre si
pasó corriente peligrosa**. Tras cualquier incidente de este tipo, el fusible entra en
la lista de piezas a verificar o cambiar, no se asume sano.

⏳ **Pendiente para la próxima sesión** (nada de esto se hizo todavía):
1. **Verificar con multímetro cuál cable es `+` y cuál `−`** en el tramo que sale
   del nodo estrella hacia el Buck nuevo, **antes** de conectarlo — no confiar en
   el cableado tal como está armado, dado que ya falló una vez.
2. Reemplazar `C1` y el capacitor de salida con los repuestos ya confirmados en
   stock.
3. **Probar el Buck nuevo aislado** (sin drivers ni ESP32 conectados) y confirmar
   ~5V en su salida antes de reintegrar el resto del sistema.
4. Recién ahí, **repetir V3** del plan de cableado (potencia armada: `B+` de cada
   driver ~12V, `VCC` de cada driver ~5V) — quedó interrumpida por este incidente,
   nunca se completó.
5. Re-calibrar y confirmar el nuevo Buck en su salida (no asumir 4.98V del
   módulo viejo).

## Alimentación parásita por I2C (lección, 2026-08-14)

**Un módulo I2C sin VCC puede seguir contestando en el bus.** La corriente entra por
los diodos de protección ESD de SDA/SCL (que están en alto por los pull-ups) y
alimenta el chip a ~1-2.7V con unos pocos µA. Costó varias horas de diagnóstico.

Firma del problema:

| Observación | Explicación |
|---|---|
| LED del módulo apagado | Un LED pide mA; la vía parásita da µA |
| `WHO_AM_I` responde correcto | Leer un registro estático consume µA |
| `mpu.begin()` falla | Hace reset y despierta giro/acelerómetro — consumo real |
| Escaneo I2C con fantasmas en 0x06/0x07 | Direcciones reservadas: nadie puede vivir ahí |

⚠️ **"El sensor contesta por I2C" NO prueba que esté alimentado.** Verificar VCC con
multímetro o mirar el LED del módulo. Detalle completo en
`docs/conexiones-registro-pruebas.md` 7.13, incluidas las hipótesis descartadas.

## Cómo alimentar durante el debug (cerrado 2026-08-14)

✅ **Batería + USB simultáneos**: la batería alimenta (Buck a 5.01V) y el USB es solo
cable de datos para el Serial. Es la configuración correcta.

⛔ **USB solo**: el riel se derrumba (VIN 2.2V, 3V3 ~1V) porque el USB intenta
energizar **hacia atrás** la salida apagada del Buck. Si hace falta, desconectar
antes el jumper del riel de 5V del Buck al VIN del ESP32.

⚠️ Los ~360Ω que se miden entre 5V y GND **no son un corto**: son el camino pasivo
del Buck back-driven. Un corto real da pocos ohms.

## Zona muerta de los motores bajo carga real

> ⛔ **PRE — todos los umbrales de duty de esta sección se midieron con el L298N.** Con los
> BTS7960 (MOSFET, sin la caída de los BJT) la zona muerta en duty **va a ser menor** y hay
> que re-medirla. ✅ El método y el protocolo aire/piso siguen valiendo.

**Medición original (Nano, 2026-07-30)**: robot tethered (cuerda que limita el pitch a ±30°), ruedas sobre alfombra, M1+M2 combinados.

- **Umbral de arranque bajo carga: ~3.8V (duty ≈ 77/255)**. Por debajo, respuesta nula o inconsistente.
- **Sin patinaje** en el barrido → descarta problema de tracción con la alfombra.
- Deja **37% de margen** hasta el cap de seguridad (121/255).

**Medición por motor separado, visual, ruedas en el aire (ESP32, Test 2, 2026-08-05)** — ver `docs/conexiones-registro-pruebas.md` sección 7.8:

- **M1: duty 90–95** (~4.3–4.6V con batería real a 12.3V). **M2: duty 85–95** (~4.1–4.6V).
- Rango, no valor puntual — varió entre ciclos por cogging torque del motor DC/reductora, esperable.

**Medición por encoder, más precisa, ruedas en el aire (ESP32, Test 3, 2026-08-05)** — ver `docs/conexiones-registro-pruebas.md` sección 7.11:

- **M1: duty 60–99, media 85.2** (n=15). **M2: duty 60–116, media 83.9** (n=10, una corrida descartada por anómala — ver 7.11).
- Umbrales más bajos que la medición visual — **consistente, no contradictorio**: el encoder detecta el arranque antes de que sea perceptible a ojo. El método por encoder es el más confiable de los dos.
- ⚠️ **Pendiente repetir en piso/tethered** para completar el protocolo aire/piso y aislar cuánto del umbral es fricción de rodadura real.
- ⚠️ No aplicado en firmware todavía. Dato para la futura compensación de zona muerta en el PID — usar la medición por encoder (7.11), no la visual (7.7/7.8).

## Asimetría M1 / M2 (medida 2026-08-14, Test 6)

> 🔶 **Mixto.** ✅ El **hecho** sobrevive: es asimetría de los motores, no del driver —
> confirmada por dos caminos independientes, M2 entrega ~81% del torque de M1 y arranca
> ~10 puntos de duty más tarde, y el firmware la tiene que compensar. ⛔ Los **valores
> absolutos** (gf, N·m, duty) son del L298N: re-medirlos con el driver nuevo.

**Los dos motores no son equivalentes.** Con lazo cerrado sosteniendo la misma
consigna de velocidad:

| Consigna | M1 (duty) | M2 (duty) | Δ duty |
|---|---|---|---|
| 500 RPM | 504 (134) | 504 (149) | +15 |
| −500 RPM | −504 (129) | −504 (153) | +24 |
| −200 RPM | −191 (84) | −191 (109) | +25 |

**M2 necesita 15-25 puntos más de duty que M1** para la misma velocidad, y su zona
muerta es mayor (M2 no arranca hasta duty ~90; M1 arranca en 60-70, variando entre
corridas por cogging).

⚠️ **En lazo abierto el robot se va de costado.** El firmware de balanceo tiene que
compensarlo explícitamente, o usar un lazo de velocidad por rueda como el del Test 6.
Detalle en `docs/conexiones-registro-pruebas.md` 7.14.

✅ **Confirmada por un camino independiente (2026-08-16, Test 7)**: midiendo torque
estático con balanza —sin lazo de control, sin encoders, sin velocidad de por medio—
M2 vuelve a salir más débil que M1.

| | M1 | M2 |
|---|---|---|
| Fuerza a duty 160 | 72.0 ± 3.5 gf | 58.3 ± 6.8 gf |
| Umbral (duty) | 91.6 ± 1.1 | 101.6 ± 2.5 |
| Pendiente (N·m/duty) | 3.45e-4 | 2.97e-4 |

**M2 entrega el 81% del torque de M1** y arranca 10 puntos de duty más tarde. La
asimetría es **del hardware, no del lazo de velocidad** — queda cerrada como
propiedad de los motores. M2 además es menos repetible (±12% contra ±5%), coherente
con tener más fricción interna.

✅ **La asimetría NO depende del modo de decaimiento** (2026-08-17, Test 9, 7.19):
M2/M1 da **0.68 en coast y 0.71 en brake**, con el banco corregido. El firmware
compensa lo mismo en cualquiera de los dos modos.

⛔ Durante esa sesión se anotó que "M2 da el 60% en brake" y **quedó retractado**: era
artefacto de un montaje que cedía bajo carga alta. Eso además **le quita el apoyo
principal a la conjetura de que la causa sea eléctrica** (`R_m(M2) > R_m(M1)`), que se
sostenía justamente en esa asimetría diferencial.

## ω₀ = 8.0 rad/s — VALOR VIGENTE (chasis rearmado, medido 2026-09-09)

**ω₀ = 8.0 ± 0.1 rad/s** (T₀ = 0.785–0.790 s), sobre el chasis de balsa rearmado,
colgado del eje de los motores. **6 clips independientes**, dispersión 1.2%.
Detalle en [`docs/conexiones-registro-pruebas.md`](docs/conexiones-registro-pruebas.md) 7.22
y en [`algoritmos_evolutivos_pso/video_omega0/2026-09-09_rearmado/`](algoritmos_evolutivos_pso/video_omega0/2026-09-09_rearmado/README.md).

⛔ **Reemplaza a los 7.4 rad/s de la sección siguiente.** Ese valor **no estaba
mal** — era de otra planta: el chasis viejo de contrachapado, de 26 cm. El
rearmado mide **22 cm** (eje de giro al extremo estructural, con cinta) y es más
liviano, así que un ω₀ más alto es lo esperado.

**Dos estimadores con supuestos distintos, 0.7% de diferencia:**

```
extrapolando T contra A^2 a amplitud cero   T0 = 0.7901 s  ->  w0 = 7.95 rad/s
promediando solo los periodos con A < 10    T0 = 0.7844 s  ->  w0 = 8.01 rad/s
```

Chequeos que pasan: `L_eq` = 15.0–16.0 cm contra 22 cm de cuerpo (banda 11–22);
barra uniforme predice 8.18 rad/s desde un solo número, 3% de error; cuerpo
rígido (mitad lejana vs. cercana al eje) con 0.6 ms de diferencia contra 6.4 ms
de 3σ; PCA y ángulo del centroide coincidiendo; submuestreo a 15 fps sin efecto.

**Fricción seca (Coulomb) confirmada de nuevo** (R² lineal 0.968 vs exponencial
0.903) — no corre la frecuencia, no hay corrección que aplicar.

⚠️ **El efecto de amplitud finita es real y hay que corregirlo**: `T(A) =
T₀(1+A²/16)`. Medido en estos clips, T va de 0.798 s a 12° hasta 0.825 s a 43°.
**Nunca promediar períodos de amplitudes distintas.** La medición de 2026-08-15
no aplicó esta corrección.

✅ **Escala angular del video, cerrada (2026-09-10): sobre-lee 23-36%.** Nivel
digital sobre el robot en reposo, mismo ensamble que E/F, sin cámara: **0.6°**.
Amplitud física real de suelta = 13° − 0.6° = **12.4°**, contra 15.29° (E) y
16.87° (F) que midió el video — 1.23× y 1.36×. Consistente con el pivote de la
imagen mal ubicado (no se pudo ajustar desde los datos, ver 7.22). **ω₀ no se
corrige por esto** (el intercepto T₀ en A=0 no depende de la escala del eje de
ángulos) y de hecho el sentido del error (el video sobre-lee) confirma que la
pendiente `dT/dA²` sobre-medida es otro efecto — degradación de la máscara en
ángulos grandes — y no este. `θ_umbral` tampoco depende de esta escala: se
calcula con `K_U` (balanza, protocolo propio) y `ω₀`, ninguno pasa por los
grados de este video.

⛔ **El cronómetro no es una alternativa mejor** (evaluado 2026-09-09). Para
igualar el 1.2% del video habría que medir 10 períodos (7.84 s) con error menor a
89 ms, contra ~100 ms de un start-stop humano. Peor: no puede corregir la amplitud
finita y **no detecta un montaje malo**, que es exactamente lo que invalidó los
2.33 y 4.07 rad/s. Si se usa igual como confirmación: soltar desde ~15°, cronometrar
**20** períodos contando **al pasar por la vertical**, esperar 15.5–16.0 s.

### Consecuencias

- **ω₀² pasa de 54.8 a 63.2 (+15%).** `θ_umbral = K_U·MAX_DUTY/ω₀²` baja un 15% a
  igual torque.
- ✅ **`m` y `m·l` re-medidos (2026-09-10)** — ver la sección "J = 0.0159 kg·m²"
  abajo. `K_U` con el driver nuevo sigue pendiente; `θ_umbral` sigue sin número
  vigente hasta entonces.
- Las ganancias del PSO (`Kp=3500, Ki=2297.02, Kd=484.22`) son de la planta vieja.

## J = 0.0159 kg·m² — VALOR VIGENTE (chasis rearmado, medido 2026-09-10)

**`m·l = 0.1040 kg·m`, `l = 9.04 cm`, `J = 0.0159 kg·m²`.** Método de dos apoyos
(7.17), reusando el mismo bracket/bulón flojo del banco de ω₀, ahora fijo a la
mesa. **Ruedas sacadas en las dos mediciones** (esta y los videos de ω₀) — mismo
sistema físico, condición necesaria para combinarlas en `J`. Detalle en
[`docs/conexiones-registro-pruebas.md`](docs/conexiones-registro-pruebas.md) 7.23.

⛔ **Reemplaza a `m·l=0.1265 kg·m`, `l=12.2cm`, `J=0.0227 kg·m²` de la sección de
abajo** — chasis viejo, no estaban mal, otra planta.

```
m = 1150 g (sin ruedas)
m·l = 0.5072 kg × 0.205 m = 0.1040 kg·m   (5 tomas a D=20.5cm, media 507.2g, ±1.7%)
l = m·l/m = 9.04 cm
J = m·g·l/ω₀² = 0.0159 kg·m²
```

⚠️ **Chequeo que casi descarta la medición**: por equilibrio de momentos, `N·D`
tiene que ser constante sin importar dónde apoye el extremo lejano — no lo era al
variar `D` a propósito (522g/22cm, 877g/17cm, 997g/10cm: `m·l` calculado sale
0.1148, 0.1491, 0.0997 kg·m, hasta 50% de dispersión). Causa más probable: a `D`
chico el robot queda casi vertical y algo más toca la mesa, rompiendo el sistema
de dos apoyos. Se repitió solo cerca de 20-22cm, donde el riesgo es menor.

⚠️ Un punto suelto a 22cm (522g → `m·l=0.1148`) sigue 10% arriba del resultado
adoptado y no se promedió con el resto, para no esconder la discrepancia — no
bloquea (la muestra de 20.5cm tiene 5 tomas al ±1.7%), pero queda sin explicar.

Radio de giro `√(J/m) = 11.8 cm`, contra `L/√3 = 12.7 cm` de una barra uniforme de
22cm — 93% del ideal, algo más concentrado hacia el pivote (motores casi sobre el
eje), mismo signo que el chasis viejo (98.6% del ideal).

⚠️ **No necesariamente es el estado final del robot** — si al medir faltaban
componentes por montar, `l`/`J` describen el ensamble parcial de hoy, no la
configuración final. Re-verificar si el armado cambia de forma apreciable.

**`J` cierra. `θ_umbral` todavía no** — sigue faltando `K_U` con el driver nuevo.
⚠️ Ya no está bloqueado por el cableado: **los BTS7960 quedaron cableados el
2026-09-12** (ver `docs/conexiones-registro-pruebas.md` 3.1b). Lo que falta es
correr la medición con balanza, dos motores en brake a la vez.

## ⛔ ω₀ = 7.4 rad/s (chasis VIEJO, 2026-08-15) — superado por 8.0, pero el método sigue valiendo

> ⛔ **Valor superado por la sección anterior.** Se conserva entera porque el
> chequeo de `L_eq`, las dos mediciones falsas y las lecciones de método son lo
> que evitó repetir el error, y 7.22 las reusa.

**Valor de esta sección: ω₀ = 7.4 rad/s** (T ≈ 0.845 s), medido con el robot colgado del
eje de las ruedas mediante brackets de chapa atornillados a una viga fija, bulón
flojo trabajando como pasador. 5 sueltas, 13 períodos completos. Detalle en
[`docs/conexiones-registro-pruebas.md`](docs/conexiones-registro-pruebas.md) 7.12.

⛔ **Reemplaza a 2.33 rad/s (13/08) y a 4.07 rad/s (10/08). Las dos estaban mal.**
El 2.33 circuló dos días por todo el proyecto y alimentó el modelo de PSO.

**Cuerpo del robot: 26 cm** del eje de los motores a la batería (el punto más
alejado del pivote). De ahí sale la verificación que cierra el caso.

### El chequeo que hay que aplicar a toda medición futura de ω₀

```
L_eq = g·(T/2π)²     tiene que dar del orden del tamaño del cuerpo
```

Si da metros para un robot de 26 cm, no se midió el robot. Cuesta una línea de
aritmética y habría descartado las dos mediciones falsas el mismo día:

| Medición | T | ω₀ | L_eq | CG que exigiría |
|---|---|---|---|---|
| **2026-08-15 (vigente)** | **0.845 s** | **7.4** | **17.7 cm** | **13.6 cm = 52% del cuerpo** ✅ |
| 2026-08-13 (descartada) | 2.699 s | 2.33 | 181 cm | 180.7 cm, o 0.31 cm ⛔ |
| 2026-08-10 (descartada) | 1.544 s | 4.07 | 59 cm | 58.3 cm, o 0.97 cm ⛔ |

Las dos viejas exigen un CG **fuera del robot**, o un robot equilibrado a 3 mm del
eje — que colgaría sin orientación preferida, y no es lo que se ve en el video.

**Predicción independiente**: una barra uniforme de 26 cm pivotada en un extremo da
`L_eq = 2L/3 = 17.3 cm → T = 0.835 s → ω₀ = 7.52 rad/s`. Cae dentro del rango
medido con 2% de error, a partir de un solo número. El robot se comporta casi
exactamente como una barra uniforme de su propia longitud: los motores pesados
sobre el pivote y la batería en la punta se compensan.

⚠️ **La lección, que ya van dos veces**: ambas mediciones falsas tenían dispersión
baja (0.8% y 1.5%) y eso se leyó como señal de calidad. **La repetibilidad no
valida nada — hay que validar contra la física.**

### Consecuencias (ω₀² pasa de 5.43 a 54.8, factor 10)

- **El umbral de K_U se multiplica por 10**: `K_U ≥ 0.030` con duty 160 para
  recuperarse de 5°, no 0.0030. El 0.03 que el notebook asumía a ojo queda justo en
  el límite.
- **La cota del Test 5 (K_U ≈ 0.001) queda 30× corta.** Era una medición inválida y
  probablemente subestima mucho, pero la vara subió un orden de magnitud.
- **El robot es un péndulo corto.** L_eq = 17 cm: se comporta como una regla, no
  como un palo de escoba. Constante de divergencia `1/ω₀ = 0.135 s`; de 1° a 10° en
  ~0.31 s. El lazo de control tiene que ser mucho más rápido de lo que sugería el
  modelo viejo.
- **Las ganancias PID que optimizó el PSO son de otra planta** — corrían con 2.33.

## ⛔ K_U = 0.0123 (medido 2026-08-16) — el robot NO puede balancear con estos motores

> 🔄 **REVERTIDO EL 2026-08-17 — el veredicto de esta sección ya NO rige.** Todo lo de
> acá se midió en **modo coast**, que es lo que el firmware venía haciendo sin que
> nadie lo hubiera decidido. Pasando a **brake** (Test 9, 7.19) el torque se
> multiplica por **2.67** sin tocar el hardware: **K_U = 0.0326, θ recuperable =
> 5.47°, contra un criterio de 5.0°. EL ROBOT PASA.**
>
> ✅ **La medición de esta sección es correcta y quedó validada**: el Test 9 volvió a
> medir el coast con el banco corregido y dio 0.0443 N·m contra los 0.0447 de acá —
> 1% de diferencia. No había un error de medición; había una palanca de firmware sin
> usar.
>
> ⚠️ **Condicionado al ensayo térmico**: el 5.47° se midió a duty 160 en brake, que es
> tope de medición. A duty 127 la proyección da ~4.3° y no pasa. Ver "⚡ Todo el
> proyecto conmuta en modo COAST" abajo.

> 🔄🔄 **REVERTIDO OTRA VEZ EL 2026-08-18 — el "EL ROBOT PASA" de arriba se midió con
> UN motor a la vez.** Con los dos motores conduciendo a la vez —la condición real de
> balanceo— el torque cae **31-36%** (medido directo con la balanza, no calculado) y
> **θ recuperable baja a 3.6°**. Contra el criterio de 5.0°, **no pasa**. Detalle en
> [`docs/conexiones-registro-pruebas.md`](docs/conexiones-registro-pruebas.md) 7.20.
>
> ⚠️ **Esto NO es el "ensayo térmico" pendiente — es otra cosa, y es un dato medido,
> no una proyección.** La caída aparece ya en la primera lectura, sin crecer con el
> tiempo: no es calentamiento acumulado, es un techo de carga simultánea. V_bus casi
> no se mueve bajo esta carga (−2.3%), lo que descarta que sea la batería/cableado
> hundiéndose — apunta a algo interno del L298N cuando sus dos puentes conducen
> juntos. **Sin confirmar cuál es el mecanismo.**
>
> ⚠️⚠️ **El criterio bajó de 5.0° a 3.0° por decisión explícita del usuario
> (2026-08-18), no por una nueva derivación.** Contra 3.0°, el 3.6° medido **sí
> pasa** (~20% de margen). Pero esa decisión está **sin justificar contra la
> perturbación real que el robot necesita tolerar** — bajar el criterio para que
> empate con el torque disponible es razonamiento circular si no se ata a un
> requisito externo (golpe, piso irregular, error de arranque). Tratar 3.0° como
> criterio de trabajo provisorio hasta que se documente esa justificación.

> 🔄🔄🔄 **ACTUALIZADO EL 2026-08-19/20 — nuevo dato, no un tercer volantazo.** El
> Test 10 (perturbación real por colocación manual, pendiente desde 7.20) se corrió:
> **media 1.98°, peor caso 3.06° (n=22)**. Y `K_U` se remidió **con los dos motores
> en brake conduciendo a la vez** (la condición real, no un motor aislado): **K_U =
> 0.0218** rad/s² por duty — consistente con aplicarle la caída de 31-36% de 7.20 al
> K_U combinado de 7.19 (0.0326 × 0.64–0.69 ≈ 0.0209–0.0225).
>
> Con eso: `θ_umbral = K_U·MAX_DUTY/ω₀² ≈ 3.65°` — a 0.3% del 3.66° medido directo
> con balanza en 7.20 ese mismo día (dos caminos independientes, mismo número).
>
> ⚠️⚠️ **Ese 3.65° es de la planta VIEJA y ya no rige (2026-09-09).** Se calculó con
> ω₀ = 7.4 y con el chasis de contrachapado. Con ω₀ = 8.0 el denominador sube 15%,
> pero `K_U` también cambió (el chasis rearmado tiene menos masa y otro `J`), así que
> **no se puede corregir propagando solo ω₀**. Queda sin número vigente hasta re-medir
> `m` y `m·l` por dos apoyos. Tampoco rige el margen del 20% contra el Test 10.
>
> **Contra el peor caso real (3.06°), sobraba ~20% de margen.** De los 9 escenarios
> del PSO (3 ángulos × 3 velocidades), 8 estabilizan; el único que no —3°, +0.15
> rad/s combinados— es el mismo límite físico de torque, no una falla de sintonía.
>
> ⚠️ **Esto no está volcado todavía a `docs/conexiones-registro-pruebas.md`** (la
> última sección ahí es 7.21) — vive por ahora en
> `notebooks/pso_pendulo_invertido.ipynb` y en `informes/borrador_entrega.md`.
> Pendiente transcribirlo si se quiere mantener el mismo nivel de registro que el
> resto del proyecto.
>
> ⚠️ **No cierra sola la decisión de reducción de motor** (ver pendientes abajo): el
> escenario más exigente sigue fallando, y el propio informe de PSO recomienda
> "validar contra el robot real cuando el driver se reemplace por uno con más
> margen de torque" — ni quien escribió ese número lo da como definitivo para
> cargar en el robot todavía.

**Resultado cerrado. Le falta un factor 2.4 de torque** *(en coast — ver el recuadro)*.
Es un resultado de diseño, no de firmware: ningún ajuste de PID ni optimización por
PSO compensa un déficit de torque. Detalle en
[`docs/conexiones-registro-pruebas.md`](docs/conexiones-registro-pruebas.md) 7.17.

| | Medido | Criterio | |
|---|---|---|---|
| **Ángulo recuperable** | **2.07°** | 5.0° | 0.41× |
| **K_U** | **0.0123** | 0.030 | 0.41× |
| **Torque a duty 160** | **0.0447 N·m** | 0.108 N·m | **falta 2.42×** |

### La cadena de medición

```
sin θmax = τ_total / (m·g·l)        K_U = τ_total / (J · u_max)
```

| Cantidad | Valor | Cómo se midió |
|---|---|---|
| τ_total (duty 160) | 0.0447 N·m | Balanza de cocina, Test 7, 8 corridas |
| m | 1038 g | Balanza |
| m·l | 0.1265 kg·m | Dos apoyos: lectura 486.7 g × D 26 cm |
| l | **12.2 cm** | m·l / m — 47% del cuerpo |
| J | 0.0227 kg·m² | m·g·l / ω₀² |
| ω₀ | 7.4 rad/s | Video, 7.12 |

⚠️ **`l` = 12.2 cm es la tercera confirmación independiente de ω₀ = 7.4 rad/s**,
esta vez desde masa y geometría sin tocar el video. El radio de giro que implica
es 8.4 cm, contra 7.5 cm de una barra uniforme — un poco más, exactamente lo que
corresponde a un cuerpo con los motores en un extremo y la batería en el otro.

### Reemplaza al resultado del Test 5b (0.022–0.042)

El banco colgante daba 3.72° neto y 7.02° bruto. Esto da 2.07°: un factor 1.8
contra el neto y 3.4 contra el bruto.

**Se le cree a la medición con balanza**, por dos razones:

1. El banco colgante mide el ángulo **a través del bracket de chapa**, y la
   torsión de esa chapa el IMU la lee como inclinación del cuerpo. Era la
   limitación anotada desde que se armó ese test. La balanza no pasa por el
   bracket: mide fuerza contra un plato apoyado en la mesa.
2. Descartado que sea al revés: en el banco colgante corrían **los dos motores a
   la vez**, o sea el doble de corriente y más caída en el Buck y el L298N. Si
   algo, ahí cada motor entregaba *menos* torque, no más.

### El hallazgo de método: se separó fricción de zona muerta

| Medición | Umbral (duty) | Qué incluye |
|---|---|---|
| Test 3, rueda al aire | ~85 | Zona muerta eléctrica |
| **Test 7, balanza** | **91.6 (M1) / 101.6 (M2)** | Zona muerta eléctrica |
| Test 5b, banco colgante | 105–118 | Eléctrica **+ fricción de reductora** |

Como en el Test 7 nada tiene que moverse, el umbral que aparece es solo el
eléctrico. La diferencia contra el banco colgante (~15–25 puntos de duty) **es**
la fricción de la reductora, ahora medida y no estimada.

### Las palancas para cerrar el déficit

| Palanca | Ganancia | Contra |
|---|---|---|
| **Reducción del motor** | hasta 3× | Menos velocidad de rueda |
| Cap de PWM (160→más) | ~1.5× | Excede los 6V del motor; térmico en stall |
| Bajar `m·l` | ~1.3× | Sube ω₀; exige rediseñar el chasis |

**La reducción es la única que alcanza sola.** Con 2.42× haría falta una versión de
~330 RPM en vez de 793. La velocidad de rueda cae de 2.9 a 1.2 m/s — para un robot
balanceador 1.2 m/s sobra de lejos, así que el sacrificio es asumible.

⚠️ **Mover la batería NO es una palanca útil**, aunque lo parezca. Pesa 123 g: el
12% de la masa y el 25% de `m·l`. Llevarla al eje da +29% (θmax 2.07° → 2.66°),
baja el déficit de 2.42× a 1.88× —o sea que **igual hace falta cambiar el motor**,
solo cambia cuál— y de yapa sube ω₀ de 7.4 a 8.2 rad/s. No compensa el desarme más
la sesión de video para re-medir ω₀.

El problema es que la masa está **distribuida**: los motores son lo más pesado pero
están sobre el pivote y no cuentan (6% de `m·l`); el 68% restante son maderas,
varillas roscadas, L298N, Buck y protoboard, repartidos con un brazo promedio de
~17 cm. No hay una sola pieza que mover.

## ⚡ Todo el proyecto conmuta en modo COAST (hallazgo 2026-08-17)

> 🔶 **Mixto.** ⛔ Los números (2.67×, K_U, θ) son del **L298N**. ✅ El concepto aplica igual
> al BTS7960 y es una palanca de torque gratis — pero en ese driver **coast vs. brake no se
> midió todavía** (registro 8.7 es inferencia de datasheet, no balanza). No usarlo como
> supuesto hasta medirlo.

**En todos los sketches el PWM va sobre ENA/ENB con los IN fijos.** Cuando ENA baja,
las dos salidas del puente quedan en **alta impedancia** y la corriente inductiva del
motor vuelve al bus por los diodos: las pestañas quedan a tensión **negativa**
durante todo el tramo OFF. Eso es *fast decay* (coast).

```
COAST (lo que hay hoy)    EN = PWM,  IN_a = 1,    IN_b = 0
BRAKE (sin probar)        EN = 1,    IN_a = PWM,  IN_b = 0
```

En BRAKE, durante el OFF quedan `IN_a = IN_b = 0` con `EN = 1`: ambas salidas
forzadas a bajo, **el motor cortocircuitado**. `V_off ≈ 0` y la corriente media —y
por lo tanto el torque— sube al mismo duty. **Es un cambio de firmware, cuesta cero.**

✅ **MEDIDO (2026-08-17, Test 9 — ver `docs/conexiones-registro-pruebas.md` 7.19).**
Balanza, protocolo del Test 7, montaje firme, **r = 33 mm medido con calibre**:

| duty 160 | COAST | BRAKE | Ganancia |
|---|---|---|---|
| M1 | 81.67 gf (n=6) | **213.4 gf** (n=5, ±2%) | 2.61× |
| M2 | 55.3 gf (n=7) | **152.2 gf** (n=5, ±4%) | 2.75× |

| | COAST | **BRAKE** | Criterio |
|---|---|---|---|
| τ total | 0.0443 N·m | **0.1184 N·m** | 0.1082 N·m |
| θ recuperable | 2.05° | **5.47°** | 5.0° |
| K_U | 0.0121 | **0.0326** | 0.030 |
| | ⛔ | ✅ **pasa, margen 9.4%** | |

**Ganancia: 2.67×.** Tres consistencias internas la sostienen: el coast de hoy
reproduce el Test 7 al 1%, las dos ganancias brake/coast coinciden (2.61 y 2.75 —
tienen que hacerlo, el modo de decaimiento es del puente y no del motor), y la
asimetría M2/M1 da igual en ambos modos (0.68 y 0.71).

⏳ **Condicionado al ensayo térmico de ciclo continuo.** El margen del 9.4% vive
entero a duty 160; **a duty 127 la proyección da ~4.3° y no pasa.** Es el próximo
paso del proyecto.

⛔ **Dos hallazgos intermedios de esta misma sesión quedaron RETRACTADOS** — ambos
eran artefactos de un banco que cedía bajo carga alta: (1) "la asimetría empeora en
brake, 60%" — es 0.71, igual que en coast; (2) "la pendiente de brake no es
consistente" — con el montaje firme es regular. Detalle en 7.19.

⚠️ **`V_off` lo fija el modo de decaimiento, NO la tecnología del transistor.** Un
puente de MOSFETs cableado igual tendría el mismo `V_off` negativo; cambiar de driver
reduce la caída en conducción, no este término. Por eso esto se prueba **antes** de
comprar nada.

⚠️ **Si se adopta BRAKE hay que re-derivar el cap de PWM.** El tope de 160 se calibró
en coast; en brake el mismo duty entrega bastante más.

- **`CAP_BRAKE` del Test 9 se subió de 127 a 160 el 2026-08-17, SOLO PARA MEDIR.** El
  127 salía de `127/255 × 12.1 V ≈ 6.0 V` (nominal del motor) **ignorando la caída
  del driver**, así que era más conservador de lo necesario.
- ⚠️ **Arriba de duty 127 en brake el motor pasa su nominal de 6 V.** Aceptable en
  pulsos de 3.5 s con 12 s de descanso (ciclo ~22%; verificado: el L298N apenas
  entibia). **No** es permiso para operar ahí.
- ⚠️ **El cap operativo del firmware es otra decisión, y más conservadora.** Hay que
  fijarlo con un **ensayo térmico de ciclo continuo** — balanceando, la carga térmica
  es ~4.5× la del protocolo de medición. Subir el tope para medir no autoriza subirlo
  para operar.

✅ Las pull-downs siguen protegiendo en BRAKE: si el ESP32 se cuelga, sus GPIO quedan
en alta impedancia, las pull-downs llevan IN1–IN4 a bajo y el puente pone ambas
salidas a masa. Motor frenado, sin tracción.

## ⚠️ El L298N degrada su salida al calentarse (2026-08-17)

> ⛔ **PRE — específico del L298N**, que ya no está en el robot. ✅ Lo que sobrevive: la regla
> de que toda comparación use el mismo protocolo térmico, y que una medición en stall sobre
> un driver que se calienta mide un blanco móvil. **Verificar si el BTS7960 hace lo mismo**
> antes de dar por bueno cualquier barrido largo.

Midiendo en stall, **la lectura arranca alta y baja durante la ventana**. No es la
fuente: el bus, monitoreado en vivo en VIN+ durante el pulso, sólo cayó de **12.12 V
a 12.07 V**.

- **Ninguna medición eléctrica en stall sobre este driver es régimen permanente** —
  son puntos arbitrarios de una curva que baja. De ahí sale la dispersión de ~0.4 V
  del Test 8b, no de la resolución del instrumento.
- **Es un problema operativo, no sólo de medición**: el robot perdería torque justo
  cuando más lo necesita.
- **Cualquier comparación tiene que usar el mismo protocolo térmico.** El Test 7 usa
  pulsos de 3.5 s con 12 s de descanso; los Tests 8/8b usaron 8 s seguidos y **sus
  números no son comparables con los del Test 7**.

## Lecciones de método (acumuladas)

Van tres veces que una medición con buena pinta resulta ser de la cantidad equivocada
(ω₀ dos veces, K_U dos veces, la caída del L298N). El patrón se repite, así que las
reglas van juntas acá:

1. **La repetibilidad no valida nada** — hay que validar contra la física. Las dos
   mediciones falsas de ω₀ tenían dispersión de 0.8% y 1.5%.
2. **Un modelo que ajusta tampoco valida nada — mirar los residuos.** Y si el ajuste
   cambia mucho según qué puntos entren, los datos no determinan los parámetros
   (Test 8b: `V_off` pasa de −0.60 a −3.08 V según se usen 9 puntos o 3).
3. **Randomizar el orden del barrido.** Los 4 barridos del Test 8b usaron el mismo
   orden de duty, así que el calentamiento acumulado reproduce el mismo patrón de
   residuos y se confunde con señal.
4. **Verificar el cero del instrumento antes de construir una hipótesis encima.** Un
   multímetro puede tener ~1Ω de offset, y el modo continuidad ("beep") dispara con
   cualquier cosa por debajo de ~20-50Ω: no distingue nada a esa escala. Dos cables
   independientes que dan idéntico valor son sospecha de piso del instrumento, no de
   defecto coincidente.
5. **Ante la duda, medir directo en vez de inferir.** Lo que cerró ω₀ y K_U fue
   abandonar la cadena inferencial (video, banco colgante, multímetro) y medir la
   cantidad de interés con una balanza.
6. **Buscar consistencias que la física obliga, y usarlas como test.** Es lo que
   destapó el banco flojo del Test 9: la ganancia brake/coast **tiene** que ser igual
   en los dos motores (el modo de decaimiento es del puente, no del motor), y la
   asimetría M2/M1 **tiene** que ser igual en los dos modos. Cuando no daban, había
   un problema de montaje. Vale más que acumular corridas.
7. **Un banco puede estar bien calibrado en un régimen y mal en otro.** El del Test 9
   reproducía el Test 7 en coast (80 gf) y estaba 15% corrido en brake (210 gf),
   porque el amarre cedía sólo bajo carga alta. **Validar en el régimen en el que se
   va a medir**, no en uno más suave.
8. **Anotar los hallazgos intermedios como provisorios.** En una sola sesión del Test
   9 se registraron tres "hallazgos" que después se retractaron, los tres nacidos de
   regularidades aparentes en datos tomados con un banco que cedía.
9. **Chequear si el estimador arrastra un término conocido antes de promediar.**
   El período de un péndulo depende de la amplitud: `T(A) = T₀(1+A²/16)`. Promediar
   períodos de amplitudes distintas mezcla un sesgo del 3-5% con el ruido y lo hace
   invisible. Medido en 7.22: T va de 0.798 s a 12° hasta 0.825 s a 43°. La medición
   de 2026-08-15 no lo corrigió. **Cuando la teoría predice una dependencia, hay que
   graficarla y extrapolar, no promediar.**
10. **Distinguir qué parte del resultado depende de cada supuesto.** En 7.22 la escala
   angular del video quedó sin calibrar, pero eso **no toca a ω₀** (los cruces por el
   equilibrio no se mueven si el eje de ángulos está escalado, y `T₀` es el intercepto
   en A=0) y **sí toca a `θ_umbral`**. Sin esa distinción, un problema real de una
   cantidad se lee como si invalidara toda la sesión.
11. **Verificar polaridad con el multímetro antes de conectar cualquier módulo nuevo
   a la batería, siempre — sin importar cuántas veces se haya hecho bien antes.**
   Costó un Buck y dos capacitores (2026-09-10, ver "Buck destruido por polaridad
   invertida"). El proyecto ya tenía la regla de no usar código de colores en los
   cables porque no es confiable — esta fue la primera vez que ese riesgo se
   materializó del lado de potencia en vez del de señal.
12. **"No saltó el fusible" no es evidencia de que no pasó corriente peligrosa.**
   Un componente puede fallar en corto y limitar o cortar la corriente antes de que
   el fusible llegue a reaccionar. Tampoco "no está hinchado" es evidencia de que un
   capacitor esté sano si hubo humo real — la inspección visual no reemplaza medir
   continuidad/resistencia en los nodos que importan.
13. **Una medición que cierra un punto abierto no está cerrada hasta que su DECISIÓN
   está escrita en todas las fuentes que mencionan ese punto.** El resultado de V1
   (`B−` ↔ `GND` del header dan continuidad → no cablear el `GND` del header al riel
   lógico) quedó solo en `docs/plan-cableado-senales.html`; el registro, este archivo y
   `plano-potencia.html` seguían diciendo "a medir / condicionado". Al recablear se
   consultaron esas tres y se resolvió la ambigüedad haciendo justo lo que V1 había
   descartado — **lazo de masa armado** (2026-09-12, ver 7.26). Un resultado en un solo
   archivo es **peor** que no tenerlo: da la sensación de que el punto está resuelto
   mientras quien cablea lee "pendiente" y decide solo. Al cerrar una medición, hacer
   `grep` del punto en todo `docs/` y actualizar cada aparición.

## Medición de τ(u) con balanza — método (Test 7, 2026-08-16)

> ✅ **MÉTODO VIGENTE — es el que hay que usar para medir `K_U` con el driver nuevo.** Los
> números de ejemplo son PRE (L298N), pero el procedimiento, las cuatro advertencias de
> montaje y las dos correcciones del Test 9 (`r` es del montaje; el amarre tiene que ser
> firme y el coast no detecta si no lo es) aplican tal cual.

Mide el torque **directamente como fuerza**, sin pasar por la fricción de la
reductora ni por la flexión del bracket. **No hay umbral de despegue**: con el
chasis amarrado y la rueda retenida por el hilo nada tiene que moverse, así que se
mide en todo el rango de duty.

Sketch: [`test7_torque_balanza_esp32/`](test/test7_torque_balanza_esp32/test7_torque_balanza_esp32.ino)

### Montaje

```
        chasis amarrado a la mesa
   ┌──────────────────────┐
   │                      │   ( O )  ← rueda, sobresale del borde
   ├──────────────────────┤     │       de la mesa
   │////  mesa  //////////│     │  hilo tangente, vertical
   │                            │
   │                          ┌─┴─┐
   │                          │ W │  ← pesa (~860 g)
   │                        ┌─┴───┴─┐
   │                        │balanza│
```

```
Motor apagado   → lectura = W
Motor encendido → lectura = W − F
F = W − lectura        τ = F · r        (r = 35 mm, borde de la rueda)
```

- ⚠️ **La pesa no es opcional, y no es por el rango de la balanza.** Un hilo solo
  puede tirar hacia la rueda, o sea hacia arriba. La pesa le da al motor algo que
  levantar: al enrollar el hilo la aliviana y la lectura baja.
- ⚠️ **La pesa tiene que ser más pesada que la fuerza máxima**, o el motor la
  levanta del todo y sigue enrollando. Con ~130 gf de fuerza máxima, 860 g sobra.
- ⚠️ **El hilo sale por el costado y baja vertical.** Ahí el brazo es exactamente
  `r`; en diagonal, `τ = F·r` queda sobrestimado.
- ⚠️ **La línea de base se toma antes de cada pulso**, no una sola vez: deriva
  ~10 g por corrida y ese corrimiento se le sumaría entero a F.
- Un motor por vez, pulsos de 3.5 s (stall) con 12 s de descanso.

### ⚠️ Dos correcciones al método (2026-08-17, Test 9 — ver 7.19)

**1. `r` es del MONTAJE, no del robot.** El Test 7 asumía 35 mm; medido con calibre
con el hilo ajustado dio **33 mm**. Son 6% de error directo sobre τ. El sketch del
Test 9 lo pide por Serial (`r`). **Medirlo con calibre en cada remontaje.**

**2. El amarre tiene que ser FIRME, y el coast no detecta si no lo es.** Tres
versiones del mismo banco dieron tres escalas distintas. El diagnóstico que lo
localizó: entre la penúltima y la última versión, **el coast no se movió nada (81.67
gf las dos veces) y el brake subió 15%** (185 → 213.4 gf). A 80 gf la firmeza no
importa; a 210 gf sí — **el hilo cedía sólo bajo carga alta**.

⚠️ **Un banco puede estar validado contra el Test 7 en coast y aun así estar 15%
corrido en brake.** Cualquier comparación entre modos tiene que hacerse **sin tocar
el montaje entre medio**.

⚠️ **Síntoma de amarre flojo**: dispersión alta y no-monotonicidad con el duty. M2
quedó con ±37% en coast contra ±2% de M1 — ese lado sigue cediendo, revisar antes de
volver a medirlo.

### Medición de `m·l` por dos apoyos

Robot horizontal, apoyo fijo **exactamente bajo el eje de las ruedas**, el otro
extremo sobre la balanza, separados `D`. Tomando momentos sobre el eje:

```
m·l = lectura × D          l = m·l / m
```

⚠️ **El apoyo del eje tiene que ser un PIVOTE LIBRE, no un empotramiento.** Si el
bulón del bracket queda apretado, el apoyo transmite momento y el sistema queda
estáticamente indeterminado: la balanza lee de menos y `θmax` sale falsamente
grande. Verificación: levantar el extremo libre y soltarlo — tiene que pivotar y
volver a apoyar solo.

⚠️ **`D` va hasta el punto de contacto sobre el plato**, no hasta la punta del
robot. Y ambos contactos tienen que ser líneas, no superficies, o `D` queda
indefinido.

## Estado actual (verificado físicamente)

- Continuidad global sin batería: ~2.5MΩ (sistema completo), ~1MΩ (sin Buck) — sin evidencia de corto, consistente con fuga normal de electrolíticos en paralelo. ⚠️ Medido antes del rearmado del chasis — no repetido desde entonces.
- ✅ **Buck reemplazado y calibrado en 5.01V (2026-09-12)** tras su destrucción por polaridad invertida el 2026-09-10. Se cambiaron también `C1`, el capacitor de salida y **el fusible de 10A**, que quedó dañado. El "4.98V" histórico era de la unidad vieja — ya no es referencia. Ver "Buck destruido por polaridad invertida" más arriba; de ese bloque solo queda pendiente repetir V3.
- ✅ **Capacitores — bulk reconstruido tras el rearmado del chasis (2026-09-03), cerrado el resto (2026-09-12).** El armado físico se rehizo desde cero y se habían perdido todos los electrolíticos (C1, C4, C4b, C5); solo sobrevivieron C2/C3, soldados a los motores y no al protoboard. C1, C4, C5, C9, C10, C11, C6 y C<sub>en</sub> ya están todos reinstalados — el protoboard lógico se rearmó de nuevo en esta sesión (fila 3 = MPU/encoders, fila 12 = 3V3+C6, fila 13 = EN+C_en, fila 1 = GND ESP32, todos al riel "−"). Detalle, stock real disponible y lo que falta en `docs/plano-conexiones-esp32.html` §4 y `docs/conexiones-registro-pruebas.md` §6/7.25 — no repetir la lista siguiente sin consultar esas fuentes, que se actualizan más seguido:
  - C1: 100µF/25V — bornera de entrada del Buck (ver más arriba), como conductor separado junto al cable combinado que viene de la soldadura. ⛔ **Instalado 2026-09-03, dañado en el incidente de polaridad invertida (2026-09-10) — reemplazo con repuesto en stock, pendiente para la próxima sesión.**
  - C2/C3: 100nF (motores) — ✅ sobrevivieron el rearmado.
  - C4: 1000µF/16V, protoboard ESP32 fila 30-A (junto al cable de `VIN` en 30-B). ✅ **Instalado (2026-09-03).** ⏳ Pendiente verificar con multímetro continuidad 30-A↔30-C y GND del protoboard↔GND lógico. No expuesto al incidente de polaridad (está del lado lógico, sin alimentación en ese momento).
  - C5: 1000µF/50V, salida del Buck — ⛔ **Reinstalado 2026-09-03, sospechado tras el incidente de polaridad invertida (2026-09-10)** — sin hinchazón visible pero en el camino directo de la falla; reemplazo por precaución con repuesto en stock, pendiente.
  - C6, C<sub>en</sub> *(nuevos respecto del inventario original)*: 100nF cerámico (marcado 104 en el stock del usuario) en 3V3 y pin `EN` respectivamente. ✅ **Instalados (2026-09-12)** sobre el protoboard rearmado — fila 12 (3V3) y fila 13 (EN) al riel "−". Verificados con multímetro sin corto (135kΩ y abierto respectivamente, ver `docs/conexiones-registro-pruebas.md` 7.25). Pendiente la prueba de energización real del sistema completo.
  - C9 *(nuevo)*: 100nF cerámico, fila 30-A del protoboard — soldado junto con C4 (mismo empalme). ✅ **Instalado (2026-09-06).**
  - C10/C11 *(nuevos)*: 100nF cerámico ×2, soldados directo en el header de señal de cada IBT-2 (`VCC`↔`GND`), no en el protoboard del ESP32. ✅ **Instalados (2026-09-06)**, uno por driver.

## Convención de estructura física — 4 niveles de balsa (VIGENTE, rearmado 2026-09-03)

**El robot consta de 4 niveles**, no 3 ni 5. Balsa, 76×150×10mm cada uno, unidos por
varillas roscadas. De abajo hacia arriba — lo pesado y con reacción de torque abajo, lo
liviano arriba, porque cada centímetro que sube un componente le cuesta margen de
recuperación (θmax = τ/(m·g·l)):

| Nivel | Contenido |
|---|---|
| **N1** (el más bajo, sobre el eje) | Motores (cara inferior) + MPU6050 sobre su protoboard de 35×46mm (cara superior) |
| **N2** | Batería 3S + Buck XL4016 + switch, los tres girados 90° (lado largo transversal). Fusible en la cara inferior |
| **N3** | 2× BTS7960 (IBT-2), montados directo arriba, escalonados, disipadores hacia arriba |
| **N4** (el más alto) | Protoboard del ESP32 (84×56mm) girada, USB-C hacia el borde libre |

Distribución dimensional detallada de cada nivel en "Drivers a nivel propio" más abajo
y en [`docs/plano-distribucion-niveles.html`](docs/plano-distribucion-niveles.html).

⛔ **No hay N5.** Figuró como "carga, opcional, sin definir" en el plano durante el
diseño, pero nunca se cortó ni forma parte del robot. Hay material de sobra en los
listones si alguna vez se decide sumarlo.

⛔ **Estructura vieja, ya inexistente** (3 niveles de contrachapado, era del L298N): batería/switch/fusible
arriba, Buck+L298N+motores al medio, protoboard lógico abajo, Arduino Nano en un flanco.
Se conserva solo como referencia histórica — **toda la caracterización de torque
vigente (`K_U`, `θ_umbral`, ganancias del PSO) se midió sobre ella**, que es el motivo
por el que esos números son de "la planta vieja".

## Rediseño del chasis a balsa (decidido 2026-08-24) — ✅ ARMADO (2026-09-03)

> ✅ **Ya no está "en curso": el chasis de balsa está cortado y armado.** Sobre él se
> midieron `ω₀ = 8.0 rad/s` (2026-09-09) y `m·l`/`J` (2026-09-10) — ver esas secciones.
> Lo que sigue es el registro de cómo se llegó al diseño, con sus proyecciones de masa
> (hechas *antes* de armarlo, así que donde haya diferencia gana la medición real).

**Motivo**: quedan 8 semanas para empezar a probar algoritmos de aprendizaje
por refuerzo, y el firmware (Kalman+PID+RL) todavía no arrancó — es el
verdadero cuello de botella del proyecto, no el chasis. Se evaluó pasar a
impresión 3D (Fusion 360 + impresión rentada) y **se decidió posponerla**:
quedan decisiones mecánicas abiertas (driver de reemplazo, ensayo térmico,
R_m de los motores) que todavía moverían el diseño "final", y una impresión
rentada agrega un turnaround externo justo cuando más conviene poder iterar
en el día. Se sigue en madera — esta vez **balsa** (no la madera de
aeromodelismo original: esa era contrachapado, ~0.62 g/cm³ medido en
24g/lámina) — sobre 2 listones ya comprados de **910×76×10mm**.

⛔ **Nota de época, ya superada**: cuando se escribió esto, "elegir driver de
reemplazo" seguía abierto entre BTS7960 / DRV8871 / TB6612FNG. **Se cerró el
2026-08-29: 2× BTS7960.** El Nivel 1 se dimensionó
alrededor de 2× BTS7960 (50×50mm, disipador 26mm, medidos) porque son los
módulos que el usuario ya tiene en mano — si el driver elegido termina siendo
otro, ese nivel hay que rehacerlo.

⛔ **La tabla de niveles que sigue está SUPERADA — ver "Drivers a nivel
propio" más abajo para la distribución vigente de 4 niveles.** Se conserva
como registro de cómo se llegó hasta acá.

**Distribución de niveles** (de abajo hacia arriba — invierte el orden de la
estructura vigente: acá lo pesado y con reacción de torque va abajo, lo
liviano y sin momento va arriba, porque cada centímetro que sube un
componente le cuesta margen de recuperación al robot, criterio
θmax = τ/(m·g·l)):

| Nivel | Contenido | Ancho×fondo | Nota |
|---|---|---|---|
| N1 (más bajo, sobre el eje) | 2× BTS7960 escalonados (uno detrás del otro — lado a lado necesitaban 115mm, no entraban en la tira de 76mm) + MPU6050 | 76×160mm | Disipadores hacia arriba. Espesor 10mm sin adelgazar — carga la reacción de torque de los motores. |
| N2 | Batería 3S + XL4016 + switch + fusible | 76×160mm | Batería y buck girados 90° para entrar en 76mm de ancho. Switch/fusible en pestaña sobre el borde. |
| N3 | Protoboard + ESP32 | 76×160mm | Protoboard girada (56mm transversal, no 84mm) para entrar en la tira. USB-C hacia el borde libre. |
| N4 (el más alto, desmontable) | Plataforma de carga — **vacía, sin componentes propios** | 76×160mm | ✅ Sumada al plano 2026-08-27. Es el punto más caro en m·l del stack (arm ≈15cm) — placa sola cuesta poco (~0.003 kg·m), pero **cualquier carga futura tiene techo**, ver tabla de payload más abajo. |

✅ **Ajustado 2026-08-27**: los tres niveles obligatorios se cortan a la misma
longitud (160mm, la que ya exigía N2) en vez de a medida — simplifica el corte
(una sola marca, misma pieza tres veces) a cambio de ~10g de balsa extra
repartidos entre N1 y N3. Efecto sobre m·l: despreciable, ver tabla siguiente.
Listones reales confirmados en **76×915mm** (no 910mm), sin impacto en el plan
de corte — sobra material de sobra en los dos casos.

Se eliminó del plan original el nivel de ventiladores (coolers 50×50×10,
8000 RPM): el BTS7960 (MOSFET) disipa mucho menos que el L298N (BJT), y
8000 RPM montados rígido al mismo chasis que el MPU6050 se alían dentro de
la banda de un lazo de control a ~200 Hz.

**Plano completo**: [`docs/plano-distribucion-niveles.html`](docs/plano-distribucion-niveles.html).

**Proyección de m·l y θmax** (balsa 0.16 g/cm³ asumida — **sin confirmar**):

| | Estructura vigente | 3 niveles | + N4 vacío |
|---|---|---|---|
| m·l | 0.1265 kg·m | ≈0.057 kg·m | ≈0.060 kg·m |
| θmax (τ=0.079 N·m, brake, 2 motores) | 3.6° | ≈8.2° | **≈7.8°** |
| Margen vs. peor perturbación real (Test 10, 3.06°) | +18% | +168% | **+155%** |

**Techo de carga futura sobre N4** (arm ≈15cm — la placa vacía ya está en la
proyección de arriba, esto es lo que admite *encima* de esa placa):

| Objetivo | Carga máxima admisible |
|---|---|
| θmax ≥ 5° (conservador) | ~200g |
| Margen del 20% — el estándar que ya usa el proyecto | **~400g (recomendado)** |
| θmax = 3.06° — cero margen, línea roja, no acercarse | ~550g |

⚠️ Es una proyección de cálculo, no una medición. ✅ **Ya se midió sobre el chasis
armado** (2026-09-09/10): `ω₀ = 8.0 rad/s`, `m = 1150 g`, `m·l = 0.1040 kg·m`,
`l = 9.04 cm`, `J = 0.0159 kg·m²`. ⚠️ **El `m·l` real salió 63% más alto que el
proyectado acá (0.1040 contra ≈0.060-0.064)**, así que los θmax y los márgenes de
estas tablas son optimistas — usar los valores medidos, no estos. Sigue pendiente
`K_U` con el driver nuevo y re-correr el PSO.

## ⛔ Montaje de los motores en N1 — soportes separados (SUPERADO, ver "Drivers a nivel propio" más abajo)

**Hallazgo**: los motores y los drivers no entran juntos en los 150-160mm de
N1. La brida de montaje de cada motor mide **40×40mm** (medida), con 4
agujeros de **Ø4mm** espaciados **24.5mm** (a través del eje) **× 30.4mm** (a
lo largo del eje), centro a centro — convertido desde 20.5mm/26.4mm punto a
punto informados por el usuario, sumando el diámetro del agujero. El mínimo
para que motor+driver+driver+motor entren en línea, con margen cero entre
piezas, es:

```
40 (motor) + 50 (BTS7960) + 50 (BTS7960) + 40 (motor) = 180mm
```

Ya supera los 150-160mm disponibles antes de sumar ningún margen de cableado.
No es un ajuste fino — es geométricamente imposible en el largo actual.

**Resolución**: N1 se queda como está (150mm, con drivers+MPU) — no se
recorta de nuevo. En cambio, se suman **2 piezas nuevas separadas** ("soporte
motor", **76×65mm** cada una — ver ajuste siguiente —, mismas 2 posiciones de
agujero de varilla que un extremo de N1) atornilladas/pegadas contra cada
extremo corto de N1, con la brida del motor en la mitad exterior de cada una
— el eje del motor queda al ras del borde exterior de estos soportes, no del
borde de N1.

⚠️ **Ajustado 2026-08-28 (segunda vuelta)**: el soporte de motor pasó de 50mm
a **65mm** de profundidad. El motor completo mide **51.6mm** (reductora +
motor + encoder, medido — ver "Zona muerta de los motores" más abajo en el
archivo). Con 50mm, la brida (montada cerca de la punta del eje, peor caso
sin confirmar) dejaba el cuerpo del motor prácticamente pegado al borde que
se une a N1 — justo donde el driver, orientado con el conector hacia afuera
para alejarlo del MPU, necesita sus propias borneras. Las dos cosas
competían por el mismo espacio, en la misma cara. Los 65mm (15mm extra de
colchón sobre el diseño original) resuelven esto **sin tocar N1** — su
distribución interna sigue cerrando exacto en 150mm, ver arriba. El soporte
todavía no está cortado, así que el cambio no cuesta nada.

✅ **Recomendado usar contrachapado del chasis viejo para estas 2 piezas, no
balsa** — están prácticamente sobre el eje de la rueda (brazo ≈0), así que el
material no pesa nada en m·l, y cargan todo el torque de reacción del motor:
es el único punto del rediseño donde más resistencia no cuesta margen.

**Plantilla**: [`docs/obsoleto/plantilla-imprimible-nivel1-completa.html`](docs/obsoleto/plantilla-imprimible-nivel1-completa.html) —
tira continua de las 3 piezas en una hoja, para garantizar que los dos ejes queden colineales al pegarla.
⚠️ La abertura para el eje quedó con tamaño provisorio — no se sabe cuánto
sobresale el eje más allá de la brida, ajustar contra el motor real antes del
corte definitivo.

## ⛔ Drivers abajo, MPU arriba — recortes de disipador en N1 (SUPERADO, ver "Drivers a nivel propio" más abajo)

**Hallazgo**: el MPU6050 va montado sobre su propio protoboard chico (fijado
con cinta doble faz), de **34.5×46mm** — muy por encima de los 8mm de hueco
que había entre los dos BTS7960 en el diseño original. Si drivers y MPU
tuvieran que compartir la cara superior de N1, no entran: sobran solo 18mm
libres contra los 34.5mm que hacen falta.

**Resolución (propuesta por el usuario)**: los BTS7960 pasan a la cara
**inferior** de N1 (más cerca de los motores que manejan — cableado de
potencia más corto, de yapa). El disipador de cada uno —medido en **50×32mm**,
casi dos tercios del módulo completo de 50×50mm— asoma hacia arriba a través
de un **recorte pasante** en N1, quedando expuesto al aire entre N1 y N2
(sigue cumpliendo "disipadores hacia arriba"). El MPU6050 con su protoboard
queda solo, centrado, en la cara superior.

**Por qué funciona**: al estar en caras opuestas, el driver completo (50×50)
y el MPU (34.5×46) ya no compiten por el mismo largo de N1 — solo compite el
**recorte del disipador** (32mm, no 50mm) con la zona del MPU. La cuenta:

```
16 (margen esquina) + 32 (recorte M1) + 9.75 (colchón) + 34.5 (MPU)
  + 9.75 (colchón) + 32 (recorte M2) + 16 (margen esquina) = 150mm exacto
```

⚠️ **Punto sin cerrar**: no se sabe dónde caen los 4 tornillos de montaje del
BTS7960 respecto de su disipador (el paso de ~35mm del inicio del proyecto
sigue sin confirmar con calibre). Si el driver se orienta con el conector
hacia el borde exterior de N1 (hacia el motor), sus tornillos deberían quedar
lejos del MPU — pero **hace falta un dry-fit real** antes de perforar: apoyar
el driver sobre la plantilla impresa, marcar los 4 tornillos, y correr el MPU
dentro del colchón de 9.75mm si hiciera falta.

**Plantillas actualizadas**: [`docs/obsoleto/plantilla-imprimible-nivel1.html`](docs/obsoleto/plantilla-imprimible-nivel1.html)
y [`docs/obsoleto/plantilla-imprimible-nivel1-completa.html`](docs/obsoleto/plantilla-imprimible-nivel1-completa.html).

**Notas específicas de trabajar con balsa** (no aplican al contrachapado
anterior):
- ⛔ Nunca tornillo autorroscante directo en balsa — arranca la fibra con poco
  torque. Insertos roscados termofusionados o tuerca-pasante para los 4
  tornillos de montaje de cada BTS7960.
- ⚠️ Parche de refuerzo (~14×14mm, pegado antes de perforar) en los 4
  agujeros de varilla de cada nivel — balsa se aplasta bajo la arandela con
  poco par de apriete. **16 parches en total** (4 por nivel × 4 niveles, ya con N4 sumado).
- ⚠️ N1 carga la reacción de torque de los motores — si la placa maciza
  resulta demasiado flexible torsionalmente, considerar laminar dos capas
  más finas con la fibra cruzada 90°. Pendiente de verificar una vez armado.
- ⚠️ Durabilidad ante las caídas del entrenamiento de RL, sin resolver:
  depende de si el entrenamiento corre primero en simulación (bajo impacto
  físico) o directo en hardware real con política sin entrenar (caídas
  frecuentes y bruscas). Sin decidir todavía.

## Drivers a nivel propio — arquitectura de 4 niveles (decidido 2026-08-28)

> ⚠️ **Esta sección se titulaba "arquitectura de 5 niveles" y era incorrecto** (corregido
> 2026-09-12). El robot tiene **4 niveles** (N1–N4, los 4 cortados y armados); el "N5"
> era una plataforma de carga opcional que nunca se cortó ni forma parte del robot.

Revierte los dos enfoques anteriores (arriba, marcados SUPERADO). En vez de
seguir parchando conflictos de espacio en N1 uno atrás de otro (motor+driver
no entraban, MPU+driver no entraban en la misma cara, cuerpo del motor
chocaba con las borneras del driver), **los drivers pasan a tener su propio
nivel**, separado de los motores y del MPU. Elimina de un saque los tres
conflictos — motores y drivers ya no comparten nada.

**Consecuencia directa**: los soportes de motor de 76×65mm y los recortes de
disipador en N1 (las dos secciones SUPERADO de arriba) **ya no hacen falta**.
El motor se ancla directo sobre la cara inferior de N1, tal como en el primer
boceto del proyecto — la brida (40×40mm, 4× Ø4mm, 24.5×30.4mm centro a
centro) es una sola pieza en L: el lado largo se atornilla a N1, el corto
abraza el cuerpo del motor. No hay un segundo punto de anclaje.

⛔ **Corregido 2026-08-28 (esta sesión) — la asignación de ejes de la sección
anterior estaba invertida.** Foto del motor real: los 4 tornillos van
**paralelos al eje** (perpendiculares a la cara donde está la brida, no
alrededor de él) — o sea que atornillan esa cara **plana contra la cara
inferior de N1** (tornillo entrando vertical, de arriba hacia abajo). El
motor queda libre de girar alrededor de ese eje de atornillado hasta que se
ajustan los tornillos, y la dirección final del eje (hacia dónde apunta la
rueda) depende exclusivamente de qué lado del rectángulo de agujeros quede
alineado con el largo de N1. Medido en la
pieza real: **20mm en la dirección del eje del motor, 26mm en la
perpendicular** — coincide con el 20.5/26.4mm "punto a punto" ya registrado
arriba, pero la conversión a centro-a-centro se había etiquetado al revés.
**Correcto: 24.5mm a lo largo del eje (hay que
alinearlo con el largo de N1, 150mm) × 30.4mm a través del eje (con el ancho
de N1, 76mm).** Con la asignación vieja, la única forma de hacer coincidir
la brida real con agujeros perforados según esa plantilla era girando el
motor 90° — eje apuntando al frente/atrás del robot en vez de a los
costados. Ya corregido en
[`docs/plantilla-imprimible-niveles.html`](docs/plantilla-imprimible-niveles.html)
(la plantilla vigente).

**Confirmado el mismo día**: dos protoboards, no uno — protoboard del ESP32
**84×56mm** (sin cambios) y protoboard del MPU6050 **35×46mm** (afina el
34.5×46 usado hasta acá). Switch **45.5×28.6×13mm**, fusible **41.1×13×8mm**
— medidos por primera vez, antes se trataban como pestañas genéricas.

✅ **Cerrado 2026-08-29**: los 4 niveles **ya están cortados a 76×150mm cada
uno** (no 160mm — se mantiene la uniformidad de largo, pero al valor real de
corte). El fusible lo ubica el usuario a su criterio en la cara inferior de
N2, no entra en la cuenta de abajo.

**Distribución de niveles**, de abajo hacia arriba, diseñada para entrar en
esos 150mm fijos:

| Nivel | Contenido | Cómo entra en 150mm |
|---|---|---|
| N1 (sobre el eje) | Motores (cara inferior) + MPU6050 sobre protoboard 35×46mm (cara superior) | `40 (brida motor A) + 17.5 (aire) + 35 (MPU) + 17.5 (aire) + 40 (brida motor B) = 150mm` exacto. Brida al ras de cada borde corto. |
| N2 | Batería(72×35) + Buck(64×47) + Switch(45.5×28.6), en línea, **girados 90°** (lado largo transversal) | `10 + 35 (batería) + 10 + 47 (buck) + 10 + 28.6 (switch) + 9.4 = 150mm`. Girar cada componente para que el lado largo quede transversal es lo que lo hace entrar — sin girar, batería+buck+switch solos ya suman 164.6mm. |
| N3 | 2× BTS7960, montados **directo arriba** — ya no comparte cara con nada | `16 + 50 + 18 (aire) + 50 + 16 = 150mm` exacto. Vuelve a ser tan simple como el primer boceto: disipadores hacia arriba, sin recorte. Sigue necesitando escalonado (lado a lado exige 115mm > 76mm de ancho). |
| N4 | Protoboard ESP32 84×56mm girada | `33 + 84 + 33 = 150mm` — sobra para la pestaña del USB-C. |
⛔ **No existe N5.** Durante el diseño figuró una quinta placa de carga "opcional, sin
definir"; **nunca se cortó y no forma parte del robot**. Queda material de sobra en los
listones si alguna vez se decide sumarla — pero hasta entonces, el robot es de 4 niveles.

⚠️ **Sin confirmar**: altura del BTS7960 montado (driver + disipador) sobre
N3 — determina el hueco N3→N4, todavía sin cerrar con un número medido. Usado
35mm como estimación provisoria (mismo orden que batería/buck) en el plano.

**m·l y θmax — proyectado vs. MEDIDO** (⚠️ la proyección quedó 63% corta; detalle y
causa en [`docs/plano-distribucion-niveles.html`](docs/plano-distribucion-niveles.html)):

| | Proyectado (2026-08-29) | **MEDIDO (2026-09-10)** |
|---|---|---|
| m·l | ≈ 0.064 kg·m | **0.1040 kg·m** |
| m | ≈ 1058 g (con ruedas) | **1150 g (sin ruedas)** |
| l | ≈ 6.0 cm implícito | **9.04 cm** |
| θmax (τ=0.079 N·m, brake, 2 motores) | ≈ 7.2° | **≈ 4.44°** |
| Margen vs. peor perturbación real (Test 10, 3.06°) | ≈ +136% | **≈ +45%** |

**Por qué falló**: las masas marcadas como sin confirmar (BTS7960, XL4016, switch,
fusible, protoboard+ESP32+cableado, varillas) estaban subestimadas, y el error se
concentró en los niveles altos, donde cada gramo pesa el doble en `m·l`.

✅ **Sigue siendo mejor que el chasis viejo, pero por 18%, no por 50%**: con el mismo τ,
contrachapado daba 3.65° (`m·l`=0.1265) y balsa da 4.44°. El margen contra la
perturbación real pasa de +19% a +45%.

⚠️ **El 4.44° es un piso**: τ=0.079 N·m se midió con el **L298N**. Con los BTS7960
(MOSFET, sin la caída de los BJT) debería subir — pero `K_U` con el driver nuevo
todavía no se midió. Usar 4.44° como cota conservadora hasta entonces.

⚠️ Baja un poco respecto a la variante anterior (+155%, la que metía los drivers
en N1 junto al MPU) porque darle nivel propio a los drivers suma una placa más con
masa a un brazo medio — sigue siendo un margen amplio, y se pagó a cambio de eliminar
los tres conflictos de espacio que N1 no podía resolver.

⚠️ Los márgenes de +155% / +136% de esta comparación son **los dos proyectados**, así
que la comparación entre variantes sigue valiendo cualitativamente, pero ninguno de los
dos números es real — el medido es **+45%** (ver la tabla de arriba).

✅ **Plantillas actualizadas 2026-08-29**: [`docs/plantilla-imprimible-niveles.html`](docs/plantilla-imprimible-niveles.html)
(+ `.pdf`) cubre los 4 niveles bajo esta arquitectura, en escala real 1:1.
Reemplaza a las tres plantillas viejas de N1, **ya movidas a `docs/obsoleto/`**.

## Decisiones revertidas — arquitectura ESP8266 + Arduino Nano (2026-07-29 a 2026-08-04)

Se conserva el registro porque el aprendizaje sigue siendo válido y evita repetir el
mismo camino.

**Qué era**: ESP8266 como cerebro (Kalman+PID+PSO) y Arduino Nano como ejecutor
"tonto" de motores, comunicados por UART hardware. El ESP8266 no tenía pines
suficientes para manejar I2C + encoders + las 6 líneas del L298N, de ahí el segundo
micro.

**Por qué se abandonó**: el enlace UART Nano→ESP8266 nunca llegó a funcionar de
forma estable, tras ~4 sesiones de diagnóstico. Requería un divisor resistivo
(1kΩ/2kΩ) porque el TX del Nano sale a 5V y el ESP8266 no tolera 5V en sus GPIO.

**Lo que se descartó, con evidencia, durante ese diagnóstico** (útil como referencia
metodológica):
- Topología del divisor: correcta, verificada por resistencia repetidamente.
- Componentes: R1=0.98kΩ, R2+R3=1.98kΩ medidas en aislado — sanas.
- Pin RX del ESP8266: sano (loopback TX↔RX propio: 405/405 bytes).
- GND común entre placas: confirmado.
- Voltaje estático en el nodo: correcto (~2.5–3.3V según fuente).
- Dirección ESP→Nano: **funcionaba al 100%** (LED del Nano parpadeaba con cada PING).
- Dirección Nano→ESP: **0 bytes** en 408s de transmisión continua de `0x55`.

**Diagnóstico final**: contacto físico marginal, no error de diseño. La prueba
concluyente fue que **mover el protoboard del ESP8266 —sin tocar ningún cable—
cambiaba el resultado** (0 → 32 → 653 bytes), y que los bytes que llegaban eran
casi-correctos pero corruptos (`0xD5`, `0xFF` en vez de `0x55`). Sospecha final sin
confirmar: la masa entre los dos protoboards.

**Lecciones que siguen aplicando:**
1. Una medición estática correcta (resistencia, voltaje en reposo) **no garantiza**
   que una señal dinámica pase. El multímetro presiona el contacto y usa corriente
   mínima; una señal que conmuta miles de veces por segundo no perdona un contacto
   marginal.
2. Sacar y reinsertar un módulo entero del protoboard para flashearlo reintroduce
   riesgo de mal contacto en **todos** sus pines cada vez. Preferible desconectar
   solo los cables puntuales necesarios.
3. Instrumentar el firmware para distinguir "no llega nada" de "llega corrupto"
   (el contador de bytes crudos) fue lo que permitió descartar hipótesis en vez de
   adivinar.

**Qué se deprecó con el cambio**: el Nano como ejecutor, el enlace UART entre chips,
el divisor resistivo R1/R2/R3, el Test 4 completo, el LED indicador en D5 del Nano,
las advertencias sobre el conflicto D0/D1 y las filas J21/J22, y la restricción de
"debug solo por WiFi porque TX/RX están reservados".

**Qué sobrevivió sin cambios** (propiedades del hardware, no del micro): calibración
del MPU6050, convención de ejes, cap de PWM 121/255, zona muerta ~77/255, el fix de
alimentación lógica del L298N y toda la topología de alimentación.

## Estado de los tests tras la migración

⚠️ **Todos los sketches existentes fueron escritos para ESP8266 o AVR.** Ninguno
compila tal cual para ESP32. Estado real:

| Test | Sketch actual | Estado tras migración |
|---|---|---|
| 1 — MPU6050 | **[`test1_mpu_esp32/`](test/test1_mpu_esp32/test1_mpu_esp32.ino)** | ✅ **Portado, flasheado y validado en hardware real** (2026-08-05). Reporta por USB **y** WiFi. Muestra magnitud cruda vs. calibrada para validar in situ el bias/escala del Test 1b — confirmado: calibrada da 9.76–9.82 m/s² (vs. 9.81 ideal) en las 3 orientaciones probadas. Convención de ejes (Z=arriba, Y=pitch, X=ruedas) reconfirmada físicamente en este hardware. |
| 1 — MPU6050 (viejo) | `test1_mpu_gy521/` (ESP8266) | Superado por el anterior |
| 1b — Calibración MPU | `test1b_calibracion_mpu/` (ESP8266) | Portar igual que Test 1 |
| 2 — Motores | **[`test2_motores_esp32/`](test/test2_motores_esp32/test2_motores_esp32.ino)** | ✅ **Flasheado y validado en hardware real** (2026-08-05). Ambos motores responden a 3.3V — riesgo de nivel lógico descartado. Encontrado y corregido un segundo bug de GND (ver "GND común ESP32↔L298N"). M1 con sentido de giro invertido — corregir físicamente en las borneras OUT1/OUT2. Zona muerta por motor medida en aire, pendiente repetir en piso. |
| 3 — Encoders | **[`test3_encoders_esp32/`](test/test3_encoders_esp32/test3_encoders_esp32.ino)** | ✅ **Flasheado y validado en hardware real** (2026-08-05). Ambos canales (A y B) por motor, decodificación en cuadratura 1x. PPR confirmado (69.1), sentido de giro validado (M1/M2 opuestos por diseño, montaje en espejo), zona muerta por encoder medida en aire (M1 85.2, M2 83.9 duty medio). Reporte por WiFi (AP `RobotBalance_Test3`). ⏳ pendiente repetir Fase 2 en piso/tethered. |
| 5 — K_U + zona muerta frío/caliente | **[`test5_ku_esp32/`](test/test5_ku_esp32/test5_ku_esp32.ino)** | ⛔ **Corrido 2026-08-14, sin K_U usable.** Ver `docs/conexiones-registro-pruebas.md` 7.15. Tres fallas de método: robot sostenido a mano (la mano absorbe el torque de reacción), ruedas al aire (la reacción existe solo mientras la rueda acelera), y estimador por recta sobre 250 ms (asume aceleración constante ante un transitorio). ✅ Sí sirvió para **descartar la hipótesis del calentamiento** y para acotar **K_U ≈ 0.001, no 0.03**. ⚠️ Con el ω₀ corregido a 7.4 rad/s (7.12), el K_U **necesario** es 0.030 — esa cota de 0.001 queda 30× corta. |
| 5b — K_U estático | **[`test5b_ku_estatico_esp32/`](test/test5b_ku_estatico_esp32/test5b_ku_estatico_esp32.ino)** | ⛔ **SUPERADO por el Test 7.** Corrido 2026-08-15, dio K_U entre 0.022 y 0.042 (ver 7.16); el Test 7 lo cerró en **0.0123** midiendo el torque sin pasar por el bracket, cuya flexión inflaba este resultado ~1.8×. Con el eje **trabado** al bracket, un duty sostenido lleva el cuerpo a un ángulo de equilibrio; medición **estática** con el acelerómetro, sin derivar el giroscopio. Modo **`v`** (20 s) verifica el montaje antes de gastar el barrido — es de donde salieron las dos mediciones confiables. ✅ Aportó el hallazgo de que **el ángulo estacionado mide la fricción directamente**. ⛔ El barrido con ajuste lineal **no sirve** en este sistema: hay un umbral de despegue en duty ≈140 y ajustar una recta a un escalón dio dos veredictos falsos opuestos. Reporte por Serial (no WiFi: el AP se cae con batería y el motor trabaja cerca de stall). |
| 7 — τ(u) con balanza | **[`test7_torque_balanza_esp32/`](test/test7_torque_balanza_esp32/test7_torque_balanza_esp32.ino)** | ✅ **Corrido 2026-08-16, K_U CERRADO.** Mide el torque directamente como fuerza con una balanza de cocina: chasis amarrado, hilo tangente a la rueda tirando de una pesa. **No hay umbral de despegue** porque nada se mueve, así que se mide en todo el rango de duty. Resolvió las dos ambigüedades que el Test 5b no pudo. Barrido guiado por Serial: el operador mira la balanza y tipea la lectura, el sketch hace la cuenta. Línea de base tomada antes de cada pulso. |
| 0 — Scanner I2C | **[`test0_i2c_scanner/`](test/test0_i2c_scanner/test0_i2c_scanner.ino)** | ✅ **Herramienta de diagnóstico** (2026-08-14). I2C crudo, sin librería Adafruit. Barre las 127 direcciones a 100 y 400 kHz, lee `WHO_AM_I` en 0x68/0x69 e informa el nivel de reposo del bus. Distingue "no hay nadie" de "está en otra dirección" de "contesta a una velocidad y no a la otra" — cosas que el error genérico de `mpu.begin()` no separa. |
| 8 — Caída del L298N (multímetro) | [`test8_caida_l298n_esp32/`](test/test8_caida_l298n_esp32/test8_caida_l298n_esp32.ino) + [`test8b_caida_barrido_esp32/`](test/test8b_caida_barrido_esp32/test8b_caida_barrido_esp32.ino) | ⛔ **Corridos 2026-08-17, la vía no sirvió.** Ver `docs/conexiones-registro-pruebas.md` 7.18. Los dos modelos son falsos: el Test 8 asume 0 V en el tramo OFF del PWM, el 8b asume `V_off` constante (R² 0.77–0.83 en 4 barridos, ajuste mal condicionado). Y la lectura **arranca alta y baja**: el L298N degrada su salida al calentarse, así que toda medición eléctrica en stall mide un blanco móvil. ⚠️ El 8b además **imprime física equivocada** sobre la ganancia con MOSFETs — ignorar ese bloque. ✅ Sí aportó el hallazgo del modo coast (ver abajo) y descartó que el bus se hunda (12.12 → 12.07 V). |
| 9 — Torque COAST vs BRAKE | **[`test9_decay_torque_esp32/`](test/test9_decay_torque_esp32/test9_decay_torque_esp32.ino)** | ✅ **Corrido 2026-08-17, extendido 2026-08-18 — revierte el veredicto dos veces.** Ver `docs/conexiones-registro-pruebas.md` 7.19 y 7.20. **17/08**: pasar de coast a brake multiplica el torque por 2.67 (un motor a la vez): θ 2.05°→5.47°, pasa el criterio de 5.0°. **18/08**: con los DOS motores conduciendo a la vez (condición real) el torque cae 31-36% desde la primera lectura (no es térmico) y θ baja a **3.6°** — ya no pasa contra 5.0°. Criterio bajado a 3.0° por decisión del usuario, sin justificar todavía contra la perturbación real. Modos agregados: `e` (sostenido continuo) y `w` (intermitente ON/OFF) para el ensayo térmico. Radio configurable por Serial (`r`, 33 mm) — es del montaje, no del robot. |
| 10 — Error de colocación manual | **[`test10_pitch_colocacion_esp32/`](test/test10_pitch_colocacion_esp32/test10_pitch_colocacion_esp32.ino)** | ✅ **Corrido (2026-08-19/20).** Perturbación real medida: **media 1.98°, peor caso 3.06° (n=22)** — contra el θ_umbral≈3.65° recalculado con K_U bajo carga simultánea, deja ~20% de margen. Ya cargado en `notebooks/pso_pendulo_invertido.ipynb` como los 3 escenarios de ángulo (1.5°/2.0°/3.0°) y de velocidad (−0.15/0/+0.15 rad/s). ⚠️ Resultado sin transcribir todavía a `docs/conexiones-registro-pruebas.md` (queda en el notebook y en `informes/borrador_entrega.md`). |
| 11 — Bring-up BTS7960 | [`test11_motores_bts7960_esp32/`](test/test11_motores_bts7960_esp32/test11_motores_bts7960_esp32.ino) | ⏳ **Compila, no flasheado.** ⚠️ **Sus `#define` estaban MAL y se corrigieron el 2026-09-12**: tenía `M1_RPWM = 14` y el cableado real usa **GPIO19** (el 14 tiene salida activa en el boot). Flashearlo como estaba habría dado el síntoma clásico de "el firmware reporta bien y el motor no gira". Ya corregido y recompilado OK. ✅ Nivel lógico 3.3V cerrado por datasheet (8.2). ⏳ Sigue faltando: **las 2 pull-downs en `EN`** (8.4 de este archivo / sección 4 del registro), resolver borneras y masa (8.3/8.4), y el cap de PWM re-derivado a 121 (8.5). **Coast vs. brake en este driver sigue sin medirse** — la inferencia de 8.7 es del datasheet, no de la balanza. |
| 6 — Velocidad por encoder | **[`test6_velocidad_esp32/`](test/test6_velocidad_esp32/test6_velocidad_esp32.ino)** | ✅ **Flasheado y validado** (2026-08-14). Modo 1: diagnóstico en lazo abierto, un motor por vez, aísla motor/canal/encoder. Modo 2: control de velocidad PI por rueda realimentado por encoder, sin filtro. De acá salieron la revisión del cap de PWM y la asimetría M1/M2. Topes de seguridad en capas. AP `RobotBalance_Test6`. |
| 4 — UART ESP↔Nano | `test4_uart_esp/` + `test4_uart_nano/` | **Deprecado** — ya no existe enlace entre chips. Conservar como historial o eliminar. |
| — | `test4_diag_0x55/` | **Deprecado** — sketch de diagnóstico del enlace eliminado. |

**Resultados que siguen siendo válidos** (no hay que repetirlos): calibración del
MPU6050 (Test 1/1b) y zona muerta bajo carga (Test 2), porque son propiedades del
sensor y de la mecánica, no del microcontrolador.

## Flujo de trabajo con hardware

- Claude Code puede **compilar/verificar** sketches con `arduino-cli`. Cores instalados: `arduino:avr` 1.8.8, `esp8266:esp8266` 3.1.2, y **`esp32:esp32` 3.3.11** (instalado 2026-08-04). **FQBN de esta placa: `esp32:esp32:nodemcu-32s`.** Librerías `Adafruit MPU6050`, `Adafruit Unified Sensor`, `Adafruit BusIO` ya instaladas.
  ```bash
  arduino-cli compile --fqbn esp32:esp32:nodemcu-32s test/<sketch>
  ```
- **El flasheo lo hace el usuario manualmente**, no Claude Code. Claude Code no debe ejecutar `arduino-cli upload` ni escribir al puerto serie sin pedirlo explícitamente cada vez.
- **Entorno Python del proyecto**: `.venv/` en la raíz (Python 3.12, ya en `.gitignore`), con `numpy`, `matplotlib` y `reportlab` instalados el 2026-08-16. Hace falta para correr `pso_pendulo_invertido.ipynb`, los scripts de `video_omega0/` y para regenerar el PDF teórico. ⚠️ El Python del sistema **no** tiene ninguno de los tres.
  ```bash
  .venv/bin/python algoritmos_evolutivos_pso/pdf_teoria/generar_teoria.py
  ```
- El chip USB-serie de esta placa es un **CP2102** — puede requerir el driver CP210x de Silicon Labs en macOS si el puerto no aparece.

## Estado del diseño / Pendientes abiertos

**Migración a ESP32:**
- [x] Evaluación de disponibilidad de pines — 12 GPIO necesarios, holgado en un ESP32 de 38 pines.
- [x] Mapeo de pines definido.
- [x] **Instalar los 4 pull-downs de 10kΩ en IN1–IN4** — ✅ hecho 2026-08-05, ver "Seguridad: pull-downs" arriba (ubicación real: protoboard del ESP32, no el header del L298N).
- [x] Recablear: I2C, encoders (ambos canales) y las 6 líneas del L298N al ESP32 — ✅ cableado completo 2026-08-05, detalle en `docs/conexiones-registro-pruebas.md` sección 3.1.
- [x] Portar Test 1 a ESP32 — `test/test1_mpu_esp32/`, compila. ✅ Flasheado y validado en hardware real (2026-08-05): bias/escala del acelerómetro y convención de ejes confirmados en las 3 orientaciones.
- [ ] Portar Test 1b a ESP32.
- [x] Reescribir Test 2 para ESP32 (LEDC) — `test/test2_motores_esp32/`, flasheado y validado (2026-08-05). Riesgo de 3.3V hacia el L298N **descartado**.
- [x] Corregir sentido de giro de M1 físicamente (intercambiar OUT1/OUT2 en la bornera) — hecho 2026-08-05. Pendiente verificarlo objetivamente con la Fase 1 del Test 3.
- [x] Escribir Test 3 (encoders) con ambos canales — `test/test3_encoders_esp32/`, compila.
- [x] Correr Test 3 Fase 0 — **PPR confirmado: 69.1 pulsos/vuelta**, igual en ambos motores (2026-08-05).
- [x] Correr Test 3 Fase 2 en aire — ✅ hecho (2026-08-05): M1 85.2, M2 83.9 duty medio. [ ] Falta la condición piso/tethered.
- [x] Escribir Test 5 (K_U + zona muerta frío/caliente) — `test/test5_ku_esp32/`.
- [x] Correr Test 5 — hecho 2026-08-14, **sin K_U usable** (ver 7.15).
- [x] **Medir K_U con el eje trabado** — corrido 2026-08-15, dio 0.022–0.042 (7.16). ⛔ **Superado por el Test 7**, que lo cerró en 0.0123.
- [x] **Cerrar K_U midiendo el torque con balanza** — ✅ hecho 2026-08-16 con [`test7_torque_balanza_esp32/`](test/test7_torque_balanza_esp32/test7_torque_balanza_esp32.ino). **K_U = 0.0123, θ recuperable = 2.07°, criterio 5°. NO PASA — falta un factor 2.4 de torque.** Ver la sección "K_U = 0.0123" arriba y el registro 7.17.
- [x] Medir **m** (1038 g) y **m·l** (0.1265 kg·m → l = 12.2 cm) — ✅ 2026-08-16. `l` es la tercera confirmación independiente de ω₀ = 7.4 rad/s.
- [x] **Correr el Test 9 (COAST vs BRAKE)** — ✅ 2026-08-17. **Ganancia 2.67×: θ recuperable 2.05° → 5.47°, contra un criterio de 5.0°** (un motor a la vez). Ver 7.19 y la sección "⚡ Todo el proyecto conmuta en modo COAST".
- [x] **Barrido de duty en BRAKE** — ✅ 2026-08-17. **θ = 5.47° a duty 160** (un motor a la vez). Ver 7.19.
- [x] **Torque con los DOS motores simultáneos** — ✅ 2026-08-18. **⛔ Revierte lo anterior: el torque cae 31-36% con carga simultánea, θ baja a 3.6°.** No pasa contra 5.0°; pasa contra el criterio provisorio de 3.0°. Medido, no proyectado. Ver 7.20.
- [x] **Rastrear el origen del criterio de 5°** — ✅ 2026-08-18. Es `THETA0` del notebook de PSO (perturbación de simulación, sin derivación física — ver 7.21). Ninguno de los dos números (5° ni 3°) está fundado en una perturbación real medida.
- [x] **Correr el Test 10** — ✅ 2026-08-19/20. Perturbación real: media 1.98°, peor caso 3.06° (n=22). Contra el θ_umbral≈3.65° recalculado (ver la sección "K_U = 0.0123" arriba), deja ~20% de margen salvo en el escenario combinado más exigente. Sin transcribir todavía a `docs/conexiones-registro-pruebas.md`.
- [ ] **Aislar el mecanismo de la caída por carga simultánea** (7.20): ¿interno al L298N (los dos puentes compartiendo algo) o algo más en el camino de potencia? El V_bus estable (−2.3%) descarta que sea simplemente la batería/cableado hundiéndose.
- [ ] **Ensayo térmico de ciclo continuo — parcialmente hecho, sin cerrar** (`e`/`w` en el sketch, 7.20). Ciclo 20% (0.5s ON/2s OFF) a duty 160: M1 cortado por calor a los 180s sin completar, M2 completó 270s sin abortar. Falta terminar de caracterizar antes de fijar el cap operativo.
- [ ] **Portar el modo BRAKE a los sketches de control** (`EN = 1` fijo, PWM sobre los pines IN) — sigue siendo la ganancia disponible, aunque el número final a usar ahora es 3.6°, no 5.47°. ⚠️ Re-derivar los topes: en brake el mismo duty entrega mucho más.
- [ ] **DECISIÓN DE DISEÑO: cambiar la reducción de los motores.** 🔄 **Con el Test 10 (2026-08-19/20), el cuadro cambió.** Contra la perturbación real medida (peor caso 3.06°) en vez del criterio arbitrario de 5.0°/3.0°, el θ_umbral≈3.65° actual **sobra por ~20%** — 8 de 9 escenarios simulados estabilizan sin cambiar el motor. Solo falla la combinación más severa (3° + 0.15 rad/s simultáneos), un límite físico de torque confirmado por dos caminos independientes. **Ya no bloquea empezar el firmware de control**, pero sigue siendo la única palanca para cerrar ese escenario 9 — no descartada, solo des-priorizada. El propio informe de PSO recomienda no validar en el robot real hasta reemplazar el driver, así que tratar las ganancias actuales como punto de partida para banco, no para suelta libre.
- [ ] **Revisar el cap de PWM de 160.** τ(u) sigue creciendo lineal ahí, sin saturación — el cap está dejando torque sobre la mesa. Se fijó con un cálculo que ya se sabe equivocado. ⚠️ **La vía de medir la tensión en las pestañas está descartada** (7.18): la pregunta se responde con τ(u) medido por balanza, que ya muestra que no hay saturación. Lo que falta es fijar el límite por criterio **térmico**, no eléctrico. Y si se adopta BRAKE, el cap hay que re-derivarlo entero — deja de significar lo mismo.
- [x] **Corregir el modelo del notebook de PSO.** ✅ Hecho 2026-08-19/20, en `notebooks/pso_pendulo_invertido.ipynb`: `ω₀=7.4`, `K_U=0.0218` (brake, dos motores a la vez — no el valor de un motor aislado), zona muerta asimétrica `92/102` con función escalón (`PROP_M1=0.575`), y los 3 escenarios de ángulo/velocidad tomados del Test 10 en vez de un `THETA0` supuesto. **Ganancias resultantes: Kp=3500, Ki=2297.02, Kd=484.22** (costo 55.60, sobre 9 escenarios). ⚠️ **Kp queda clavado en el límite superior del espacio de búsqueda** (19/20 semillas) — la función de costo (ITAE) no penaliza conmutar rápido, así que el modelo no tiene un óptimo interior para Kp y empuja a comportamiento bang-bang; el propio informe lo deja como trabajo futuro. **No cargar estas ganancias directo en firmware sin rampa ni validación en banco** — están optimizadas contra un modelo sin ruido de sensor, sin latencia de Kalman ni backlash de la reductora.
- [x] **Hipótesis del calentamiento — ✅ DESCARTADA** (2026-08-14). Los barridos frío y caliente se superponen por completo. No hace falta ninguna regla de "calentar antes de evaluar".
- [ ] **El AP se cae con batería sola** (sin USB). Es el síntoma que abrió la sesión del 2026-08-14 y sigue abierto. Hipótesis viva: picos de corriente del radio WiFi que el Buck no sostiene. Ya se sumó un segundo capacitor de 1000µF/50V en paralelo con C4 y no alcanzó.
- [x] ~~**Medir voltaje directo en las pestañas del motor a duty 160**~~ — ⛔ **intentado y descartado (2026-08-17), no reintentar.** Tests 8 y 8b, ver `docs/conexiones-registro-pruebas.md` 7.18: los dos modelos son falsos y el driver degrada su salida al calentarse, así que la medición es un blanco móvil. **Además era el instrumento equivocado para la pregunta**: este ítem existía para cerrar la revisión del cap de PWM, y el Test 7 ya mide τ(u) directo y muestra que sigue lineal en 160 — la tensión nunca fue necesaria para eso.
- [ ] **Medir `R_m` de ambos motores** (óhmetro con offset restado, promediando varias posiciones de rotor). Barato. Da la corriente de stall, que le falta al modelo de PSO, testea la conjetura de 7.18 de que la asimetría M1/M2 sea eléctrica (`R_m(M2) ≈ 1.6 × R_m(M1)`) y no mecánica, y **decide entre los candidatos de driver de 7.21** (TB6612FNG alcanza solo si la corriente de stall es modesta; si no, DRV8871 ×2). ⚠️ Esa conjetura de asimetría eléctrica es floja — no asignarle peso hasta medirla.
- [x] **Elegir driver de reemplazo** — ✅ **2026-08-29: 2× BTS7960 (IBT-2)**, los módulos que el usuario ya tiene en mano y alrededor de los cuales se cortó N3. Chips separados por construcción, que es lo que se buscaba contra la hipótesis de 7.20. Ver "Resto del hardware" arriba y `docs/conexiones-registro-pruebas.md` sección 8.
- [x] ~~Resolver la capacidad de la bornera del nodo estrella antes de cablear potencia~~ — ✅ **Cerrado 2026-09-03.** Se resolvió por empalme/pigtail: batería + los 2× `B+`/`B−` se sueldan aparte (nodo estrella real) y de ahí sale un solo cable combinado por lado hacia la bornera del Buck, que solo aloja ese cable + C1 — 2 conductores, no 4. Ver `docs/conexiones-registro-pruebas.md` §8.3. ⚠️ Sigue siendo una bornera más chica que la dedicada que se había planeado — no forzar un tercer conductor ahí sin volver a evaluar. **No** puentear `B+` de un driver al otro — viola la topología estrella justo en el caso de dos motores conduciendo a la vez.
- [x] ⚠️ **Medir continuidad `B−` ↔ `GND` del header en cada módulo IBT-2** (8.4) — ✅ **medido 2026-09-10 (V1): DAN CONTINUIDAD en los dos módulos.** Son la misma pista. **Decisión vigente: cablear solo `B−` al nodo estrella; NO llevar el `GND` del header al riel lógico** (sería lazo de masa). ⚠️ Este resultado había quedado registrado solo en `docs/plan-cableado-senales.html` y por eso se recableó mal el 2026-09-12 — lazo de masa armado y corregido, ver `docs/conexiones-registro-pruebas.md` 7.26 y 8.4.
- [x] **Instalar 2 pull-downs de 10kΩ en las líneas `EN`** — ✅ **2026-09-12**, pero con un cambio respecto al plan original: quedaron soldadas **sobre el protoboard rearmado** (fila 10 para M1, fila 9 para M2, ambas al riel "−"), no en el header de cada IBT-2 como se había decidido — dificultad práctica de soldar en el header. ⚠️ **Consecuencia aceptada, no reabrir sin razón nueva**: protegen "desde el protoboard hacia adelante", igual que las 4 viejas — no cierran el "último tramo" (cable protoboard→header `EN`) que la ubicación en el header hubiera cerrado. Detalle en `docs/conexiones-registro-pruebas.md` 3.1b. Las 4 viejas se conservan, ahora sobre `RPWM`/`LPWM` (reuso de GPIO de la migración L298N→BTS7960, no una adición nueva).
  - [x] ✅ **Medición previa — HECHA el 2026-09-10 (V2), en los dos módulos.** Se buscaba descartar un pull-up interno en `EN`, que con el pull-down de 10kΩ habría armado un divisor dejando el pin en ~2.5V en vez de un bajo real. **Resultado: no es un pull-up resistivo** — `R_EN` y `L_EN` dan ~1.6MΩ en un sentido y `OL` en el otro (patrón de diodo/clamp ESD hacia `VCC`, no de resistor), igual en los dos pines y los dos módulos. **El pull-down de 10kΩ es seguro.** Detalle en `docs/plan-cableado-senales.html` Sección 0.
- [ ] **Re-derivar el cap de PWM para el driver nuevo** (8.5). El 160 se calibró contra los BJT del L298N; con MOSFET (R_ON 7–10 mΩ) esa caída desaparece y el mismo duty entrega mucho más. Punto de partida: **121**.
- [ ] **Medir coast vs. brake en el BTS7960** (8.7). Hay una inferencia del datasheet de que con `EN=1`/`LPWM=0` ya hace brake de fábrica — daría gratis la ganancia de 2.67× del Test 9. ⚠️ Es inferencia, no medición: no usarla en ningún cálculo hasta pasarla por la balanza.
- [x] Instalar C6 (100nF, desacople 3V3) — ✅ **2026-09-12**, junto con C_en, sobre el protoboard rearmado. Ver `docs/conexiones-registro-pruebas.md` 7.25.
- [ ] Revisar §0 y §7 del documento de teoría HTML (describen la arquitectura vieja).

**Rediseño del chasis (2026-08-24):**
- [x] Decidir madera vs. impresión 3D — ✅ madera, por el timeline de 8 semanas hasta empezar a probar RL. Fusion 360 queda pospuesto hasta que el lazo de control balancee de forma repetible.
- [x] ~~Definir distribución de niveles — N1 (drivers+MPU) / N2 (batería+buck+switch) / N3 (protoboard+ESP32) / N4 (carga, vacía)~~ — ⛔ **superado 2026-08-28** por la arquitectura de 4 niveles con drivers en nivel propio, ver "Drivers a nivel propio". `docs/plano-distribucion-niveles.html` reescrito 2026-08-29 con la versión vigente.
- [x] ~~Uniformar el largo de los 4 niveles a 160mm~~ — ⛔ superado 2026-08-28/29: los 4 niveles ya estaban cortados a **150mm**, no 160mm. Se mantiene la uniformidad (decisión del usuario) al valor real de corte — ver "Drivers a nivel propio", todo el contenido entra girando componentes.
- [x] Sumar N4 al plano (antes opcional/sin cortar) — ✅ 2026-08-27, placa vacía. Ver techo de carga futura en la tabla de "Rediseño del chasis a balsa".
- [ ] Pesar los 2 módulos BTS7960 con disipador — dato más flojo de la proyección de masa.
- [ ] Pesar el XL4016, el switch, el fusible, y el conjunto protoboard+ESP32+cableado montado.
- [ ] Cortar un cupón de densidad de la balsa (60×20mm) y pesarlo — confirmar 0.16 g/cm³ asumido.
- [ ] Preparar los 16 parches de refuerzo (4 por nivel, ya con N4 sumado) antes de perforar las esquinas.
- [ ] Decidir insertos roscados vs. tuerca-pasante para los tornillos de montaje de cada BTS7960.
- [ ] Definir si el entrenamiento de RL corre primero en simulación o directo en hardware real — decide si hace falta reforzar los cantos de N1 contra caídas repetidas.
- [x] ~~Resolver el conflicto de espacio motor+drivers en N1 con soportes separados~~ — ⛔ **superado 2026-08-28**, ver "Drivers a nivel propio" abajo: los drivers pasan a N3, ya no hace falta ningún soporte de motor separado ni recorte de disipador en N1.
- [x] ~~Resolver el conflicto de espacio MPU+drivers en la misma cara de N1~~ — ⛔ **superado**, mismo motivo — al no compartir nivel, no hay conflicto que resolver con recortes.
- [x] ~~Resolver el conflicto de espacio motor+borneras del driver en la unión soporte↔N1~~ — ⛔ **superado**, mismo motivo.
- [x] Decidir si separar drivers en su propio nivel — ✅ 2026-08-28, **N3 dedicado a los 2 BTS7960**, montados directo arriba, sin recorte. Ver "Drivers a nivel propio" arriba.
- [x] ~~Confirmar qué hacer con N2/N3/N4 si ya se cortaron a 160mm~~ — ✅ 2026-08-29: **los 4 niveles ya están cortados, a 150mm** (no 160mm). Se mantiene uniformidad al valor real. Ver tabla arriba.
- [x] ~~Confirmar si mantener uniformidad de largo entre niveles~~ — ✅ el usuario confirmó mantenerla. Con batería/buck/switch girados 90° (lado largo transversal), todo el contenido de los 4 niveles entra en 150mm sin forzar nada.
- [ ] Medir la altura del BTS7960 montado (driver + disipador) sobre N3 — define el hueco N3→N4, hoy es 35mm estimado sin confirmar.
- [x] Generar plantillas imprimibles nuevas para N1, N2, N3 y N4 — ✅ 2026-08-29, `docs/plantilla-imprimible-niveles.html` (+ `.pdf`), escala real 1:1, 2 niveles por hoja.
- [x] **Re-medir ω₀ del chasis rearmado** — ✅ **2026-09-09: ω₀ = 8.0 ± 0.1 rad/s**, 6 clips de video, dispersión 1.2%, todos los chequeos pasan. Ver la sección "ω₀ = 8.0 rad/s" arriba y el registro 7.22. El montaje pivotó limpio: el chequeo de cuerpo rígido da 0.6 ms contra 6.4 ms de 3σ.
- [x] **Re-medir `m` y `m·l` del chasis rearmado** — ✅ 2026-09-10: `m=1150g`
      (sin ruedas), `m·l=0.1040 kg·m`, `l=9.04cm`, `J=0.0159 kg·m²`. Ver la
      sección "J = 0.0159 kg·m²" arriba y el registro 7.23. Un punto suelto a
      `D=22cm` (10% de diferencia) queda sin explicar, no bloqueante.
- [ ] 🎯 **Re-medir `K_U` con balanza** (dos motores en brake conduciendo a la vez —
      condición real, ver 7.20) y **re-correr el PSO**. ✅ **Ya no está bloqueado por
      el cableado: los BTS7960 quedaron cableados el 2026-09-12.** Con `ω₀`, `m`, `m·l`
      y `J` todos cerrados, **`K_U` es el único parámetro que falta para recalcular
      `θ_umbral`** y tener un presupuesto de torque vigente. ⚠️ Antes hay que completar
      V3 (primera energización de potencia) y decidir coast-vs-brake midiéndolo (8.7),
      porque el modo de decaimiento cambia `K_U` por un factor ~2.7.
- [x] **Calibrar la escala angular absoluta del banco de video** — ✅ 2026-09-10: nivel digital sobre el robot en reposo (mismo ensamble, sin cámara) da 0.6°. El video sobre-lee la amplitud de suelta 23-36% frente a la medición directa (12.4°) — atribuible al pivote de imagen mal ubicado. No afecta a ω₀ ni a `θ_umbral` (ninguno de los dos pasa por la escala de grados de este video). Ver 7.22.
- [ ] **Marcas de color sobre la cinta blanca** (3 o 4, gruesas y saturadas, separadas varios cm) si se vuelve a filmar. La cinta actual con línea roja fina no rindió; las marcas cerrarían a la vez el ajuste del eje de giro y la escala angular. Ver 7.22.

**Incidente 2026-09-10 — ✅ recuperado (2026-09-12), queda solo V3:**
- [x] **Verificar con multímetro la polaridad / continuidad del Buck nuevo antes de conectarlo** — ✅ 2026-09-12, cumple con lo esperado. Ver "Buck destruido por polaridad invertida" arriba.
- [x] **Reemplazar `C1` y el capacitor de salida (1000µF/50V)** — ✅ 2026-09-12, con los repuestos en stock.
- [x] **Probar el Buck nuevo y confirmar la salida** — ✅ 2026-09-12, calibrado en **5.01V**. Ese es el valor de referencia vigente; el 4.98V era de la unidad destruida.
- [ ] **Repetir V3** del plan de cableado (potencia armada: `B+` de cada driver ~12V, `VCC` de cada driver ~5V) — quedó interrumpida por el incidente, **nunca se completó**. Es lo único que falta de este bloque.
- [x] Re-calibrar el Buck nuevo — ✅ hecho, ver arriba.

**Anteriores, siguen abiertos:**
- [x] Confirmar si C5 se sumó o no — ✅ instalado 2026-08-05, en la salida de 5V del Buck. ⛔ Esa unidad quedó dañada en el incidente de 2026-09-10, ver arriba.
- [x] Reubicar C4 al ESP32 — ✅ instalado 2026-08-05. Pendiente confirmar que sostiene los picos de corriente por WiFi bajo uso real.
- [x] Revisar si C7 (desacople del Nano) sigue teniendo sentido — ✅ no, cerrado 2026-08-05: el Nano no se usa, C7 queda sin función.
- [ ] Reemplazar C1 por uno de 25V/35V antes de pruebas prolongadas a máxima carga. ⛔ El C1 vigente quedó dañado en el incidente de 2026-09-10 — el reemplazo por stock disponible es 100µF/25V de nuevo, este pendiente de subir a 25V/35V sigue abierto igual.
- [x] Recargar la batería — ✅ 12.32V el 2026-08-05 (antes 10.80V). [ ] Falta confirmar balance celda por celda (última medición por celda fue con el pack descargado).
- [ ] Aplicar bias/escala del acelerómetro en firmware.
- [x] Repetir la medición de zona muerta por motor separado, condición aire — protocolo en `docs/conexiones-registro-pruebas.md` sección 7.8. [ ] Falta condición piso/tethered.
- [x] Estimar ω₀ por el método del período de oscilación colgante — ✅ 2026-08-15: 7.4 rad/s sobre el chasis viejo (7.12). ⛔ **Superado 2026-09-09 por ω₀ = 8.0 rad/s sobre el chasis rearmado** (6 clips, 7.22) — ver la sección "ω₀ = 8.0 rad/s" arriba.
  - ⛔ **Dos mediciones anteriores descartadas**: 4.07 rad/s (2026-08-10, dos péndulos acoplados) y **2.33 rad/s (2026-08-13, sostenido a mano)**. Las dos tenían dispersión baja (0.8% y 1.5%) y las dos medían la cantidad equivocada.
- [ ] Firmware: filtro de Kalman, lazo PID, generación de PWM — no iniciado. Ya no está bloqueado por la decisión de reducción de motor (ver pendientes de arriba), pero sí por portar el modo BRAKE al sketch de control y por validar en banco antes de soltar el robot.
- [x] Implementación de PSO para ajuste de ganancias PID — ✅ hecho en Python (`notebooks/pso_pendulo_invertido.ipynb`), no en el robot. Corre sobre parámetros medidos reales; ver "Corregir el modelo del notebook de PSO" arriba. Falta portar el resultado (Kp/Ki/Kd) al firmware — no es un simple copiar-pegar, ver el aviso sobre Kp clavado en el límite de búsqueda.
- [x] Crear `docs/obsoleto/plano-electrico.html` y `pinout-esp32.html` — hecho, más `plano-protoboard-esp32.html` (armado físico del protoboard del ESP32, cableado real confirmado) y `plano-imprimible-esp32.html`.

## Nomenclatura de sketches

- `test/` — sketches de validación por subsistema (vigentes, pendientes de migrar a ESP32).
- `firmware/` — sketches previos de exploración con ESP8266. Historial, superados.

## Recursos

Las fichas técnicas/pinouts originales (imágenes) viven en el Proyecto de Claude.ai — pendientes de traer a `docs/hardware/`.
