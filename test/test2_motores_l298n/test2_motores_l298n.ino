/*
  ========================================================================
  TEST 2 — Señal PWM a ambos motores vía L298N (Arduino Nano)
  ========================================================================

  Objetivo: validar que el Nano comanda ambos motores en ambos sentidos,
  con el voltaje promedio limitado por software, y medir la zona muerta
  (duty minimo al que cada motor arranca) — dato que hara falta despues
  para el lazo PID.

  v2 (2026-07-30): la Fase C ahora barre CADA MOTOR POR SEPARADO (antes
  barria ambos a la vez, lo que no permitia saber cual de los dos arranca
  primero). Correr este sketch DOS VECES -- una con las ruedas en el aire
  (sin carga) y otra en el piso, tethered (carga real) -- para completar
  las 4 mediciones: M1/M2 x aire/piso. El firmware no sabe en que
  condicion esta corriendo; el operador lo arma fisicamente y anota los
  resultados de cada corrida por separado.

  CABLEADO (docs/conexiones-registro-pruebas.md, seccion 4):
    ENA = D9  (PWM, velocidad Motor 1)   | protoboard J25
    IN1 = D4  (direccion Motor 1)        | protoboard J30
    IN2 = D7  (direccion Motor 1)        | protoboard J29
    ENB = D10 (PWM, velocidad Motor 2)   | protoboard J26
    IN3 = D8  (direccion Motor 2)        | protoboard J27
    IN4 = D12 (direccion Motor 2)        | protoboard J28

    L298N OUT1/OUT2 --> Motor 1 (derecho)
    L298N OUT3/OUT4 --> Motor 2 (izquierdo)

  ------------------------------------------------------------------------
  ⚠️ ANTES DE FLASHEAR — CONFLICTO DE UART
     D0/D1 del Nano estan cableados al ESP8266 (filas J21/J22) y son los
     mismos pines que usa el USB. Con el ESP8266 conectado hay contencion
     en el bus: el flasheo puede fallar o quedar corrupto.
     => DESCONECTAR las filas J21/J22 antes de flashear y de correr este
        test. El Serial de abajo es solo para este test aislado.

  ⚠️ ANTES DE ENERGIZAR — MECANICA
     Levantar el robot: las ruedas NO deben tocar el piso. Este test no
     controla balanceo, solo verifica PWM y sentido de giro.

  ⚠️ PENDIENTE DE HARDWARE (CLAUDE.md)
     C1 sigue siendo de 16V sobre un riel que llega a 12.6V. El margen es
     escaso para los picos de arranque/frenado que genera este test.
     Correr ciclos cortos hasta reemplazarlo por uno de 25V/35V.

  ------------------------------------------------------------------------
  LIMITE DE VOLTAJE (CLAUDE.md, "Limite de PWM a los motores")

    Bateria 3S llena .......... 12.6 V
    Motor rated ...............  6.0 V   (JGB37-520B, 793RPM, confirmado --
                                          la lectura previa de 12V/160RPM
                                          era de un motor similar en foto,
                                          no el instalado)
    Duty maximo = 6.0 / 12.6 = 47.6%  =>  121 / 255

  El limite es sobre el VOLTAJE PROMEDIO entregado al motor. Durante el
  tiempo en alto del PWM la tension en bornes sigue siendo la del riel;
  eso es inherente a como funciona cualquier driver PWM sobre un motor
  con escobillas y es practica estandar — la inductancia del bobinado
  promedia la corriente. Lo que se limita, y lo que define el calentamiento
  del bobinado, es el promedio.

  El tope se aplica con constrain() dentro de setMotor(), no como simple
  constante documentada: ningun valor pedido puede superarlo, venga de
  donde venga.
  ========================================================================
*/

// --- Pines (no cambiar sin actualizar docs/conexiones-registro-pruebas.md)
const uint8_t ENA = 9;
const uint8_t IN1 = 4;
const uint8_t IN2 = 7;
const uint8_t ENB = 10;
const uint8_t IN3 = 8;
const uint8_t IN4 = 12;

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
  analogWrite(enPin, duty);
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
  Serial.println(" TEST 2 - Motores via L298N");
  Serial.println("========================================");
  Serial.print("MAX_PWM_DUTY : "); Serial.print(MAX_PWM_DUTY);
  Serial.print("  (~"); Serial.print(dutyToVolts(MAX_PWM_DUTY), 2); Serial.println(" V prom. a 12.6V)");
  Serial.print("TEST_DUTY    : "); Serial.print(TEST_DUTY);
  Serial.print("  (~"); Serial.print(dutyToVolts(TEST_DUTY), 2); Serial.println(" V prom. a 12.6V)");
  Serial.println("----------------------------------------");
  Serial.println("RUEDAS LEVANTADAS. Comienza en 3 s...");
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }

  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENB, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

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

  Objetivo concreto: identificar la ZONA MUERTA de cada motor por
  separado — el duty minimo al que efectivamente arranca. Por debajo de
  ese valor el L298N entrega tension pero el motor no vence su propia
  friccion estatica. Barrer ambos motores juntos (como en la v1 de este
  test) no permite saber cual de los dos arranca primero si difieren.

  Este dato importa mas adelante: un PID que pide correcciones chicas
  cerca del punto de equilibrio va a mandar duties dentro de la zona
  muerta y el robot no va a responder. La compensacion de zona muerta
  es un ajuste estandar en robots de balanceo, pero primero hay que
  medirla -- y medirla por motor, no promediada entre los dos.

  PROTOCOLO (ver CLAUDE.md / conversacion 2026-07-30): correr este
  sketch dos veces, anotando los resultados de cada corrida aparte:
    1) Robot levantado, ruedas sin tocar nada (sin carga).
    2) Robot en el piso, tethered (carga real).
  Con las 4 mediciones (M1/M2 x aire/piso) se sabe cuanto pesa la
  friccion de rodadura real sobre la de los rodamientos, y si hace
  falta compensar la zona muerta con un umbral distinto por motor.
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
  Serial.println("  Anotar el duty en el que CADA motor arranca a girar.");
  Serial.println("  Recordar en que condicion corre esta vuelta: aire o piso.");

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
