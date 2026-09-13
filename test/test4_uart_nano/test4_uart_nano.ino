/*
  ========================================================================
  TEST 4 — Enlace UART ESP8266 <-> Arduino Nano (lado Nano)
  ========================================================================

  Objetivo: recibir los PING del ESP8266, parpadear un LED indicador
  como confirmacion visual local, y responder con PONG usando el mismo
  numero recibido.

  CABLEADO:
    Nano RX (D0) <-- directo       <-- ESP8266 TX (GPIO1)
    Nano TX (D1) --> divisor 1k/2k --> ESP8266 RX (GPIO3)
    LED indicador: D5 -> resistencia 220 ohm -> anodo LED amarillo
                   -> catodo LED -> GND
    (ver docs/conexiones-registro-pruebas.md seccion 4 para el detalle
    del divisor, ya armado y verificado por resistencia y por voltaje)

  ------------------------------------------------------------------------
  ⚠️ ESTE TEST NO PUEDE USAR Serial POR USB PARA DEBUG
     D0/D1 estan ocupados por el enlace con el ESP8266 bajo prueba. El
     unico indicador local disponible es el LED en D5. Mismo conflicto
     que ya vimos en el Test 2: desconectar las filas J21/J22 (Nano <->
     ESP8266) antes de flashear este sketch por USB, y reconectarlas
     recien para correr el test.

  BAUDIOS: 9600 -- debe coincidir exactamente con el lado ESP8266.
  ------------------------------------------------------------------------
*/

const uint8_t  LED_PIN   = 5;
const uint32_t UART_BAUD = 9600;

String rxBuffer = "";

void blinkOnce() {
  digitalWrite(LED_PIN, HIGH);
  delay(80);
  digitalWrite(LED_PIN, LOW);
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.begin(UART_BAUD);

  /*
    Parpadeo de arranque (3 veces), independiente de si llega algo por
    UART o no. Sirve para separar dos preguntas distintas: "¿el LED y su
    cableado estan bien?" (esto lo confirma solo) de "¿el enlace UART
    funciona?" (eso lo confirman los parpadeos posteriores, uno por cada
    PING recibido).
  */
  for (uint8_t i = 0; i < 3; i++) {
    blinkOnce();
    delay(120);
  }
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      rxBuffer.trim();
      if (rxBuffer.startsWith("PING ")) {
        String num = rxBuffer.substring(5);
        blinkOnce();
        Serial.println("PONG " + num);
      }
      rxBuffer = "";
    } else if (c != '\r') {
      rxBuffer += c;
      if (rxBuffer.length() > 40) rxBuffer = ""; // basura/ruido, descartar
    }
  }
}
