/* ============================================================================
   TEST 13 -- A que riel cuelgan los pull-ups I2C del GY-521
   ESP32 NodeMCU-32S  ·  creado 2026-09-16

   PARA QUE SIRVE
   --------------
   El riel 3V3 del ESP32 mide 3.515V (2026-09-16), por encima del maximo de
   operacion del MPU6050 (3.46V). Lo que importa es que tension ve el chip.
   Si el GY-521 tiene regulador propio, sus pull-ups de 2.2k (marcados "222")
   cuelgan de la salida de ese regulador, y SDA/SCL en reposo miden MENOS que
   el riel 3V3.

   Este sketch deja SDA (GPIO21) y SCL (GPIO22) en INPUT sin pull interno y
   NO inicializa Wire: Wire.begin activa pull-ups internos del ESP32 y
   contaminaria la medicion. Asi las lineas quedan sostenidas solo por los
   pull-ups del modulo.

   QUE MEDIR (multimetro, contra GND, todos los decimales)
   -------------------------------------------------------
     3V3 del ESP32 (fila 12)   -> referencia, se espera ~3.515V
     SDA (fila 17, columna J)
     SCL (fila 14, columna J)

     SDA y SCL claramente menores que 3V3 (~3.3V)
        -> el GY-521 tiene riel propio regulado: el MPU no ve los 3.515V.
     SDA y SCL iguales a 3V3
        -> no concluye (los pull-ups pueden estar del lado de VCC).
     SDA y SCL ~0V
        -> el modulo no esta alimentado: revisar VCC del GY-521.

   SEGURIDAD
   ---------
   Las 6 lineas de los drivers quedan forzadas en BAJO desde el arranque.
   Igual: correrlo con los motores desconectados.

   Reporte por Serial USB a 115200.
   ============================================================================ */

#include "esp_system.h"

const uint8_t PIN_SDA = 21;
const uint8_t PIN_SCL = 22;

// EN M1, RPWM M1, LPWM M1, EN M2, RPWM M2, LPWM M2 (mapeo vigente, 3.1b)
const uint8_t PINES_DRIVER[] = { 27, 19, 13, 4, 16, 17 };

const uint32_t PERIODO_MS = 5000;
uint32_t tUltimo = 0;

void imprimirReset() {
  esp_reset_reason_t r = esp_reset_reason();
  Serial.printf("[RESET] esp_reset_reason() = %d", (int)r);
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
}

void imprimirLineas() {
  int sda = digitalRead(PIN_SDA);
  int scl = digitalRead(PIN_SCL);
  Serial.printf("[I2C] digitalRead  SDA(GPIO21)=%d  SCL(GPIO22)=%d", sda, scl);
  if (sda && scl) {
    Serial.println(F("  -> pull-ups del modulo presentes. MEDIR con multimetro."));
  } else {
    Serial.println(F("  -> alguna linea en 0: el GY-521 no esta alimentado o no esta conectado."));
  }
}

void setup() {
  for (uint8_t p : PINES_DRIVER) {     // primero que nada: drivers en estado seguro
    pinMode(p, OUTPUT);
    digitalWrite(p, LOW);
  }
  pinMode(PIN_SDA, INPUT);             // sin pull interno, sin Wire
  pinMode(PIN_SCL, INPUT);

  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println(F("############################################################"));
  Serial.println(F("#  TEST 13 -- a que riel cuelgan los pull-ups del GY-521    #"));
  Serial.println(F("#  >>> MOTORES DESCONECTADOS <<<                           #"));
  Serial.println(F("############################################################"));
  imprimirReset();
  Serial.println();
  Serial.println(F("SDA y SCL en INPUT sin pull interno. Wire NO inicializado."));
  Serial.println(F("Los 6 pines de los drivers forzados en BAJO."));
  Serial.println();
  Serial.println(F("MEDIR contra GND, con todos los decimales:"));
  Serial.println(F("  1. 3V3 del ESP32  (fila 12)       referencia, ~3.515V"));
  Serial.println(F("  2. SDA            (fila 17, col J)"));
  Serial.println(F("  3. SCL            (fila 14, col J)"));
  Serial.println();
  Serial.println(F("  SDA/SCL claramente menores que 3V3 -> GY-521 con regulador propio."));
  Serial.println(F("  SDA/SCL iguales a 3V3              -> no concluye."));
  Serial.println();
  imprimirLineas();
}

void loop() {
  if (millis() - tUltimo >= PERIODO_MS) {
    tUltimo = millis();
    imprimirLineas();
  }
}
