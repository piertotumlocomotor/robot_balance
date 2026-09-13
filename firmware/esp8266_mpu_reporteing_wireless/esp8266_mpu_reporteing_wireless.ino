/*
  ========================================================================
  PROYECTO: Telemetría Wi-Fi MPU6050 + Alimentación Buck 5V
  ========================================================================
  
  MAPEO DE PINES DOCUMENTADOS:
    - Buck LM2596 OUT+ (5.0V) --> ESP8266 VIN
    - Buck LM2596 OUT- (GND)  --> ESP8266 GND
    - MPU6050 VCC             --> ESP8266 3V3
    - MPU6050 GND             --> ESP8266 GND
    - MPU6050 SCL             --> ESP8266 D1 (GPIO 5)
    - MPU6050 SDA             --> ESP8266 D2 (GPIO 4)
  ========================================================================
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// Credenciales para la red Wi-Fi que creará el ESP8266
const char *ssid = "Robot_MPU6050_Test";
const char *password = "12345678"; // Mínimo 8 caracteres

// Servidor Web en el puerto 80
ESP8266WebServer server(80);

// Sensor MPU6050
Adafruit_MPU6050 mpu;

// Estructura global para guardar últimas lecturas
struct SensorData {
  float ax, ay, az;
  float gx, gy, gz;
  float temp;
} data;

// Página Web HTML + JavaScript
const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Telemetria MPU6050</title>
  <style>
    body { font-family: Arial, sans-serif; background: #121212; color: #fff; text-align: center; margin: 0; padding: 20px; }
    h2 { color: #00e676; }
    .card { background: #1e1e1e; border-radius: 10px; padding: 15px; margin: 15px auto; max-width: 400px; box-shadow: 0 4px 10px rgba(0,0,0,0.5); }
    .val { font-size: 1.2em; font-weight: bold; color: #00bcd4; }
  </style>
</head>
<body>
  <h2>Robot Telemetry - MPU6050</h2>
  <div class="card">
    <h3>Acelerometro (m/s&sup2;)</h3>
    <p>X: <span id="ax" class="val">0.00</span> | Y: <span id="ay" class="val">0.00</span> | Z: <span id="az" class="val">0.00</span></p>
  </div>
  <div class="card">
    <h3>Giroscopio (rad/s)</h3>
    <p>X: <span id="gx" class="val">0.00</span> | Y: <span id="gy" class="val">0.00</span> | Z: <span id="gz" class="val">0.00</span></p>
  </div>
  <div class="card">
    <h3>Temperatura</h3>
    <p><span id="temp" class="val">0.0</span> &deg;C</p>
  </div>

  <script>
    setInterval(() => {
      fetch('/data')
        .then(response => response.json())
        .then(json => {
          document.getElementById('ax').innerText = json.ax;
          document.getElementById('ay').innerText = json.ay;
          document.getElementById('az').innerText = json.az;
          document.getElementById('gx').innerText = json.gx;
          document.getElementById('gy').innerText = json.gy;
          document.getElementById('gz').innerText = json.gz;
          document.getElementById('temp').innerText = json.temp;
        });
    }, 200); // Consulta cada 200ms (5 Hz)
  </script>
</body>
</html>
)rawliteral";

// Endpoint HTTP principal: Sirve la interfaz web
void handleRoot() {
  server.send(200, "text/html", HTML_PAGE);
}

// Endpoint HTTP JSON: Retorna las lecturas actualizadas en formato JSON
void handleData() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  String json = "{";
  json += "\"ax\":" + String(a.acceleration.x, 2) + ",";
  json += "\"ay\":" + String(a.acceleration.y, 2) + ",";
  json += "\"az\":" + String(a.acceleration.z, 2) + ",";
  json += "\"gx\":" + String(g.gyro.x, 2) + ",";
  json += "\"gy\":" + String(g.gyro.y, 2) + ",";
  json += "\"gz\":" + String(g.gyro.z, 2) + ",";
  json += "\"temp\":" + String(temp.temperature, 1);
  json += "}";

  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);

  // Inicialización I2C
  Wire.begin(D2, D1); // SDA = D2 (GPIO 4), SCL = D1 (GPIO 5)

  if (!mpu.begin()) {
    Serial.println("Error al detectar MPU6050!");
    while (1) delay(10);
  }

  // Configuración del MPU6050
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  // Configurar ESP8266 como Punto de Acceso Wi-Fi
  WiFi.softAP(ssid, password);
  IPAddress myIP = WiFi.softAPIP();
  
  Serial.println("\n----------------------------------");
  Serial.print("Punto de Acceso iniciado: ");
  Serial.println(ssid);
  Serial.print("Direccion IP: ");
  Serial.println(myIP);
  Serial.println("----------------------------------");

  // Rutas del Servidor Web
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
}

void loop() {
  server.handleClient(); // Procesar peticiones Wi-Fi
}