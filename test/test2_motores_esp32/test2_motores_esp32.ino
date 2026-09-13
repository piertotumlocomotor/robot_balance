/*
  ========================================================================
  TEST 2 — Señal PWM a ambos motores via L298N (ESP32, NodeMCU ESP-32S)
  ========================================================================

  Portado desde test/test2_motores_l298n/ (Arduino Nano, deprecado tras la
  migracion a ESP32 -- ver CLAUDE.md "Decisiones revertidas").

  OBJETIVO PRINCIPAL DE ESTA CORRIDA: no es solo confirmar sentido de giro
  y zona muerta (eso ya se midio una vez, seccion 7.7 de
  docs/conexiones-registro-pruebas.md, y sigue siendo valido porque es
  propiedad del motor/mecanica, no del microcontrolador). El objetivo
  central es validar el RIESGO ABIERTO #1 de la arquitectura ESP32: el
  Nano manejaba IN1-4/ENA/ENB a 5V logicos, el ESP32 los maneja a 3.3V.
  El L298N es TTL-compatible y muy probablemente funcione iguel, pero eso
  no esta confirmado contra datasheet en este proyecto -- este test es la
  forma directa de comprobarlo. Si los motores NO responden con este
  sketch corriendo (fases reportadas por Serial pero sin movimiento),
  es la senal de que hace falta un level shifter en las 6 lineas.

  CAMBIOS VS. LA VERSION NANO:
    - Pines nuevos (docs/conexiones-registro-pruebas.md, seccion 3).
    - PWM: analogWrite() no existe en ESP32. Se usa el periferico LEDC
      con la API del core 3.x (ledcAttach/ledcWrite por PIN, no por
      canal -- ver CLAUDE.md "Limite de PWM a los motores").
    - Resolucion LEDC fijada a 8 bits para que MAX_PWM_DUTY=121 siga
      siendo directamente aplicable sin reconvertir escalas.
    - Debug solo por USB serie: ya no hay enlace UART entre chips que
      proteja TX/RX, y este test corre con el robot quieto sobre la mesa
      (ruedas en el aire o tethered), no hace falta WiFi.

  CABLEADO (docs/conexiones-registro-pruebas.md, seccion 3):
    ENA = GPIO27 (P27)  -- PWM velocidad Motor 1
    IN1 = GPIO14 (P14)  -- direccion Motor 1
    IN2 = GPIO13 (P13)  -- direccion Motor 1
    ENB = GPIO4  (P4)   -- PWM velocidad Motor 2
    IN3 = GPIO16 (P16)  -- direccion Motor 2
    IN4 = GPIO17 (P17)  -- direccion Motor 2

    L298N OUT1/OUT2 --> Motor 1 (derecho)
    L298N OUT3/OUT4 --> Motor 2 (izquierdo)

  ------------------------------------------------------------------------
  ⚠️ ANTES DE ENERGIZAR — MECANICA
     Levantar el robot: las ruedas NO deben tocar el piso (o tethered con
     el pitch limitado a ±30°, igual que en 7.7). Este test no controla
     balanceo, solo verifica PWM, sentido de giro y respuesta del L298N.

  ⚠️ SEGURIDAD PENDIENTE (CLAUDE.md, seccion "pull-downs en las entradas
     del L298N") — los 4 pull-down de 10kOhm en IN1-4 TODAVIA NO ESTAN
     INSTALADOS. Sin ellos, si el ESP32 se resetea o cuelga a mitad de
     este test, IN1-4 quedan en alta impedancia y el L298N puede quedar
     en un estado indefinido (no necesariamente parado). Tener la mano
     cerca del switch de la bateria durante toda la corrida.

  ⚠️ PENDIENTE DE HARDWARE (CLAUDE.md)
     C1 sigue siendo de 16V sobre un riel que llega a 12.6V. El margen es
     escaso para los picos de arranque/frenado que genera este test.
     Correr ciclos cortos hasta reemplazarlo por uno de 25V/35V.

  ------------------------------------------------------------------------
  LIMITE DE VOLTAJE (CLAUDE.md, "Limite de PWM a los motores")

    Bateria 3S llena .......... 12.6 V
    Motor rated ...............  6.0 V   (JGB37-520B, 793RPM)
    Duty maximo = 6.0 / 12.6 = 47.6%  =>  121 / 255

  El limite es sobre el VOLTAJE PROMEDIO entregado al motor -- durante el
  tiempo en alto del PWM la tension en bornes sigue siendo la del riel;
  eso es inherente a como funciona cualquier driver PWM sobre un motor
  con escobillas. El tope se aplica con constrain() dentro de setMotor(),
  no como constante que cada llamada deba respetar por convencion.
  ========================================================================
*/

#include <Arduino.h>

// --- Pines (no cambiar sin actualizar docs/conexiones-registro-pruebas.md)
const uint8_t ENA = 27;
const uint8_t IN1 = 14;
const uint8_t IN2 = 13;
const uint8_t ENB = 4;
const uint8_t IN3 = 16;
const uint8_t IN4 = 17;

// --- LEDC (PWM por hardware del ESP32) -----------------------------------
const uint32_t PWM_FREQ_HZ = 20000;  // fuera del rango audible
const uint8_t  PWM_RES_BITS = 8;     // 0..255, mismo rango que analogWrite()

// --- Limites y parametros del test --------------------------------------
const uint8_t MAX_PWM_DUTY = 121;  // 6.0V / 12.6V * 255 — tope duro
const uint8_t TEST_DUTY    = 70;   // ~27% del rango: movimiento leve
const uint8_t RAMP_START   = 20;   // inicio del barrido de zona muerta
const uint8_t RAMP_STEP    = 5;
const uint16_t RAMP_HOLD_MS = 600; // tiempo en cada escalon del barrido

const int8_t FWD  =  1;
const int8_t REV  = -1;
const int8_t STOP =  0;

/*
  Comanda un canal del L298N.

  direction: FWD / REV / STOP. Con STOP ambas entradas quedan en bajo,
  lo que deja el motor en rueda libre (coast). No se usa frenado activo
  (ambas entradas en alto) en este test: el frenado genera el pico de
  corriente mas alto del sistema y C1 todavia esta subdimensionado.
*/
void setMotor(uint8_t enPin, uint8_t inA, uint8_t inB, int8_t direction, uint8_t duty) {
  duty = constrain(duty, 0, MAX_PWM_DUTY);

  if (direction > 0) {
    digitalWrite(inA, HIGH);
    digitalWrite(inB, LOW);
  } else if (direction < 0) {
    digitalWrite(inA, LOW);
    digitalWrite(inB, HIGH);
  } else {
    digitalWrite(inA, LOW);
    digitalWrite(inB, LOW);
    duty = 0;
  }
  ledcWrite(enPin, duty);
}

void motor1(int8_t dir, uint8_t duty) { setMotor(ENA, IN1, IN2, dir, duty); }
void motor2(int8_t dir, uint8_t duty) { setMotor(ENB, IN3, IN4, dir, duty); }

void stopAll() {
  motor1(STOP, 0);
  motor2(STOP, 0);
}

// Voltaje promedio aproximado que corresponde a un duty dado, con bateria llena.
float dutyToVolts(uint8_t duty) {
  return (duty / 255.0f) * 12.6f;
}

void banner() {
  Serial.println();
  Serial.println("========================================");
  Serial.println(" TEST 2 - Motores via L298N (ESP32)");
  Serial.println("========================================");
  Serial.print("MAX_PWM_DUTY : "); Serial.print(MAX_PWM_DUTY);
  Serial.print("  (~"); Serial.print(dutyToVolts(MAX_PWM_DUTY), 2); Serial.println(" V prom. a 12.6V)");
  Serial.print("TEST_DUTY    : "); Serial.print(TEST_DUTY);
  Serial.print("  (~"); Serial.print(dutyToVolts(TEST_DUTY), 2); Serial.println(" V prom. a 12.6V)");
  Serial.println("----------------------------------------");
  Serial.println("Si esta fase termina y NINGUN motor se movio,");
  Serial.println("es la senal de que 3.3V no alcanza para el L298N");
  Serial.println("(riesgo documentado en CLAUDE.md) -- haria falta");
  Serial.println("un level shifter en IN1-4/ENA/ENB.");
  Serial.println("----------------------------------------");
  Serial.println("RUEDAS LEVANTADAS O TETHERED. Comienza en 3 s...");
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(200);  // sin while(!Serial) -- el ESP32 no lo necesita, y bloquearia sin USB conectado

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  // API core 3.x: por pin, no por canal (ver CLAUDE.md).
  ledcAttach(ENA, PWM_FREQ_HZ, PWM_RES_BITS);
  ledcAttach(ENB, PWM_FREQ_HZ, PWM_RES_BITS);

  // Estado seguro antes de cualquier otra cosa.
  stopAll();

  banner();
  delay(3000);
}

// --- Fases del test ------------------------------------------------------

/*
  Fase A: cada motor por separado, en ambos sentidos.
  Sirve para confirmar el mapeo de pines y el sentido de giro real de
  cada rueda respecto del frente del robot.
*/
void faseIndividual() {
  Serial.println("[FASE A] Motores por separado");

  Serial.println("  M1 adelante");
  motor1(FWD, TEST_DUTY); delay(1500); stopAll(); delay(800);

  Serial.println("  M1 atras");
  motor1(REV, TEST_DUTY); delay(1500); stopAll(); delay(800);

  Serial.println("  M2 adelante");
  motor2(FWD, TEST_DUTY); delay(1500); stopAll(); delay(800);

  Serial.println("  M2 atras");
  motor2(REV, TEST_DUTY); delay(1500); stopAll(); delay(800);
}

/*
  Fase B: ambos motores a la vez, que es la condicion real de operacion.
  Ademas de verificar el comando simultaneo, es la primera prueba de
  carga sobre la fuente: si el Buck o la bateria no sostienen la corriente,
  se vera aca (reset del micro, caida de tension, motores que titubean).
*/
void faseSimultanea() {
  Serial.println("[FASE B] Ambos motores simultaneos");

  Serial.println("  Ambos adelante");
  motor1(FWD, TEST_DUTY); motor2(FWD, TEST_DUTY);
  delay(2000); stopAll(); delay(1000);

  Serial.println("  Ambos atras");
  motor1(REV, TEST_DUTY); motor2(REV, TEST_DUTY);
  delay(2000); stopAll(); delay(1000);

  Serial.println("  Sentidos opuestos (giro sobre el eje)");
  motor1(FWD, TEST_DUTY); motor2(REV, TEST_DUTY);
  delay(2000); stopAll(); delay(1000);
}

/*
  Fase C: barrido ascendente de duty, UN MOTOR A LA VEZ.

  La zona muerta (~77/255, seccion 7.7) es propiedad del motor y la
  mecanica, no del microcontrolador -- no hace falta remedirla salvo que
  se quiera repetir el protocolo aire/piso de la seccion 7.8. Esta fase
  se conserva igual porque, ademas de la zona muerta, sirve como barrido
  de carga sostenida: si el L298N va a fallar por corriente insuficiente
  desde el ESP32 (3.3V) en vez de por voltaje, es mas probable que se note
  aca que en pulsos cortos de 1.5s.
*/
void barridoMotor(const char* nombre, uint8_t enPin, uint8_t inA, uint8_t inB) {
  Serial.print("  -- "); Serial.print(nombre); Serial.println(" --");

  for (uint8_t duty = RAMP_START; duty <= MAX_PWM_DUTY; duty += RAMP_STEP) {
    Serial.print("     duty="); Serial.print(duty);
    Serial.print("  (~");       Serial.print(dutyToVolts(duty), 2);
    Serial.println(" V prom.)");

    setMotor(enPin, inA, inB, FWD, duty);
    delay(RAMP_HOLD_MS);
  }

  setMotor(enPin, inA, inB, STOP, 0);
  delay(800);
}

void faseZonaMuerta() {
  Serial.println("[FASE C] Barrido de zona muerta -- un motor a la vez");
  Serial.println("  (Referencia ya medida: ~77/255, seccion 7.7. Repetir");
  Serial.println("   solo si se quiere completar el protocolo aire/piso");
  Serial.println("   de la seccion 7.8.)");

  barridoMotor("M1", ENA, IN1, IN2);
  barridoMotor("M2", ENB, IN3, IN4);

  stopAll();
  Serial.println("  Barrido completo.");
  delay(1500);
}

void loop() {
  faseIndividual();
  faseSimultanea();
  faseZonaMuerta();

  stopAll();
  Serial.println();
  Serial.println("[INFO] Ciclo completo. Reinicia en 5 s.");
  Serial.println();
  delay(5000);
}
