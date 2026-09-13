/*
  ========================================================================
  TEST 1 (ESP32) — Reporte del MPU6050 (GY-521)
  ========================================================================

  Port del Test 1 original (ESP8266) a la arquitectura de un solo ESP32.

  Objetivo: validar que el GY-521 responde por I2C en el micro nuevo y
  entrega lecturas coherentes y estables, antes de sumar cualquier lazo
  de control.

  CABLEADO (docs/conexiones-registro-pruebas.md, seccion 3):
    GY-521 SCL --> ESP32 GPIO 22
    GY-521 SDA --> ESP32 GPIO 21
    GY-521 VCC --> ESP32 3V3
    GY-521 GND --> ESP32 GND

  ------------------------------------------------------------------------
  DOS CAMBIOS RESPECTO DE LA VERSION ESP8266

  1. Serial por USB VUELVE A ESTAR DISPONIBLE.
     La version anterior no podia usarlo: TX/RX estaban reservados para el
     enlace UART con el Arduino Nano. Con un solo micro ese enlace no
     existe, asi que este sketch reporta por USB *y* por WiFi. El USB es
     mas comodo para desarrollo; el AP sigue sirviendo con el robot en
     movimiento.

  2. Muestra la magnitud de aceleracion CRUDA y CALIBRADA en paralelo.
     Los valores de bias/escala se midieron en el Test 1b pero nunca se
     aplicaron a ningun firmware (pendiente abierto en CLAUDE.md). Aca se
     aplican solo para *mostrar* el resultado, sin usarlos para nada mas:
     si la columna calibrada da ~9.81 con el robot quieto, valida esos
     valores in situ y cierra el pendiente. Si no, hay que revisarlos.

  ------------------------------------------------------------------------
  COMO USARLO
    Opcion A (USB):  abrir el Monitor Serie a 115200 baud.
    Opcion B (WiFi): conectarse a la red RobotBalance_Test1
                     y abrir http://192.168.4.1

    Con el robot quieto y apoyado, pulsar "Calibrar giroscopio" en la
    pagina (o mandar 'c' por Serial) y esperar ~5 s.

  ⚠️ NO energizar la etapa de potencia para este test: sin nada
     comandando las entradas del L298N, quedan flotantes. Este test solo
     necesita el riel logico. (Los pull-down de 10k en IN1-IN4 resuelven
     esto de forma permanente, pero todavia no estan instalados.)
  ========================================================================
*/

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// --- Pines (docs/conexiones-registro-pruebas.md seccion 3) ---------------
const uint8_t PIN_SDA = 21;
const uint8_t PIN_SCL = 22;

// --- Access Point ---------------------------------------------------------
const char *AP_SSID = "RobotBalance_Test1";
const char *AP_PASS = "balance2026";

// --- Muestreo -------------------------------------------------------------
const uint16_t SAMPLE_INTERVAL_MS = 20;    // 50 Hz
const uint16_t CALIB_SAMPLES      = 250;   // ~5 s a 50 Hz
const uint16_t SERIAL_REPORT_MS   = 500;   // ritmo legible por USB

/*
  Calibracion del acelerometro medida en el Test 1b (CLAUDE.md).
  Correccion:  valor_real = (crudo - bias) / escala

  Se usan SOLO para mostrar la columna calibrada y verificar que esos
  numeros siguen siendo correctos. No alimentan ningun calculo de control
  todavia -- esa decision sigue abierta.
*/
const float ACC_BIAS[3]  = { 0.265f, 0.050f, 0.560f };  // X, Y, Z  (m/s^2)
const float ACC_SCALE[3] = { 1.005f, 1.000f, 1.015f };  // X, Y, Z  (1.0 = ideal)
const float GRAVITY = 9.80665f;

WebServer server(80);
Adafruit_MPU6050 mpu;

// --- Estado ---------------------------------------------------------------
bool   mpuOk = false;
String mpuError = "";

float gyroBiasX = 0, gyroBiasY = 0, gyroBiasZ = 0;
bool  biasApplied = false;

float ax = 0, ay = 0, az = 0;          // crudo
float axc = 0, ayc = 0, azc = 0;       // calibrado
float gx = 0, gy = 0, gz = 0;
float accMag = 0, accMagCal = 0, pitch = 0, tempC = 0;

struct Stats {
  uint32_t n = 0;
  double   sum = 0, sumSq = 0;
  float    minV = 0, maxV = 0;

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
    return var > 0 ? (float)sqrt(var) : 0.0f;   // var<0 solo por redondeo
  }
};

Stats statMag, statMagCal, statGx, statGy, statGz;

// Calibracion no bloqueante: el servidor tiene que seguir respondiendo
bool     calibrating = false;
uint16_t calibCount  = 0;
double   calibSumX = 0, calibSumY = 0, calibSumZ = 0;

uint32_t lastSample = 0;
uint32_t lastSerial = 0;

/*
  Convencion de ejes CONFIRMADA en el Test 1 + 1b (CLAUDE.md):
    Z = arriba, Y = inclinacion/pitch (frente-atras), X = eje de las ruedas.
  Verificada por acelerometro (Z, Y) y por rotacion pura sobre X
  (pico 0.571 rad/s en X vs 0.137/0.041 en Z/Y -> razon 4.2x).
  Esta formula ya refleja esa convencion; no requiere cambios.
*/
float computePitch(float x, float y, float z) {
  return atan2f(y, sqrtf(x * x + z * z)) * 180.0f / PI;
}

void startGyroCalibration() {
  if (!mpuOk || calibrating) return;
  calibrating = true;
  calibCount  = 0;
  calibSumX = calibSumY = calibSumZ = 0;
  Serial.println(F("[INFO] Calibrando giroscopio... mantener el robot QUIETO ~5 s"));
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
<title>Test 1 ESP32 - MPU6050</title>
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
</style>
</head>
<body>
  <h1>Test 1 — MPU6050 (GY-521) · ESP32</h1>
  <div class="sub">Reporte por WiFi · muestreo a 50 Hz</div>

  <div class="card">
    <h2>Estado</h2>
    <div id="status" class="big warn">conectando…</div>
    <div class="note" id="statusNote"></div>
  </div>

  <div class="card">
    <h2>Chequeo de sanidad</h2>
    <table>
      <tr><td class="lbl">|Aceleración| cruda</td><td class="val big" id="mag">–</td></tr>
      <tr><td class="lbl">|Aceleración| calibrada</td><td class="val big" id="magCal">–</td></tr>
    </table>
    <div class="note">Ambas deben dar ~9.81 m/s² en reposo, sin importar la orientación.
      La <b>calibrada</b> aplica el bias/escala medidos en el Test 1b — si esa fila da
      ~9.81 y la cruda no, valida esos valores. Si tampoco da, hay que revisarlos.</div>
  </div>

  <div class="card">
    <h2>Pitch estimado</h2>
    <table>
      <tr><td class="lbl">Inclinación</td><td class="val big" id="pitch">–</td></tr>
    </table>
    <div class="note">Convención confirmada: Z arriba, Y frente (pitch),
      X eje de las ruedas.</div>
  </div>

  <div class="card">
    <h2>Acelerómetro (m/s²)</h2>
    <table>
      <tr><td class="lbl"></td><td class="num">crudo</td><td class="num">calibrado</td></tr>
      <tr><td class="lbl">X</td><td class="num" id="ax">–</td><td class="num" id="axc">–</td></tr>
      <tr><td class="lbl">Y</td><td class="num" id="ay">–</td><td class="num" id="ayc">–</td></tr>
      <tr><td class="lbl">Z</td><td class="num" id="az">–</td><td class="num" id="azc">–</td></tr>
    </table>
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
    <div class="note">Mantener el robot <b>quieto y apoyado</b> los ~5 s que dura.
      Con el bias aplicado, los tres ejes deben rondar 0.000 en reposo.</div>
  </div>

  <div class="card">
    <h2>Estabilidad <span id="nsamp" style="color:#666;font-weight:400"></span></h2>
    <table>
      <tr><td class="lbl"></td><td class="num">min</td><td class="num">máx</td><td class="num">σ</td></tr>
      <tr><td class="lbl">|Acel| cruda</td><td class="num" id="magMin">–</td><td class="num" id="magMax">–</td><td class="num" id="magSd">–</td></tr>
      <tr><td class="lbl">|Acel| calib.</td><td class="num" id="magcMin">–</td><td class="num" id="magcMax">–</td><td class="num" id="magcSd">–</td></tr>
      <tr><td class="lbl">Giro X</td><td class="num" id="gxMin">–</td><td class="num" id="gxMax">–</td><td class="num" id="gxSd">–</td></tr>
      <tr><td class="lbl">Giro Y</td><td class="num" id="gyMin">–</td><td class="num" id="gyMax">–</td><td class="num" id="gySd">–</td></tr>
      <tr><td class="lbl">Giro Z</td><td class="num" id="gzMin">–</td><td class="num" id="gzMax">–</td><td class="num" id="gzSd">–</td></tr>
    </table>
    <button class="sec" onclick="resetStats()">Reiniciar estadísticas</button>
    <div class="note">σ mide el ruido real del sensor. Sirve como línea de base: si
      más adelante crece al encender los motores, el ruido es de alimentación o de
      escobillas, no del sensor. Comparar contra los valores del ESP8266 permite ver
      si el micro nuevo introduce más o menos ruido.</div>
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

    document.getElementById('mag').textContent    = f(j.mag) + ' m/s²';
    document.getElementById('magCal').textContent = f(j.magCal) + ' m/s²';
    document.getElementById('pitch').textContent  = f(j.pitch, 1) + '°';

    document.getElementById('ax').textContent  = f(j.ax);
    document.getElementById('ay').textContent  = f(j.ay);
    document.getElementById('az').textContent  = f(j.az);
    document.getElementById('axc').textContent = f(j.axc);
    document.getElementById('ayc').textContent = f(j.ayc);
    document.getElementById('azc').textContent = f(j.azc);

    document.getElementById('gx').textContent = f(j.gx, 3);
    document.getElementById('gy').textContent = f(j.gy, 3);
    document.getElementById('gz').textContent = f(j.gz, 3);
    document.getElementById('temp').textContent = f(j.temp, 1) + ' °C';

    document.getElementById('bias').textContent = j.biasApplied
      ? `X ${f(j.bx,4)} · Y ${f(j.by,4)} · Z ${f(j.bz,4)}` : 'no';

    document.getElementById('nsamp').textContent = '· ' + j.n + ' muestras';
    const s = j.stats;
    document.getElementById('magMin').textContent  = f(s.magMin);
    document.getElementById('magMax').textContent  = f(s.magMax);
    document.getElementById('magSd').textContent   = f(s.magSd, 3);
    document.getElementById('magcMin').textContent = f(s.magcMin);
    document.getElementById('magcMax').textContent = f(s.magcMax);
    document.getElementById('magcSd').textContent  = f(s.magcSd, 3);
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
    setStatus('SIN CONEXIÓN', 'bad', 'El ESP32 no responde. ¿Se reinició?');
  }
}

const calibrate  = () => fetch('/calibrate');
const resetStats = () => fetch('/reset-stats');

setInterval(tick, 250);
tick();
</script>
</body>
</html>
)rawliteral";

// ========================================================================
// Endpoints
// ========================================================================
void handleRoot() { server.send_P(200, "text/html", HTML_PAGE); }

void handleData() {
  String j = "{";
  j += "\"mpuOk\":";           j += (mpuOk ? "true" : "false");
  j += ",\"error\":\"";        j += mpuError; j += "\"";
  j += ",\"calibrating\":";    j += (calibrating ? "true" : "false");
  j += ",\"calibProgress\":";  j += String((calibCount * 100) / CALIB_SAMPLES);
  j += ",\"biasApplied\":";    j += (biasApplied ? "true" : "false");

  j += ",\"ax\":"  + String(ax, 3)  + ",\"ay\":"  + String(ay, 3)  + ",\"az\":"  + String(az, 3);
  j += ",\"axc\":" + String(axc, 3) + ",\"ayc\":" + String(ayc, 3) + ",\"azc\":" + String(azc, 3);
  j += ",\"gx\":"  + String(gx, 4)  + ",\"gy\":"  + String(gy, 4)  + ",\"gz\":"  + String(gz, 4);
  j += ",\"mag\":" + String(accMag, 3) + ",\"magCal\":" + String(accMagCal, 3);
  j += ",\"pitch\":" + String(pitch, 2) + ",\"temp\":" + String(tempC, 1);

  j += ",\"bx\":" + String(gyroBiasX, 5);
  j += ",\"by\":" + String(gyroBiasY, 5);
  j += ",\"bz\":" + String(gyroBiasZ, 5);

  j += ",\"n\":" + String(statMag.n);
  j += ",\"stats\":{";
  j += "\"magMin\":"  + String(statMag.minV, 3)    + ",\"magMax\":"  + String(statMag.maxV, 3)    + ",\"magSd\":"  + String(statMag.stddev(), 4);
  j += ",\"magcMin\":" + String(statMagCal.minV, 3) + ",\"magcMax\":" + String(statMagCal.maxV, 3) + ",\"magcSd\":" + String(statMagCal.stddev(), 4);
  j += ",\"gxMin\":"  + String(statGx.minV, 4)     + ",\"gxMax\":"  + String(statGx.maxV, 4)     + ",\"gxSd\":"  + String(statGx.stddev(), 5);
  j += ",\"gyMin\":"  + String(statGy.minV, 4)     + ",\"gyMax\":"  + String(statGy.maxV, 4)     + ",\"gySd\":"  + String(statGy.stddev(), 5);
  j += ",\"gzMin\":"  + String(statGz.minV, 4)     + ",\"gzMax\":"  + String(statGz.maxV, 4)     + ",\"gzSd\":"  + String(statGz.stddev(), 5);
  j += "}}";

  server.send(200, "application/json", j);
}

void handleCalibrate()  { startGyroCalibration(); server.send(200, "text/plain", "ok"); }
void handleResetStats() {
  statMag.reset(); statMagCal.reset();
  statGx.reset();  statGy.reset(); statGz.reset();
  server.send(200, "text/plain", "ok");
}

// ========================================================================
void setup() {
  Serial.begin(115200);
  delay(300);                       // margen para que el USB enumere

  Serial.println();
  Serial.println(F("========================================"));
  Serial.println(F(" TEST 1 (ESP32) - MPU6050 / GY-521"));
  Serial.println(F("========================================"));

  Wire.begin(PIN_SDA, PIN_SCL);

  /*
    Si el sensor falla NO se detiene la ejecucion. Un while(1) dejaria sin
    AP y sin pagina, es decir sin ninguna via de diagnostico. El error se
    muestra en la pagina y por Serial, y el servidor sigue vivo.
  */
  if (mpu.begin()) {
    mpuOk = true;
    /*
      Acelerometro +-2G: la aceleracion util ronda 1G; el rango mas chico
        da la mejor resolucion.
      Giroscopio +-250 deg/s: suficiente cerca de la vertical. Subir a
        +-500 si se ve saturacion durante el balanceo.
      Pasabajos interno 21 Hz: corta ruido de escobillas sin agregar
        demasiado retardo de fase al futuro lazo de control.
    */
    mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
    mpu.setGyroRange(MPU6050_RANGE_250_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    Serial.println(F("[OK]   MPU6050 inicializado en 0x68"));
  } else {
    mpuOk = false;
    mpuError = "MPU6050 no responde en 0x68. Revisar: (1) 3.3V en el modulo, "
               "(2) SDA=GPIO21 y SCL=GPIO22 sin invertir, (3) GND comun.";
    Serial.println(F("[ERROR] MPU6050 no responde en 0x68."));
    Serial.println(F("        1. 3.3V en el modulo"));
    Serial.println(F("        2. SDA=GPIO21, SCL=GPIO22 sin invertir"));
    Serial.println(F("        3. GND comun"));
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print(F("[INFO] AP \""));  Serial.print(AP_SSID);
  Serial.print(F("\" en http://"));  Serial.println(WiFi.softAPIP());

  server.on("/",            handleRoot);
  server.on("/data",        handleData);
  server.on("/calibrate",   handleCalibrate);
  server.on("/reset-stats", handleResetStats);
  server.begin();

  Serial.println(F("[INFO] Mandar 'c' por Serial para calibrar el giroscopio."));
  Serial.println();

  lastSample = millis();
  lastSerial = millis();
}

void loop() {
  server.handleClient();

  // Comando por USB: 'c' dispara la calibracion, igual que el boton web.
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'c' || c == 'C') startGyroCalibration();
  }

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

  // Correccion del Test 1b: (crudo - bias) / escala. Solo para mostrar.
  axc = (ax - ACC_BIAS[0]) / ACC_SCALE[0];
  ayc = (ay - ACC_BIAS[1]) / ACC_SCALE[1];
  azc = (az - ACC_BIAS[2]) / ACC_SCALE[2];

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

      Serial.print(F("[OK]   Bias del giro (rad/s): X="));
      Serial.print(gyroBiasX, 5); Serial.print(F("  Y="));
      Serial.print(gyroBiasY, 5); Serial.print(F("  Z="));
      Serial.println(gyroBiasZ, 5);
      Serial.println(F("       Tipico por debajo de |0.05|. Mucho mayor sugiere"));
      Serial.println(F("       que el robot se movio durante la calibracion."));
    }
  }

  gx = g.gyro.x - gyroBiasX;
  gy = g.gyro.y - gyroBiasY;
  gz = g.gyro.z - gyroBiasZ;

  /*
    Magnitud del vector aceleracion: el mejor chequeo de sanidad que hay
    para este sensor. En reposo debe dar ~9.81 m/s^2 sin importar la
    orientacion del modulo -- es una propiedad fisica, no depende del
    montaje. Si da algo muy distinto, el problema es el sensor o el bus.
  */
  accMag    = sqrtf(ax * ax + ay * ay + az * az);
  accMagCal = sqrtf(axc * axc + ayc * ayc + azc * azc);
  pitch     = computePitch(ax, ay, az);

  statMag.add(accMag);
  statMagCal.add(accMagCal);
  statGx.add(gx);
  statGy.add(gy);
  statGz.add(gz);

  if (now - lastSerial >= SERIAL_REPORT_MS) {
    lastSerial = now;
    Serial.print(F("Acel["));
    Serial.print(ax, 2); Serial.print(F(", "));
    Serial.print(ay, 2); Serial.print(F(", "));
    Serial.print(az, 2); Serial.print(F("]  |a|="));
    Serial.print(accMag, 2);
    Serial.print(F(" cal="));  Serial.print(accMagCal, 2);
    Serial.print(F("  Giro["));
    Serial.print(gx, 3); Serial.print(F(", "));
    Serial.print(gy, 3); Serial.print(F(", "));
    Serial.print(gz, 3); Serial.print(F("]  pitch="));
    Serial.print(pitch, 1); Serial.print(F("deg  T="));
    Serial.print(tempC, 1); Serial.println(F("C"));
  }
}
