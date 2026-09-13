/*
  ========================================================================
  TEST 5 — Medicion de K_U (acoplamiento comando de motor -> aceleracion
           angular del cuerpo) + zona muerta, en frio y en caliente
  ========================================================================

  POR QUE EXISTE:
  El modelo del pendulo invertido usado en el proyecto de PSO es

      theta'' = OMEGA0^2 * theta  +  K_U * u_efectivo

  OMEGA0, la zona muerta (~85) y el cap de PWM estan medidos sobre el robot
  real. K_U NO: hasta ahora es un valor asumido (0.03), y es el que escala
  directamente el umbral teorico de Kp. Este test lo mide.

  ⛔ SKETCH SUPERADO (2026-08-15). Usar test5b_ku_estatico_esp32 en su lugar.
     Dos razones:

     1. Se corrio el 2026-08-14 y no dio un K_U usable (ver el registro 7.15).
     2. Fue escrito cuando OMEGA0 valia 2.33 rad/s y el cap 121. Los dos
        cambiaron: OMEGA0 real = 7.4 rad/s (7.12) y cap = 160. Como K_U
        escala con OMEGA0^2, el umbral de pasa/no pasa paso de 0.004 a 0.030.

     El test5b mide la desviacion ESTATICA con el eje trabado en vez de
     derivar el giroscopio durante un transitorio.

  IDEA DEL METODO:
  Con el robot SUSPENDIDO DEL EJE DE LAS RUEDAS y colgando en reposo, el
  torque de gravedad es cero en esa posicion de equilibrio. Si en ese
  instante se aplica un escalon de duty conocido, la ecuacion de arriba
  se reduce (mientras theta siga siendo chico) a:

      theta'' ~= K_U * u        =>   K_U = theta'' / u

  theta'' se obtiene del PICO de velocidad angular medido por el giroscopio
  durante el transitorio de arranque -- ver `struct Medicion` para por que se
  abandono el estimador por pendiente que tenia la version original.

  ⚠️ EJE DEL GIROSCOPIO: se usa **giro X**, no Y. La convencion del
  proyecto (CLAUDE.md) dice "Y = inclinacion/pitch" refiriendose al eje
  del ACELEROMETRO que apunta al frente. La ROTACION sobre el eje de las
  ruedas -- que es la que inclina al robot -- registra en **giro X**,
  confirmado en el Test 1b (pico 0.571 rad/s en X vs 0.137/0.041 en Z/Y).
  Derivar el eje equivocado daria un K_U sin sentido.

  BONUS -- hipotesis del calentamiento:
  El mismo barrido corre dos veces: una en FRIO (motores sin usar) y otra
  en CALIENTE (tras 30 s de movimiento continuo). Si la zona muerta baja
  en caliente, queda confirmado que conviene calentar los motores antes de
  evaluar el equilibrio; si las dos curvas se superponen, la hipotesis
  queda descartada con evidencia. Los datos previos del Test 3 NO la
  respaldan (el umbral subia entre repeticiones, no bajaba), pero ese
  analisis tenia n chico y mucho ruido de cogging.

  ------------------------------------------------------------------------
  MONTAJE FISICO

  1. Suspender el robot DEL EJE DE LAS RUEDAS, colgando libre. Las ruedas
     deben poder girar sin tocar nada.

     ⛔ NO SOSTENERLO A MANO. La corrida del 2026-08-14 se hizo asi y fallo:
     la mano no solo agrega ruido, ABSORBE el torque de reaccion que es
     justamente lo que se quiere medir (se midieron theta de ~0.05 grados).
     Hace falta un punto de anclaje RIGIDO -- una barra fija, o abrazaderas
     del eje al borde de una mesa. Nada blando entre el eje y el anclaje.

  2. Dejarlo quieto y esperar a que deje de oscilar ANTES de arrancar.
     Con anclaje rigido oscila MAS que sostenido a mano (la mano amortiguaba),
     asi que este paso pasa a importar mas, no menos.
  3. Alimentar normalmente (bateria -> Buck -> riel logico; L298N con su
     VIN+ y su pin logico "5V").

  ⚠️ El robot va a dar tirones en cada escalon. Asegurarse de que la
     suspension aguante y de que nada quede al alcance de las ruedas.

  ------------------------------------------------------------------------
  CABLEADO: identico al Test 3 (docs/conexiones-registro-pruebas.md, 3.1)
    I2C:     SDA=GPIO21, SCL=GPIO22
    Motor 1: ENA=GPIO27, IN1=GPIO14, IN2=GPIO13
    Motor 2: ENB=GPIO4,  IN3=GPIO16, IN4=GPIO17

  REPORTE: WiFi, AP "RobotBalance_Test5" (clave balance2026) -> 192.168.4.1
           y tambien por Serial a 115200.
  ========================================================================
*/

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// --- Pines ---------------------------------------------------------------
const uint8_t I2C_SDA = 21, I2C_SCL = 22;
const uint8_t ENA = 27, IN1 = 14, IN2 = 13;
const uint8_t ENB = 4,  IN3 = 16, IN4 = 17;

// --- PWM (LEDC, API core 3.x) --------------------------------------------
const uint32_t PWM_FREQ_HZ  = 20000;
const uint8_t  PWM_RES_BITS = 8;

/*
  Tope subido de 121 a 160 (revision del 2026-08-14, Test 6). El 121 asumia que
  al motor le llega todo el voltaje del bus; medido por encoder, a duty 121 el
  motor recibe el equivalente a ~2.6V y recien a 160 llega a ~5.0V. K_U es una
  PENDIENTE, y una pendiente se estima mejor sobre un rango ancho: medirla solo
  hasta 121 era medirla con el motor a media maquina.
*/
const uint8_t  MAX_PWM_DUTY = 160;

// --- Parametros del experimento ------------------------------------------
/*
  Se sacaron los duties de zona muerta (60, 80, 90). El Test 5 mueve los dos
  motores juntos y M2 no arranca hasta duty ~90, asi que todo lo de abajo solo
  medía ruido. Queda el 70 a proposito como CONTROL NULO: da la referencia de
  cuanto vale el ruido cuando se sabe que no hay señal fisica.
  El tiempo liberado se reinvierte en mas repeticiones y en el tramo alto.
*/
const uint8_t  DUTIES[]   = {70, 100, 110, 121, 135, 150, 160};
const uint8_t  N_DUTIES   = sizeof(DUTIES) / sizeof(DUTIES[0]);
const uint8_t  N_REPS     = 5;      // era 3: con esta dispersion no alcanzaba
const uint16_t MEASURE_MS = 500;    // era 250: el transitorio de arranque dura mas
const uint16_t REST_MS    = 8000;   // espera a que deje de oscilar entre escalones
const uint16_t WARMUP_MS  = 30000;  // movimiento continuo antes del barrido "caliente"
const uint8_t  WARMUP_DUTY = 110;

const uint16_t MAX_SAMPLES = 900;   // holgado para MEASURE_MS a ~500 Hz

// Duty para el que se vuelca la traza cruda (rep 1), y cuantos puntos imprimir.
const uint8_t  DUTY_TRAZA    = 121;
const uint8_t  PUNTOS_TRAZA  = 50;

const int8_t FWD = 1, REV = -1, STOP = 0;

// --- Estado --------------------------------------------------------------
Adafruit_MPU6050 mpu;
WebServer server(80);

float gyroBiasX = 0;

/*
  ESTIMADOR (cambiado el 2026-08-14).

  El estimador viejo era la pendiente de giroX(t) por minimos cuadrados sobre la
  ventana entera, que asume aceleracion CONSTANTE. Con las ruedas al aire eso es
  falso: el motor acelera la rueda, entrega un golpe de torque de reaccion sobre
  el cuerpo, y despues la rueda queda girando libre y la reaccion cae casi a
  cero. Ajustar una recta a un pico que sube y baja da cualquier cosa, incluso
  signo negativo -- que es exactamente lo que paso en la corrida del 2026-08-14.

  Estimador nuevo: se busca el PICO de velocidad angular y el instante en que
  ocurre. La aceleracion media durante la subida es omegaPico/tPico, y de ahi

      K_U = (omegaPico / tPico) / duty

  Es robusto a lo que pase despues del pico, que es justo la parte que arruinaba
  el ajuste lineal. Se conserva la pendiente vieja en paralelo para poder
  comparar los dos estimadores sobre los mismos datos.
*/
struct Medicion {
  float omegaPico;    // rad/s, con signo
  float tPico;        // s desde el escalon
  float alphaMedia;   // rad/s^2 = omegaPico/tPico
  float ku;           // alphaMedia/duty
  float thetaDeg;     // grados recorridos (control de validez)
  float pendiente;    // estimador viejo, solo para comparar
};

struct Punto {
  uint8_t   duty;
  Medicion  m[N_REPS];
  uint8_t   n;
};

/*
  Selector de campo para promediar cualquier metrica de la struct. Va aca arriba
  y no junto a `promedio()` porque el IDE de Arduino genera los prototipos de las
  funciones al principio del archivo: si el enum se declara despues, el prototipo
  de `promedio(const Punto&, Campo)` no compila.
*/
enum Campo { C_ALPHA, C_KU, C_THETA, C_WPICO, C_TPICO, C_PEND };

Punto fria[N_DUTIES];
Punto caliente[N_DUTIES];
volatile bool friaLista = false, calienteLista = false;

volatile bool  corriendo = false;
volatile uint8_t faseActiva = 0;   // 0=idle, 1=barrido frio, 2=barrido caliente
volatile bool  abortar = false;
volatile uint8_t progreso = 0;

/*
  Buffer fijo, no String: `estado` lo escribe la tarea de medicion y lo lee
  el handler web desde otro contexto. Un String se realoca al reasignarse,
  asi que el lector podria quedar apuntando a memoria liberada. Con un char[]
  el peor caso es leer un mensaje a medio escribir -- feo, pero no un crash.
*/
char estado[96] = "Listo. Robot suspendido del eje y quieto.";
void setEstado(const char* fmt, ...) {
  va_list ap; va_start(ap, fmt);
  vsnprintf(estado, sizeof(estado), fmt, ap);
  va_end(ap);
}

// --- Control de motores --------------------------------------------------
void setMotor(uint8_t enPin, uint8_t inA, uint8_t inB, int8_t dir, uint8_t duty) {
  duty = constrain(duty, 0, MAX_PWM_DUTY);
  if (dir > 0)      { digitalWrite(inA, HIGH); digitalWrite(inB, LOW);  }
  else if (dir < 0) { digitalWrite(inA, LOW);  digitalWrite(inB, HIGH); }
  else              { digitalWrite(inA, LOW);  digitalWrite(inB, LOW); duty = 0; }
  ledcWrite(enPin, duty);
}

// Ambos motores juntos: es la condicion real de operacion, y duplica el
// torque de reaccion sobre el cuerpo (mejor relacion senal/ruido).
void ambos(int8_t dir, uint8_t duty) {
  setMotor(ENA, IN1, IN2, dir, duty);
  setMotor(ENB, IN3, IN4, dir, duty);
}
void stopAll() { ambos(STOP, 0); }

// --- Giroscopio ----------------------------------------------------------
float leerGiroX() {
  sensors_event_t a, g, t;
  mpu.getEvent(&a, &g, &t);
  return g.gyro.x - gyroBiasX;
}

void calibrarGiro(uint16_t muestras = 400) {
  setEstado("Calibrando giroscopio (mantener quieto)...");
  double suma = 0;
  for (uint16_t i = 0; i < muestras; i++) {
    sensors_event_t a, g, t;
    mpu.getEvent(&a, &g, &t);
    suma += g.gyro.x;
    delay(3);
  }
  gyroBiasX = (float)(suma / muestras);
  Serial.print(F("Bias giro X = ")); Serial.println(gyroBiasX, 5);
}

/*
  Aplica un escalon de duty, captura giroX(t) y devuelve las metricas del
  escalon. Ver el comentario de `struct Medicion` para el porque del estimador.

  `volcarTraza` imprime la traza cruda decimada por Serial: sirve para VER la
  forma real de la respuesta en vez de suponerla.
*/
Medicion medirEscalon(uint8_t duty, bool volcarTraza) {
  static float ts[MAX_SAMPLES], gs[MAX_SAMPLES];
  Medicion r = {0, 0, 0, 0, 0, 0};
  uint16_t n = 0;

  uint32_t t0 = micros();
  ambos(FWD, duty);

  while ((micros() - t0) < (uint32_t)MEASURE_MS * 1000UL && n < MAX_SAMPLES) {
    ts[n] = (micros() - t0) / 1e6f;
    gs[n] = leerGiroX();
    n++;
  }
  stopAll();

  if (n < 10) return r;

  /*
    Para BUSCAR el pico se usa un promedio movil de 5 muestras: sin eso, una
    sola muestra ruidosa se lleva el maximo y el estimador queda atado al ruido.
    No es un filtro de fusion -- los datos crudos se conservan para todo lo
    demas (integracion y pendiente).
  */
  const uint8_t W = 5;
  uint16_t iPico = 0;
  float    vPico = 0;
  for (uint16_t i = W / 2; i < n - W / 2; i++) {
    float s = 0;
    for (uint8_t k = 0; k < W; k++) s += gs[i - W / 2 + k];
    s /= W;
    if (fabs(s) > fabs(vPico)) { vPico = s; iPico = i; }
  }
  r.omegaPico = vPico;
  r.tPico     = ts[iPico];
  r.alphaMedia = (r.tPico > 1e-4f) ? (r.omegaPico / r.tPico) : 0;
  r.ku         = (duty > 0) ? (r.alphaMedia / duty) : 0;

  // integracion trapezoidal de la velocidad angular -> angulo recorrido
  double theta = 0;
  for (uint16_t i = 1; i < n; i++) {
    theta += 0.5 * (gs[i] + gs[i - 1]) * (ts[i] - ts[i - 1]);
  }
  r.thetaDeg = fabs(theta) * 180.0 / PI;

  // estimador viejo (minimos cuadrados), solo para comparar
  double st = 0, sg = 0, stt = 0, stg = 0;
  for (uint16_t i = 0; i < n; i++) {
    st += ts[i]; sg += gs[i];
    stt += (double)ts[i] * ts[i];
    stg += (double)ts[i] * gs[i];
  }
  double den = n * stt - st * st;
  r.pendiente = (den != 0) ? (float)((n * stg - st * sg) / den) : 0;

  if (volcarTraza) {
    Serial.print(F("\n  --- traza cruda, duty ")); Serial.print(duty);
    Serial.print(F(" (")); Serial.print(n); Serial.println(F(" muestras) ---"));
    Serial.println(F("  t_s,giroX_rad_s"));
    uint16_t paso = (n > PUNTOS_TRAZA) ? (n / PUNTOS_TRAZA) : 1;
    for (uint16_t i = 0; i < n; i += paso) {
      Serial.print(F("  ")); Serial.print(ts[i], 4);
      Serial.print(','); Serial.println(gs[i], 4);
    }
    Serial.println(F("  --- fin traza ---"));
  }

  return r;
}

void correrBarrido(Punto* dest, const char* etiqueta) {
  Serial.print(F("\n=== Barrido ")); Serial.print(etiqueta); Serial.println(F(" ==="));
  for (uint8_t d = 0; d < N_DUTIES && !abortar; d++) {
    dest[d].duty = DUTIES[d];
    dest[d].n = 0;
    for (uint8_t r = 0; r < N_REPS && !abortar; r++) {
      setEstado("Barrido %s: duty %u rep %u/%u", etiqueta, DUTIES[d], r + 1, N_REPS);

      bool traza = (DUTIES[d] == DUTY_TRAZA && r == 0);
      Medicion m = medirEscalon(DUTIES[d], traza);

      dest[d].m[r] = m;
      dest[d].n++;

      Serial.print(F("  duty=")); Serial.print(DUTIES[d]);
      Serial.print(F(" rep=")); Serial.print(r + 1);
      Serial.print(F("  w_pico=")); Serial.print(m.omegaPico, 4);
      Serial.print(F(" @")); Serial.print(m.tPico, 3); Serial.print(F("s"));
      Serial.print(F("  alpha=")); Serial.print(m.alphaMedia, 3);
      Serial.print(F(" rad/s2  K_U=")); Serial.print(m.ku, 5);
      Serial.print(F("  theta=")); Serial.print(m.thetaDeg, 2);
      Serial.print(F(" deg  [pend_vieja=")); Serial.print(m.pendiente, 3);
      Serial.println(F("]"));

      progreso = (uint8_t)(100.0 * (d * N_REPS + r + 1) / (N_DUTIES * N_REPS));

      // esperar a que deje de oscilar antes del proximo escalon
      uint32_t t0 = millis();
      while (millis() - t0 < REST_MS && !abortar) delay(20);
    }
  }
  stopAll();
}

void tareaMedicion(void* pv) {
  for (;;) {
    uint8_t f = faseActiva;
    if (f == 1 || f == 2) {
      corriendo = true;
      calibrarGiro();

      if (f == 2) {
        setEstado("Calentando motores (30 s de movimiento continuo)...");
        Serial.println(F("\nCalentando motores..."));
        uint32_t t0 = millis();
        while (millis() - t0 < WARMUP_MS && !abortar) {
          ambos(FWD, WARMUP_DUTY); vTaskDelay(pdMS_TO_TICKS(700));
          ambos(REV, WARMUP_DUTY); vTaskDelay(pdMS_TO_TICKS(700));
        }
        stopAll();
        setEstado("Esperando que deje de oscilar tras el calentamiento...");
        vTaskDelay(pdMS_TO_TICKS(REST_MS));
        calibrarGiro();  // el bias del giro deriva con la temperatura
      }

      correrBarrido(f == 1 ? fria : caliente, f == 1 ? "FRIO" : "CALIENTE");
      if (f == 1) friaLista = true; else calienteLista = true;

      if (abortar) setEstado("Abortado.");
      else setEstado("Barrido %s completo.", f == 1 ? "frio" : "caliente");
      faseActiva = 0; abortar = false; corriendo = false;
    } else {
      vTaskDelay(pdMS_TO_TICKS(50));
    }
  }
}

// --- Pagina web ----------------------------------------------------------
const char PAGE[] PROGMEM = R"HTML(
<!DOCTYPE html><html lang="es"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Test 5 - K_U</title><style>
:root{--bg:#12151c;--card:#191e28;--ink:#e9e6df;--dim:#9aa0ad;--faint:#6b7180;
--ok:#4fae63;--warn:#d98e3a;--bad:#d1554a;--acc:#5b9fd9;--rule:#2a3040;--mono:Menlo,monospace}
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
table{width:100%;border-collapse:collapse;font-size:.76rem;margin-top:.4rem}
th{text-align:left;font-family:var(--mono);font-size:.62rem;text-transform:uppercase;
color:var(--dim);border-bottom:1px solid var(--rule);padding:.3rem}
td{padding:.3rem;border-bottom:1px solid var(--rule);font-family:var(--mono)}
.note{font-size:.72rem;color:var(--faint);margin-top:.5rem}
.bar{height:5px;background:var(--rule);border-radius:3px;overflow:hidden;margin-top:.5rem}
.bar i{display:block;height:100%;background:var(--acc);width:0}
.big{font-size:1.3rem;font-weight:700;font-family:var(--mono);color:var(--acc)}
</style></head><body>
<h1>Test 5 — K_U y zona muerta (frío vs caliente)</h1>
<div class="sub">Robot suspendido del eje de las ruedas, colgando libre</div>

<div class="card">
  <h2>Estado</h2>
  <div id="st">—</div>
  <div class="bar"><i id="bar"></i></div>
  <div style="margin-top:.6rem">
    <button onclick="go('/start?f=1')">Barrido en FRÍO</button>
    <button onclick="go('/start?f=2')">Barrido en CALIENTE</button>
    <button class="stop" onclick="go('/stop')">PARAR</button>
  </div>
  <div class="note">Correr primero el frío, con los motores sin usar. El caliente
  mueve los motores 30 s antes de medir.</div>
</div>

<div class="card">
  <h2>K_U estimado</h2>
  <div>frío: <span class="big" id="kuf">—</span> &nbsp; caliente: <span class="big" id="kuc">—</span></div>
  <div class="note">Promedio de K_U sobre los escalones que superaron la zona muerta.
  Este es el número que reemplaza el 0.03 asumido en el notebook de PSO.</div>
</div>

<div class="card">
  <h2>Detalle por escalón</h2>
  <table><thead><tr><th>duty</th><th>ω pico</th><th>α frío</th><th>K_U frío</th><th>α cal.</th><th>K_U cal.</th><th>θ rec.</th></tr></thead>
  <tbody id="tb"><tr><td colspan="7" style="color:var(--faint)">sin datos</td></tr></tbody></table>
  <div class="note">α = ω_pico/t_pico, la aceleración media durante la subida.
  <b>Lo que hay que mirar es si K_U es parecido en todos los duties</b> — si lo es,
  el modelo lineal se sostiene. El duty 70 es control nulo: ahí se espera ruido.
  θ recorrido &gt; ~5° significa que la gravedad dejó de ser despreciable.</div>
</div>

<script>
function go(u){fetch(u).then(()=>tick())}
function f(x,d){return (x===null||x===undefined)?'—':Number(x).toFixed(d)}
function tick(){fetch('/data').then(r=>r.json()).then(d=>{
  document.getElementById('st').textContent=d.msg;
  document.getElementById('bar').style.width=d.pct+'%';
  document.getElementById('kuf').textContent=d.kuf>0?f(d.kuf,4):'—';
  document.getElementById('kuc').textContent=d.kuc>0?f(d.kuc,4):'—';
  var rows='';
  for(var i=0;i<d.duties.length;i++){
    rows+='<tr><td>'+d.duties[i]+'</td><td>'+f(d.wf[i],3)+'</td><td>'+f(d.af[i],3)+
          '</td><td>'+f(d.kf[i],5)+'</td><td>'+f(d.ac[i],3)+'</td><td>'+f(d.kc[i],5)+
          '</td><td>'+f(d.th[i],1)+'°</td></tr>';
  }
  if(rows) document.getElementById('tb').innerHTML=rows;
})}
setInterval(tick,800);tick();
</script></body></html>
)HTML";

float promedio(const Punto& p, Campo c) {
  if (p.n == 0) return 0;
  float s = 0;
  for (uint8_t i = 0; i < p.n; i++) {
    switch (c) {
      case C_ALPHA: s += p.m[i].alphaMedia; break;
      case C_KU:    s += p.m[i].ku;         break;
      case C_THETA: s += p.m[i].thetaDeg;   break;
      case C_WPICO: s += p.m[i].omegaPico;  break;
      case C_TPICO: s += p.m[i].tPico;      break;
      case C_PEND:  s += p.m[i].pendiente;  break;
    }
  }
  return s / p.n;
}

/*
  K_U global: promedio sobre los escalones que superaron la zona muerta.

  El duty de control nulo (70) queda excluido por el umbral, que es justamente
  su razon de ser: da la referencia de ruido contra la cual se decide que
  escalones tienen señal real.
*/
float kuGlobal(const Punto* p, bool lista) {
  if (!lista) return 0;
  float suma = 0; uint8_t cuenta = 0;
  for (uint8_t d = 0; d < N_DUTIES; d++) {
    if (fabs(promedio(p[d], C_ALPHA)) > 0.15f) {   // "se movio de verdad"
      suma += fabs(promedio(p[d], C_KU));
      cuenta++;
    }
  }
  return cuenta ? suma / cuenta : 0;
}

void handleData() {
  String j = "{";
  j += "\"msg\":\"" + String(estado) + "\",\"pct\":" + String(progreso);
  j += ",\"kuf\":" + String(kuGlobal(fria, friaLista), 5);
  j += ",\"kuc\":" + String(kuGlobal(caliente, calienteLista), 5);

  j += ",\"duties\":[";
  for (uint8_t d = 0; d < N_DUTIES; d++) { j += String(DUTIES[d]); if (d + 1 < N_DUTIES) j += ","; }
  j += "],\"wf\":[";
  for (uint8_t d = 0; d < N_DUTIES; d++) { j += String(promedio(fria[d], C_WPICO), 4); if (d + 1 < N_DUTIES) j += ","; }
  j += "],\"af\":[";
  for (uint8_t d = 0; d < N_DUTIES; d++) { j += String(promedio(fria[d], C_ALPHA), 4); if (d + 1 < N_DUTIES) j += ","; }
  j += "],\"kf\":[";
  for (uint8_t d = 0; d < N_DUTIES; d++) { j += String(promedio(fria[d], C_KU), 5); if (d + 1 < N_DUTIES) j += ","; }
  j += "],\"ac\":[";
  for (uint8_t d = 0; d < N_DUTIES; d++) { j += String(promedio(caliente[d], C_ALPHA), 4); if (d + 1 < N_DUTIES) j += ","; }
  j += "],\"kc\":[";
  for (uint8_t d = 0; d < N_DUTIES; d++) { j += String(promedio(caliente[d], C_KU), 5); if (d + 1 < N_DUTIES) j += ","; }
  j += "],\"th\":[";
  for (uint8_t d = 0; d < N_DUTIES; d++) { j += String(promedio(fria[d], C_THETA), 2); if (d + 1 < N_DUTIES) j += ","; }
  j += "]}";
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

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);  // 400 kHz: mas muestras en la ventana de 250 ms
  if (!mpu.begin()) {
    Serial.println(F("ERROR: MPU6050 no responde. Revisar I2C."));
    while (1) delay(1000);
  }
  mpu.setGyroRange(MPU6050_RANGE_250_DEG);
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setFilterBandwidth(MPU6050_BAND_94_HZ);  // ancho para no atenuar el escalon

  for (uint8_t d = 0; d < N_DUTIES; d++) { fria[d].n = 0; caliente[d].n = 0; }

  WiFi.softAP("RobotBalance_Test5", "balance2026");
  Serial.println(F("\n=== TEST 5 - K_U y zona muerta ==="));
  Serial.print(F("AP: RobotBalance_Test5 -> http://"));
  Serial.println(WiFi.softAPIP());

  server.on("/", []() { server.send_P(200, "text/html", PAGE); });
  server.on("/data", handleData);
  server.on("/stop", []() {
    abortar = true; stopAll(); setEstado("PARADA manual.");
    server.send(200, "text/plain", "ok");
  });
  server.on("/start", []() {
    if (corriendo) { server.send(409, "text/plain", "ocupado"); return; }
    uint8_t f = server.arg("f").toInt();
    if (f == 1 || f == 2) { abortar = false; progreso = 0; faseActiva = f; }
    server.send(200, "text/plain", "ok");
  });
  server.begin();

  xTaskCreatePinnedToCore(tareaMedicion, "medicion", 8192, NULL, 1, NULL, 1);
}

void loop() {
  server.handleClient();
  delay(2);
}
