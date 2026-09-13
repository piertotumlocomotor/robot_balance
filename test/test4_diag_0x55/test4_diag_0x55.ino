/*
  ========================================================================
  DIAGNOSTICO — Patron 0x55 continuo (Nano, temporal)
  ========================================================================

  NO es parte del Test 4 -- es un sketch descartable para responder una
  sola pregunta con el multimetro: ¿la señal realmente CAMBIA en el nodo
  del divisor durante transmision activa, o queda clavada?

  Manda 0x55 (binario 01010101) sin parar, de corrido. Ese byte en
  particular, enviado en continuo, produce una onda cuadrada perfecta que
  alterna en cada bit -- a 9600 baud, ~4800 Hz con 50% de duty cycle.

  COMO MEDIR:
    Multimetro en VOLTAJE DC, puntas en el nodo del divisor (o directo en
    el pin RX del ESP8266, da lo mismo electricamente) respecto de GND.

  COMO INTERPRETAR:
    - Reposo (sin este sketch, TX en alto fijo) ya sabemos que da ~3.3V.
    - Con este sketch corriendo, si la señal SI llega y cambia de verdad,
      el multimetro promedia la onda cuadrada y deberia mostrar algo
      cercano a la MITAD: ~1.5-1.7V.
    - Si sigue marcando ~3.3V fijo, sin moverse, la señal no esta
      pasando durante transmision activa -- aunque el reposo mida bien,
      confirma el contacto flojo/marginal que sospechamos.

  ⚠️ Cuando termines esta prueba, volver a flashear test4_uart_nano.ino
  (el sketch real de Test 4) -- este es solo temporal.

  ⚠️ Mismo requisito que siempre: desconectar J21/J22 antes de flashear
  por USB, reconectar despues para correr la prueba.

  BAUDIOS: 9600 -- mismo que el resto del Test 4 (aunque para esta prueba
  puntual no importa si el ESP8266 esta escuchando o no).
  ========================================================================
*/

const uint32_t UART_BAUD = 9600;

void setup() {
  Serial.begin(UART_BAUD);
}

void loop() {
  Serial.write(0x55);
}
