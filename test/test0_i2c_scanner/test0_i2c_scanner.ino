/*
  ========================================================================
  TEST 0 — Scanner I2C (diagnostico)
  ========================================================================

  POR QUE EXISTE:
  El Test 5 muere en `mpu.begin()` con "MPU6050 no responde". Ese error no
  distingue entre tres causas muy distintas:

    a) No hay NADIE en el bus  -> problema de cableado SDA/SCL.
    b) Hay alguien, pero en 0x69 en vez de 0x68 -> el pin AD0 del GY-521
       quedo en alto. `mpu.begin()` sin argumentos solo busca 0x68.
    c) Contesta a 100 kHz pero no a 400 kHz -> contacto marginal o cables
       largos. El Test 5 corre a 400 kHz.

  Este sketch barre el bus a las dos velocidades e imprime que direcciones
  contestan. NO usa la libreria Adafruit: habla I2C crudo, asi que aisla el
  problema de cualquier cosa de la libreria.

  ------------------------------------------------------------------------
  CABLEADO: el mismo del Test 5
    SDA = GPIO21   SCL = GPIO22
    GY-521: VCC a 3V3, GND al GND logico

  ALIMENTACION: alcanza con USB solo, SIEMPRE QUE el jumper del riel de 5V
  del Buck al VIN del ESP32 este desconectado (si no, el USB intenta
  energizar hacia atras la salida apagada del Buck y el riel se derrumba).

  REPORTE: Serial a 115200. Sin WiFi a proposito -- menos cosas que puedan
  fallar antes de llegar al diagnostico.
  ========================================================================
*/

#include <Arduino.h>
#include <Wire.h>

const uint8_t I2C_SDA = 21, I2C_SCL = 22;

const uint8_t MPU_ADDR_AD0_LOW  = 0x68;  // AD0 a GND (esperado)
const uint8_t MPU_ADDR_AD0_HIGH = 0x69;  // AD0 en alto
const uint8_t REG_WHO_AM_I      = 0x75;

uint8_t barrer(uint32_t clockHz) {
  Wire.setClock(clockHz);
  Serial.print(F("\n--- Barrido a "));
  Serial.print(clockHz / 1000);
  Serial.println(F(" kHz ---"));

  uint8_t encontrados = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      Serial.print(F("  Dispositivo en 0x"));
      if (addr < 16) Serial.print('0');
      Serial.print(addr, HEX);
      if (addr == MPU_ADDR_AD0_LOW)  Serial.print(F("  <- MPU6050 (AD0 bajo, lo esperado)"));
      if (addr == MPU_ADDR_AD0_HIGH) Serial.print(F("  <- MPU6050 (AD0 EN ALTO: por eso falla mpu.begin())"));
      Serial.println();
      encontrados++;
    }
  }
  if (encontrados == 0) Serial.println(F("  (nadie contesta)"));
  return encontrados;
}

/*
  WHO_AM_I devuelve 0x68 en un MPU6050 sano (el valor coincide con la
  direccion por casualidad historica, no son lo mismo). Leerlo confirma que
  el chip no solo hace ACK sino que responde datos coherentes.
*/
void leerWhoAmI(uint8_t addr) {
  Wire.beginTransmission(addr);
  Wire.write(REG_WHO_AM_I);
  if (Wire.endTransmission(false) != 0) return;

  if (Wire.requestFrom((int)addr, 1) != 1) return;
  uint8_t v = Wire.read();

  Serial.print(F("  WHO_AM_I en 0x"));
  Serial.print(addr, HEX);
  Serial.print(F(" = 0x"));
  Serial.print(v, HEX);
  Serial.println(v == 0x68 ? F("  (MPU6050 sano)") : F("  (valor inesperado)"));
}

void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println(F("\n\n=== TEST 0 - Scanner I2C ==="));
  Serial.print(F("SDA=GPIO")); Serial.print(I2C_SDA);
  Serial.print(F("  SCL=GPIO")); Serial.println(I2C_SCL);

  Wire.begin(I2C_SDA, I2C_SCL);

  /*
    Nivel de reposo del bus: con el GY-521 alimentado, sus pull-ups internos
    (4.7k a VCC) deben mantener SDA y SCL en alto. Si se leen en BAJO, el
    sensor no esta alimentado o hay un corto a GND -- y ninguna cantidad de
    barridos va a encontrar nada.
  */
  pinMode(I2C_SDA, INPUT);
  pinMode(I2C_SCL, INPUT);
  int sdaNivel = digitalRead(I2C_SDA);
  int sclNivel = digitalRead(I2C_SCL);
  Serial.print(F("Reposo del bus -> SDA=")); Serial.print(sdaNivel ? F("ALTO") : F("BAJO"));
  Serial.print(F("  SCL="));                 Serial.println(sclNivel ? F("ALTO") : F("BAJO"));
  if (!sdaNivel || !sclNivel) {
    Serial.println(F("  !! Se esperan AMBOS en ALTO. Uno en BAJO = sin pull-up:"));
    Serial.println(F("     revisar VCC del GY-521, o corto de esa linea a GND."));
  }
  Wire.begin(I2C_SDA, I2C_SCL);
}

void loop() {
  uint8_t n100 = barrer(100000);
  uint8_t n400 = barrer(400000);

  Serial.println(F("\n--- Lectura de WHO_AM_I ---"));
  Wire.setClock(100000);
  leerWhoAmI(MPU_ADDR_AD0_LOW);
  leerWhoAmI(MPU_ADDR_AD0_HIGH);

  Serial.println(F("\n--- Resumen ---"));
  if (n100 == 0 && n400 == 0) {
    Serial.println(F("Nadie en el bus a ninguna velocidad."));
    Serial.println(F("-> Cableado de SDA/SCL, o sensor sin alimentar/danado."));
  } else if (n100 > 0 && n400 == 0) {
    Serial.println(F("Contesta a 100 kHz pero NO a 400 kHz."));
    Serial.println(F("-> Contacto marginal o cables largos. El Test 5 usa 400 kHz:"));
    Serial.println(F("   bajarlo a 100 kHz, o mejorar el contacto fisico."));
  } else {
    Serial.println(F("Bus sano. Ver arriba en que direccion contesta."));
  }

  Serial.println(F("\n(repite en 5 s)"));
  delay(5000);
}
