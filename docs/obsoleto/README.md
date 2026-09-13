# ⛔ Documentación DEPRECADA — planta anterior al rearmado

Todo lo que está en esta carpeta describe el **robot viejo**: chasis de contrachapado
de 3 niveles y driver **L298N** (o, en un caso, la arquitectura ESP8266 + Arduino Nano,
dos atrás). **Ese robot ya no existe.**

> **Regla de uso (establecida 2026-09-12):** esta carpeta sirve **solo para repasar
> errores y experiencias**. **No se usa para planificar, cablear, construir, cortar ni
> calcular.** La planificación del robot nuevo se hace exclusivamente sobre la
> documentación vigente.

## La frontera

| | PRE — deprecado (esta carpeta) | POST — vigente |
|---|---|---|
| Driver | L298N (BJT) | **2× BTS7960 / IBT-2** (MOSFET), desde 2026-08-29 |
| Chasis | Contrachapado, 3 niveles, ~26cm | **Balsa, 4 niveles, 76×150×10mm**, rearmado 2026-09-03 |
| Protoboard | Filas 1–30, zona de puentes 20–30 | **Rehecho 2026-09-12** — filas en `conexiones-registro-pruebas.md` §3.1b |

La frontera es **de qué robot habla el documento**, no cuándo se escribió: los planos de
diseño del robot nuevo (balsa, niveles, drivers) se escribieron entre el 2026-08-24 y el
2026-08-29, antes del rearmado físico, y son POST.

## Documentación vigente — usar esta

- **[`../../CLAUDE.md`](../../CLAUDE.md)** — fuente de verdad.
- **[`../conexiones-registro-pruebas.md`](../conexiones-registro-pruebas.md)** — pin a pin y registro de pruebas. **§3.1b** tiene las filas vigentes del protoboard.
- **[`../plano-potencia.html`](../plano-potencia.html)** + **[`../plano-senales.html`](../plano-senales.html)** — potencia y señales con BTS7960.
- **[`../plano-conexiones-esp32.html`](../plano-conexiones-esp32.html)** + **[`../plano-imprimible-conexiones-esp32.html`](../plano-imprimible-conexiones-esp32.html)** — capacitores y hoja de cableado.
- **[`../plan-cableado-senales.html`](../plan-cableado-senales.html)** — verificaciones V1/V2/V3.
- **[`../plano-distribucion-niveles.html`](../plano-distribucion-niveles.html)** + **[`../plantilla-imprimible-niveles.html`](../plantilla-imprimible-niveles.html)** — los 4 niveles de balsa y las plantillas 1:1.
- **[`../ensayo-ml-dos-apoyos.html`](../ensayo-ml-dos-apoyos.html)** — banco de `m·l` del chasis rearmado.

## Contenido de esta carpeta

| Archivo | Por qué quedó obsoleto | Reemplazado por |
|---|---|---|
| `plano-electrico.html` | Potencia + señales con **L298N** (2026-08-29). | `../plano-potencia.html` + `../plano-senales.html` |
| `plano-protoboard-esp32.html` | Protoboard de la era L298N (ESP32 en filas 1–19, puentes en 20–30). El protoboard se rehizo **dos veces** desde entonces. | `../conexiones-registro-pruebas.md` §3.1b |
| `plano-imprimible-esp32.html` | Consolidado del anterior; señales etiquetadas `IN1`–`IN4`, pull-downs en filas 21/22/28/29. | `../plano-imprimible-conexiones-esp32.html` |
| `plantilla-imprimible-nivel1-completa.html` (+ `.pdf`) | Arquitectura de **soportes de motor separados** en N1, superada 2026-08-28. | `../plantilla-imprimible-niveles.html` |
| `plantilla-imprimible-nivel1.html` (+ `.pdf`) | Arquitectura de **recortes de disipador** en N1, con drivers y MPU en el mismo nivel. Superada 2026-08-28. | Ídem |
| `plantilla-imprimible-soporte-motor.html` (+ 2 `.pdf`) | La pieza de soporte de 76×65mm dejó de existir: el motor se ancla directo a N1. | Ídem |
| `reporte_01.html` | Instrumental inercial en arquitectura **ESP8266**, dos atrás. La calibración del MPU6050 sigue válida pero vive en `../conexiones-registro-pruebas.md` §7.5 y §7.10. | — |

## Qué de la etapa vieja SÍ sigue valiendo

No todo lo PRE es inútil — lo que no depende del driver ni del chasis sobrevivió, y está
registrado en la documentación vigente, no acá:

- **Calibración del MPU6050** (bias, escala, convención de ejes) — propiedad del sensor.
- **Encoders**: PPR 69.1, mapeo de colores, M1/M2 en espejo.
- **Motor**: JGB37-520B, 6V, 793 RPM; asimetría M1/M2 (M2 ≈ 81% del torque de M1).
- **Las 13 lecciones de método** de `CLAUDE.md` — la mitad nacieron de errores de esta
  etapa y son el motivo por el que esta carpeta existe en vez de borrarse.

## Qué NO usar de acá, nunca, como cantidad

`ω₀ = 7.4` (ni 2.33 ni 4.07) · `m·l = 0.1265` · `l = 12.2cm` · `J = 0.0227` ·
`K_U = 0.0123 / 0.0326 / 0.0218` · `θ_umbral ≈ 3.65°` · cap de PWM `160`/`170` ·
ganancias PSO `Kp=3500, Ki=2297.02, Kd=484.22`. Todos se midieron con el L298N y/o el
chasis de contrachapado. Los vigentes están en `CLAUDE.md`.

---

Antes de mover un archivo nuevo acá: agregar su fila a la tabla, ponerle el banner de
deprecado arriba del `<body>`, y actualizar el enlace en `CLAUDE.md` ("Archivos de
referencia").
