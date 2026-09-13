/*
  Test 2 — Verificación de PWM a motores vía L298N (Arduino Nano)

  Objetivo: confirmar que el Nano puede mover ambos motores levemente
  y en ambas direcciones, sin superar el límite de PWM definido.

  Pines (docs/conexiones-registro-pruebas.md, sección 4):
    ENA = D9  (velocidad Motor 1) | IN1 = D4, IN2 = D7 (dirección Motor 1)
    ENB = D10 (velocidad Motor 2) | IN3 = D8, IN4 = D12 (dirección Motor 2)

  Límite de PWM (CLAUDE.md, "Límite de PWM a los motores"):
    Batería 3S llena = 12.6V máx. Motor rated a 6V (confirmado por el usuario
    2026-07-29 — contradice etiqueta física original de 12V, sin resolver aún,
    ver CLAUDE.md). Duty máximo = 6.0V / 12.6V = 47.6% => 121/255.
    Se aplica con constrain() como tope duro en setMotor(), no solo como
    constante documentada — así ningún valor pedido puede superarlo.

  ⚠️ Antes de correr: levantar el robot para que las ruedas no toquen el
  piso (no es un test de balanceo, es solo verificación de PWM/dirección).
*/

const uint8_t ENA = 9;
const uint8_t IN1 = 4;
const uint8_t IN2 = 7;
const uint8_t ENB = 10;
const uint8_t IN3 = 8;
const uint8_t IN4 = 12;

const uint8_t MAX_PWM_DUTY = 121; // 6V / 12.6V * 255 — ver CLAUDE.md
const uint8_t TEST_DUTY = 70;     // movimiento leve, bien por debajo del cap

void setMotor(uint8_t enaPin, uint8_t inA, uint8_t inB, int8_t direction, uint8_t duty) {
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
  }
  analogWrite(enaPin, duty);
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

  setMotor(ENA, IN1, IN2, 0, 0);
  setMotor(ENB, IN3, IN4, 0, 0);

  Serial.println("\n[INFO] Test 2 - Motores via L298N");
  Serial.print("[INFO] MAX_PWM_DUTY = "); Serial.println(MAX_PWM_DUTY);
  Serial.print("[INFO] TEST_DUTY = "); Serial.println(TEST_DUTY);
}

void loop() {
  Serial.println("[TEST] Motor 1 adelante, 1.5s");
  setMotor(ENA, IN1, IN2, 1, TEST_DUTY);
  delay(1500);
  setMotor(ENA, IN1, IN2, 0, 0);
  delay(1000);

  Serial.println("[TEST] Motor 1 atras, 1.5s");
  setMotor(ENA, IN1, IN2, -1, TEST_DUTY);
  delay(1500);
  setMotor(ENA, IN1, IN2, 0, 0);
  delay(1000);

  Serial.println("[TEST] Motor 2 adelante, 1.5s");
  setMotor(ENB, IN3, IN4, 1, TEST_DUTY);
  delay(1500);
  setMotor(ENB, IN3, IN4, 0, 0);
  delay(1000);

  Serial.println("[TEST] Motor 2 atras, 1.5s");
  setMotor(ENB, IN3, IN4, -1, TEST_DUTY);
  delay(1500);
  setMotor(ENB, IN3, IN4, 0, 0);
  delay(1000);

  Serial.println("[TEST] Ciclo completo. Pausa 3s.");
  delay(3000);
}
