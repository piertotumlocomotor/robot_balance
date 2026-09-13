/*
  ========================================================================
  TEST 8b — Caida del L298N por barrido de duty + ajuste lineal
  ========================================================================

  ⛔⛔ SUPERADO — NO USAR SUS RESULTADOS (corrido y descartado 2026-08-17)
  ⛔⛔ Ver docs/conexiones-registro-pruebas.md 7.18

  Diagnostico bien el error del Test 8, pero SU PROPIO MODELO TAMBIEN ES
  FALSO. Asume que V_off es CONSTANTE y no lo es: depende de cuanta corriente
  habia, o sea del duty. Ninguna recta puede describir eso.

  Evidencia: 4 barridos (M1 y M2, dos repeticiones cada uno) dieron R2 entre
  0.77 y 0.83. Y el ajuste esta MAL CONDICIONADO — con los 9 puntos de M1 da
  V_off=-0.60 V y caida 9.10 V; con solo los tres duties altos da V_off=-3.08 V
  y caida 6.76 V. Si la respuesta cambia tanto segun que puntos entren, los
  datos no determinan los parametros.

  ⚠️ ADEMAS ESTE SKETCH IMPRIME FISICA EQUIVOCADA. Su cota optimista asume
  que un puente de MOSFETs llevaria V_off a ~0. Es falso: V_off lo fija el
  MODO DE DECAIMIENTO, no la tecnologia del transistor. Un puente de MOSFETs
  cableado igual tendria el mismo V_off negativo. Ignorar por completo el
  bloque "Si se cambia el driver por MOSFETs" y el veredicto contra el
  deficit de 7.17.

  ⚠️ Su barrido usa siempre el mismo orden de duty (60,90,120,150), asi que
  cualquier efecto dependiente del orden (calentamiento acumulado) reproduce
  el mismo patron de residuos. Faltaba randomizar. La conclusion de que el
  modelo lineal no sirve se sostiene por la inconsistencia entre duties, no
  por que el patron se repita.

  ⚠️ Sus puntos de duty bajo casi no aportan: entre repeticiones, duty 60 dio
  0.40 y 0.60 V, duty 90 dio 0.40 y 1.00 V (~50% de dispersion).

  LO QUE SI QUEDO EN PIE de este sketch, y vale para el resto del proyecto:
  el V_off negativo es la firma de estar en COAST (PWM sobre ENABLE). Ese
  hallazgo es el que motiva el Test 9.

  => Lo que se hace en su lugar: medir el torque DIRECTO con la balanza.
     Ver test9_decay_torque_esp32/.

  Se conserva como historial del metodo.

  ------------------------------------------------------------------------

  POR QUE EXISTE (que estaba mal en el Test 8)

  El Test 8 asumia que durante el tramo OFF del PWM el motor ve 0 V:

      V_medido = (duty/255) * (V_bus - V_caida)

  Con una carga inductiva y el PWM aplicado sobre el pin ENABLE eso NO es
  cierto. Cuando ENA baja, las salidas del puente quedan en alta impedancia
  y la corriente del motor sigue circulando por los diodos, poniendo las
  pestanas a tension NEGATIVA, no a cero.

  Evidencia de los datos del propio Test 8 (M1, rueda trabada):

      duty 150 -> V=1.8 V -> caida implicita 9.08 V
      duty 160 -> V=2.3 V -> caida implicita 8.45 V

  Si el modelo fuera correcto las dos caidas tendrian que dar IGUAL. Difieren
  0.6 V entre dos duties separados apenas 4% -> error sistematico del modelo,
  no dispersion de medicion.

  ------------------------------------------------------------------------
  EL MODELO CORRECTO

  Sin asumir nada sobre el tramo OFF:

      V_medido = frac * V_on + (1 - frac) * V_off        frac = duty/255

  Reordenado, es una RECTA en frac:

      V_medido = (V_on - V_off) * frac + V_off
                 \_____________/         \___/
                    pendiente          ordenada

  Entonces, midiendo a varios duty y ajustando una recta:

      V_off = ordenada al origen
      V_on  = pendiente + ordenada
      caida real del puente en conduccion = V_bus - V_on

  Nada de esto asume V_off = 0. Si el ajuste devuelve V_off ~ 0, el modelo
  viejo era correcto despues de todo; si devuelve negativo, no lo era.

  ⚠️ Hacen falta duties BIEN SEPARADOS. Con 150 y 160 la extrapolacion
  amplifica el error de lectura ~10x. Por eso el barrido usa 60..150.

  ------------------------------------------------------------------------
  COMO SE USA

    b -> cargar V_bus (VIN+ del L298N contra GND de potencia)
    1 -> motor 1     2 -> motor 2     i -> invertir sentido
    s -> BARRIDO completo (60, 90, 120, 150) - es el modo principal
    d -> fijar duty para un punto suelto
    p -> medir UN punto al duty actual y sumarlo al set
    f -> ajustar la recta con los puntos cargados
    l -> listar puntos     r -> borrar puntos     x -> parar

  ⚠️ RUEDA TRABADA. Es la condicion de corriente maxima y la misma en la
  que se midio el torque del Test 7. Sirve el hilo+pesa del Test 7, una
  morsa o una cuna: solo hace falta que la rueda NO gire.

  ⚠️ PUNTAS ENGANCHADAS ANTES DE ARRANCAR. La ventana es de 8 s.

  ⚠️ DEJAR ENFRIAR ENTRE PUNTOS. En stall el L298N y el motor disipan
  fuerte. El barrido pide ENTER entre puntos justamente para eso.

  ⚠️ TODO EL BARRIDO EN LA MISMA CONDICION. Mismo motor, mismo sentido,
  misma traba, mismas puntas. Si cambia algo a mitad de camino, la recta
  ajusta datos de dos sistemas distintos y el resultado no significa nada.

  CABLEADO: identico al Test 3/5/7/8 (docs/conexiones-registro-pruebas.md 3.1)
    Motor 1: ENA=GPIO27, IN1=GPIO14, IN2=GPIO13
    Motor 2: ENB=GPIO4,  IN3=GPIO16, IN4=GPIO17
  ========================================================================
*/

#include <Arduino.h>

// --- Pines ---------------------------------------------------------------
const uint8_t ENA = 27, IN1 = 14, IN2 = 13;
const uint8_t ENB = 4,  IN3 = 16, IN4 = 17;

// --- PWM (LEDC, API core 3.x) --------------------------------------------
const uint32_t PWM_FREQ_HZ  = 20000;
const uint8_t  PWM_RES_BITS = 8;
const uint8_t  MAX_PWM_DUTY = 160;   // tope absoluto, ninguna ruta lo cruza

const uint16_t VENTANA_MS   = 8000;
const float    V_MOSFET     = 0.2f;  // caida tipica de un puente de MOSFETs
const uint8_t  DUTY_OPERACION = 160; // punto en el que se evalua la ganancia
const float    DEFICIT_717  = 2.42f; // deficit de torque medido en 7.17

// Duties del barrido: bien separados, para que el ajuste no extrapole.
const uint8_t DUTIES[]  = {60, 90, 120, 150};
const uint8_t N_DUTIES  = sizeof(DUTIES) / sizeof(DUTIES[0]);

// --- Set de puntos -------------------------------------------------------
const uint8_t N_MAX = 12;
uint8_t nPts = 0;
uint8_t dutyPt[N_MAX];
float   vPt[N_MAX];

// --- Estado --------------------------------------------------------------
float   vBus    = 0;
uint8_t duty    = 120;
uint8_t motor   = 1;
int8_t  sentido = 1;

// --- Control de motores --------------------------------------------------
void aplicar(uint8_t m, int8_t dir, uint8_t d) {
  d = constrain(d, 0, MAX_PWM_DUTY);
  uint8_t en = (m == 1) ? ENA : ENB;
  uint8_t a  = (m == 1) ? IN1 : IN3;
  uint8_t b  = (m == 1) ? IN2 : IN4;
  if (dir > 0)      { digitalWrite(a, HIGH); digitalWrite(b, LOW);  }
  else if (dir < 0) { digitalWrite(a, LOW);  digitalWrite(b, HIGH); }
  else              { digitalWrite(a, LOW);  digitalWrite(b, LOW); d = 0; }
  ledcWrite(en, d);
}

void pararTodo() { aplicar(1, 0, 0); aplicar(2, 0, 0); }

// --- Entrada por Serial --------------------------------------------------
void vaciarEntrada() { while (Serial.available()) Serial.read(); }

float leerNumero(const char* prompt) {
  Serial.print(prompt); Serial.flush(); vaciarEntrada();
  String buf = "";
  while (true) {
    if (!Serial.available()) { delay(10); continue; }
    char c = Serial.read();
    if (c == 'x') { Serial.println(F(" [cancelado]")); return NAN; }
    if (c == '\n' || c == '\r') {
      if (buf.length() == 0) continue;
      Serial.println(buf);
      return buf.toFloat();
    }
    if ((c >= '0' && c <= '9') || c == '.' || c == '-') buf += c;
  }
}

bool esperarEnter(const char* prompt) {
  Serial.print(prompt); Serial.flush(); vaciarEntrada();
  while (true) {
    if (!Serial.available()) { delay(10); continue; }
    char c = Serial.read();
    if (c == 'x') { Serial.println(F(" [cancelado]")); return false; }
    if (c == '\n' || c == '\r') { Serial.println(); return true; }
  }
}

// --- Medicion de UN punto ------------------------------------------------
// Devuelve false si el operador cancelo.
bool medirPunto(uint8_t d) {
  if (nPts >= N_MAX) { Serial.println(F("\nSet lleno. Usar 'r' para borrar.")); return false; }

  Serial.print(F("\n--- Punto: motor ")); Serial.print(motor);
  Serial.print(F(", ")); Serial.print(sentido > 0 ? F("FWD") : F("REV"));
  Serial.print(F(", duty ")); Serial.print(d);
  Serial.print(F(" (")); Serial.print(100.0f * d / 255.0f, 1); Serial.println(F("%) ---"));
  Serial.println(F("Puntas en las pestanas del motor, multimetro en DC. RUEDA TRABADA."));

  if (!esperarEnter("ENTER cuando este listo (o 'x' para cortar): ")) return false;

  for (uint8_t i = 3; i > 0; i--) { Serial.print(i); Serial.print(F("... ")); Serial.flush(); delay(1000); }
  Serial.println(F("APLICANDO - LEE EL MULTIMETRO"));
  Serial.flush();

  aplicar(motor, sentido, d);
  delay(VENTANA_MS);
  pararTodo();
  Serial.println(F("listo."));

  float v = leerNumero("Tension medida en el motor (V): ");
  if (isnan(v)) return false;

  dutyPt[nPts] = d;
  vPt[nPts]    = v;
  nPts++;
  Serial.print(F("Punto guardado. Total: ")); Serial.println(nPts);
  return true;
}

// --- Listado -------------------------------------------------------------
void listar() {
  Serial.println(F("\n--- Puntos cargados ---"));
  if (nPts == 0) { Serial.println(F("(ninguno)")); return; }
  Serial.println(F("  duty    frac      V_medido"));
  for (uint8_t i = 0; i < nPts; i++) {
    Serial.print(F("  "));   Serial.print(dutyPt[i]);
    Serial.print(F("\t"));   Serial.print(dutyPt[i] / 255.0f, 4);
    Serial.print(F("\t"));   Serial.print(vPt[i], 2);
    Serial.println(F(" V"));
  }
}

// --- Ajuste lineal y reporte --------------------------------------------
void ajustar() {
  if (nPts < 3) {
    Serial.println(F("\nHacen falta al menos 3 puntos para que el ajuste"));
    Serial.println(F("signifique algo. Con 2 la recta pasa exacta y no hay"));
    Serial.println(F("forma de saber si el modelo lineal sirve."));
    return;
  }
  if (vBus <= 0) {
    Serial.println(F("\nFalta V_bus: medir en VIN+ del L298N y cargarlo con 'b'."));
    return;
  }

  // Minimos cuadrados: V = m*frac + b
  float Sf = 0, Sv = 0, Sff = 0, Sfv = 0;
  for (uint8_t i = 0; i < nPts; i++) {
    float f = dutyPt[i] / 255.0f;
    Sf += f; Sv += vPt[i]; Sff += f * f; Sfv += f * vPt[i];
  }
  float n    = (float)nPts;
  float den  = n * Sff - Sf * Sf;
  if (fabsf(den) < 1e-9f) {
    Serial.println(F("\nTodos los puntos estan al mismo duty: no se puede ajustar."));
    return;
  }
  float m = (n * Sfv - Sf * Sv) / den;   // pendiente = V_on - V_off
  float b = (Sv - m * Sf) / n;           // ordenada  = V_off

  float vOff = b;
  float vOn  = m + b;

  // Calidad del ajuste
  float vMed = Sv / n, ssTot = 0, ssRes = 0, maxRes = 0;
  for (uint8_t i = 0; i < nPts; i++) {
    float f    = dutyPt[i] / 255.0f;
    float pred = m * f + b;
    float res  = vPt[i] - pred;
    ssRes += res * res;
    ssTot += (vPt[i] - vMed) * (vPt[i] - vMed);
    if (fabsf(res) > fabsf(maxRes)) maxRes = res;
  }
  float r2 = (ssTot > 1e-9f) ? (1.0f - ssRes / ssTot) : NAN;

  Serial.println(F("\n================ AJUSTE ================"));
  Serial.print(F("Puntos: ")); Serial.print(nPts);
  Serial.print(F("   V_bus: ")); Serial.print(vBus, 2); Serial.println(F(" V"));

  Serial.println(F("\n  duty    V_medido   V_ajuste   residuo"));
  for (uint8_t i = 0; i < nPts; i++) {
    float f    = dutyPt[i] / 255.0f;
    float pred = m * f + b;
    Serial.print(F("  "));  Serial.print(dutyPt[i]);
    Serial.print(F("\t"));  Serial.print(vPt[i], 2);
    Serial.print(F("\t"));  Serial.print(pred, 2);
    Serial.print(F("\t"));  Serial.print(vPt[i] - pred, 2);
    Serial.println();
  }

  Serial.print(F("\nR2 = ")); Serial.print(r2, 4);
  Serial.print(F("   residuo max = ")); Serial.print(maxRes, 2); Serial.println(F(" V"));
  if (!isnan(r2) && r2 < 0.98f)
    Serial.println(F("  ⚠️ R2 bajo: el modelo lineal no describe bien estos datos."));

  Serial.println(F("\n----- Parametros del puente -----"));
  Serial.print(F("V_off (tramo OFF)  = ")); Serial.print(vOff, 2); Serial.println(F(" V"));
  Serial.print(F("V_on  (tramo ON)   = ")); Serial.print(vOn, 2);  Serial.println(F(" V"));
  Serial.print(F("CAIDA EN CONDUCCION = ")); Serial.print(vBus - vOn, 2);
  Serial.println(F(" V   <-- la caida real del L298N"));

  Serial.println(F("\n----- Veredicto sobre el modelo viejo -----"));
  if (fabsf(vOff) < 0.3f) {
    Serial.println(F("V_off ~ 0: el modelo del Test 8 era correcto."));
  } else {
    Serial.println(F("V_off NO es cero -> el Test 8 sobrestimaba la caida."));
    float caidaVieja = vBus - (vOn * (float)DUTY_OPERACION / 255.0f
                       + vOff * (1.0f - (float)DUTY_OPERACION / 255.0f))
                       * 255.0f / (float)DUTY_OPERACION;
    Serial.print(F("  Caida que habria reportado el Test 8 a duty "));
    Serial.print(DUTY_OPERACION); Serial.print(F(": ")); Serial.print(caidaVieja, 2);
    Serial.println(F(" V"));
  }

  // --- Ganancia esperada con un puente de MOSFETs ------------------------
  float frac  = (float)DUTY_OPERACION / 255.0f;
  float vAhora = frac * vOn + (1.0f - frac) * vOff;   // promedio actual
  if (vAhora <= 0.01f) {
    Serial.println(F("\nPromedio no positivo a duty de operacion: revisar datos."));
    Serial.println(F("========================================\n"));
    return;
  }
  float vOnNuevo = vBus - V_MOSFET;
  float ganPeor  = (frac * vOnNuevo + (1.0f - frac) * vOff) / vAhora; // OFF igual
  float ganMejor = (frac * vOnNuevo) / vAhora;                        // OFF ~ 0

  Serial.print(F("\n----- Si se cambia el driver por MOSFETs (a duty "));
  Serial.print(DUTY_OPERACION); Serial.println(F(") -----"));
  Serial.println(F("En stall el torque es proporcional a la tension MEDIA."));
  Serial.print(F("Media actual = ")); Serial.print(vAhora, 2); Serial.println(F(" V"));
  Serial.print(F("Ganancia estimada: entre ")); Serial.print(ganPeor, 2);
  Serial.print(F("x y ")); Serial.print(ganMejor, 2); Serial.println(F("x"));
  Serial.println(F("  (el rango sale de no saber que hace el puente nuevo en el"));
  Serial.println(F("   tramo OFF: cota baja = igual que ahora, alta = OFF ~ 0)"));

  Serial.println(F("\n----- Contra el deficit de 7.17 -----"));
  Serial.print(F("Deficit a cubrir: ")); Serial.print(DEFICIT_717, 2); Serial.println(F("x"));
  Serial.print(F("Quedaria entre ")); Serial.print(DEFICIT_717 / ganMejor, 2);
  Serial.print(F("x y ")); Serial.print(DEFICIT_717 / ganPeor, 2); Serial.println(F("x"));

  if (ganPeor >= DEFICIT_717) {
    Serial.println(F("=> Aun en el peor caso el driver alcanza SOLO."));
  } else if (ganMejor < DEFICIT_717) {
    Serial.println(F("=> Ni en el mejor caso alcanza solo: hace falta tambien"));
    Serial.print(F("   cambiar la reduccion. RPM objetivo entre ~"));
    Serial.print(793.0f * ganPeor / DEFICIT_717, 0); Serial.print(F(" y ~"));
    Serial.print(793.0f * ganMejor / DEFICIT_717, 0); Serial.println(F("."));
  } else {
    Serial.println(F("=> AMBIGUO: alcanza o no segun el comportamiento en OFF."));
    Serial.println(F("   No decidir la compra con esto solo - medir el puente"));
    Serial.println(F("   nuevo, o dimensionar la reduccion por el caso peor."));
  }
  Serial.println(F("========================================\n"));
}

// --- Barrido -------------------------------------------------------------
void barrido() {
  if (vBus <= 0) {
    Serial.println(F("\nFalta V_bus: medir en VIN+ del L298N y cargarlo con 'b'."));
    return;
  }
  Serial.println(F("\n########## BARRIDO ##########"));
  Serial.print(F("Motor ")); Serial.print(motor);
  Serial.print(F(", ")); Serial.print(sentido > 0 ? F("FWD") : F("REV"));
  Serial.print(F(", duties:"));
  for (uint8_t i = 0; i < N_DUTIES; i++) { Serial.print(F(" ")); Serial.print(DUTIES[i]); }
  Serial.println();
  Serial.println(F("NO cambiar motor, sentido, traba ni puntas durante el barrido."));
  Serial.println(F("Entre punto y punto: dejar enfriar antes de dar ENTER."));

  for (uint8_t i = 0; i < N_DUTIES; i++) {
    Serial.print(F("\n>>> Punto ")); Serial.print(i + 1);
    Serial.print(F(" de ")); Serial.println(N_DUTIES);
    if (!medirPunto(DUTIES[i])) { Serial.println(F("\nBarrido interrumpido.")); return; }
  }
  Serial.println(F("\nBarrido completo."));
  ajustar();
}

// --- Menu ----------------------------------------------------------------
void menu() {
  Serial.println(F("\n=== TEST 8b - caida del L298N por barrido ==="));
  Serial.print(F("V_bus = "));
  if (vBus > 0) { Serial.print(vBus, 2); Serial.print(F(" V")); } else Serial.print(F("SIN CARGAR"));
  Serial.print(F("   motor = ")); Serial.print(motor);
  Serial.print(F("   sentido = ")); Serial.print(sentido > 0 ? F("FWD") : F("REV"));
  Serial.print(F("   duty(p) = ")); Serial.print(duty);
  Serial.print(F("   puntos = ")); Serial.println(nPts);
  Serial.println(F("b=V_bus  1/2=motor  i=invertir  s=BARRIDO  d=duty  p=punto"));
  Serial.println(F("f=ajustar  l=listar  r=borrar  x=parar"));
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  ledcAttach(ENA, PWM_FREQ_HZ, PWM_RES_BITS);
  ledcAttach(ENB, PWM_FREQ_HZ, PWM_RES_BITS);
  pararTodo();

  Serial.println(F("\n\n########################################"));
  Serial.println(F("TEST 8b - caida del L298N por barrido de duty"));
  Serial.println(F("########################################"));
  Serial.println(F("Corrige el Test 8, que asumia 0 V en el tramo OFF del PWM."));
  Serial.println(F("Aca V_off sale del ajuste, no se asume."));
  Serial.println(F("\nOrden sugerido:"));
  Serial.println(F("  1. Medir VIN+ del L298N -> cargar con 'b'"));
  Serial.println(F("  2. TRABAR la rueda (hilo+pesa del Test 7, morsa, cuna)"));
  Serial.println(F("  3. Puntas en las pestanas del motor, multimetro en DC"));
  Serial.println(F("  4. Elegir motor con 1/2, sentido con 'i'"));
  Serial.println(F("  5. 's' -> barrido completo. Ajusta y reporta al terminar."));
  Serial.println(F("\n⚠️ Bateria + USB. Dejar enfriar entre puntos."));
  menu();
}

void loop() {
  if (!Serial.available()) { delay(10); return; }
  char c = Serial.read();
  switch (c) {
    case 'b': { float v = leerNumero("V_bus medido en VIN+ del L298N (V): ");
                if (!isnan(v) && v > 0) vBus = v; menu(); break; }
    case 'd': { float v = leerNumero("Duty para el punto suelto (1-160): ");
                if (!isnan(v) && v > 0) duty = constrain((int)v, 1, MAX_PWM_DUTY);
                menu(); break; }
    case '1': motor = 1; menu(); break;
    case '2': motor = 2; menu(); break;
    case 'i': sentido = -sentido; menu(); break;
    case 's': barrido(); menu(); break;
    case 'p': medirPunto(duty); menu(); break;
    case 'f': ajustar(); menu(); break;
    case 'l': listar(); menu(); break;
    case 'r': nPts = 0; Serial.println(F("\nPuntos borrados.")); menu(); break;
    case 'x': pararTodo(); Serial.println(F("\nPARADO.")); menu(); break;
    default: break;
  }
}
