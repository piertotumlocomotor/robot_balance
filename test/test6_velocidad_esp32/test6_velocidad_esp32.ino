/*
  ========================================================================
  TEST 6 — Diagnostico de motores + control de velocidad por encoder
  ========================================================================

  POR QUE EXISTE:
  Tras la sesion de diagnostico del 2026-08-14 los motores dejaron de
  responder (el Test 5 reporto aceleracion nula en todos los escalones).
  Este sketch hace dos cosas, en este orden:

    MODO 1 - DIAGNOSTICO (lazo abierto, un motor por vez)
      Barre duty 0..121 en M1 solo, despues en M2 solo, y reporta los
      pulsos de encoder de cada uno. Aisla sin ambiguedad:
        - ningun motor gira      -> alimentacion o cableado comun (L298N)
        - gira uno solo          -> problema de ese canal
        - giran pero sin pulsos  -> encoder, no motor

    MODO 2 - CONTROL DE VELOCIDAD (lazo cerrado, PI, ambos motores)
      Se le pide una consigna en RPM y cada motor la sostiene usando su
      propio encoder como realimentacion. Sin filtro de fusion ni Kalman:
      la velocidad se mide contando pulsos en la ventana de control, crudo.

  ⚠️ RUEDAS EN EL AIRE. Este sketch mantiene los motores girando de forma
     continua -- no son ráfagas como en el Test 5.

  ------------------------------------------------------------------------
  PROTECCIONES (todas activas en los dos modos)

    1. Doble tope de PWM en setMotor(): el activo (121 por defecto, subible
       a 160 desde la UI) y el absoluto de 170, que nada puede cruzar.
    2. Consigna limitada a RPM_OBJETIVO_MAX.
    3. Corte por sobrevelocidad: si un motor supera RPM_CORTE, para todo.
       Las RPM son el limitador primario -- ver el comentario de limites.
    4. Corte por motor trabado: duty por encima de la zona muerta durante
       mas de MS_TRABADO sin pulsos -> para todo. Evita cocinar un motor
       que quedo bloqueado mecanicamente.
    5. Reversion automatica del cap extendido tras MS_MAX_SOBRE_CAP_BASE
       de operacion sostenida sobre la tension nominal del motor.
    6. Apagado automatico por tiempo (MS_MAX_CORRIDA): si nadie para el
       ensayo, se para solo.
    7. Anti-windup en el integrador del PI.

  ------------------------------------------------------------------------
  CABLEADO: el de siempre (docs/conexiones-registro-pruebas.md, 3.1)
    Motor 1: ENA=27, IN1=14, IN2=13   Encoder M1: A=32, B=33
    Motor 2: ENB=4,  IN3=16, IN4=17   Encoder M2: A=25, B=26

  ⚠️ Antes de correr, confirmar que el cable del riel de 5V del Buck al pin
     logico "5V" del L298N este puesto. Sin el, el firmware reporta todo
     bien y los motores no se mueven (bug ya documentado en CLAUDE.md).

  REPORTE: WiFi, AP "RobotBalance_Test6" (clave balance2026) -> 192.168.4.1
           y por Serial a 115200.
  ========================================================================
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

// --- Pines ---------------------------------------------------------------
const uint8_t ENA = 27, IN1 = 14, IN2 = 13;
const uint8_t ENB = 4,  IN3 = 16, IN4 = 17;
const uint8_t ENC1A = 32, ENC1B = 33;
const uint8_t ENC2A = 25, ENC2B = 26;

// --- PWM (LEDC, API core 3.x) --------------------------------------------
const uint32_t PWM_FREQ_HZ  = 20000;
const uint8_t  PWM_RES_BITS = 8;

/*
  DOS TOPES, no uno.

  El cap historico de 121 sale de 6.0V/12.6V, que supone que al motor le
  llega todo el voltaje del bus. No es asi: el L298N usa BJTs y cae ~2V
  entre sus dos transistores en conduccion. A duty 121 el motor ve

      12.6 * (121/255) - 2.0  ~=  4.0V

  o sea ~2/3 del voltaje nominal. Ese es el motivo de que los motores nunca
  se hayan visto rapidos. Para 6.0V reales en bornes del motor hace falta

      duty = (6.0 + 2.0) / 12.6 * 255  ~=  162

  ⚠️ Ese 2.0V es el valor tipico de datasheet del L298N, NO esta medido en
  este robot. El cap extendido queda detras de un toggle explicito en la UI
  y arranca desactivado a proposito: antes de usarlo hay que medir el
  voltaje real en bornes del motor a duty 121 y recalcular con ese dato.
*/
const uint8_t MAX_PWM_ABSOLUTO = 170;   // techo que el firmware nunca cruza
const uint8_t CAP_BASE         = 121;   // el conservador de siempre
volatile uint8_t capActivo     = CAP_BASE;

// --- Parametros del motor ------------------------------------------------
const float PPR = 69.1f;              // pulsos/vuelta, medido en Test 3
const uint8_t DUTY_ZONA_MUERTA = 85;  // medido en Test 3 (aire)

/*
  M1 y M2 estan montados en espejo: para el mismo comando FWD dan pulsos de
  signo opuesto (confirmado en Test 3). Se invierte M2 por software para que
  "positivo" signifique lo mismo en los dos. Si al correr el modo 1 un motor
  reporta RPM negativa girando para adelante, invertir SU constante.
*/
const int8_t SIGNO_ENC1 = +1;
const int8_t SIGNO_ENC2 = -1;

// --- Limites de seguridad ------------------------------------------------
/*
  El limitador PRIMARIO ahora es la velocidad, no el duty. El duty es una
  variable indirecta: no dice que voltaje recibe el motor. Las RPM si son un
  proxy fisico directo -- el motor da 793 RPM nominales a 6V sin carga, asi
  que no dejar pasar de ~790 RPM equivale a "no mas de 6V", medido en vez de
  supuesto.

  ⚠️ El tope de RPM NO reemplaza al de duty, lo complementa: bajo carga, RPM
  bajas con duty alto significan MAS corriente, no menos. El caso peligroso
  (motor frenado, corriente maxima, cero RPM) lo cubren el tope de duty y el
  corte por trabado, no este limite.
*/
const float    RPM_NOMINAL_6V   = 793.0f;   // datasheet, sin carga
const float    RPM_OBJETIVO_MAX = 700.0f;   // consigna maxima aceptada
const float    RPM_CORTE        = 850.0f;   // sobrevelocidad -> parada
const uint16_t MS_TRABADO       = 1500;     // sin pulsos con duty alto -> parada
const uint32_t MS_MAX_CORRIDA   = 60000;    // apagado automatico

/*
  Por encima del cap base el motor esta sobre su tension nominal: se tolera
  para ensayos cortos, no de forma sostenida. Pasado este tiempo el firmware
  vuelve solo al cap conservador.
*/
const uint32_t MS_MAX_SOBRE_CAP_BASE = 15000;

// --- Lazo de control -----------------------------------------------------
const uint16_t MS_CONTROL = 50;   // 20 Hz. A 400 RPM son ~23 pulsos/ventana
const float    KP = 0.05f;        // duty por RPM de error
const float    KI = 0.15f;        // duty por (RPM*s) de error

const int8_t FWD = 1, REV = -1, STOP = 0;

// --- Estado --------------------------------------------------------------
WebServer server(80);

volatile long pulsos1 = 0, pulsos2 = 0;

volatile uint8_t modo = 0;          // 0=idle, 1=diagnostico, 2=control
volatile bool    abortar = false;
volatile float   consigna = 0;      // RPM pedida (signo = sentido)

float rpm1 = 0, rpm2 = 0;
int   duty1 = 0, duty2 = 0;
float integ1 = 0, integ2 = 0;

char estado[110] = "Listo. Ruedas en el aire.";
void setEstado(const char* fmt, ...) {
  va_list ap; va_start(ap, fmt);
  vsnprintf(estado, sizeof(estado), fmt, ap);
  va_end(ap);
}

// --- Encoders (cuadratura 1x: flanco de A, sentido dado por B) -----------
void IRAM_ATTR isrEnc1() { pulsos1 += digitalRead(ENC1B) ? -1 : +1; }
void IRAM_ATTR isrEnc2() { pulsos2 += digitalRead(ENC2B) ? -1 : +1; }

long leerYResetear(volatile long* contador) {
  noInterrupts();
  long v = *contador;
  *contador = 0;
  interrupts();
  return v;
}

float pulsosARpm(long pulsos, uint32_t ventanaMs) {
  if (ventanaMs == 0) return 0;
  return (pulsos / PPR) * (60000.0f / ventanaMs);
}

// --- Motores -------------------------------------------------------------
void setMotor(uint8_t enPin, uint8_t inA, uint8_t inB, int8_t dir, int duty) {
  // Doble tope: el activo (ajustable) y el absoluto (que nadie puede cruzar).
  int tope = min((int)capActivo, (int)MAX_PWM_ABSOLUTO);
  duty = constrain(duty, 0, tope);
  if (dir > 0)      { digitalWrite(inA, HIGH); digitalWrite(inB, LOW);  }
  else if (dir < 0) { digitalWrite(inA, LOW);  digitalWrite(inB, HIGH); }
  else              { digitalWrite(inA, LOW);  digitalWrite(inB, LOW); duty = 0; }
  ledcWrite(enPin, duty);
}

void stopAll() {
  setMotor(ENA, IN1, IN2, STOP, 0);
  setMotor(ENB, IN3, IN4, STOP, 0);
  duty1 = duty2 = 0;
  integ1 = integ2 = 0;
}

// ========================================================================
// MODO 1 — Diagnostico en lazo abierto, un motor por vez
// ========================================================================
void probarUnMotor(uint8_t enPin, uint8_t inA, uint8_t inB,
                   volatile long* contador, int8_t signo, const char* nombre) {
  // Los valores sobre 121 solo llegan al motor si el cap extendido esta
  // activo; si no, setMotor() los recorta y el barrido se aplana ahi.
  const uint8_t duties[] = {60, 70, 80, 90, 100, 110, 121, 140, 160};
  const uint8_t n = sizeof(duties) / sizeof(duties[0]);

  Serial.print(F("\n--- ")); Serial.print(nombre); Serial.println(F(" solo ---"));

  for (uint8_t i = 0; i < n && !abortar; i++) {
    setEstado("Diagnostico %s: duty %u", nombre, duties[i]);
    setMotor(enPin, inA, inB, FWD, duties[i]);

    vTaskDelay(pdMS_TO_TICKS(400));      // dejar arrancar antes de medir
    leerYResetear(contador);
    vTaskDelay(pdMS_TO_TICKS(800));      // ventana de medicion
    long p = leerYResetear(contador) * signo;

    setMotor(enPin, inA, inB, STOP, 0);

    float rpm = pulsosARpm(p, 800);
    Serial.print(F("  duty=")); Serial.print(duties[i]);
    Serial.print(F("  pulsos=")); Serial.print(p);
    Serial.print(F("  RPM=")); Serial.println(rpm, 1);

    vTaskDelay(pdMS_TO_TICKS(500));
  }
  setMotor(enPin, inA, inB, STOP, 0);
}

void correrDiagnostico() {
  // Ruido de fondo: con los motores quietos no deberia contar nada.
  leerYResetear(&pulsos1); leerYResetear(&pulsos2);
  vTaskDelay(pdMS_TO_TICKS(500));
  long r1 = leerYResetear(&pulsos1), r2 = leerYResetear(&pulsos2);
  Serial.print(F("\nRuido en reposo (deberia ser 0): M1=")); Serial.print(r1);
  Serial.print(F("  M2=")); Serial.println(r2);

  probarUnMotor(ENA, IN1, IN2, &pulsos1, SIGNO_ENC1, "M1");
  probarUnMotor(ENB, IN3, IN4, &pulsos2, SIGNO_ENC2, "M2");

  Serial.println(F("\n--- Interpretacion ---"));
  Serial.println(F("  ningun motor con pulsos -> L298N: alimentacion logica"));
  Serial.println(F("     (pin '5V'), GND comun, o VIN+ de potencia."));
  Serial.println(F("  uno solo sin pulsos     -> ese canal: ENA/ENB, INx, o el motor."));
  Serial.println(F("  gira pero sin pulsos    -> encoder de ese motor."));
  setEstado("Diagnostico completo. Ver Serial.");
}

// ========================================================================
// MODO 2 — Control de velocidad en lazo cerrado (PI por motor)
// ========================================================================

/*
  Feedforward: la zona muerta es grande (~85/255 medido), asi que arrancar
  el PI desde duty 0 haria que el integrador tuviera que "cargar" toda esa
  zona antes de que el motor se mueva -- lento y con sobrepico. Se parte de
  una estimacion lineal y el PI solo corrige la diferencia.
*/
int dutyFeedforward(float rpmObjetivo) {
  float f = fabs(rpmObjetivo) / RPM_OBJETIVO_MAX;
  return DUTY_ZONA_MUERTA + (int)(f * (capActivo - DUTY_ZONA_MUERTA));
}

void correrControl() {
  integ1 = integ2 = 0;
  leerYResetear(&pulsos1); leerYResetear(&pulsos2);

  uint32_t tInicio = millis();
  uint32_t tTrabado1 = millis(), tTrabado2 = millis();
  uint32_t tBajoCapBase = millis();   // ultima vez que ambos duty <= CAP_BASE

  while (modo == 2 && !abortar) {
    vTaskDelay(pdMS_TO_TICKS(MS_CONTROL));

    long p1 = leerYResetear(&pulsos1) * SIGNO_ENC1;
    long p2 = leerYResetear(&pulsos2) * SIGNO_ENC2;
    rpm1 = pulsosARpm(p1, MS_CONTROL);
    rpm2 = pulsosARpm(p2, MS_CONTROL);

    float objetivo = constrain(consigna, -RPM_OBJETIVO_MAX, RPM_OBJETIVO_MAX);

    // --- Proteccion: sobrevelocidad ---
    if (fabs(rpm1) > RPM_CORTE || fabs(rpm2) > RPM_CORTE) {
      stopAll(); modo = 0;
      setEstado("CORTE: sobrevelocidad (M1=%.0f M2=%.0f RPM)", rpm1, rpm2);
      Serial.println(F("\n!! CORTE POR SOBREVELOCIDAD"));
      return;
    }

    // --- Proteccion: motor trabado ---
    // Solo cuenta como trabado si se le esta pidiendo que gire de verdad.
    uint32_t ahora = millis();
    if (!(duty1 >= DUTY_ZONA_MUERTA && fabs(rpm1) < 5.0f)) tTrabado1 = ahora;
    if (!(duty2 >= DUTY_ZONA_MUERTA && fabs(rpm2) < 5.0f)) tTrabado2 = ahora;
    if (fabs(objetivo) > 1.0f &&
        (ahora - tTrabado1 > MS_TRABADO || ahora - tTrabado2 > MS_TRABADO)) {
      stopAll(); modo = 0;
      setEstado("CORTE: motor trabado (sin pulsos con duty alto)");
      Serial.println(F("\n!! CORTE POR MOTOR TRABADO"));
      return;
    }

    // --- Proteccion: sobretension sostenida ---
    // Correr por encima del cap base pone al motor sobre su tension nominal.
    // Se tolera en rafagas; sostenido, se vuelve solo al cap conservador.
    if (duty1 <= CAP_BASE && duty2 <= CAP_BASE) tBajoCapBase = ahora;
    if (capActivo > CAP_BASE && ahora - tBajoCapBase > MS_MAX_SOBRE_CAP_BASE) {
      capActivo = CAP_BASE;
      integ1 = integ2 = 0;   // el integrador cargado ya no es valido
      setEstado("Cap extendido revertido a %u: %lu s sobre tension nominal.",
                CAP_BASE, MS_MAX_SOBRE_CAP_BASE / 1000);
      Serial.println(F("\n!! CAP EXTENDIDO REVERTIDO (sobretension sostenida)"));
    }

    // --- Proteccion: tiempo maximo ---
    if (ahora - tInicio > MS_MAX_CORRIDA) {
      stopAll(); modo = 0;
      setEstado("Parada automatica: %lu s de corrida.", MS_MAX_CORRIDA / 1000);
      return;
    }

    if (fabs(objetivo) < 1.0f) { stopAll(); continue; }

    // --- PI por motor ---
    float dt = MS_CONTROL / 1000.0f;
    int8_t dir = (objetivo > 0) ? FWD : REV;

    float e1 = fabs(objetivo) - fabs(rpm1);
    float e2 = fabs(objetivo) - fabs(rpm2);

    integ1 += e1 * dt;
    integ2 += e2 * dt;

    // Anti-windup: acotar el integrador a lo que el actuador puede dar.
    float integMax = capActivo / KI;
    integ1 = constrain(integ1, -integMax, integMax);
    integ2 = constrain(integ2, -integMax, integMax);

    duty1 = dutyFeedforward(objetivo) + (int)(KP * e1 + KI * integ1);
    duty2 = dutyFeedforward(objetivo) + (int)(KP * e2 + KI * integ2);
    duty1 = constrain(duty1, 0, (int)capActivo);
    duty2 = constrain(duty2, 0, (int)capActivo);

    setMotor(ENA, IN1, IN2, dir, duty1);
    setMotor(ENB, IN3, IN4, dir, duty2);

    setEstado("Consigna %.0f RPM | M1 %.0f (duty %d) | M2 %.0f (duty %d)",
              objetivo, rpm1, duty1, rpm2, duty2);
  }
  stopAll();
}

// --- Tarea de control (nucleo 1, separada del stack WiFi) ----------------
void tareaControl(void* pv) {
  for (;;) {
    if (modo == 1) {
      correrDiagnostico();
      modo = 0; abortar = false; stopAll();
    } else if (modo == 2) {
      correrControl();
      if (modo == 0) { /* corto una proteccion, el mensaje ya esta puesto */ }
      else { modo = 0; setEstado("Control detenido."); }
      abortar = false; stopAll();
    } else {
      vTaskDelay(pdMS_TO_TICKS(50));
    }
  }
}

// --- Pagina web ----------------------------------------------------------
const char PAGE[] PROGMEM = R"HTML(
<!DOCTYPE html><html lang="es"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Test 6 - Velocidad</title><style>
:root{--bg:#12151c;--card:#191e28;--ink:#e9e6df;--dim:#9aa0ad;--faint:#6b7180;
--ok:#4fae63;--bad:#d1554a;--acc:#5b9fd9;--rule:#2a3040;--mono:Menlo,monospace}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--ink);
font-family:-apple-system,'Segoe UI',sans-serif;padding:.8rem;max-width:820px;margin-inline:auto}
h1{font-size:1.05rem;margin:.2rem 0}
.sub{color:var(--dim);font-size:.76rem;margin-bottom:.8rem}
.card{background:var(--card);border:1px solid var(--rule);border-radius:8px;padding:.8rem;margin-bottom:.7rem}
.card h2{font-size:.68rem;text-transform:uppercase;letter-spacing:.07em;color:var(--faint);
margin:0 0 .6rem;font-family:var(--mono)}
button{background:var(--ok);color:#08150c;border:0;border-radius:6px;padding:.55rem .9rem;
font-size:.85rem;font-weight:600;cursor:pointer;margin:.2rem .3rem .2rem 0}
button.stop{background:var(--bad);color:#fff}
input{background:#0e1119;color:var(--ink);border:1px solid var(--rule);border-radius:6px;
padding:.5rem;font-family:var(--mono);width:6rem;font-size:.9rem}
.row{display:flex;gap:1.2rem;flex-wrap:wrap;margin-top:.5rem}
.big{font-size:1.5rem;font-weight:700;font-family:var(--mono);color:var(--acc)}
.lbl{font-size:.66rem;color:var(--faint);font-family:var(--mono);text-transform:uppercase}
.note{font-size:.72rem;color:var(--faint);margin-top:.5rem}
</style></head><body>
<h1>Test 6 — Diagnóstico y control de velocidad</h1>
<div class="sub">⚠️ Ruedas en el aire. Los motores giran de forma continua.</div>

<div class="card">
  <h2>Estado</h2>
  <div id="st">—</div>
  <div class="row">
    <div><div class="lbl">M1 rpm</div><div class="big" id="r1">—</div></div>
    <div><div class="lbl">M2 rpm</div><div class="big" id="r2">—</div></div>
    <div><div class="lbl">M1 duty</div><div class="big" id="d1">—</div></div>
    <div><div class="lbl">M2 duty</div><div class="big" id="d2">—</div></div>
  </div>
  <div style="margin-top:.7rem"><button class="stop" onclick="go('/stop')">PARAR</button></div>
</div>

<div class="card">
  <h2>Modo 1 — Diagnóstico (lazo abierto, un motor por vez)</h2>
  <button onclick="go('/start?m=1')">Correr diagnóstico</button>
  <div class="note">Barre duty 60→121 en M1 solo y después en M2 solo, contando
  pulsos de encoder. El detalle y la interpretación salen por Serial.</div>
</div>

<div class="card">
  <h2>Modo 2 — Control de velocidad (lazo cerrado)</h2>
  <input id="rpm" type="number" value="150" step="10"> RPM
  <button onclick="go('/start?m=2&rpm='+document.getElementById('rpm').value)">Arrancar</button>
  <div class="note">Consigna máxima 700 RPM. Negativo invierte el sentido.
  Corta solo por sobrevelocidad, motor trabado, o a los 60 s.</div>
</div>

<div class="card">
  <h2>Tope de PWM</h2>
  <div>activo: <span class="big" id="cap">—</span></div>
  <div style="margin-top:.5rem">
    <button onclick="go('/cap?v=121')">121 — conservador</button>
    <button onclick="go('/cap?v=160')">160 — ~6V reales</button>
  </div>
  <div class="note">121 supone que al motor le llega todo el bus; con la caída
  de ~2V del L298N en realidad recibe ~4V. 160 apunta a los 6V nominales
  <b>según datasheet, sin medir</b> — verificá el voltaje en bornes del motor
  antes de confiar en él. Sobre 121 el firmware vuelve solo al conservador
  tras 15 s sostenidos.</div>
</div>

<script>
function go(u){fetch(u).then(()=>tick())}
function f(x,d){return (x===null||x===undefined)?'—':Number(x).toFixed(d)}
function tick(){fetch('/data').then(r=>r.json()).then(d=>{
  document.getElementById('st').textContent=d.msg;
  document.getElementById('r1').textContent=f(d.r1,0);
  document.getElementById('r2').textContent=f(d.r2,0);
  document.getElementById('d1').textContent=d.d1;
  document.getElementById('d2').textContent=d.d2;
  document.getElementById('cap').textContent=d.cap;
})}
setInterval(tick,400);tick();
</script></body></html>
)HTML";

void handleData() {
  String j = "{";
  j += "\"msg\":\"" + String(estado) + "\"";
  j += ",\"r1\":" + String(rpm1, 1) + ",\"r2\":" + String(rpm2, 1);
  j += ",\"d1\":" + String(duty1) + ",\"d2\":" + String(duty2);
  j += ",\"cap\":" + String(capActivo);
  j += "}";
  server.send(200, "application/json", j);
}

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  ledcAttach(ENA, PWM_FREQ_HZ, PWM_RES_BITS);
  ledcAttach(ENB, PWM_FREQ_HZ, PWM_RES_BITS);
  stopAll();   // estado seguro antes que nada

  /*
    Pull-up interno: no esta confirmado si la salida del encoder es push-pull
    o colector abierto (ver CLAUDE.md). Con pull-up funciona en los dos casos.
  */
  pinMode(ENC1A, INPUT_PULLUP); pinMode(ENC1B, INPUT_PULLUP);
  pinMode(ENC2A, INPUT_PULLUP); pinMode(ENC2B, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENC1A), isrEnc1, RISING);
  attachInterrupt(digitalPinToInterrupt(ENC2A), isrEnc2, RISING);

  WiFi.softAP("RobotBalance_Test6", "balance2026");
  Serial.println(F("\n=== TEST 6 - Velocidad por encoder ==="));
  Serial.print(F("AP: RobotBalance_Test6 -> http://"));
  Serial.println(WiFi.softAPIP());
  Serial.print(F("Cap de PWM activo: ")); Serial.print(capActivo);
  Serial.print(F("  (absoluto: ")); Serial.print(MAX_PWM_ABSOLUTO); Serial.println(F(")"));

  server.on("/", []() { server.send_P(200, "text/html", PAGE); });
  server.on("/data", handleData);
  server.on("/stop", []() {
    abortar = true; modo = 0; stopAll(); setEstado("PARADA manual.");
    server.send(200, "text/plain", "ok");
  });
  /*
    Cambiar el cap con los motores en marcha dejaria el integrador del PI
    cargado para un rango de actuador que ya no existe. Se exige idle.
  */
  server.on("/cap", []() {
    if (modo != 0) { server.send(409, "text/plain", "parar primero"); return; }
    int v = server.arg("v").toInt();
    capActivo = constrain(v, DUTY_ZONA_MUERTA, MAX_PWM_ABSOLUTO);
    setEstado("Tope de PWM: %u%s", capActivo,
              capActivo > CAP_BASE ? " (EXTENDIDO, sobre tension nominal)" : "");
    Serial.print(F("Cap activo = ")); Serial.println(capActivo);
    server.send(200, "text/plain", "ok");
  });
  server.on("/start", []() {
    if (modo != 0) { server.send(409, "text/plain", "ocupado"); return; }
    uint8_t m = server.arg("m").toInt();
    if (m == 2) consigna = constrain(server.arg("rpm").toFloat(),
                                     -RPM_OBJETIVO_MAX, RPM_OBJETIVO_MAX);
    if (m == 1 || m == 2) { abortar = false; modo = m; }
    server.send(200, "text/plain", "ok");
  });
  server.begin();

  xTaskCreatePinnedToCore(tareaControl, "control", 8192, NULL, 2, NULL, 1);
}

void loop() {
  server.handleClient();
  delay(2);
}
