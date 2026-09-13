/* ============================================================================
   TEST 12 -- Verificacion de mapeo de senales y de las pull-downs
   ESP32 NodeMCU-32S  ·  2x BTS7960 (IBT-2)  ·  creado 2026-09-13

   PARA QUE SIRVE
   --------------
   Etapa 2.1 de docs/plan-pruebas-pre-energizacion.html. Dos cosas que no se
   pueden verificar de otra forma:

   1. MAPEO: pone UNA linea de senal en alto a la vez, para que el operador
      mida con el multimetro en el pin del header del modulo y confirme que
      llega a donde tiene que llegar. Es la prueba que cierra el riesgo del
      reordenamiento del 2026-09-12 (M1 paso al lado derecho del protoboard y
      M2 al izquierdo) y del cambio de RPWM M1 de GPIO14 a GPIO19.

   2. PULL-DOWNS: modo 'z' pone los 6 GPIO en alta impedancia (INPUT), o sea
      simula un ESP32 colgado o reseteado. Las 6 lineas tienen que medir ~0V.
      Eso es lo unico que garantiza que un cuelgue no arranque los motores.
      Medir en Ohm no alcanza: esto lo prueba en la condicion real.

   POR QUE ES SEGURO
   -----------------
   Nunca se pone mas de una linea en alto a la vez, y el resto queda forzado
   en bajo. Por la tabla de verdad del BTS7960 eso no puede hacer girar nada:

     - EN alto, RPWM=LPWM=0  ->  puente habilitado con las dos salidas a masa
                                 (brake). Sin torque, el motor no gira.
     - RPWM o LPWM alto, EN=0 ->  stand-by. Ambos MOSFET apagados, no pasa nada.

   Igual: CORRER ESTO CON LOS MOTORES DESCONECTADOS (M+/M- fuera de las dos
   borneras). Es gratis y saca del juego cualquier sorpresa.

   NO genera PWM a proposito -- son niveles DC estables, para medir con el
   multimetro. Por eso usa digitalWrite y no LEDC.

   PINES  (mapeo vigente, ver CLAUDE.md y conexiones-registro-pruebas.md 3.1b)
   --------------------------------------------------------------------------
     M1 (derecho, lado F-J del protoboard):
        EN   = GPIO27   -> IBT-2 #1  R_EN + L_EN (atados)     fila 10 (H,I)
        RPWM = GPIO19   -> IBT-2 #1  RPWM                     fila 6  (I)
        LPWM = GPIO13   -> IBT-2 #1  LPWM                     fila 8  (I)

     M2 (izquierdo, lado A-E del protoboard):
        EN   = GPIO4    -> IBT-2 #2  R_EN + L_EN (atados)     fila 9  (C,B)
        RPWM = GPIO16   -> IBT-2 #2  RPWM                     fila 5  (B)
        LPWM = GPIO17   -> IBT-2 #2  LPWM                     fila 7  (B)

   ATENCION: RPWM de M1 es GPIO19, NO GPIO14. El 14 saca senal durante el boot
   del ESP32 (reportado, no figura en el datasheet oficial) y cae bajo la misma
   regla que GPIO5 de este proyecto: no usarlo para nada que mueva un motor.
   GPIO14 tiene que estar LIBRE, sin ningun cable.

   Reporte por Serial USB a 115200. No usa WiFi: no hace falta y el AP se cae
   con bateria sola (pendiente abierto del proyecto).
   ============================================================================ */

#include "esp_system.h"

// --- Pines: mapeo confirmado contra el cableado real (2026-09-12/13) --------
const uint8_t M1_EN = 27, M1_RPWM = 19, M1_LPWM = 13;
const uint8_t M2_EN = 4,  M2_RPWM = 16, M2_LPWM = 17;

// Pin que NO se debe usar: se chequea que siga libre, no se maneja nunca.
const uint8_t PIN_PROHIBIDO = 14;

struct Linea {
  uint8_t     gpio;
  const char *nombre;
  const char *destino;   // donde poner la punta del multimetro
  const char *fila;      // fila del protoboard, para ubicarse
};

const Linea LINEAS[] = {
  { M1_EN,   "EN   M1", "IBT-2 #1  R_EN y L_EN (los DOS pines)", "fila 10 (H,I)" },
  { M1_RPWM, "RPWM M1", "IBT-2 #1  RPWM",                        "fila 6  (I)"   },
  { M1_LPWM, "LPWM M1", "IBT-2 #1  LPWM",                        "fila 8  (I)"   },
  { M2_EN,   "EN   M2", "IBT-2 #2  R_EN y L_EN (los DOS pines)", "fila 9  (C,B)" },
  { M2_RPWM, "RPWM M2", "IBT-2 #2  RPWM",                        "fila 5  (B)"   },
  { M2_LPWM, "LPWM M2", "IBT-2 #2  LPWM",                        "fila 7  (B)"   },
};
const uint8_t N_LINEAS = sizeof(LINEAS) / sizeof(LINEAS[0]);

// Una linea en alto se apaga sola: si el operador se distrae, no queda
// energizada indefinidamente.
const uint32_t TIMEOUT_MS = 30000;

int8_t  activa      = -1;        // indice de la linea en alto, -1 = ninguna
bool    modoHiZ     = false;
uint32_t tActiva    = 0;
uint32_t tUltimoAviso = 0;

// ---------------------------------------------------------------------------

void todasBajo() {
  for (uint8_t i = 0; i < N_LINEAS; i++) {
    pinMode(LINEAS[i].gpio, OUTPUT);
    digitalWrite(LINEAS[i].gpio, LOW);
  }
  activa  = -1;
  modoHiZ = false;
}

void todasHiZ() {
  // INPUT = alta impedancia. Simula ESP32 colgado/reseteado: los GPIO quedan
  // flotando y las pull-downs externas son las que tienen que sostener el bajo.
  for (uint8_t i = 0; i < N_LINEAS; i++) pinMode(LINEAS[i].gpio, INPUT);
  activa  = -1;
  modoHiZ = true;
}

void activar(uint8_t idx) {
  todasBajo();                       // garantiza que no haya dos en alto
  digitalWrite(LINEAS[idx].gpio, HIGH);
  activa  = idx;
  tActiva = millis();
  tUltimoAviso = 0;

  Serial.println();
  Serial.println(F("--------------------------------------------------------"));
  Serial.printf("  EN ALTO:  %s   (GPIO%u)\n", LINEAS[idx].nombre, LINEAS[idx].gpio);
  Serial.printf("  MEDIR EN: %s\n", LINEAS[idx].destino);
  Serial.printf("  Protoboard: %s\n", LINEAS[idx].fila);
  Serial.println(F("  ESPERADO: ~3.3V en ese pin del header, y ~0V en las otras 5."));
  Serial.println(F("  Si mide 0V ahi y 3.3V en otro pin -> hay un CRUCE."));
  Serial.printf("  Se apaga solo en %lu s. '0' para apagar ya.\n", TIMEOUT_MS / 1000);
  Serial.println(F("--------------------------------------------------------"));
}

void estado() {
  Serial.println();
  Serial.println(F("=== ESTADO ==="));
  if (modoHiZ) {
    Serial.println(F("Modo: ALTA IMPEDANCIA (los 6 GPIO en INPUT)."));
    Serial.println(F("Las 6 lineas deben medir ~0V por las pull-downs externas."));
  } else if (activa < 0) {
    Serial.println(F("Modo: TODAS EN BAJO (0V forzado por el ESP32)."));
  } else {
    Serial.printf("Modo: %s en ALTO (GPIO%u).\n", LINEAS[activa].nombre, LINEAS[activa].gpio);
  }
  Serial.printf("GPIO%u (prohibido) no se maneja nunca: verificar a ojo que siga sin cable.\n",
                PIN_PROHIBIDO);
}

void menu() {
  Serial.println();
  Serial.println(F("================ TEST 12 -- MAPEO DE SENALES ================"));
  Serial.println(F("  1  EN   M1  (GPIO27)  -> IBT-2 #1 R_EN + L_EN"));
  Serial.println(F("  2  RPWM M1  (GPIO19)  -> IBT-2 #1 RPWM     <-- 19, no 14"));
  Serial.println(F("  3  LPWM M1  (GPIO13)  -> IBT-2 #1 LPWM"));
  Serial.println(F("  4  EN   M2  (GPIO4)   -> IBT-2 #2 R_EN + L_EN"));
  Serial.println(F("  5  RPWM M2  (GPIO16)  -> IBT-2 #2 RPWM"));
  Serial.println(F("  6  LPWM M2  (GPIO17)  -> IBT-2 #2 LPWM"));
  Serial.println(F("  0  todas en BAJO (estado seguro)"));
  Serial.println(F("  z  ALTA IMPEDANCIA -- prueba de las pull-downs (ver abajo)"));
  Serial.println(F("  s  estado    ?  este menu"));
  Serial.println(F("------------------------------------------------------------"));
  Serial.println(F("  MODO 'z': simula un ESP32 colgado. Las 6 lineas tienen que"));
  Serial.println(F("  medir ~0V. Si alguna sube, la proteccion por hardware NO"));
  Serial.println(F("  esta funcionando y no hay que conectar los motores."));
  Serial.println(F("============================================================"));
}

void setup() {
  todasBajo();                       // primero que nada: estado seguro

  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println(F("############################################################"));
  Serial.println(F("#  TEST 12 -- mapeo de senales y pull-downs                 #"));
  Serial.println(F("#                                                          #"));
  Serial.println(F("#  >>> CORRER CON LOS MOTORES DESCONECTADOS <<<             #"));
  Serial.println(F("#  (M+ / M- fuera de las dos borneras de potencia)          #"));
  Serial.println(F("#                                                          #"));
  Serial.println(F("#  No genera PWM: son niveles DC para medir con multimetro. #"));
  Serial.println(F("#  Nunca hay mas de una linea en alto a la vez.             #"));
  Serial.println(F("############################################################"));

  // Doble funcion: la Etapa 2 del plan pide justamente este dato y nunca se
  // habia mirado. ESP_RST_BROWNOUT (6) = el riel se hunde al arrancar.
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
    Serial.println(F("[!] BROWNOUT: la alimentacion no sostiene al ESP32."));
    Serial.println(F("[!] Revisar Buck, C4/C5 y el jumper a VIN antes de seguir."));
  }

  menu();
  estado();
}

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r' || c == ' ') return;

    if (c >= '1' && c <= '6') {
      activar((uint8_t)(c - '1'));
    } else if (c == '0') {
      todasBajo();
      Serial.println(F("\n[OK] Todas en BAJO (estado seguro)."));
    } else if (c == 'z' || c == 'Z') {
      todasHiZ();
      Serial.println();
      Serial.println(F("--------------------------------------------------------"));
      Serial.println(F("  ALTA IMPEDANCIA: los 6 GPIO en INPUT."));
      Serial.println(F("  Equivale a un ESP32 colgado o en reset."));
      Serial.println(F("  MEDIR las 6 lineas contra GND -> TODAS ~0V."));
      Serial.println(F("  Si alguna sube, NO conectar los motores: las"));
      Serial.println(F("  pull-downs no estan cumpliendo su funcion."));
      Serial.println(F("  '0' para volver al estado forzado en bajo."));
      Serial.println(F("--------------------------------------------------------"));
    } else if (c == 's' || c == 'S') {
      estado();
    } else if (c == '?' || c == 'h' || c == 'H') {
      menu();
    } else {
      Serial.printf("[?] '%c' no es una opcion. '?' para el menu.\n", c);
    }
  }

  // Apagado automatico de la linea activa.
  if (activa >= 0) {
    uint32_t t = millis() - tActiva;
    if (t >= TIMEOUT_MS) {
      Serial.printf("\n[TIMEOUT] %s vuelve a BAJO tras %lu s.\n",
                    LINEAS[activa].nombre, TIMEOUT_MS / 1000);
      todasBajo();
    } else if (t - tUltimoAviso >= 10000) {
      tUltimoAviso = t;
      Serial.printf("[.] %s sigue en alto (%lu s restantes)\n",
                    LINEAS[activa].nombre, (TIMEOUT_MS - t) / 1000);
    }
  }
}
