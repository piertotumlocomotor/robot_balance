/*
  ========================================================================
  TEST 1 — Reporte del MPU6050 (GY-521) por WiFi
  ========================================================================

  Objetivo: validar que el GY-521 responde por I2C y entrega lecturas
  coherentes y estables, antes de sumar cualquier lazo de control.

  El reporte va por WiFi (Access Point propio), NO por USB. Esto respeta
  la decision de diseño de reservar TX/RX (GPIO1/GPIO3) para el enlace con
  el Arduino Nano: no hace falta desconectar nada para correr este test.

  CABLEADO (docs/conexiones-registro-pruebas.md, seccion 3):
    GY-521 SCL --> ESP8266 D1 (GPIO5)   | protoboard derecho, fila 23
    GY-521 SDA --> ESP8266 D2 (GPIO4)   | protoboard derecho, fila 22
    GY-521 VCC --> ESP8266 3V           | protoboard izquierdo, fila 26
    GY-521 GND --> ESP8266 G            | protoboard izquierdo, fila 23

  ------------------------------------------------------------------------
  COMO USARLO
    1. Alimentar solo el riel logico. NO energizar la etapa de potencia:
       sin el Nano comandando, las entradas del L298N quedan flotantes y
       no esta confirmado por datasheet que tengan pull-down interno.
    2. Conectarse a la red WiFi:  RobotBalance_Test1
    3. Abrir en el navegador:     http://192.168.4.1
    4. Con el robot quieto, pulsar "Calibrar giroscopio" y esperar.

  NOTA: el bootloader ROM del ESP8266 escupe unos bytes por TX a 74880
  baud en cada reset. Es inevitable y llega al RX del Nano como basura;
  es inofensivo mientras el Nano no este interpretando ese puerto.

  ------------------------------------------------------------------------
  ESTE TEST TAMBIEN ESTRESA LA ALIMENTACION
  El WiFi genera picos de corriente en el ESP8266. Si las lecturas se
  degradan o la placa se reinicia al levantar el AP, el problema no es el
  sensor sino el riel de 5V o C4 (1000uF en VIN del ESP). Vale la pena
  saberlo de entrada para no perseguir un fantasma en el MPU.
  ========================================================================
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// --- Configuracion del Access Point --------------------------------------
const char *AP_SSID = "RobotBalance_Test1";
const char *AP_PASS = "balance2026";   // minimo 8 caracteres

// --- Configuracion del muestreo ------------------------------------------
const uint16_t SAMPLE_INTERVAL_MS = 20;    // 50 Hz
const uint16_t CALIB_SAMPLES      = 250;   // ~5 s de calibracion a 50 Hz

ESP8266WebServer server(80);
Adafruit_MPU6050 mpu;

// --- Estado global -------------------------------------------------------
bool mpuOk = false;
String mpuError = "";

float gyroBiasX = 0.0f, gyroBiasY = 0.0f, gyroBiasZ = 0.0f;
bool  biasApplied = false;

// Ultima muestra
float ax = 0, ay = 0, az = 0;
float gx = 0, gy = 0, gz = 0;
float accMag = 0, pitch = 0, tempC = 0;

// Estadisticas acumuladas desde el ultimo reset
struct Stats {
  uint32_t n = 0;
  double sum = 0, sumSq = 0;
  float  minV = 0, maxV = 0;

  void add(float v) {
    if (n == 0) { minV = v; maxV = v; }
    else { if (v < minV) minV = v; if (v > maxV) maxV = v; }
    sum   += v;
    sumSq += (double)v * v;
    n++;
  }
  void reset() { n = 0; sum = 0; sumSq = 0; minV = 0; maxV = 0; }
  float mean() const { return n ? (float)(sum / n) : 0.0f; }
  float stddev() const {
    if (n < 2) return 0.0f;
    double m = sum / n;
    double var = (sumSq / n) - (m * m);
    return var > 0 ? (float)sqrt(var) : 0.0f;  // var<0 solo por error de redondeo
  }
};

Stats statMag, statGx, statGy, statGz;

// Calibracion no bloqueante (el servidor debe seguir respondiendo)
bool     calibrating = false;
uint16_t calibCount  = 0;
double   calibSumX = 0, calibSumY = 0, calibSumZ = 0;

uint32_t lastSample = 0;

/*
  ⚠️ CONVENCION DE EJES NO CONFIRMADA
  La orientacion real del GY-521 sobre el chasis no esta verificada contra
  el silkscreen del modulo. Se asume eje Y hacia el frente, Z hacia arriba.

  PROCEDIMIENTO: con el robot vertical y quieto, mirar en la pagina cual de
  los tres ejes del acelerometro marca ~9.8 m/s^2. Ese es el eje "arriba".
  Si no es Z, corregir esta formula antes de usar el pitch para control.
*/
float computePitch(float x, float y, float z) {
  return atan2f(y, sqrtf(x * x + z * z)) * 180.0f / PI;
}

// ========================================================================
// Pagina web
// ========================================================================
const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Test 1 - MPU6050</title>
<style>
  :root { color-scheme: dark; }
  body { font-family: -apple-system, system-ui, sans-serif; background:#111; color:#eee;
         margin:0; padding:16px; max-width:760px; margin-inline:auto; }
  h1 { font-size:1.1rem; color:#00e676; margin:0 0 4px; }
  .sub { font-size:.8rem; color:#888; margin-bottom:16px; }
  .card { background:#1c1c1c; border:1px solid #2c2c2c; border-radius:10px;
          padding:14px; margin-bottom:12px; }
  .card h2 { font-size:.75rem; text-transform:uppercase; letter-spacing:.08em;
             color:#888; margin:0 0 10px; font-weight:600; }
  table { width:100%; border-collapse:collapse; font-variant-numeric:tabular-nums; }
  td { padding:5px 4px; border-bottom:1px solid #262626; font-size:.9rem; }
  tr:last-child td { border-bottom:none; }
  td.lbl { color:#999; width:34%; }
  td.val { text-align:right; font-weight:600; color:#4fc3f7; }
  td.num { text-align:right; color:#bbb; font-size:.85rem; }
  .big { font-size:1.5rem; font-weight:700; }
  .ok { color:#00e676; } .warn { color:#ffb300; } .bad { color:#ff5252; }
  button { background:#00e676; color:#062; border:0; border-radius:8px;
           padding:10px 16px; font-weight:700; font-size:.9rem; cursor:pointer;
           margin-right:8px; }
  button.sec { background:#333; color:#ddd; }
  button:disabled { opacity:.45; cursor:default; }
  .note { font-size:.75rem; color:#777; margin-top:10px; line-height:1.5; }
  #status { font-weight:700; }
</style>
</head>
<body>
  <h1>Test 1 — MPU6050 (GY-521)</h1>
  <div class="sub">Reporte por WiFi · muestreo a 50 Hz</div>

  <div class="card">
    <h2>Estado</h2>
    <div id="status" class="big warn">conectando…</div>
    <div class="note" id="statusNote"></div>
  </div>

  <div class="card">
    <h2>Chequeo de sanidad</h2>
    <table>
      <tr><td class="lbl">|Aceleración|</td>
          <td class="val big" id="mag">–</td></tr>
    </table>
    <div class="note">En reposo debe dar ~9.81 m/s² sin importar la orientación
      del módulo. Si da otra cosa, el problema es el sensor o el bus I2C, no el montaje.</div>
  </div>

  <div class="card">
    <h2>Pitch estimado</h2>
    <table>
      <tr><td class="lbl">Inclinación</td><td class="val big" id="pitch">–</td></tr>
    </table>
    <div class="note">Calculado como <code>atan2(Y, √(X²+Z²))</code>: asume Z hacia
      arriba, Y hacia el frente y X a lo largo del eje de las ruedas. Verificar contra
      el montaje real antes de usarlo para control.</div>
  </div>

  <div class="card">
    <h2>Acelerómetro (m/s²)</h2>
    <table>
      <tr><td class="lbl">X</td><td class="val" id="ax">–</td></tr>
      <tr><td class="lbl">Y</td><td class="val" id="ay">–</td></tr>
      <tr><td class="lbl">Z</td><td class="val" id="az">–</td></tr>
    </table>
    <div class="note">Con el robot vertical y quieto, anotar cuál de los tres ejes
      marca ~9.8. Ese es el eje &laquo;arriba&raquo; y define si hay que corregir el
      cálculo del pitch.</div>
  </div>

  <div class="card">
    <h2>Giroscopio corregido (rad/s)</h2>
    <table>
      <tr><td class="lbl">X</td><td class="val" id="gx">–</td></tr>
      <tr><td class="lbl">Y</td><td class="val" id="gy">–</td></tr>
      <tr><td class="lbl">Z</td><td class="val" id="gz">–</td></tr>
      <tr><td class="lbl">Bias aplicado</td><td class="val" id="bias">no</td></tr>
    </table>
    <button id="calBtn" onclick="calibrate()">Calibrar giroscopio</button>
    <div class="note">Mantener el robot <b>quieto y apoyado</b> durante los ~5 s que
      dura. Con el bias aplicado, los tres ejes deben rondar 0.000 en reposo.</div>
  </div>

  <div class="card">
    <h2>Estabilidad <span id="nsamp" style="color:#666;font-weight:400"></span></h2>
    <table>
      <tr><td class="lbl"></td><td class="num">min</td><td class="num">máx</td><td class="num">σ</td></tr>
      <tr><td class="lbl">|Acel|</td><td class="num" id="magMin">–</td><td class="num" id="magMax">–</td><td class="num" id="magSd">–</td></tr>
      <tr><td class="lbl">Giro X</td><td class="num" id="gxMin">–</td><td class="num" id="gxMax">–</td><td class="num" id="gxSd">–</td></tr>
      <tr><td class="lbl">Giro Y</td><td class="num" id="gyMin">–</td><td class="num" id="gyMax">–</td><td class="num" id="gySd">–</td></tr>
      <tr><td class="lbl">Giro Z</td><td class="num" id="gzMin">–</td><td class="num" id="gzMax">–</td><td class="num" id="gzSd">–</td></tr>
    </table>
    <button class="sec" onclick="resetStats()">Reiniciar estadísticas</button>
    <div class="note">σ es la desviación estándar: mide el ruido real del sensor mucho
      mejor que mirar números pasar. Con el robot quieto debería ser chica y estable.
      Si crece al encender otras cargas, hay ruido de alimentación.</div>
  </div>

  <div class="card">
    <h2>Temperatura</h2>
    <table><tr><td class="lbl">Chip MPU6050</td><td class="val" id="temp">–</td></tr></table>
  </div>

<script>
const f = (v, d = 2) => (v === null || v === undefined) ? '–' : Number(v).toFixed(d);

function setStatus(txt, cls, note) {
  const el = document.getElementById('status');
  el.textContent = txt;
  el.className = 'big ' + cls;
  document.getElementById('statusNote').textContent = note || '';
}

async function tick() {
  try {
    const r = await fetch('/data');
    const j = await r.json();

    if (!j.mpuOk) {
      setStatus('SENSOR NO RESPONDE', 'bad', j.error);
    } else if (j.calibrating) {
      setStatus('calibrando… ' + j.calibProgress + '%', 'warn', 'Mantener el robot quieto.');
    } else {
      setStatus('OK', 'ok', 'Sensor respondiendo en el bus I2C.');
    }

    document.getElementById('calBtn').disabled = j.calibrating || !j.mpuOk;

    document.getElementById('mag').textContent = f(j.mag) + ' m/s²';
    document.getElementById('pitch').textContent = f(j.pitch, 1) + '°';
    document.getElementById('ax').textContent  = f(j.ax);
    document.getElementById('ay').textContent  = f(j.ay);
    document.getElementById('az').textContent  = f(j.az);
    document.getElementById('gx').textContent  = f(j.gx, 3);
    document.getElementById('gy').textContent  = f(j.gy, 3);
    document.getElementById('gz').textContent  = f(j.gz, 3);
    document.getElementById('temp').textContent = f(j.temp, 1) + ' °C';

    document.getElementById('bias').textContent = j.biasApplied
      ? `X ${f(j.bx,4)} · Y ${f(j.by,4)} · Z ${f(j.bz,4)}` : 'no';

    document.getElementById('nsamp').textContent = '· ' + j.n + ' muestras';
    const s = j.stats;
    document.getElementById('magMin').textContent = f(s.magMin);
    document.getElementById('magMax').textContent = f(s.magMax);
    document.getElementById('magSd').textContent  = f(s.magSd, 3);
    document.getElementById('gxMin').textContent = f(s.gxMin, 3);
    document.getElementById('gxMax').textContent = f(s.gxMax, 3);
    document.getElementById('gxSd').textContent  = f(s.gxSd, 4);
    document.getElementById('gyMin').textContent = f(s.gyMin, 3);
    document.getElementById('gyMax').textContent = f(s.gyMax, 3);
    document.getElementById('gySd').textContent  = f(s.gySd, 4);
    document.getElementById('gzMin').textContent = f(s.gzMin, 3);
    document.getElementById('gzMax').textContent = f(s.gzMax, 3);
    document.getElementById('gzSd').textContent  = f(s.gzSd, 4);
  } catch (e) {
    setStatus('SIN CONEXIÓN', 'bad', 'El ESP8266 no responde. ¿Se reinició?');
  }
}

const calibrate   = () => fetch('/calibrate');
const resetStats  = () => fetch('/reset-stats');

setInterval(tick, 250);
tick();
</script>
</body>
</html>
)rawliteral";

// ========================================================================
// Endpoints
// ========================================================================
void handleRoot() {
  server.send_P(200, "text/html", HTML_PAGE);
}

void handleData() {
  String j = "{";
  j += "\"mpuOk\":";  j += (mpuOk ? "true" : "false");
  j += ",\"error\":\""; j += mpuError; j += "\"";
  j += ",\"calibrating\":"; j += (calibrating ? "true" : "false");
  j += ",\"calibProgress\":"; j += String((calibCount * 100) / CALIB_SAMPLES);
  j += ",\"biasApplied\":";   j += (biasApplied ? "true" : "false");

  j += ",\"ax\":"   + String(ax, 3);
  j += ",\"ay\":"   + String(ay, 3);
  j += ",\"az\":"   + String(az, 3);
  j += ",\"gx\":"   + String(gx, 4);
  j += ",\"gy\":"   + String(gy, 4);
  j += ",\"gz\":"   + String(gz, 4);
  j += ",\"mag\":"  + String(accMag, 3);
  j += ",\"pitch\":"+ String(pitch, 2);
  j += ",\"temp\":" + String(tempC, 1);

  j += ",\"bx\":" + String(gyroBiasX, 5);
  j += ",\"by\":" + String(gyroBiasY, 5);
  j += ",\"bz\":" + String(gyroBiasZ, 5);

  j += ",\"n\":" + String(statMag.n);
  j += ",\"stats\":{";
  j += "\"magMin\":" + String(statMag.minV, 3) + ",\"magMax\":" + String(statMag.maxV, 3) + ",\"magSd\":" + String(statMag.stddev(), 4);
  j += ",\"gxMin\":" + String(statGx.minV, 4) + ",\"gxMax\":" + String(statGx.maxV, 4) + ",\"gxSd\":" + String(statGx.stddev(), 5);
  j += ",\"gyMin\":" + String(statGy.minV, 4) + ",\"gyMax\":" + String(statGy.maxV, 4) + ",\"gySd\":" + String(statGy.stddev(), 5);
  j += ",\"gzMin\":" + String(statGz.minV, 4) + ",\"gzMax\":" + String(statGz.maxV, 4) + ",\"gzSd\":" + String(statGz.stddev(), 5);
  j += "}}";

  server.send(200, "application/json", j);
}

void handleCalibrate() {
  if (mpuOk && !calibrating) {
    calibrating = true;
    calibCount  = 0;
    calibSumX = calibSumY = calibSumZ = 0;
  }
  server.send(200, "text/plain", "ok");
}

void handleResetStats() {
  statMag.reset(); statGx.reset(); statGy.reset(); statGz.reset();
  server.send(200, "text/plain", "ok");
}

// ========================================================================
void setup() {
  /*
    Sin Serial.begin() a proposito: el reporte va por WiFi y GPIO1/GPIO3
    quedan libres para el enlace con el Nano. Cualquier diagnostico va a
    la pagina web, no al puerto serie.
  */

  Wire.begin(D2, D1);   // SDA = D2 (GPIO4), SCL = D1 (GPIO5)

  /*
    Si el sensor falla NO se detiene la ejecucion. Con el reporte por WiFi,
    un while(1) dejaria al usuario sin ninguna informacion: el AP nunca
    levantaria. El error se muestra en la pagina y el servidor sigue vivo.
  */
  if (mpu.begin()) {
    mpuOk = true;
    /*
      Rangos elegidos para un pendulo invertido:
        - Acelerometro +-2G: la aceleracion util ronda 1G (gravedad); el
          rango mas chico da la mejor resolucion disponible.
        - Giroscopio +-250 deg/s: suficiente cerca de la vertical. Si se ve
          saturacion durante el balanceo, subir a +-500.
        - Pasabajos interno 21 Hz: corta ruido de escobillas sin agregar
          demasiado retardo de fase al futuro lazo de control.
    */
    mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
    mpu.setGyroRange(MPU6050_RANGE_250_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  } else {
    mpuOk = false;
    mpuError = "MPU6050 no responde en 0x68. Revisar: (1) 3.3V en el modulo, "
               "(2) SDA=D2 y SCL=D1 sin invertir, (3) GND comun.";
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);

  server.on("/",            handleRoot);
  server.on("/data",        handleData);
  server.on("/calibrate",   handleCalibrate);
  server.on("/reset-stats", handleResetStats);
  server.begin();

  lastSample = millis();
}

void loop() {
  server.handleClient();

  if (!mpuOk) return;

  uint32_t now = millis();
  if (now - lastSample < SAMPLE_INTERVAL_MS) return;
  lastSample = now;

  sensors_event_t a, g, t;
  mpu.getEvent(&a, &g, &t);

  ax = a.acceleration.x;
  ay = a.acceleration.y;
  az = a.acceleration.z;
  tempC = t.temperature;

  if (calibrating) {
    calibSumX += g.gyro.x;
    calibSumY += g.gyro.y;
    calibSumZ += g.gyro.z;
    calibCount++;

    if (calibCount >= CALIB_SAMPLES) {
      gyroBiasX = (float)(calibSumX / CALIB_SAMPLES);
      gyroBiasY = (float)(calibSumY / CALIB_SAMPLES);
      gyroBiasZ = (float)(calibSumZ / CALIB_SAMPLES);
      biasApplied = true;
      calibrating = false;
      // Las estadisticas previas se tomaron con otro bias: ya no comparan.
      statGx.reset(); statGy.reset(); statGz.reset();
    }
  }

  gx = g.gyro.x - gyroBiasX;
  gy = g.gyro.y - gyroBiasY;
  gz = g.gyro.z - gyroBiasZ;

  accMag = sqrtf(ax * ax + ay * ay + az * az);
  pitch  = computePitch(ax, ay, az);

  statMag.add(accMag);
  statGx.add(gx);
  statGy.add(gy);
  statGz.add(gz);
}
