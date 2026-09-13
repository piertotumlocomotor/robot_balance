/*
  ========================================================================
  TEST 3 — Encoders + zona muerta medida POR ENCODER (ESP32)
  ========================================================================

  Reemplaza la Fase C del Test 2 como metodo de medicion de zona muerta.

  POR QUE EXISTE ESTE TEST:
  La zona muerta medida en el Test 2 (duty 85-95 segun el motor) se obtuvo
  mirando la rueda girar. Ese metodo no distingue "el motor arranco" de
  "el motor temblo y se quedo", y no separa la variabilidad real del motor
  de la variabilidad del ojo del operador. Los encoders convierten eso en
  un umbral objetivo: cuentan pulsos o no cuentan.

  ORDEN DE LAS FASES -- no es arbitrario:
    Fase 0: validar los encoders A MANO, sin motores. Si el instrumento no
            esta validado primero, un resultado raro en la Fase 2 no se
            puede atribuir (motor que arranca tarde vs. encoder que no
            cuenta). Ademas mide el PPR, que sigue sin confirmarse en el
            proyecto (CLAUDE.md: "no asumir resolucion/PPR del encoder sin
            confirmarla") y del que depende MOVE_THRESHOLD.
    Fase 1: sentido de giro, verificado por el SIGNO del contador, no por
            observacion. Confirma la correccion fisica de M1 (cables
            intercambiados en OUT1/OUT2 el 2026-08-05).
    Fase 2: barrido fino con repeticiones y estadistica.

  ------------------------------------------------------------------------
  CABLEADO (docs/conexiones-registro-pruebas.md, seccion 3)

    ENCODERS                          MOTORES (via L298N)
      M1 canal A -> GPIO32 (P32)        ENA -> GPIO27 (P27)  PWM M1
      M1 canal B -> GPIO33 (P33)        IN1 -> GPIO14 (P14)  dir M1
      M2 canal A -> GPIO25 (P25)        IN2 -> GPIO13 (P13)  dir M1
      M2 canal B -> GPIO26 (P26)        ENB -> GPIO4  (P4)   PWM M2
                                        IN3 -> GPIO16 (P16)  dir M2
      Encoder VCC (azul)  -> 3V3        IN4 -> GPIO17 (P17)  dir M2
      Encoder GND (negro) -> GND logico

    ⚠️ GND del ESP32 DEBE estar unido al riel GND logico comun. Que el
       Serial por USB funcione NO lo garantiza -- el USB trae su propia
       referencia. Ese fue el bug del 2026-08-05 (ver CLAUDE.md, "GND
       comun ESP32<->L298N").

  PULL-UPS: los 4 canales se configuran con INPUT_PULLUP. No esta
  confirmado si la salida de estos encoders es push-pull o colector
  abierto (CLAUDE.md). Si es push-pull, el pull-up interno es inofensivo
  (el driver lo domina); si es colector abierto, es obligatorio. Cubre
  ambos casos sin tener que confirmarlo antes.

  ------------------------------------------------------------------------
  ⚠️ ANTES DE ENERGIZAR

  - RUEDAS EN EL AIRE para la Fase 0 (hay que girarlas a mano) y para la
    Fase 1. Para la Fase 2, elegir la condicion y ANOTARLA: el protocolo
    de la seccion 7.8 pide aire y piso/tethered por separado.
  - ⚠️ Los 4 pull-down de 10kOhm en IN1-4 pueden NO estar instalados
    todavia. Sin ellos, un reset o cuelgue del ESP32 deja esas entradas
    en alta impedancia y el L298N en estado indefinido. Mano cerca del
    switch de la bateria durante toda la corrida.
  - C1 sigue siendo de 16V sobre un riel de hasta 12.6V -- ciclos cortos.

  ⚠️ ESTE TEST LEVANTA EL AP DE WIFI MIENTRAS LOS MOTORES CORREN. Es la
     primera vez que ambas cargas coinciden en el ESP32, que tiene picos
     de corriente mayores que el ESP8266. Si la placa se REINICIA a mitad
     de un barrido, NO es un bug del sketch: es el pendiente abierto de
     C4 (todavia montado donde iba el ESP8266) manifestandose. El contador
     de arranques de abajo lo hace evidente.

  ------------------------------------------------------------------------
  LIMITE DE VOLTAJE (CLAUDE.md, "Limite de PWM a los motores")
    Bateria 3S llena 12.6V / motor rated 6.0V => duty max 121/255.
    Se aplica con constrain() dentro de setMotor().
  ========================================================================
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "soc/gpio_reg.h"

// --- Pines ---------------------------------------------------------------
const uint8_t ENC_M1_A = 32;
const uint8_t ENC_M1_B = 33;
const uint8_t ENC_M2_A = 25;
const uint8_t ENC_M2_B = 26;

const uint8_t ENA = 27, IN1 = 14, IN2 = 13;
const uint8_t ENB = 4,  IN3 = 16, IN4 = 17;

// --- PWM (LEDC, API core 3.x: por pin, no por canal) ---------------------
const uint32_t PWM_FREQ_HZ  = 20000;
const uint8_t  PWM_RES_BITS = 8;

// --- Limites -------------------------------------------------------------
const uint8_t MAX_PWM_DUTY = 121;   // 6.0V / 12.6V * 255 — tope duro

// --- Parametros del barrido de zona muerta -------------------------------
/*
  RAMP_START=60 y no 20: la corrida del Test 2 ya establecio que por debajo
  de ~77 no arranca ningun motor. Empezar en 60 deja margen de sobra por
  debajo del minimo conocido y ahorra ~40 escalones inutiles por repeticion
  (con 5 repeticiones x 2 motores, son minutos).

  RAMP_STEP=1 (el Test 2 usaba 5): la resolucion del paso ES la resolucion
  de la medicion. Con paso 5 no se puede distinguir 88 de 90.
*/
const uint8_t  RAMP_START   = 20;
const uint8_t  RAMP_STEP    = 1;
const uint16_t RESPOND_MS   = 120;   // dejar que el motor reaccione al nuevo duty
const uint16_t WINDOW_MS    = 300;   // ventana de conteo
const uint16_t REST_MS      = 1500;  // reposo entre repeticiones
const uint8_t  N_TRIALS     = 5;

/*
  MOVE_THRESHOLD: pulsos minimos en WINDOW_MS para considerar "arranco".
  ⚠️ Valor provisorio. Depende del PPR real, que se mide en la Fase 0. Si
  la Fase 0 revela un PPR muy bajo, subirlo puede hacer que el test no
  detecte giros lentos reales; muy alto, que cuente vibracion como giro.
  Revisar despues de conocer el PPR.
*/
const int32_t MOVE_THRESHOLD = 5;

const uint8_t DIR_TEST_DUTY = 110;  // holgadamente por encima de la zona muerta conocida

const int8_t FWD = 1, REV = -1, STOP = 0;

// --- Tipos de resultado --------------------------------------------------
/*
  Definidos antes de cualquier funcion a proposito: el IDE de Arduino genera
  los prototipos automaticamente y los inserta antes de la primera funcion
  del archivo. Si un struct se declara despues, cualquier funcion que lo use
  como tipo de retorno no compila ("does not name a type").
*/
struct DirResult {
  int32_t fwd = 0;      // delta del encoder con el motor comandado FWD
  int32_t rev = 0;      // delta con el motor comandado REV
  bool    done = false;
};

struct DeadZone {
  int16_t trial[N_TRIALS];   // duty de arranque; -1 = no arranco hasta el tope
  uint8_t n = 0;
};

struct Stats { int16_t mn, mx; float mean, sd; uint8_t valid; };

DirResult dirM1, dirM2;
DeadZone  dzM1,  dzM2;

float pprM1 = 0, pprM2 = 0;   // pulsos por vuelta medidos en Fase 0

// --- Estado de los encoders ----------------------------------------------
volatile int32_t encM1 = 0;
volatile int32_t encM2 = 0;

/*
  Lectura de GPIO por registro directo. digitalRead() vive en flash y no
  esta garantizado que sea seguro llamarlo desde una ISR (si la flash esta
  ocupada, el acceso puede fallar). Leer el registro es IRAM-safe y ademas
  mas rapido, que importa con el motor a full.
*/
static inline int IRAM_ATTR fastRead(uint8_t pin) {
  if (pin < 32) return (REG_READ(GPIO_IN_REG) >> pin) & 0x1;
  return (REG_READ(GPIO_IN1_REG) >> (pin - 32)) & 0x1;
}

/*
  Decodificacion en cuadratura 1x: interrupcion en el flanco de subida del
  canal A, el nivel de B en ese instante da el sentido. No se usa 4x
  (interrumpir en ambos flancos de ambos canales) porque para "arranco o
  no arranco" y para contar vueltas alcanza de sobra, y 1x cuarta la carga
  de ISR con los motores girando rapido.
*/
void IRAM_ATTR isrM1() { encM1 += fastRead(ENC_M1_B) ? -1 : 1; }
void IRAM_ATTR isrM2() { encM2 += fastRead(ENC_M2_B) ? -1 : 1; }

// --- Estado de ejecucion -------------------------------------------------
enum Phase : uint8_t { PH_IDLE = 0, PH_DIRECTION = 1, PH_DEADZONE = 2 };
volatile Phase   activePhase = PH_IDLE;
volatile bool    abortFlag   = false;
volatile uint8_t progressPct = 0;
String statusMsg = "Listo. Empezar por la Fase 0 (girar ruedas a mano).";

WebServer server(80);

// --- Control de motores --------------------------------------------------
void setMotor(uint8_t enPin, uint8_t inA, uint8_t inB, int8_t dir, uint8_t duty) {
  duty = constrain(duty, 0, MAX_PWM_DUTY);
  if (dir > 0)      { digitalWrite(inA, HIGH); digitalWrite(inB, LOW);  }
  else if (dir < 0) { digitalWrite(inA, LOW);  digitalWrite(inB, HIGH); }
  else              { digitalWrite(inA, LOW);  digitalWrite(inB, LOW); duty = 0; }
  ledcWrite(enPin, duty);
}

void motor1(int8_t d, uint8_t duty) { setMotor(ENA, IN1, IN2, d, duty); }
void motor2(int8_t d, uint8_t duty) { setMotor(ENB, IN3, IN4, d, duty); }
void stopAll() { motor1(STOP, 0); motor2(STOP, 0); }

float dutyToVolts(uint8_t duty) { return (duty / 255.0f) * 12.6f; }

// --- Estadistica ---------------------------------------------------------
Stats computeStats(const DeadZone& dz) {
  Stats s{0, 0, 0, 0, 0};
  int32_t sum = 0;
  s.mn = 32767; s.mx = -1;
  for (uint8_t i = 0; i < dz.n; i++) {
    if (dz.trial[i] < 0) continue;          // no arranco: no contamina la media
    s.valid++;
    sum += dz.trial[i];
    if (dz.trial[i] < s.mn) s.mn = dz.trial[i];
    if (dz.trial[i] > s.mx) s.mx = dz.trial[i];
  }
  if (s.valid == 0) { s.mn = 0; return s; }
  s.mean = (float)sum / s.valid;
  float acc = 0;
  for (uint8_t i = 0; i < dz.n; i++) {
    if (dz.trial[i] < 0) continue;
    float d = dz.trial[i] - s.mean;
    acc += d * d;
  }
  s.sd = (s.valid > 1) ? sqrtf(acc / (s.valid - 1)) : 0;
  return s;
}

// --- Fases ---------------------------------------------------------------

/*
  Fase 1: cada motor a duty fijo en ambos sentidos, midiendo el DELTA del
  encoder. Lo que importa no es el valor absoluto sino el signo, y sobre
  todo que M1 y M2 coincidan: despues de intercambiar los cables de M1 en
  OUT1/OUT2 (2026-08-05), ambos deberian dar el mismo signo para FWD.
*/
void runDirection() {
  statusMsg = "Fase 1: verificando sentido de giro...";
  dirM1 = DirResult(); dirM2 = DirResult();

  struct { const char* name; void (*mot)(int8_t, uint8_t); DirResult* out; } seq[] = {
    {"M1", motor1, &dirM1},
    {"M2", motor2, &dirM2},
  };

  for (uint8_t i = 0; i < 2 && !abortFlag; i++) {
    for (int8_t dir = 1; dir >= -1 && !abortFlag; dir -= 2) {
      int32_t before = (i == 0) ? encM1 : encM2;
      seq[i].mot(dir, DIR_TEST_DUTY);
      vTaskDelay(pdMS_TO_TICKS(1200));
      int32_t after = (i == 0) ? encM1 : encM2;
      seq[i].mot(STOP, 0);
      vTaskDelay(pdMS_TO_TICKS(800));

      if (dir > 0) seq[i].out->fwd = after - before;
      else         seq[i].out->rev = after - before;
      progressPct = (i * 2 + (dir > 0 ? 1 : 2)) * 25;
    }
    seq[i].out->done = true;
  }
  stopAll();
  statusMsg = abortFlag ? "Fase 1 abortada." : "Fase 1 completa.";
}

/*
  Fase 2: barrido ascendente fino, deteccion de arranque por encoder.

  Para cada escalon de duty: aplicar, esperar RESPOND_MS a que el motor
  reaccione, poner el contador en cero, contar durante WINDOW_MS. Si el
  |conteo| supera MOVE_THRESHOLD, ese duty es el umbral de esa repeticion
  y se corta el barrido (no tiene sentido seguir subiendo).

  Entre repeticiones hay REST_MS de reposo: al frenar, el rotor y los
  engranajes quedan en una posicion distinta cada vez, y esa posicion
  cambia el torque de arranque (cogging). Esa es la fuente principal de la
  dispersion que se vio a ojo en el Test 2 -- aca queda cuantificada como
  desviacion estandar en vez de como impresion.
*/
int16_t sweepOnce(uint8_t enPin, uint8_t inA, uint8_t inB, volatile int32_t* enc) {
  for (uint16_t duty = RAMP_START; duty <= MAX_PWM_DUTY; duty += RAMP_STEP) {
    if (abortFlag) break;
    setMotor(enPin, inA, inB, FWD, duty);
    vTaskDelay(pdMS_TO_TICKS(RESPOND_MS));
    *enc = 0;
    vTaskDelay(pdMS_TO_TICKS(WINDOW_MS));
    int32_t counted = *enc;
    if (counted < 0) counted = -counted;
    if (counted >= MOVE_THRESHOLD) {
      setMotor(enPin, inA, inB, STOP, 0);
      return (int16_t)duty;
    }
  }
  setMotor(enPin, inA, inB, STOP, 0);
  return -1;   // no arranco ni al tope
}

void runDeadZone() {
  statusMsg = "Fase 2: barrido de zona muerta...";
  dzM1 = DeadZone(); dzM2 = DeadZone();

  for (uint8_t t = 0; t < N_TRIALS && !abortFlag; t++) {
    dzM1.trial[dzM1.n++] = sweepOnce(ENA, IN1, IN2, &encM1);
    vTaskDelay(pdMS_TO_TICKS(REST_MS));
    progressPct = (uint8_t)(((t * 2 + 1) * 100.0f) / (N_TRIALS * 2));

    if (abortFlag) break;

    dzM2.trial[dzM2.n++] = sweepOnce(ENB, IN3, IN4, &encM2);
    vTaskDelay(pdMS_TO_TICKS(REST_MS));
    progressPct = (uint8_t)(((t * 2 + 2) * 100.0f) / (N_TRIALS * 2));
  }
  stopAll();
  statusMsg = abortFlag ? "Fase 2 abortada." : "Fase 2 completa.";
}

/*
  Tarea de medicion en su propio contexto FreeRTOS.

  No es adorno: el lazo de medicion bloquea por segundos y el servidor web
  tiene que seguir respondiendo. Ademas es el mismo patron que va a hacer
  falta para el lazo de control (PID fijado a un nucleo, WiFi en el otro),
  que fue una de las razones para migrar al ESP32 -- conviene tenerlo
  validado desde ahora y no descubrirlo con el robot intentando balancear.
*/
void measureTask(void* pv) {
  for (;;) {
    Phase p = activePhase;
    if (p == PH_DIRECTION)     { runDirection(); activePhase = PH_IDLE; abortFlag = false; }
    else if (p == PH_DEADZONE) { runDeadZone();  activePhase = PH_IDLE; abortFlag = false; }
    else vTaskDelay(pdMS_TO_TICKS(50));
  }
}

// --- Pagina web ----------------------------------------------------------
const char PAGE[] PROGMEM = R"HTML(
<!DOCTYPE html><html lang="es"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Test 3 - Encoders</title><style>
:root{--bg:#12151c;--card:#191e28;--ink:#e9e6df;--dim:#9aa0ad;--faint:#6b7180;
--ok:#4fae63;--warn:#d98e3a;--bad:#d1554a;--acc:#5b9fd9;--rule:#2a3040;
--mono:Menlo,Consolas,monospace}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--ink);
font-family:-apple-system,'Segoe UI',sans-serif;padding:.8rem;max-width:760px;
margin-inline:auto;line-height:1.45}
h1{font-size:1.1rem;margin:.2rem 0 .1rem}
.sub{color:var(--dim);font-size:.78rem;margin-bottom:.9rem}
.card{background:var(--card);border:1px solid var(--rule);border-radius:8px;
padding:.8rem;margin-bottom:.7rem}
.card h2{font-size:.7rem;text-transform:uppercase;letter-spacing:.07em;
color:var(--faint);margin:0 0 .6rem;font-family:var(--mono)}
.big{font-size:1.6rem;font-weight:700;font-family:var(--mono);color:var(--acc)}
.row{display:flex;justify-content:space-between;align-items:center;
padding:.3rem 0;border-bottom:1px solid var(--rule);font-size:.85rem}
.row:last-child{border-bottom:none}
.row .v{font-family:var(--mono);color:var(--acc);font-weight:600}
button{background:var(--ok);color:#08150c;border:0;border-radius:6px;
padding:.55rem .9rem;font-size:.85rem;font-weight:600;cursor:pointer;
margin:.2rem .3rem .2rem 0}
button.sec{background:var(--rule);color:var(--ink)}
button.stop{background:var(--bad);color:#fff}
button:disabled{opacity:.4;cursor:not-allowed}
input{background:#0d0f15;border:1px solid var(--rule);color:var(--ink);
border-radius:5px;padding:.4rem;width:4.5rem;font-family:var(--mono)}
table{width:100%;border-collapse:collapse;font-size:.8rem;margin-top:.4rem}
th{text-align:left;font-family:var(--mono);font-size:.65rem;text-transform:uppercase;
color:var(--dim);border-bottom:1px solid var(--rule);padding:.35rem .3rem}
td{padding:.35rem .3rem;border-bottom:1px solid var(--rule);font-family:var(--mono)}
tr:last-child td{border-bottom:none}
.note{font-size:.73rem;color:var(--faint);margin-top:.5rem}
.ok{color:var(--ok)}.warn{color:var(--warn)}.bad{color:var(--bad)}
.bar{height:5px;background:var(--rule);border-radius:3px;overflow:hidden;margin-top:.5rem}
.bar i{display:block;height:100%;background:var(--acc);width:0}
</style></head><body>
<h1>Test 3 — Encoders + zona muerta</h1>
<div class="sub">Medicion por encoder, no por observacion visual</div>

<div class="card">
  <h2>Estado</h2>
  <div id="st" style="font-size:.9rem">—</div>
  <div class="bar"><i id="bar"></i></div>
  <div class="row" style="margin-top:.5rem"><span>Arranques del ESP32</span><span class="v" id="boots">—</span></div>
  <div class="note">Si "arranques" sube durante una fase, la placa se reinicio:
  no es un fallo del test, es el pendiente de C4 / picos de corriente.</div>
  <button class="stop" onclick="go('/stop')">PARAR TODO</button>
</div>

<div class="card">
  <h2>Fase 0 — Validar encoders a mano (sin motores)</h2>
  <div class="row"><span>Contador M1</span><span class="v" id="e1">0</span></div>
  <div class="row"><span>Contador M2</span><span class="v" id="e2">0</span></div>
  <div class="note">Gira cada rueda A MANO. Ambos contadores deben moverse, y
  cambiar de signo al invertir el sentido. Si uno no se mueve, ese encoder o su
  cableado esta mal — no seguir a las fases siguientes.</div>
  <div style="margin-top:.6rem">
    <button class="sec" onclick="go('/reset')">Poner en cero</button>
    vueltas: <input id="tn" type="number" value="10" min="1">
    <button onclick="go('/ppr?turns='+document.getElementById('tn').value)">Calcular PPR</button>
  </div>
  <div class="row" style="margin-top:.4rem"><span>PPR M1 (pulsos/vuelta, 1x)</span><span class="v" id="p1">—</span></div>
  <div class="row"><span>PPR M2</span><span class="v" id="p2">—</span></div>
  <div class="note">Poner en cero, girar la rueda exactamente el numero de vueltas
  indicado, y recien ahi calcular. Dato pendiente en CLAUDE.md.</div>
</div>

<div class="card">
  <h2>Fase 1 — Sentido de giro</h2>
  <button onclick="go('/start?p=1')">Iniciar Fase 1</button>
  <table><thead><tr><th>Motor</th><th>FWD</th><th>REV</th></tr></thead>
  <tbody id="dirtb"><tr><td colspan="3" style="color:var(--faint)">sin datos</td></tr></tbody></table>
  <div id="dirv" class="note">Los deltas de FWD deben tener el MISMO signo en
  ambos motores (M1 ya fue corregido fisicamente intercambiando OUT1/OUT2).</div>
</div>

<div class="card">
  <h2>Fase 2 — Zona muerta</h2>
  <button onclick="go('/start?p=2')">Iniciar Fase 2</button>
  <div class="note">Tarda unos minutos. ANOTAR la condicion: ruedas en el aire o
  en el piso/tethered — son mediciones distintas (protocolo 7.8).</div>
  <table><thead><tr><th>Motor</th><th>min</th><th>max</th><th>media</th><th>σ</th><th>n</th></tr></thead>
  <tbody id="dztb"><tr><td colspan="6" style="color:var(--faint)">sin datos</td></tr></tbody></table>
  <table><thead><tr><th>Rep.</th><th>M1 duty</th><th>M2 duty</th></tr></thead>
  <tbody id="trtb"></tbody></table>
</div>

<script>
function go(u){fetch(u).then(()=>tick())}
function f(x){return x===null||x===undefined?'—':x}
function tick(){fetch('/data').then(r=>r.json()).then(d=>{
  document.getElementById('st').textContent=d.msg;
  document.getElementById('bar').style.width=d.pct+'%';
  document.getElementById('boots').textContent=d.boots;
  document.getElementById('e1').textContent=d.e1;
  document.getElementById('e2').textContent=d.e2;
  document.getElementById('p1').textContent=d.ppr1>0?d.ppr1.toFixed(1):'—';
  document.getElementById('p2').textContent=d.ppr2>0?d.ppr2.toFixed(1):'—';

  var t=document.getElementById('dirtb');
  if(d.dir1done||d.dir2done){
    t.innerHTML='<tr><td>M1</td><td>'+d.d1f+'</td><td>'+d.d1r+'</td></tr>'+
                '<tr><td>M2</td><td>'+d.d2f+'</td><td>'+d.d2r+'</td></tr>';
    var same=(d.d1f>0)===(d.d2f>0)&&d.d1f!==0&&d.d2f!==0;
    var v=document.getElementById('dirv');
    v.textContent=same?'OK — ambos motores giran en el mismo sentido con FWD.':
      'ATENCION — los signos NO coinciden: los motores giran en sentidos opuestos.';
    v.className='note '+(same?'ok':'bad');
  }
  var z=document.getElementById('dztb');
  if(d.n1>0||d.n2>0){
    z.innerHTML='<tr><td>M1</td><td>'+d.s1mn+'</td><td>'+d.s1mx+'</td><td>'+d.s1me.toFixed(1)+'</td><td>'+d.s1sd.toFixed(2)+'</td><td>'+d.s1n+'</td></tr>'+
                '<tr><td>M2</td><td>'+d.s2mn+'</td><td>'+d.s2mx+'</td><td>'+d.s2me.toFixed(1)+'</td><td>'+d.s2sd.toFixed(2)+'</td><td>'+d.s2n+'</td></tr>';
    var rows='';
    for(var i=0;i<Math.max(d.n1,d.n2);i++){
      rows+='<tr><td>'+(i+1)+'</td><td>'+(d.t1[i]<0?'no arranco':d.t1[i])+
            '</td><td>'+(d.t2[i]===undefined?'—':(d.t2[i]<0?'no arranco':d.t2[i]))+'</td></tr>';
    }
    document.getElementById('trtb').innerHTML=rows;
  }
})}
setInterval(tick,600);tick();
</script></body></html>
)HTML";

// --- Endpoints -----------------------------------------------------------
RTC_NOINIT_ATTR uint32_t bootCounter;   // sobrevive al reset, no al corte de energia

void handleData() {
  Stats s1 = computeStats(dzM1), s2 = computeStats(dzM2);
  String j = "{";
  j += "\"msg\":\"" + statusMsg + "\",";
  j += "\"pct\":" + String(progressPct) + ",";
  j += "\"boots\":" + String(bootCounter) + ",";
  j += "\"e1\":" + String(encM1) + ",\"e2\":" + String(encM2) + ",";
  j += "\"ppr1\":" + String(pprM1, 2) + ",\"ppr2\":" + String(pprM2, 2) + ",";
  j += "\"dir1done\":" + String(dirM1.done ? 1 : 0) + ",\"dir2done\":" + String(dirM2.done ? 1 : 0) + ",";
  j += "\"d1f\":" + String(dirM1.fwd) + ",\"d1r\":" + String(dirM1.rev) + ",";
  j += "\"d2f\":" + String(dirM2.fwd) + ",\"d2r\":" + String(dirM2.rev) + ",";
  j += "\"n1\":" + String(dzM1.n) + ",\"n2\":" + String(dzM2.n) + ",";
  j += "\"s1mn\":" + String(s1.mn) + ",\"s1mx\":" + String(s1.mx) + ",\"s1me\":" + String(s1.mean, 2) + ",\"s1sd\":" + String(s1.sd, 3) + ",\"s1n\":" + String(s1.valid) + ",";
  j += "\"s2mn\":" + String(s2.mn) + ",\"s2mx\":" + String(s2.mx) + ",\"s2me\":" + String(s2.mean, 2) + ",\"s2sd\":" + String(s2.sd, 3) + ",\"s2n\":" + String(s2.valid) + ",";
  j += "\"t1\":[";
  for (uint8_t i = 0; i < dzM1.n; i++) { j += String(dzM1.trial[i]); if (i + 1 < dzM1.n) j += ","; }
  j += "],\"t2\":[";
  for (uint8_t i = 0; i < dzM2.n; i++) { j += String(dzM2.trial[i]); if (i + 1 < dzM2.n) j += ","; }
  j += "]}";
  server.send(200, "application/json", j);
}

void setup() {
  Serial.begin(115200);
  delay(200);

  // El contador de arranques delata un reset a mitad de una fase.
  esp_reset_reason_t rr = esp_reset_reason();
  if (rr == ESP_RST_POWERON || bootCounter > 100000) bootCounter = 1;
  else bootCounter++;

  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  ledcAttach(ENA, PWM_FREQ_HZ, PWM_RES_BITS);
  ledcAttach(ENB, PWM_FREQ_HZ, PWM_RES_BITS);
  stopAll();   // estado seguro antes que nada

  pinMode(ENC_M1_A, INPUT_PULLUP); pinMode(ENC_M1_B, INPUT_PULLUP);
  pinMode(ENC_M2_A, INPUT_PULLUP); pinMode(ENC_M2_B, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENC_M1_A), isrM1, RISING);
  attachInterrupt(digitalPinToInterrupt(ENC_M2_A), isrM2, RISING);

  WiFi.softAP("RobotBalance_Test3", "balance2026");
  Serial.println();
  Serial.println("=== TEST 3 - Encoders + zona muerta ===");
  Serial.print("AP: RobotBalance_Test3  ->  http://");
  Serial.println(WiFi.softAPIP());
  Serial.print("Arranque numero: "); Serial.println(bootCounter);

  server.on("/", []() { server.send_P(200, "text/html", PAGE); });
  server.on("/data", handleData);
  server.on("/reset", []() { encM1 = 0; encM2 = 0; server.send(200, "text/plain", "ok"); });
  server.on("/stop", []() {
    abortFlag = true; stopAll();
    statusMsg = "PARADA manual.";
    server.send(200, "text/plain", "ok");
  });
  server.on("/ppr", []() {
    float turns = server.arg("turns").toFloat();
    if (turns > 0) {
      pprM1 = fabsf((float)encM1) / turns;
      pprM2 = fabsf((float)encM2) / turns;
      statusMsg = "PPR calculado sobre " + String(turns, 0) + " vueltas.";
    }
    server.send(200, "text/plain", "ok");
  });
  server.on("/start", []() {
    if (activePhase != PH_IDLE) { server.send(409, "text/plain", "ocupado"); return; }
    abortFlag = false; progressPct = 0;
    uint8_t p = server.arg("p").toInt();
    if (p == 1 || p == 2) activePhase = (Phase)p;
    server.send(200, "text/plain", "ok");
  });
  server.begin();

  xTaskCreatePinnedToCore(measureTask, "measure", 4096, NULL, 1, NULL, 1);
}

void loop() {
  server.handleClient();
  delay(2);
}
