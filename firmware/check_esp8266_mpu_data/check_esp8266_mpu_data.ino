// firmware/firmware.ino
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// Instanciamos el objeto del sensor bajo el protocolo I2C
Adafruit_MPU6050 mpu;

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); } // Esperar estabilización del puerto USB

  Serial.println("\n[INFO] Inicializando MPU6050...");

  // Intentamos inicializar el sensor en la dirección nativa 0x68
  if (!mpu.begin()) {
    Serial.println("[ERROR] ¡No se pudo encontrar el chip MPU6050! Revisa el cableado.");
    while (1) { delay(10); } // Bucle infinito de protección en caso de fallo
  }

  Serial.println("[OK] MPU6050 inicializado correctamente.");
  
  // Configuración de rangos de operación (Parámetros base)
  mpu.setAccelerometerRange(MPU6050_RANGE_2_G);   // Rango: +-2G
  mpu.setGyroRange(MPU6050_RANGE_250_DEG);       // Rango: +-250 grados/s
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);     // Filtro pasabajos digital interno (ruido hardware)
  
  delay(100);
}

void loop() {
  // Estructuras de la librería para almacenar los eventos físicos de lectura
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  /* Imprimimos los datos en el Monitor Serie en formato limpio.
    Unidades: Aceleración en m/s^2  |  Giroscopio en rad/s
  */
  Serial.print("Acc_X:"); Serial.print(a.acceleration.x); Serial.print(",");
  Serial.print("Acc_Y:"); Serial.print(a.acceleration.y); Serial.print(",");
  Serial.print("Acc_Z:"); Serial.print(a.acceleration.z); Serial.print(",");
  Serial.print("Gyr_X:"); Serial.print(g.gyro.x);         Serial.print(",");
  Serial.print("Gyr_Y:"); Serial.print(g.gyro.y);         Serial.print(",");
  Serial.print("Gyr_Z:"); Serial.println(g.gyro.z);

  // Muestreo lento controlado de 200 ms (5 Hz) para poder observar y analizar los números
  delay(200);
}