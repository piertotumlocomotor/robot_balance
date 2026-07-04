// firmware/firmware.ino
#include <Wire.h> // Librería para el manejo del protocolo I2C

void setup() {
  // Inicializa la comunicación serie a 115200 baudios para hablar con la Mac
  Serial.begin(115200);
  while (!Serial) {
    ; // Espera a que el canal de datos esté listo
  }
  
  Serial.println("\n[INFO] Inicializando Escaner I2C...");
  
  // Activa el bus I2C en los pines por defecto: D2 (SDA) y D1 (SCL)
  Wire.begin(); 
}

void loop() {
  byte error, address;
  int nDevices = 0;

  Serial.println("[INFO] Buscando sensor en el bus...");

  // Escanea las direcciones estándar del protocolo I2C
  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      // Dispositivo encontrado
      Serial.print("[OK] ¡Sensor detectado en la direccion: 0x");
      if (address < 16) Serial.print("0");
      Serial.print(address, HEX);
      Serial.println("!");
      nDevices++;
    }
  }

  if (nDevices == 0) {
    Serial.println("[ALERTA] No se detectó ningún sensor. Revisa los cables.");
  }

  // Espera 5 segundos antes de volver a verificar
  delay(5000);
}