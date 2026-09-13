/*
  ========================================================================
  TEST 1b — Calibración de acelerómetro + verificación del eje X (giro)
  ========================================================================

  Continuación del Test 1. Dos procedimientos independientes, cada uno
  con su propio botón en la página:

  A) CALIBRACION DE ACELEROMETRO (offset + escala por eje)
     El Test 1 mostró |Aceleracion| = 10.51 m/s^2 en reposo (deberia ser
     9.80665), estable y no atribuible al montaje porque la magnitud del
     vector aceleracion NO depende de la orientacion del sensor. Este
     test lo diagnostica con el metodo estandar de 6 orientaciones:
     capturar cada eje apuntando primero contra la gravedad y despues a
     favor. Para un eje bien calibrado:
        lectura_arriba  ~= +9.80665 + bias
        lectura_abajo   ~= -9.80665 + bias
     De ahi:
        bias  = (lectura_arriba + lectura_abajo) / 2
        escala = (lectura_arriba - lectura_abajo) / (2 * 9.80665)
     Un escala != 1.0 en TODOS los ejes por igual sugiere un error de
     ganancia global (ej. registro de rango mal interpretado). Un escala
     distinta POR eje sugiere desalineación o problema especifico de ese
     eje. Los 6 valores de magnitud capturados tambien quedan a la vista:
     si son todos parecidos entre si (aunque distintos de 9.80665), es
     evidencia adicional de un factor de escala uniforme.

  B) PUREZA DEL EJE X (velocidad angular)
     El Test 1 confirmo Z=arriba, Y=inclinacion (pitch) por la lectura
     del acelerometro en reposo. Falta confirmar que X es el eje de las
     ruedas, y eso el acelerometro no lo puede decir (girar sobre el eje
     de las ruedas no cambia la lectura de gravedad). Se verifica con el
     giroscopio: rotar el robot SOLO sobre el eje de las ruedas durante
     una ventana de tiempo fija y comparar los picos de |gx|, |gy|, |gz|.
     Si X domina claramente sobre Y y Z, el eje esta confirmado. Fuga
     apreciable hacia Y/Z indica que la rotacion no fue puramente sobre
     ese eje, o que hay que revisar la convencion asumida.

  CABLEADO: identico al Test 1 (docs/conexiones-registro-pruebas.md, seccion 3).
  REPORTE: por WiFi, igual que el Test 1 — no usa Serial, GPIO1/GPIO3 libres.

  ⚠️ Igual que el Test 1: alimentar solo el riel logico, no energizar la
     etapa de potencia (entradas del L298N flotantes sin el Nano activo).

  ⚠️ NOTA SOBRE EL MONTAJE FISICO: si el GY-521 esta fijo al chasis y no
     se puede orientar el chasis completo en las 6 posiciones limpiamente
     (ej. por los cables o la estructura de 3 niveles), puede ser necesario
     desconectar momentaneamente el modulo y sostenerlo a mano en cada
     orientacion, bien quieto, durante la captura (~1s por captura). Si se
     hace asi, verificar despues que la reconexion mantiene el mismo
     mapeo SDA/SCL antes de dar la calibracion por buena.
  ========================================================================
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

const char *AP_SSID = "RobotBalance_Test1b";
const char *AP_PASS = "balance2026";

const float GRAVITY = 9.80665f;

const uint16_t SAMPLE_INTERVAL_MS = 20;   // 50 Hz para el reporte en vivo
const uint16_t CALIB_GYRO_SAMPLES = 250;  // ~5 s, igual que el Test 1
const uint16_t CAPTURE_SAMPLES    = 40;   // ~0.8 s por captura de acelerometro
const uint32_t GYRO_TEST_MS       = 5000; // ventana de la prueba de eje X

ESP8266WebServer server(80);
Adafruit_MPU6050 mpu;

bool   mpuOk = false;
String mpuError = "";

// --- Lectura en vivo -------------------------------------------------------
float ax=0, ay=0, az=0, accMag=0;
float gx=0, gy=0, gz=0;
float tempC = 0;
uint32_t lastSample = 0;

// --- Calibracion de bias del giro (igual metodo que Test 1) ---------------
float gyroBiasX=0, gyroBiasY=0, gyroBiasZ=0;
bool  gyroBiasApplied = false;
bool  gyroCalibrating = false;
uint16_t gyroCalibCount = 0;
double gyroCalibSumX=0, gyroCalibSumY=0, gyroCalibSumZ=0;

// --- Calibracion de acelerometro (6 orientaciones) -------------------------
// Orden fijo: 0=Z arriba,1=Z abajo,2=Y arriba,3=Y abajo,4=X arriba,5=X abajo
struct CalibSlot {
  const char* label;
  bool  captured;
  float ax, ay, az, mag;
};

CalibSlot slots[6] = {
  {"Z arriba (normal)", false, 0,0,0,0},
  {"Z abajo (invertido)", false, 0,0,0,0},
  {"Y arriba", false, 0,0,0,0},
  {"Y abajo", false, 0,0,0,0},
  {"X arriba", false, 0,0,0,0},
  {"X abajo", false, 0,0,0,0},
};

bool  accelCalibDone = false;
float biasAx=0, scaleAx=0, biasAy=0, scaleAy=0, biasAz=0, scaleAz=0;

bool allSlotsCaptured() {
  for (uint8_t i = 0; i < 6; i++) if (!slots[i].captured) return false;
  return true;
}

// Captura bloqueante corta (~0.8s). Aceptable: es una herramienta de
// diagnóstico manual disparada por el usuario, no el firmware final.
void captureSlot(uint8_t idx) {
  if (!mpuOk || idx >= 6) return;

  double sx=0, sy=0, sz=0;
  for (uint16_t i = 0; i < CAPTURE_SAMPLES; i++) {
    sensors_event_t a, g, t;
    mpu.getEvent(&a, &g, &t);
    sx += a.acceleration.x;
    sy += a.acceleration.y;
    sz += a.acceleration.z;
    delay(20);
    yield(); // evita el watchdog del ESP8266 durante el bucle bloqueante
  }

  slots[idx].ax = (float)(sx / CAPTURE_SAMPLES);
  slots[idx].ay = (float)(sy / CAPTURE_SAMPLES);
  slots[idx].az = (float)(sz / CAPTURE_SAMPLES);
  slots[idx].mag = sqrtf(slots[idx].ax*slots[idx].ax +
                          slots[idx].ay*slots[idx].ay +
                          slots[idx].az*slots[idx].az);
  slots[idx].captured = true;

  accelCalibDone = false; // hay que recalcular
}

void computeAccelCalib() {
  // X: slot 4 (arriba) vs slot 5 (abajo)
  biasAx  = (slots[4].ax + slots[5].ax) / 2.0f;
  scaleAx = (slots[4].ax - slots[5].ax) / (2.0f * GRAVITY);
  // Y: slot 2 vs slot 3
  biasAy  = (slots[2].ay + slots[3].ay) / 2.0f;
  scaleAy = (slots[2].ay - slots[3].ay) / (2.0f * GRAVITY);
  // Z: slot 0 vs slot 1
  biasAz  = (slots[0].az + slots[1].az) / 2.0f;
  scaleAz = (slots[0].az - slots[1].az) / (2.0f * GRAVITY);

  accelCalibDone = true;
}

void resetAccelCalib() {
  for (uint8_t i = 0; i < 6; i++) slots[i].captured = false;
  accelCalibDone = false;
}

// --- Prueba de pureza del eje X (giro) --------------------------------------
bool     gyroTestRunning = false;
bool     gyroTestDone    = false;
uint32_t gyroTestStart   = 0;
float    peakGx=0, peakGy=0, peakGz=0;

void startGyroTest() {
  gyroTestRunning = true;
  gyroTestDone    = false;
  gyroTestStart   = millis();
  peakGx = peakGy = peakGz = 0;
}

// ========================================================================
// Página
// ========================================================================
const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Test 1b - Calibracion MPU</title>
<style>
  :root { color-scheme: dark; }
  body { font-family: -apple-system, system-ui, sans-serif; background:#111; color:#eee;
         margin:0; padding:16px; max-width:760px; margin-inline:auto; }
  h1 { font-size:1.1rem; color:#00e676; margin:0 0 4px; }
  h3 { font-size:.95rem; margin:0 0 8px; color:#ddd; }
  .sub { font-size:.8rem; color:#888; margin-bottom:16px; }
  .card { background:#1c1c1c; border:1px solid #2c2c2c; border-radius:10px;
          padding:14px; margin-bottom:12px; }
  .card h2 { font-size:.75rem; text-transform:uppercase; letter-spacing:.08em;
             color:#888; margin:0 0 10px; font-weight:600; }
  table { width:100%; border-collapse:collapse; font-variant-numeric:tabular-nums; }
  td { padding:5px 4px; border-bottom:1px solid #262626; font-size:.85rem; }
  tr:last-child td { border-bottom:none; }
  td.lbl { color:#999; }
  td.val { text-align:right; font-weight:600; color:#4fc3f7; }
  .ok { color:#00e676 !important; } .warn { color:#ffb300 !important; } .bad { color:#ff5252 !important; }
  button { background:#00e676; color:#062; border:0; border-radius:8px;
           padding:9px 14px; font-weight:700; font-size:.82rem; cursor:pointer;
           margin:3px 4px 3px 0; }
  button.sec { background:#333; color:#ddd; }
  button:disabled { opacity:.4; cursor:default; }
  .grid { display:grid; grid-template-columns:1fr 1fr; gap:6px; }
  .note { font-size:.75rem; color:#777; margin-top:10px; line-height:1.5; }
  .pill { display:inline-block; padding:2px 8px; border-radius:20px; font-size:.72rem; font-weight:700; }
  .pill.on { background:#0e3b1f; color:#00e676; }
  .pill.off { background:#2a2a2a; color:#888; }
</style>
</head>
<body>
  <h1>Test 1b — Calibración MPU6050</h1>
  <div class="sub">Continuación del Test 1 · reporte por WiFi</div>

  <div class="card">
    <h2>Estado del sensor</h2>
    <div id="status">–</div>
  </div>

  <div class="card">
    <h2>Lectura en vivo</h2>
    <table>
      <tr><td class="lbl">Acel X / Y / Z</td>
          <td class="val" id="liveAcc">–</td></tr>
      <tr><td class="lbl">|Aceleración|</td>
          <td class="val" id="liveMag">–</td></tr>
      <tr><td class="lbl">Giro X / Y / Z</td>
          <td class="val" id="liveGyro">–</td></tr>
    </table>
  </div>

  <div class="card">
    <h3>A) Bias del giroscopio</h3>
    <button id="calBtn" onclick="calibGyro()">Calibrar giroscopio</button>
    <span id="gyroBiasTxt" class="note"></span>
    <div class="note">Igual que en el Test 1. Recomendado antes de la prueba de
      eje X, para no confundir bias con rotación real.</div>
  </div>

  <div class="card">
    <h3>B) Calibración de acelerómetro (offset + escala)</h3>
    <div class="note">Orientar el eje indicado <b>contra la gravedad</b> (apuntando
      hacia arriba), mantener quieto y presionar el botón correspondiente. Repetir
      para los 6.</div>
    <div class="grid" id="slotButtons"></div>
    <table id="slotTable" style="margin-top:8px"></table>
    <button class="sec" onclick="resetCalib()">Reiniciar capturas</button>
    <div id="calibResult" class="note"></div>
  </div>

  <div class="card">
    <h3>C) Pureza del eje X (rotación)</h3>
    <div class="note">Al presionar Iniciar, rotar el robot durante 5 s <b>solo
      sobre el eje de las ruedas (X)</b>, sin inclinarlo en otro sentido.</div>
    <button id="gyroTestBtn" onclick="startGyroTest()">Iniciar prueba (5 s)</button>
    <span id="gyroTestStatus" class="note"></span>
    <table id="gyroTestTable" style="margin-top:8px"></table>
    <div class="note" id="gyroTestVerdict"></div>
  </div>

<script>
const f = (v, d=2) => (v===null||v===undefined) ? '–' : Number(v).toFixed(d);

// --- Botones de captura, generados una vez ---
const slotBtnDiv = document.getElementById('slotButtons');
for (let i=0;i<6;i++) {
  const b = document.createElement('button');
  b.id = 'slotBtn'+i;
  b.textContent = 'Capturar';
  b.onclick = () => capture(i);
  slotBtnDiv.appendChild(b);
}

const capture   = (i) => fetch('/capture?slot='+i).then(tick);
const calibGyro = ()  => fetch('/calibrate-gyro');
const resetCalib = () => fetch('/reset-calib').then(tick);
const startGyroTest = () => fetch('/gyro-test-start');

async function tick() {
  try {
    const r = await fetch('/data');
    const j = await r.json();

    if (!j.mpuOk) {
      document.getElementById('status').innerHTML = '<b class="bad">SENSOR NO RESPONDE</b><br>'+j.error;
    } else {
      document.getElementById('status').innerHTML = '<b class="ok">OK</b> — sensor respondiendo';
    }
    document.getElementById('calBtn').disabled = j.gyroCalibrating || !j.mpuOk;

    document.getElementById('liveAcc').textContent =
      f(j.ax)+' / '+f(j.ay)+' / '+f(j.az)+' m/s²';
    document.getElementById('liveMag').textContent = f(j.mag)+' m/s²';
    document.getElementById('liveGyro').textContent =
      f(j.gx,3)+' / '+f(j.gy,3)+' / '+f(j.gz,3)+' rad/s';

    document.getElementById('gyroBiasTxt').textContent = j.gyroCalibrating
      ? ('calibrando… ' + j.gyroCalibProgress + '%')
      : (j.gyroBiasApplied ? ('bias: X '+f(j.bx,4)+' · Y '+f(j.by,4)+' · Z '+f(j.bz,4)) : 'sin calibrar');

    // --- Tabla de slots de calibración ---
    let rows = '';
    j.slots.forEach((s, i) => {
      document.getElementById('slotBtn'+i).textContent = s.captured ? '✓ '+s.label : s.label;
      document.getElementById('slotBtn'+i).className = s.captured ? 'ok' : '';
      if (s.captured) {
        rows += '<tr><td class="lbl">'+s.label+'</td>'+
                '<td class="val">'+f(s.ax)+' / '+f(s.ay)+' / '+f(s.az)+
                '  (|a|='+f(s.mag)+')</td></tr>';
      }
    });
    document.getElementById('slotTable').innerHTML = rows;

    if (j.accelCalibDone) {
      document.getElementById('calibResult').innerHTML =
        '<b>Resultado</b><br>'+
        'Bias (m/s²): X '+f(j.biasAx,3)+' · Y '+f(j.biasAy,3)+' · Z '+f(j.biasAz,3)+'<br>'+
        'Escala (1.0 = ideal): X '+f(j.scaleAx,3)+' · Y '+f(j.scaleAy,3)+' · Z '+f(j.scaleAz,3)+'<br>'+
        '<span class="'+(Math.abs(j.scaleAx-1)>0.1||Math.abs(j.scaleAy-1)>0.1||Math.abs(j.scaleAz-1)>0.1?'warn':'ok')+'">'+
        (Math.abs(j.scaleAx-1)>0.1||Math.abs(j.scaleAy-1)>0.1||Math.abs(j.scaleAz-1)>0.1
          ? 'Escala fuera de ±10% en al menos un eje — comparar si es similar en los tres (error global) o distinto (error de eje).'
          : 'Escala dentro de ±10% en los tres ejes.')+'</span>';
    } else {
      document.getElementById('calibResult').textContent =
        'Faltan capturas: ' + j.slots.filter(s=>!s.captured).length + ' de 6.';
    }

    // --- Prueba de eje X ---
    if (j.gyroTestRunning) {
      document.getElementById('gyroTestBtn').disabled = true;
      document.getElementById('gyroTestStatus').textContent =
        'rotando… ' + j.gyroTestRemainingMs + ' ms restantes';
    } else {
      document.getElementById('gyroTestBtn').disabled = !j.mpuOk;
      document.getElementById('gyroTestStatus').textContent = j.gyroTestDone ? 'completa' : '';
    }

    if (j.gyroTestDone) {
      document.getElementById('gyroTestTable').innerHTML =
        '<tr><td class="lbl">Pico |Giro X|</td><td class="val">'+f(j.peakGx,3)+' rad/s</td></tr>'+
        '<tr><td class="lbl">Pico |Giro Y|</td><td class="val">'+f(j.peakGy,3)+' rad/s</td></tr>'+
        '<tr><td class="lbl">Pico |Giro Z|</td><td class="val">'+f(j.peakGz,3)+' rad/s</td></tr>';

      const maxCross = Math.max(j.peakGy, j.peakGz);
      const ratio = maxCross > 0.001 ? (j.peakGx / maxCross) : 999;
      const verdict = document.getElementById('gyroTestVerdict');
      if (j.peakGx < 0.2) {
        verdict.innerHTML = '<span class="warn">Pico de X muy bajo — ¿hubo rotación real durante la ventana?</span>';
      } else if (ratio > 3) {
        verdict.innerHTML = '<span class="ok">X domina claramente sobre Y/Z (razón '+f(ratio,1)+'x). Eje confirmado como eje de las ruedas.</span>';
      } else {
        verdict.innerHTML = '<span class="warn">Fuga apreciable hacia Y/Z (razón '+f(ratio,1)+'x). Repetir cuidando rotar solo sobre X, o revisar la convención asumida.</span>';
      }
    } else {
      document.getElementById('gyroTestTable').innerHTML = '';
      document.getElementById('gyroTestVerdict').textContent = '';
    }

  } catch (e) {
    document.getElementById('status').innerHTML = '<b class="bad">SIN CONEXIÓN</b>';
  }
}

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

  j += ",\"ax\":" + String(ax,3) + ",\"ay\":" + String(ay,3) + ",\"az\":" + String(az,3);
  j += ",\"mag\":" + String(accMag,3);
  j += ",\"gx\":" + String(gx,4) + ",\"gy\":" + String(gy,4) + ",\"gz\":" + String(gz,4);

  j += ",\"gyroCalibrating\":"; j += (gyroCalibrating ? "true" : "false");
  j += ",\"gyroCalibProgress\":" + String((gyroCalibCount * 100) / CALIB_GYRO_SAMPLES);
  j += ",\"gyroBiasApplied\":"; j += (gyroBiasApplied ? "true" : "false");
  j += ",\"bx\":" + String(gyroBiasX,5) + ",\"by\":" + String(gyroBiasY,5) + ",\"bz\":" + String(gyroBiasZ,5);

  j += ",\"slots\":[";
  for (uint8_t i = 0; i < 6; i++) {
    if (i) j += ",";
    j += "{\"label\":\"" + String(slots[i].label) + "\"";
    j += ",\"captured\":"; j += (slots[i].captured ? "true" : "false");
    j += ",\"ax\":" + String(slots[i].ax,3) + ",\"ay\":" + String(slots[i].ay,3);
    j += ",\"az\":" + String(slots[i].az,3) + ",\"mag\":" + String(slots[i].mag,3) + "}";
  }
  j += "]";

  j += ",\"accelCalibDone\":"; j += (accelCalibDone ? "true" : "false");
  j += ",\"biasAx\":" + String(biasAx,4) + ",\"scaleAx\":" + String(scaleAx,4);
  j += ",\"biasAy\":" + String(biasAy,4) + ",\"scaleAy\":" + String(scaleAy,4);
  j += ",\"biasAz\":" + String(biasAz,4) + ",\"scaleAz\":" + String(scaleAz,4);

  j += ",\"gyroTestRunning\":"; j += (gyroTestRunning ? "true" : "false");
  j += ",\"gyroTestDone\":"; j += (gyroTestDone ? "true" : "false");
  uint32_t remaining = 0;
  if (gyroTestRunning) {
    uint32_t elapsed = millis() - gyroTestStart;
    remaining = (elapsed < GYRO_TEST_MS) ? (GYRO_TEST_MS - elapsed) : 0;
  }
  j += ",\"gyroTestRemainingMs\":" + String(remaining);
  j += ",\"peakGx\":" + String(peakGx,4) + ",\"peakGy\":" + String(peakGy,4) + ",\"peakGz\":" + String(peakGz,4);

  j += "}";
  server.send(200, "application/json", j);
}

void handleCalibrateGyro() {
  if (mpuOk && !gyroCalibrating) {
    gyroCalibrating = true;
    gyroCalibCount = 0;
    gyroCalibSumX = gyroCalibSumY = gyroCalibSumZ = 0;
  }
  server.send(200, "text/plain", "ok");
}

void handleCapture() {
  if (server.hasArg("slot")) {
    int idx = server.arg("slot").toInt();
    if (idx >= 0 && idx < 6) captureSlot((uint8_t)idx);
    if (allSlotsCaptured()) computeAccelCalib();
  }
  server.send(200, "text/plain", "ok");
}

void handleResetCalib() {
  resetAccelCalib();
  server.send(200, "text/plain", "ok");
}

void handleGyroTestStart() {
  if (mpuOk && !gyroTestRunning) startGyroTest();
  server.send(200, "text/plain", "ok");
}

// ========================================================================
void setup() {
  // Sin Serial.begin() a propósito — igual que el Test 1: GPIO1/GPIO3
  // quedan libres para el futuro enlace UART con el Nano.

  Wire.begin(D2, D1); // SDA=D2 (GPIO4), SCL=D1 (GPIO5)

  if (mpu.begin()) {
    mpuOk = true;
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

  server.on("/",               handleRoot);
  server.on("/data",           handleData);
  server.on("/calibrate-gyro", handleCalibrateGyro);
  server.on("/capture",        handleCapture);
  server.on("/reset-calib",    handleResetCalib);
  server.on("/gyro-test-start",handleGyroTestStart);
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
  accMag = sqrtf(ax*ax + ay*ay + az*az);

  if (gyroCalibrating) {
    gyroCalibSumX += g.gyro.x;
    gyroCalibSumY += g.gyro.y;
    gyroCalibSumZ += g.gyro.z;
    gyroCalibCount++;
    if (gyroCalibCount >= CALIB_GYRO_SAMPLES) {
      gyroBiasX = (float)(gyroCalibSumX / CALIB_GYRO_SAMPLES);
      gyroBiasY = (float)(gyroCalibSumY / CALIB_GYRO_SAMPLES);
      gyroBiasZ = (float)(gyroCalibSumZ / CALIB_GYRO_SAMPLES);
      gyroBiasApplied = true;
      gyroCalibrating = false;
    }
  }

  gx = g.gyro.x - gyroBiasX;
  gy = g.gyro.y - gyroBiasY;
  gz = g.gyro.z - gyroBiasZ;

  if (gyroTestRunning) {
    if (fabsf(gx) > peakGx) peakGx = fabsf(gx);
    if (fabsf(gy) > peakGy) peakGy = fabsf(gy);
    if (fabsf(gz) > peakGz) peakGz = fabsf(gz);

    if (now - gyroTestStart >= GYRO_TEST_MS) {
      gyroTestRunning = false;
      gyroTestDone = true;
    }
  }
}
