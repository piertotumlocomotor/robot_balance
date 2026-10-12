/*
  ========================================================================
  TEST 11 — Bring-up del driver BTS7960 (reemplazo del L298N) — Etapa 3
  ========================================================================

  POR QUE EXISTE

  Se compraron 2 modulos BTS7960 para reemplazar el L298N, bajo la hipotesis
  (7.20, sin confirmar) de que la caida de torque del 31-36% al conducir los
  dos motores a la vez es interna al L298N -- que tiene los dos puentes en
  un mismo die. El BTS7960 es un puente H SIMPLE: cada modulo mueve UN motor,
  asi que la arquitectura queda con dos chips separados por construccion.

  Este sketch es SOLO bring-up (Etapa 3 de docs/plan-pruebas-pre-energizacion):
  verificar que los motores responden, en que sentido, y que el signo del
  encoder es coherente con el sentido comandado -- antes de medir nada con
  la balanza. Saltear este paso ya costo horas de diagnostico tres veces en
  este proyecto (CLAUDE.md, "Checklist antes de cualquier prueba que mueva
  motores").

  ------------------------------------------------------------------------
  PINES — VERIFICADOS EN FUNCIONAMIENTO (registro §7.30, 2026-10-11)

      M1 (derecho):   EN=GPIO27 (R_EN+L_EN)  RPWM=GPIO19  LPWM=GPIO13
      M2 (izquierdo): EN=GPIO4  (R_EN+L_EN)  RPWM=GPIO16  LPWM=GPIO17
      Encoders:       M1 A/B = GPIO32/33     M2 A/B = GPIO25/26

  RPWM M1 es GPIO19, NO 14: el 14 tiene salida activa durante el boot.

  ------------------------------------------------------------------------
  PULL-DOWNS DE SEGURIDAD — INSTALADAS Y VERIFICADAS (§7.28)

  6 pull-downs de 10k (EN, RPWM, LPWM de cada motor). Si el ESP32 se cuelga
  o se resetea, sus GPIO quedan en alta impedancia y EN cae a bajo: el
  BTS7960 apaga los dos MOSFET ("To deactivate both switches, the INH pin
  has to be set to low", datasheet Infineon). Igual: mano en el switch de V+.

  ------------------------------------------------------------------------
  CAP DE PWM: 121, NO EL 160 DEL L298N

  El L298N (BJTs) se comia varios volts; el BTS7960 (MOSFET, R_ON de mOhm)
  casi no, asi que la cuenta ingenua SI aplica:

      MAX_DUTY_ABSOLUTO = 121   (6.0V / 12.6V bateria llena x 255 = 121.4)

  Subirlo es una decision aparte, despues de medir tau(u) con la balanza en
  este driver.

  ------------------------------------------------------------------------
  COAST vs BRAKE EN ESTE DRIVER: TODAVIA NO SE SABE (registro 8.7)

  Hay una inferencia del datasheet ("active freewheeling") de que con RPWM
  modulado y LPWM fijo en 0 el tramo OFF ya es brake. Es inferencia, no
  medicion: se verifica con la balanza (metodo del Test 9). Este sketch NO
  responde esa pregunta.

  ------------------------------------------------------------------------
  QUE HACE ESTE SKETCH

  Pulso CORTO (2 s por defecto; 't' lo lleva hasta 6 s para que un
  multimetro llegue a asentarse) al motor elegido, a duty bajo, y se detiene
  solo. 'x' durante el pulso lo corta en el acto. Al terminar informa, de los dos
  encoders, el delta de pulsos y las RPM del ultimo medio segundo, y traduce
  el signo a sentido fisico con la convencion medida en §7.30:

      girando hacia ADELANTE del robot:  M1 cuenta NEGATIVO, M2 POSITIVO

  Modo 'a': los dos motores juntos. Por el montaje en espejo, el mismo
  sentido electrico hace girar las ruedas en sentidos opuestos respecto del
  robot -- con las ruedas en el aire no importa; apoyado, giraria en el lugar.

  Al arrancar imprime esp_reset_reason(): si un pulso resetea la placa, el
  arranque siguiente dice si fue BROWNOUT (el riel se hundio con la corriente
  de los motores).
  ========================================================================
*/

#include <Arduino.h>
#include "soc/gpio_reg.h"

const uint8_t M1_EN = 27, M1_RPWM = 19, M1_LPWM = 13;
const uint8_t M2_EN = 4,  M2_RPWM = 16, M2_LPWM = 17;
const uint8_t ENC_M1_A = 32, ENC_M1_B = 33;
const uint8_t ENC_M2_A = 25, ENC_M2_B = 26;

const uint32_t PWM_FREQ_HZ  = 20000;
const uint8_t  PWM_RES_BITS = 8;

const uint8_t MAX_DUTY_ABSOLUTO = 121;
uint8_t duty = 40;     // arranque bien bajo; subir de a poco

const uint32_t PULSO_MIN_MS = 1000;
const uint32_t PULSO_MAX_MS = 6000; // tope: motor trabado a duty bajo, sin riesgo termico
uint32_t pulsoMs = 2000;
const uint32_t VENTANA_MS = 500;    // RPM sobre el final del pulso, ya sin el arranque
const float    PPR        = 69.1f;  // pulsos/vuelta, decodificacion 1x (§7.11, §7.30)

uint8_t motor   = 1;
int8_t  sentido = 1;

// --- Encoders ---------------------------------------------------------------
volatile int32_t encM1 = 0;
volatile int32_t encM2 = 0;

// digitalRead() vive en flash: no es seguro desde una ISR. El registro si.
static inline int IRAM_ATTR fastRead(uint8_t pin) {
  if (pin < 32) return (REG_READ(GPIO_IN_REG) >> pin) & 0x1;
  return (REG_READ(GPIO_IN1_REG) >> (pin - 32)) & 0x1;
}

// Cuadratura 1x: flanco de subida de A, el nivel de B da el sentido (igual que el Test 3).
void IRAM_ATTR isrM1() { encM1 += fastRead(ENC_M1_B) ? -1 : 1; }
void IRAM_ATTR isrM2() { encM2 += fastRead(ENC_M2_B) ? -1 : 1; }

void leerEncoders(int32_t& e1, int32_t& e2) {
  noInterrupts(); e1 = encM1; e2 = encM2; interrupts();
}

// --- Motores ----------------------------------------------------------------
void configurarPines() {
  pinMode(M1_EN, OUTPUT);   pinMode(M2_EN, OUTPUT);
  digitalWrite(M1_EN, LOW); digitalWrite(M2_EN, LOW);   // arranca deshabilitado
  ledcAttach(M1_RPWM, PWM_FREQ_HZ, PWM_RES_BITS); ledcWrite(M1_RPWM, 0);
  ledcAttach(M1_LPWM, PWM_FREQ_HZ, PWM_RES_BITS); ledcWrite(M1_LPWM, 0);
  ledcAttach(M2_RPWM, PWM_FREQ_HZ, PWM_RES_BITS); ledcWrite(M2_RPWM, 0);
  ledcAttach(M2_LPWM, PWM_FREQ_HZ, PWM_RES_BITS); ledcWrite(M2_LPWM, 0);

  pinMode(ENC_M1_A, INPUT_PULLUP); pinMode(ENC_M1_B, INPUT_PULLUP);
  pinMode(ENC_M2_A, INPUT_PULLUP); pinMode(ENC_M2_B, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENC_M1_A), isrM1, RISING);
  attachInterrupt(digitalPinToInterrupt(ENC_M2_A), isrM2, RISING);
}

// Nunca PWM en RPWM y LPWM a la vez.
void aplicarMotor(uint8_t m, int8_t dir, uint8_t d) {
  d = constrain(d, 0, MAX_DUTY_ABSOLUTO);
  uint8_t en = (m == 1) ? M1_EN : M2_EN;
  uint8_t rp = (m == 1) ? M1_RPWM : M2_RPWM;
  uint8_t lp = (m == 1) ? M1_LPWM : M2_LPWM;

  digitalWrite(en, HIGH);
  if (dir > 0)      { ledcWrite(rp, d); ledcWrite(lp, 0); }
  else if (dir < 0) { ledcWrite(rp, 0); ledcWrite(lp, d); }
  else              { ledcWrite(rp, 0); ledcWrite(lp, 0); }
}

void pararMotor(uint8_t m) {
  uint8_t en = (m == 1) ? M1_EN : M2_EN;
  uint8_t rp = (m == 1) ? M1_RPWM : M2_RPWM;
  uint8_t lp = (m == 1) ? M1_LPWM : M2_LPWM;
  ledcWrite(rp, 0); ledcWrite(lp, 0);
  digitalWrite(en, LOW);
}

void pararTodo() { pararMotor(1); pararMotor(2); }

void vaciarEntrada() { while (Serial.available()) Serial.read(); }

float leerNumero(const char* prompt) {
  Serial.print(prompt); Serial.flush(); vaciarEntrada();
  String buf = "";
  while (true) {
    if (!Serial.available()) { delay(10); continue; }
    char c = Serial.read();
    if (c == 'x') { Serial.println(F(" [cancelado]")); return NAN; }
    if (c == '\n' || c == '\r') {
      if (buf.length() == 0) continue;
      Serial.println(buf);
      return buf.toFloat();
    }
    if ((c >= '0' && c <= '9') || c == '.' || c == '-') buf += c;
  }
}

// --- Pulso con corte por 'x' y reporte de encoders -------------------------
const char* sentidoFisico(uint8_t m, int32_t delta) {
  if (delta == 0) return "sin giro";
  bool adelante = (m == 1) ? (delta < 0) : (delta > 0);   // convencion §7.30
  return adelante ? "ADELANTE del robot" : "ATRAS del robot";
}

void informarMotor(uint8_t m, int32_t delta, int32_t deltaVentana, uint32_t msVentana,
                   bool comandado) {
  Serial.printf("  M%u: delta = %+ld pulsos (%.2f vueltas)", m, (long)delta, delta / PPR);
  if (msVentana > 0) {
    float rpm = (deltaVentana / PPR) / (msVentana / 60000.0f);
    Serial.printf(", %.0f RPM (ultimos %lu ms)", fabsf(rpm), (unsigned long)msVentana);
  }
  Serial.printf(" -> %s", sentidoFisico(m, delta));
  if (!comandado && abs(delta) > 3) Serial.print(F("  [!] se movio sin comando: revisar acople"));
  Serial.println();
}

// m = 1 o 2: un motor; m = 0: los dos.
void pulso(uint8_t m) {
  uint8_t d = constrain(duty, 0, MAX_DUTY_ABSOLUTO);
  Serial.print(F("\n--- Pulso: "));
  if (m == 0) Serial.print(F("AMBOS motores"));
  else { Serial.print(F("motor ")); Serial.print(m); }
  Serial.print(F(", ")); Serial.print(sentido > 0 ? F("FWD (RPWM)") : F("REV (LPWM)"));
  Serial.printf(", duty %u (%.1f%%), %lu ms  -- 'x' corta\n",
                d, 100.0f * d / 255.0f, (unsigned long)pulsoMs);

  int32_t e1Ini, e2Ini, e1Ven = 0, e2Ven = 0;
  vaciarEntrada();
  leerEncoders(e1Ini, e2Ini);

  if (m == 1 || m == 0) aplicarMotor(1, sentido, d);
  if (m == 2 || m == 0) aplicarMotor(2, sentido, d);

  uint32_t t0 = millis(), tVen = 0;
  bool ventanaTomada = false, cortado = false;
  while (millis() - t0 < pulsoMs) {
    if (!ventanaTomada && millis() - t0 >= pulsoMs - VENTANA_MS) {
      leerEncoders(e1Ven, e2Ven);
      tVen = millis();
      ventanaTomada = true;
    }
    if (Serial.available() && Serial.read() == 'x') { cortado = true; break; }
    delay(1);
  }

  int32_t e1Fin, e2Fin;
  leerEncoders(e1Fin, e2Fin);
  uint32_t msVen = ventanaTomada ? millis() - tVen : 0;
  pararTodo();

  if (cortado) Serial.printf("[x] CORTADO a los %lu ms. Motores detenidos.\n",
                             (unsigned long)(millis() - t0));
  else         Serial.println(F("Listo, motores detenidos."));

  informarMotor(1, e1Fin - e1Ini, e1Fin - e1Ven, msVen, m == 1 || m == 0);
  informarMotor(2, e2Fin - e2Ini, e2Fin - e2Ven, msVen, m == 2 || m == 0);
  Serial.println(F("  Tocar el driver y el motor: tienen que estar frios o apenas tibios."));
}

// --- Interfaz -----------------------------------------------------------------
void menu() {
  Serial.println(F("\n=== TEST 11 - bring-up BTS7960 ==="));
  Serial.print(F("motor = ")); Serial.print(motor);
  Serial.print(F("   sentido = ")); Serial.print(sentido > 0 ? F("FWD") : F("REV"));
  Serial.print(F("   duty = ")); Serial.print(duty);
  Serial.print(F(" (tope absoluto ")); Serial.print(MAX_DUTY_ABSOLUTO); Serial.print(F(")"));
  Serial.printf("   pulso = %.1f s\n", pulsoMs / 1000.0f);
  Serial.println(F("1/2=motor  i=invertir  d=duty  t=duracion  p=pulso  a=pulso AMBOS  x=parar"));
}

void imprimirResetReason() {
  esp_reset_reason_t r = esp_reset_reason();
  Serial.printf("\n[RESET] esp_reset_reason() = %d", (int)r);
  switch (r) {
    case ESP_RST_POWERON:  Serial.println(F("  (POWERON -- arranque normal)")); break;
    case ESP_RST_SW:       Serial.println(F("  (SW -- reset por software)"));    break;
    case ESP_RST_PANIC:    Serial.println(F("  (PANIC -- excepcion en firmware)")); break;
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:      Serial.println(F("  (WATCHDOG)"));                   break;
    case ESP_RST_BROWNOUT: Serial.println(F("  (BROWNOUT <<< EL RIEL SE HUNDE)")); break;
    case ESP_RST_EXT:      Serial.println(F("  (EXT -- reset por pin EN)"));    break;
    default:               Serial.println(F("  (otro)"));                       break;
  }
  if (r == ESP_RST_BROWNOUT) {
    Serial.println(F("[!] BROWNOUT: la alimentacion no sostuvo al ESP32 -- probablemente"));
    Serial.println(F("[!] durante un pulso. NO seguir subiendo duty: revisar Buck y C4/C5."));
  }
}

void setup() {
  configurarPines();                 // estado seguro antes que nada
  Serial.begin(115200);
  delay(1000);

  Serial.println(F("\n\n########################################"));
  Serial.println(F("TEST 11 - bring-up del BTS7960 (Etapa 3)"));
  Serial.println(F("########################################"));
  Serial.println(F("Verificacion de sentido y encoders. No mide torque."));
  Serial.println(F("RUEDAS EN EL AIRE. Mano en el switch de V+."));
  Serial.println(F("Tope de duty 121 (no el 160 del L298N)."));
  Serial.println(F("Convencion (§7.30): ADELANTE del robot = M1 negativo, M2 positivo."));
  imprimirResetReason();
  Serial.println(F("\nOrden sugerido:"));
  Serial.println(F("  1. Motor 1, duty 40, FWD -> sentido y signo del encoder"));
  Serial.println(F("  2. Motor 1, REV ('i') -> que invierta"));
  Serial.println(F("  3. Repetir con motor 2"));
  Serial.println(F("  4. Si no gira, subir duty de a 10 ('d')"));
  Serial.println(F("  5. 'a' -> los dos juntos, chequeo auditivo cualitativo"));
  menu();
}

void loop() {
  if (!Serial.available()) { delay(10); return; }
  char c = Serial.read();
  switch (c) {
    case '1': motor = 1; menu(); break;
    case '2': motor = 2; menu(); break;
    case 'i': sentido = -sentido; menu(); break;
    case 'd': { float v = leerNumero("Duty: ");
                if (!isnan(v) && v > 0) duty = constrain((int)v, 1, MAX_DUTY_ABSOLUTO);
                menu(); break; }
    case 't': { float v = leerNumero("Duracion del pulso en s (1-6): ");
                if (!isnan(v) && v > 0)
                  pulsoMs = constrain((uint32_t)(v * 1000.0f), PULSO_MIN_MS, PULSO_MAX_MS);
                menu(); break; }
    case 'p': pulso(motor); menu(); break;
    case 'a': pulso(0); menu(); break;
    case 'x': pararTodo(); Serial.println(F("\nPARADO.")); menu(); break;
    default: break;
  }
}
