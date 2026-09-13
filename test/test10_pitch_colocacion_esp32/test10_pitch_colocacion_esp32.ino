/*
  ========================================================================
  TEST 10 — Error real de colocacion manual (pitch al pararlo "a ojo")
  ========================================================================

  POR QUE EXISTE

  El banco de pruebas real del usuario es: parar el robot lo mas vertical
  posible A MANO, activar el control, y que se mantenga SIN ningun estimulo
  externo. Eso significa que la perturbacion que el controlador va a tener
  que corregir no es un empujon deliberado a un angulo elegido -- es el
  error de colocacion humano.

  El debate de si el criterio de diseño debe ser 3° o 5° (ver CLAUDE.md,
  seccion K_U y docs/conexiones-registro-pruebas.md 7.20) no tiene una
  derivacion fisica en ningun lado del proyecto: ninguno de los dos numeros
  sale de medir la perturbacion real. Este sketch mide esa perturbacion
  real en vez de seguir discutiendo el numero.

  ⚠️ Pedirle al usuario que lea el pitch en pantalla mientras sostiene el
  robot es pedirle dos cosas a la vez con las manos ocupadas. Por eso ESTE
  sketch no requiere que nadie mire el Serial durante el intento: detecta
  solo cuando el robot quedo quieto (parado, sostenido, lo que sea) y
  captura el pitch en ese instante. El usuario solo tiene que: agarrar el
  robot, pararlo lo mas vertical que pueda a ojo, sostenerlo quieto ~1s,
  soltar/mover para el siguiente intento. Repetir ~10 veces.

  ------------------------------------------------------------------------
  COMO DETECTA "QUIETO" (sin gyro calibrado, sin bloquear nada)

  El pitch sale PURO DEL ACELEROMETRO (angulo estatico respecto a la
  gravedad) -- el giroscopio NO participa del calculo de pitch, solo se usa
  para decidir cuando esta lo bastante quieto como para confiar en la
  lectura del acelerometro (que en movimiento mide aceleracion real, no
  solo gravedad, y el angulo sale mal).

  Maquina de estados, actualizada a 50 Hz:

    ESPERANDO_MOVIMIENTO -> (usuario agarra/mueve el robot, |giro| > umbral)
    ESPERANDO_QUIETUD     -> (queda quieto SOSTENIDO ~0.7s, |giro| < umbral)
    CAPTURANDO            -> promedia pitch ~0.5s mas (si se mueve, aborta
                              y vuelve a ESPERANDO_QUIETUD)
    -> imprime el resultado, vuelve a ESPERANDO_MOVIMIENTO

  Volver a ESPERANDO_MOVIMIENTO despues de cada captura evita contar el
  mismo sostenido dos veces: hace falta un movimiento nuevo (el proximo
  intento) antes de que se arme la proxima captura.

  ------------------------------------------------------------------------
  CALIBRACION DEL ACELEROMETRO

  Se reusan los mismos bias/escala validados en test1_mpu_esp32 (Test 1b,
  confirmados en hardware real 2026-08-05). Importa: el bias de X es
  0.265 m/s^2, que sin corregir representa ~1.5° de error sistematico --
  significativo frente a los pocos grados que se estan tratando de medir
  aca. Sin esta correccion el resultado saldria sesgado.

  ------------------------------------------------------------------------
  QUE HACER CON EL RESULTADO

  Al final (o en cualquier momento con 'l') se imprime media, desvio,
  minimo y maximo de todas las capturas. Esa media +- desvio ES el criterio
  de diseño real para "parado a mano, sin estimulo externo" -- comparar
  contra el theta recuperable medido (3.6° con carga simultanea, Test 9,
  ver docs/conexiones-registro-pruebas.md 7.20).

  Comandos: 'l' listar capturas y estadisticas, 'r' borrar y empezar de
  nuevo.

  CABLEADO: identico al Test 1 (docs/conexiones-registro-pruebas.md 3)
    GY-521 SCL -> GPIO22   SDA -> GPIO21   VCC -> 3V3   GND -> GND
  ========================================================================
*/

#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// --- Pines -----------------------------------------------------------------
const uint8_t PIN_SDA = 21;
const uint8_t PIN_SCL = 22;

// --- Calibracion del acelerometro (Test 1b, ver CLAUDE.md) -----------------
const float ACC_BIAS[3]  = { 0.265f, 0.050f, 0.560f };  // X, Y, Z  (m/s^2)
const float ACC_SCALE[3] = { 1.005f, 1.000f, 1.015f };  // X, Y, Z

// --- Deteccion de quietud ----------------------------------------------------
const float    UMBRAL_MOVIMIENTO = 0.35f;   // rad/s, para salir de ESPERANDO_MOVIMIENTO
// 0.05 resulto demasiado estricto: sostenido a mano, el temblor natural
// ronda 0.05-0.09 rad/s (medido en vivo con 'v', 2026-08-19), tocando hasta
// ~0.12 en picos. 0.15 da margen real sobre ese piso de ruido sin acercarse
// al umbral de movimiento (0.35).
const float    UMBRAL_QUIETO     = 0.15f;   // rad/s, para contar como "quieto"
const uint16_t QUIETO_SOSTENIDO_MS = 700;   // quieto continuo antes de armar captura
const uint16_t CAPTURA_MS          = 500;   // ventana de promedio de la captura
const uint16_t SAMPLE_MS           = 20;    // 50 Hz

Adafruit_MPU6050 mpu;
bool mpuOk = false;

enum Estado { ESPERANDO_MOVIMIENTO, ESPERANDO_QUIETUD, CAPTURANDO };
Estado estado = ESPERANDO_MOVIMIENTO;

uint32_t quietoDesde = 0;
uint32_t capturaDesde = 0;
double   capturaSuma = 0;
uint16_t capturaN = 0;

const uint8_t N_MAX = 60;
float pitchCapturas[N_MAX];
uint8_t nCapturas = 0;

uint32_t lastSample = 0;
bool verbose = false;
uint32_t lastVerbosePrint = 0;

const __FlashStringHelper* nombreEstado(Estado e) {
  switch (e) {
    case ESPERANDO_MOVIMIENTO: return F("ESPERANDO_MOVIMIENTO");
    case ESPERANDO_QUIETUD:    return F("ESPERANDO_QUIETUD");
    case CAPTURANDO:           return F("CAPTURANDO");
  }
  return F("?");
}

float computePitch(float x, float y, float z) {
  // Misma convencion que test1_mpu_esp32: Z arriba, Y pitch, X ruedas.
  return atan2f(y, sqrtf(x * x + z * z)) * 180.0f / PI;
}

void listar() {
  Serial.println(F("\n--- Capturas ---"));
  if (nCapturas == 0) { Serial.println(F("(ninguna todavia)")); return; }

  double suma = 0, sumaSq = 0;
  float minV = pitchCapturas[0], maxV = pitchCapturas[0];
  for (uint8_t i = 0; i < nCapturas; i++) {
    float v = pitchCapturas[i];
    Serial.print(F("  #")); Serial.print(i + 1); Serial.print(F(": "));
    Serial.print(v, 2); Serial.println(F("°"));
    suma += v; sumaSq += (double)v * v;
    if (v < minV) minV = v;
    if (v > maxV) maxV = v;
  }
  float media = (float)(suma / nCapturas);
  float var   = (float)(sumaSq / nCapturas - media * media);
  float sd    = (var > 0) ? sqrtf(var) : 0.0f;

  Serial.println(F("---"));
  Serial.print(F("n = ")); Serial.println(nCapturas);
  Serial.print(F("media = ")); Serial.print(media, 2); Serial.println(F("°"));
  Serial.print(F("desvio (sigma) = ")); Serial.print(sd, 2); Serial.println(F("°"));
  Serial.print(F("min/max = ")); Serial.print(minV, 2);
  Serial.print(F("° / ")); Serial.print(maxV, 2); Serial.println(F("°"));
  Serial.println(F("\nEste rango es el error real de colocacion a mano --"));
  Serial.println(F("comparar contra el theta recuperable medido (Test 9)."));
}

void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println(F("########################################"));
  Serial.println(F("TEST 10 - error real de colocacion manual"));
  Serial.println(F("########################################"));
  Serial.println(F("Agarra el robot, parealo lo mas vertical que puedas A OJO,"));
  Serial.println(F("sostenelo quieto ~1s, y soltalo/movelo para el intento"));
  Serial.println(F("siguiente. NO hace falta mirar la pantalla durante el"));
  Serial.println(F("intento: la captura es automatica cuando detecta quietud."));
  Serial.println(F("Repetir ~10 veces. 'l' para ver las capturas, 'r' para borrar."));
  Serial.println(F("Si no captura nada, probar 'v' (modo verbose) para ver en vivo"));
  Serial.println(F("el estado, |giro| y pitch, y diagnosticar cual umbral no se cruza."));
  Serial.println();

  Wire.begin(PIN_SDA, PIN_SCL);

  if (mpu.begin()) {
    mpuOk = true;
    mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
    mpu.setGyroRange(MPU6050_RANGE_250_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    Serial.println(F("[OK] MPU6050 listo. Esperando el primer intento..."));
  } else {
    Serial.println(F("[ERROR] MPU6050 no responde en 0x68. Revisar 3.3V, SDA=21,"));
    Serial.println(F("        SCL=22, GND comun."));
  }

  lastSample = millis();
}

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'l' || c == 'L') listar();
    if (c == 'r' || c == 'R') { nCapturas = 0; Serial.println(F("\nCapturas borradas.")); }
    if (c == 'v' || c == 'V') {
      verbose = !verbose;
      Serial.print(F("\nModo verbose: "));
      Serial.println(verbose ? F("ON (estado + |giro| + pitch cada 200ms)") : F("OFF"));
    }
  }

  if (!mpuOk) return;

  uint32_t now = millis();
  if (now - lastSample < SAMPLE_MS) return;
  lastSample = now;

  sensors_event_t a, g, t;
  mpu.getEvent(&a, &g, &t);

  float axc = (a.acceleration.x - ACC_BIAS[0]) / ACC_SCALE[0];
  float ayc = (a.acceleration.y - ACC_BIAS[1]) / ACC_SCALE[1];
  float azc = (a.acceleration.z - ACC_BIAS[2]) / ACC_SCALE[2];
  float pitch = computePitch(axc, ayc, azc);

  float giroMag = sqrtf(g.gyro.x * g.gyro.x + g.gyro.y * g.gyro.y + g.gyro.z * g.gyro.z);

  if (verbose && now - lastVerbosePrint >= 200) {
    lastVerbosePrint = now;
    Serial.print(F("[v] ")); Serial.print(nombreEstado(estado));
    Serial.print(F("  |giro|=")); Serial.print(giroMag, 3);
    Serial.print(F(" rad/s  pitch=")); Serial.print(pitch, 1);
    Serial.println(F("°"));
  }

  switch (estado) {
    case ESPERANDO_MOVIMIENTO:
      if (giroMag > UMBRAL_MOVIMIENTO) {
        estado = ESPERANDO_QUIETUD;
        quietoDesde = 0;
        Serial.println(F(">> movimiento detectado, esperando quietud..."));
      }
      break;

    case ESPERANDO_QUIETUD:
      if (giroMag < UMBRAL_QUIETO) {
        if (quietoDesde == 0) quietoDesde = now;
        if (now - quietoDesde >= QUIETO_SOSTENIDO_MS) {
          estado = CAPTURANDO;
          capturaDesde = now;
          capturaSuma = 0;
          capturaN = 0;
          Serial.println(F("... quieto detectado, capturando ..."));
        }
      } else {
        quietoDesde = 0;   // se movio: reinicia el conteo de quietud
      }
      break;

    case CAPTURANDO:
      if (giroMag > UMBRAL_QUIETO * 1.5f) {
        // se movio durante la captura: descartar y volver a esperar quietud
        Serial.println(F("(se movio durante la captura, descartada)"));
        estado = ESPERANDO_QUIETUD;
        quietoDesde = 0;
        break;
      }
      capturaSuma += pitch;
      capturaN++;
      if (now - capturaDesde >= CAPTURA_MS) {
        float pitchCapturado = (float)(capturaSuma / capturaN);

        if (nCapturas < N_MAX) pitchCapturas[nCapturas++] = pitchCapturado;

        Serial.print(F(">>> Captura #")); Serial.print(nCapturas);
        Serial.print(F(": pitch = ")); Serial.print(pitchCapturado, 2);
        Serial.println(F("°   (mové el robot para el próximo intento)"));

        estado = ESPERANDO_MOVIMIENTO;   // exige un movimiento nuevo antes de recapturar
      }
      break;
  }
}
