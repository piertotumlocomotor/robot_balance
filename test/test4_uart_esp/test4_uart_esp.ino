/*
  ========================================================================
  TEST 4 — Enlace UART ESP8266 <-> Arduino Nano (lado ESP8266)
  ========================================================================

  Objetivo: validar que el enlace UART fisico funciona de punta a punta,
  incluido el divisor resistivo en Nano TX -> ESP RX (ver
  docs/conexiones-registro-pruebas.md seccion 4, ya armado y verificado
  por resistencia y por voltaje).

  El ESP manda un PING con un contador cada 1s por Serial (TX=GPIO1,
  RX=GPIO3, UART hardware). El Nano responde con un PONG que incluye el
  mismo numero, y ademas parpadea un LED en su D5 como confirmacion
  visual local (ver sketch del lado Nano).

  Este sketch reporta por WiFi -- no usa Serial para debug, esos mismos
  pines son el enlace bajo prueba.

  CABLEADO:
    ESP8266 TX (GPIO1) --> directo         --> Nano RX (D0)
    ESP8266 RX (GPIO3) <-- divisor 1k/2k   <-- Nano TX (D1)

  BAUDIOS: 9600 -- conservador para esta primera prueba del enlace fisico.
  Subir la tasa es una optimizacion para mas adelante, no el objetivo de
  este test.

  COMO LEER EL RESULTADO:
    - LED del Nano parpadea + tasa de exito alta aca (>80%) => enlace OK
      en ambos sentidos.
    - LED parpadea pero tasa de exito baja/nula => ESP->Nano funciona,
      el problema esta en el regreso Nano->ESP (revisar el divisor).
    - LED nunca parpadea => el problema esta en ESP->Nano (revisar el
      cable directo TX del ESP a RX del Nano).
  ========================================================================
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

const char *AP_SSID = "RobotBalance_Test4";
const char *AP_PASS = "balance2026";

const uint32_t UART_BAUD        = 9600;
const uint16_t PING_INTERVAL_MS = 1000;

ESP8266WebServer server(80);

uint32_t pingCounter    = 0;
uint32_t pingsSent      = 0;
uint32_t pongsReceived  = 0;
uint32_t lastPingMs     = 0;

String lastSent     = "-";
String lastReceived = "-";
String rxBuffer      = "";

// Diagnostico: cuenta CUALQUIER byte que el UART del ESP vea en RX,
// llegue o no a formar una linea reconocible. Distingue "no llega nada"
// de "llega basura".
uint32_t rawByteCount = 0;
uint8_t  lastRawByte  = 0;

void sendPing() {
  pingCounter++;
  String msg = "PING " + String(pingCounter);
  Serial.println(msg);
  lastSent = msg;
  pingsSent++;
}

void checkForPong() {
  while (Serial.available()) {
    char c = Serial.read();
    rawByteCount++;
    lastRawByte = (uint8_t)c;

    if (c == '\n') {
      rxBuffer.trim();
      if (rxBuffer.length() > 0) {
        lastReceived = rxBuffer;
        if (rxBuffer.equals("PONG " + String(pingCounter))) {
          pongsReceived++;
        }
      }
      rxBuffer = "";
    } else if (c != '\r') {
      rxBuffer += c;
      if (rxBuffer.length() > 40) rxBuffer = ""; // basura/ruido, descartar
    }
  }
}

const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Test 4 - UART ESP-Nano</title>
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
  td.lbl { color:#999; width:40%; }
  td.val { text-align:right; font-weight:600; color:#4fc3f7; font-family:monospace; }
  .big { font-size:1.5rem; font-weight:700; }
  .ok { color:#00e676; } .warn { color:#ffb300; } .bad { color:#ff5252; }
  .note { font-size:.75rem; color:#777; margin-top:10px; line-height:1.5; }
</style>
</head>
<body>
  <h1>Test 4 — Enlace UART ESP8266 ↔ Nano</h1>
  <div class="sub">PING/PONG por hardware UART · 9600 baud</div>

  <div class="card">
    <h2>Estado del enlace</h2>
    <div id="status" class="big warn">arrancando…</div>
  </div>

  <div class="card">
    <h2>Último intercambio</h2>
    <table>
      <tr><td class="lbl">Último PING enviado</td><td class="val" id="sent">–</td></tr>
      <tr><td class="lbl">Última respuesta recibida</td><td class="val" id="recv">–</td></tr>
    </table>
  </div>

  <div class="card">
    <h2>Estadísticas</h2>
    <table>
      <tr><td class="lbl">PINGs enviados</td><td class="val" id="sentCount">–</td></tr>
      <tr><td class="lbl">PONGs recibidos (válidos)</td><td class="val" id="recvCount">–</td></tr>
      <tr><td class="lbl">Tasa de éxito</td><td class="val" id="rate">–</td></tr>
    </table>
    <div class="note">Si el LED del Nano (D5) parpadea pero la tasa de éxito acá es
      baja: el problema está en Nano→ESP (revisar el divisor). Si el LED nunca
      parpadea: el problema está en ESP→Nano (cable directo TX-ESP a RX-Nano).</div>
  </div>

  <div class="card">
    <h2>Diagnóstico crudo (RX del ESP)</h2>
    <table>
      <tr><td class="lbl">Bytes crudos recibidos</td><td class="val" id="rawCount">–</td></tr>
      <tr><td class="lbl">Último byte crudo (hex)</td><td class="val" id="rawByte">–</td></tr>
    </table>
    <div class="note">Si "Bytes crudos" queda en 0 aunque el LED del Nano parpadee:
      no llega nada físicamente al RX del ESP — problema eléctrico, no de datos. Si
      sube pero nunca hay un PONG válido: llega algo, pero corrupto o con formato
      distinto al esperado — más probable un problema de baudios/timing.</div>
  </div>

<script>
async function tick() {
  try {
    const r = await fetch('/data');
    const j = await r.json();
    document.getElementById('sent').textContent = j.lastSent;
    document.getElementById('recv').textContent = j.lastReceived;
    document.getElementById('sentCount').textContent = j.pingsSent;
    document.getElementById('recvCount').textContent = j.pongsReceived;
    const rate = j.pingsSent > 0 ? (100*j.pongsReceived/j.pingsSent).toFixed(0)+'%' : '–';
    document.getElementById('rate').textContent = rate;
    document.getElementById('rawCount').textContent = j.rawByteCount;
    document.getElementById('rawByte').textContent = '0x' + j.lastRawByte.toString(16).toUpperCase().padStart(2,'0');

    const st = document.getElementById('status');
    if (j.pingsSent < 3) {
      st.textContent = 'arrancando…'; st.className = 'big warn';
    } else if (j.pongsReceived / j.pingsSent > 0.8) {
      st.textContent = 'OK'; st.className = 'big ok';
    } else if (j.pongsReceived > 0) {
      st.textContent = 'INESTABLE'; st.className = 'big warn';
    } else {
      st.textContent = 'SIN RESPUESTA'; st.className = 'big bad';
    }
  } catch (e) {}
}
setInterval(tick, 500);
tick();
</script>
</body>
</html>
)rawliteral";

void handleRoot() { server.send_P(200, "text/html", HTML_PAGE); }

void handleData() {
  String j = "{";
  j += "\"lastSent\":\"" + lastSent + "\",";
  j += "\"lastReceived\":\"" + lastReceived + "\",";
  j += "\"pingsSent\":" + String(pingsSent) + ",";
  j += "\"pongsReceived\":" + String(pongsReceived) + ",";
  j += "\"rawByteCount\":" + String(rawByteCount) + ",";
  j += "\"lastRawByte\":" + String(lastRawByte);
  j += "}";
  server.send(200, "application/json", j);
}

void setup() {
  // Sin debug por USB a proposito: TX/RX (GPIO1/GPIO3) son el enlace
  // bajo prueba. Todo el reporte va por WiFi.
  Serial.begin(UART_BAUD);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();

  lastPingMs = millis();
}

void loop() {
  server.handleClient();
  checkForPong();

  uint32_t now = millis();
  if (now - lastPingMs >= PING_INTERVAL_MS) {
    lastPingMs = now;
    sendPing();
  }
}
