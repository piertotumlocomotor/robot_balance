/*
  ========================================================================
  TEST 7 — τ(u) medido con balanza de cocina
  ========================================================================

  POR QUE EXISTE:
  El Test 5b dejo K_U acotado entre 0.022 y 0.042 (criterio 0.030) sin poder
  cerrarlo. Dos razones, y esta medicion elimina las dos:

    1. AMBIGUEDAD NETA/BRUTA. En el banco colgante el motor pelea contra la
       gravedad Y contra la friccion de la reductora, y no se puede separar
       cuanto es cada una. Aca la fuerza se mide DIRECTAMENTE con la balanza:
       no pasa por la reductora ni por la flexion del bracket.

    2. UMBRAL DE DESPEGUE. En el banco colgante, por debajo de duty ~140 el
       cuerpo no se movia y no habia nada que medir: 4 de 7 puntos eran ruido.
       Aca NADA TIENE QUE MOVERSE -- el chasis esta amarrado y la rueda
       retenida por el hilo -- asi que se puede medir en todo el rango de duty
       y sale una pendiente tau(u) bien condicionada.

  ------------------------------------------------------------------------
  MONTAJE

          chasis amarrado a la mesa
     ┌──────────────────────┐
     │                      │   ( O )  <- rueda, sobresale del borde
     ├──────────────────────┤     │       de la mesa
     │////  mesa  //////////│     │  hilo tangente, vertical
     │                            │
     │                          ┌─┴─┐
     │                          │ W │  <- pesa
     │                        ┌─┴───┴─┐
     │                        │balanza│

  Motor apagado   -> lectura = W
  Motor encendido -> lectura = W - F
  F = W - lectura        tau = F * r

  ⚠️ LA PESA NO ES OPCIONAL, y no es por el rango de la balanza. Un hilo solo
  puede tirar HACIA la rueda, o sea hacia arriba; no hay forma de que empuje
  el plato hacia abajo. La pesa le da al motor algo que levantar.

  ⚠️ LA PESA TIENE QUE SER MAS PESADA QUE LA FUERZA MAXIMA ESPERADA. Si el
  motor llega a levantarla del todo NO se queda quieto: sigue enrollando el
  hilo y la pesa sube hacia la rueda. Estimacion desde el Test 5b: 350-400 gf
  por motor a duty 160. Con una pesa de ~400 g se esta al limite; conviene
  1 kg o mas. Si no hay, este sketch arranca por duties bajos justamente para
  poder cortar antes de llegar ahi.

  ⚠️ EL HILO TIENE QUE SALIR POR EL COSTADO DE LA RUEDA Y BAJAR VERTICAL. Ahi
  el brazo de palanca es exactamente r. Si sale en diagonal, el brazo real es
  menor y tau = F*r queda sobrestimado.

  ⚠️ UN MOTOR POR VEZ. M1 y M2 difieren 15-25 puntos de duty para la misma
  velocidad (ver "Asimetria M1/M2" en CLAUDE.md), asi que van a dar curvas
  distintas. Se miden por separado y despues se suman.

  ⚠️ TERMICO: el motor trabaja en STALL todo el pulso. Corriente alta en el
  motor y en los BJTs del L298N. Pulsos cortos, descanso largo, y tocar los
  motores cada tanto.

  ------------------------------------------------------------------------
  COMO SE USA

    w  -> cargar el peso de la pesa (g)
    r  -> cargar el RADIO de la rueda al hilo (mm). Es la distancia del
         eje al punto donde apoya el hilo: el brazo de palanca de tau = F*r.
    1  -> seleccionar motor 1        2 -> seleccionar motor 2
    i  -> invertir el sentido de giro
    t  -> PULSO DE PRUEBA (2 s, duty 110): confirmar que ENROLLA el hilo
    g  -> barrido guiado: pulso por duty, se anota la lectura de la balanza
    x  -> parar / abortar

  ORDEN: w, r, 1, t (y si desenrolla -> i, t otra vez), g.

  CABLEADO: identico al Test 3/5 (docs/conexiones-registro-pruebas.md 3.1)
    Motor 1: ENA=GPIO27, IN1=GPIO14, IN2=GPIO13
    Motor 2: ENB=GPIO4,  IN3=GPIO16, IN4=GPIO17
  No usa el IMU: la medicion es de fuerza, no de angulo.
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

// --- Barrido -------------------------------------------------------------
/*
  Se arranca por ABAJO a proposito. Con una pesa liviana, el punto donde el
  motor la levanta del todo es el limite util de la medicion: subiendo de a
  poco se llega hasta ahi y se corta, en vez de descubrirlo a duty 160 con la
  pesa trepando hacia la rueda.

  El 70 es control nulo: por debajo de la zona muerta, ahi no puede haber
  fuerza. Lo que mida es el piso de ruido del metodo.
*/
const uint8_t  DUTIES[]  = {70, 90, 105, 120, 135, 150, 160};
const uint8_t  N_DUTIES  = sizeof(DUTIES) / sizeof(DUTIES[0]);
const uint8_t  DUTY_NULO = 70;

const uint16_t PULSO_MS    = 3500;   // en stall: corto
const uint16_t DESCANSO_MS = 12000;  // enfriamiento entre pulsos
const uint16_t PRUEBA_MS   = 2000;

const float G = 9.81f;

// --- Estado --------------------------------------------------------------
float   pesaG   = 0;      // peso de la pesa, gramos
float   radioMM = 0;      // radio de la rueda al hilo, milimetros
uint8_t motor   = 1;
int8_t  sentido = 1;
bool    abortar = false;

/*
  La linea de base se toma ANTES DE CADA PULSO, no una sola vez al principio.
  En la primera corrida (2026-08-16) la lectura en reposo se movio ~7 g entre
  el arranque y el final -- el hilo se reacomoda, la balanza deriva. Con una
  referencia unica ese corrimiento se le suma entero a F; tomandola por paso,
  se cancela. Es el mismo problema que la columna `reposo` del Test 5b.
*/
float reposoG[N_DUTIES];    // lectura en reposo, justo antes del pulso
float lecturaG[N_DUTIES];   // lectura con el motor encendido
bool  hayDato[N_DUTIES];

// --- Control de motores --------------------------------------------------
void aplicar(uint8_t m, int8_t dir, uint8_t duty) {
  duty = constrain(duty, 0, MAX_PWM_DUTY);
  uint8_t en = (m == 1) ? ENA : ENB;
  uint8_t a  = (m == 1) ? IN1 : IN3;
  uint8_t b  = (m == 1) ? IN2 : IN4;
  if (dir > 0)      { digitalWrite(a, HIGH); digitalWrite(b, LOW);  }
  else if (dir < 0) { digitalWrite(a, LOW);  digitalWrite(b, HIGH); }
  else              { digitalWrite(a, LOW);  digitalWrite(b, LOW); duty = 0; }
  ledcWrite(en, duty);
}

void pararTodo() {
  aplicar(1, 0, 0);
  aplicar(2, 0, 0);
}

// --- Entrada por Serial --------------------------------------------------
void vaciarEntrada() {
  while (Serial.available()) Serial.read();
}

/*
  Lee un numero terminado en ENTER. Devuelve NAN si se aborta con 'x'.
  Bloquea a proposito: el operador tiene que mirar la balanza, no la terminal.
*/
float leerNumero(const char* prompt) {
  Serial.print(prompt);
  Serial.flush();
  vaciarEntrada();
  String buf = "";
  while (true) {
    if (!Serial.available()) { delay(10); continue; }
    char c = Serial.read();
    if (c == 'x') { Serial.println(F(" [abortado]")); return NAN; }
    if (c == '\n' || c == '\r') {
      if (buf.length() == 0) continue;
      Serial.println(buf);
      return buf.toFloat();
    }
    if ((c >= '0' && c <= '9') || c == '.' || c == '-') buf += c;
  }
}

// Espera ENTER. false si se aborto.
bool esperarEnter(const char* prompt) {
  Serial.print(prompt);
  Serial.flush();
  vaciarEntrada();
  while (true) {
    if (!Serial.available()) { delay(10); continue; }
    char c = Serial.read();
    if (c == 'x') { Serial.println(F(" [abortado]")); return false; }
    if (c == '\n' || c == '\r') { Serial.println(); return true; }
  }
}

void cuentaRegresiva(uint8_t seg) {
  for (uint8_t i = seg; i > 0; i--) {
    Serial.print(i); Serial.print(F("... "));
    Serial.flush();
    delay(1000);
  }
}

void descanso(uint16_t ms) {
  Serial.print(F("   descanso "));
  Serial.print(ms / 1000);
  Serial.println(F(" s (enfriamiento)"));
  uint32_t t0 = millis();
  while (millis() - t0 < ms) {
    if (Serial.available() && Serial.read() == 'x') { abortar = true; return; }
    delay(50);
  }
}

// --- Prueba de sentido ---------------------------------------------------
void pruebaSentido() {
  Serial.println(F("\n--- Prueba de sentido ---"));
  Serial.print(F("Motor ")); Serial.print(motor);
  Serial.print(F(", sentido ")); Serial.print(sentido > 0 ? F("FWD") : F("REV"));
  Serial.print(F(", duty 110, ")); Serial.print(PRUEBA_MS / 1000);
  Serial.println(F(" s."));
  Serial.println(F("MIRA LA PESA. Tiene que SUBIR (la lectura baja)."));
  Serial.println(F("Si la lectura no cambia, el motor esta desenrollando:"));
  Serial.println(F("apreta 'i' para invertir y repeti con 't'."));
  if (!esperarEnter("ENTER para aplicar (o 'x'): ")) return;

  cuentaRegresiva(3);
  Serial.println(F("APLICANDO"));
  aplicar(motor, sentido, 110);
  delay(PRUEBA_MS);
  pararTodo();
  Serial.println(F("listo.\n"));
}

// --- Barrido -------------------------------------------------------------
void barrido() {
  if (pesaG <= 0 || radioMM <= 0) {
    Serial.println(F("\nFaltan datos: cargar la pesa con 'w' y el radio con 'r'."));
    return;
  }
  abortar = false;
  for (uint8_t i = 0; i < N_DUTIES; i++) hayDato[i] = false;

  Serial.println(F("\n=== Barrido de torque ==="));
  Serial.print(F("Motor ")); Serial.print(motor);
  Serial.print(F("   pesa ")); Serial.print(pesaG, 0);
  Serial.print(F(" g   radio ")); Serial.print(radioMM, 1);
  Serial.println(F(" mm"));
  Serial.println(F("En cada paso: mira la balanza durante el pulso y anota la"));
  Serial.println(F("lectura MAS BAJA que llegue a mostrar.\n"));
  Serial.println(F("⚠️ Si la lectura se acerca a 0, la pesa se esta por levantar:"));
  Serial.println(F("   apreta 'x' y pone una pesa mas pesada.\n"));

  for (uint8_t i = 0; i < N_DUTIES && !abortar; i++) {
    Serial.print(F("--- duty ")); Serial.print(DUTIES[i]);
    if (DUTIES[i] == DUTY_NULO) Serial.print(F("  (control nulo)"));
    Serial.println();

    float base = leerNumero("lectura EN REPOSO (g): ");
    if (isnan(base)) { abortar = true; break; }
    reposoG[i] = base;

    if (!esperarEnter("ENTER para aplicar (o 'x'): ")) { abortar = true; break; }
    cuentaRegresiva(3);
    Serial.print(F("MIDIENDO ("));
    Serial.print(PULSO_MS / 1000);
    Serial.println(F(" s) - MIRA LA BALANZA"));
    Serial.flush();

    aplicar(motor, sentido, DUTIES[i]);
    delay(PULSO_MS);
    pararTodo();

    float l = leerNumero("lectura mas baja (g): ");
    if (isnan(l)) { abortar = true; break; }
    lecturaG[i] = l;
    hayDato[i] = true;

    float f = base - l;
    Serial.print(F("   F = ")); Serial.print(f, 1);
    Serial.print(F(" gf   (reposo ")); Serial.print(base, 0);
    Serial.println(F(")"));
    if (l <= 5.0f) {
      Serial.println(F("   ⚠️ La lectura llego a ~0: la pesa se levanto y este"));
      Serial.println(F("      punto NO vale. Hace falta una pesa mas pesada."));
    }

    if (i < N_DUTIES - 1) descanso(DESCANSO_MS);
  }
  pararTodo();
  informe();
}

// --- Informe -------------------------------------------------------------
void informe() {
  Serial.println(F("\n\n============ RESULTADO ============"));
  Serial.print(F("Motor ")); Serial.print(motor);
  Serial.print(F("   pesa = ")); Serial.print(pesaG, 1);
  Serial.print(F(" g   r = ")); Serial.print(radioMM, 1);
  Serial.println(F(" mm"));

  Serial.println(F("\nduty  reposo  lectura    F        tau"));
  Serial.println(F("       (g)      (g)      (gf)     (N*m)"));

  double su = 0, st = 0, suu = 0, sut = 0;
  uint8_t n = 0;
  float r_m = radioMM / 1000.0f;

  for (uint8_t i = 0; i < N_DUTIES; i++) {
    if (!hayDato[i]) continue;
    float f_gf = reposoG[i] - lecturaG[i];
    float tau  = f_gf * 0.001f * G * r_m;

    Serial.print(F("  ")); Serial.print(DUTIES[i]);
    Serial.print(F("   ")); Serial.print(reposoG[i], 0);
    Serial.print(F("    ")); Serial.print(lecturaG[i], 1);
    Serial.print(F("    ")); Serial.print(f_gf, 1);
    Serial.print(F("    ")); Serial.print(tau, 5);
    if (DUTIES[i] == DUTY_NULO) Serial.print(F("   <- control nulo"));
    if (lecturaG[i] <= 5.0f)    Serial.print(F("   <- SATURADO, descartar"));
    Serial.println();

    // el nulo y los puntos saturados quedan fuera del ajuste
    if (DUTIES[i] != DUTY_NULO && lecturaG[i] > 5.0f) {
      su += DUTIES[i]; st += tau;
      suu += (double)DUTIES[i] * DUTIES[i];
      sut += (double)DUTIES[i] * tau;
      n++;
    }
  }

  Serial.println(F("\n--- Ajuste  tau = A*u + B ---"));
  if (n < 2) { Serial.println(F("Datos insuficientes.")); return; }
  double den = n * suu - su * su;
  if (den == 0) { Serial.println(F("Ajuste degenerado.")); return; }
  double A = (n * sut - su * st) / den;
  double B = (st - A * su) / n;

  Serial.print(F("A = ")); Serial.print(A, 7); Serial.println(F(" N*m por punto de duty"));
  Serial.print(F("B = ")); Serial.println(B, 5);
  if (A != 0) {
    Serial.print(F("Umbral (ordenada al origen): duty ~ "));
    Serial.println(-B / A, 1);
    Serial.println(F("  Comparar con los 105-118 del banco colgante (7.16):"));
    Serial.println(F("  aca NO deberia haber umbral de despegue, porque nada"));
    Serial.println(F("  se mueve. Si igual da alto, es zona muerta electrica."));
  }
  Serial.print(F("\ntau extrapolado a duty ")); Serial.print(MAX_PWM_DUTY);
  Serial.print(F(" = ")); Serial.print(A * MAX_PWM_DUTY + B, 5);
  Serial.println(F(" N*m  (un motor)"));

  float bLo = 1e9, bHi = -1e9;
  for (uint8_t i = 0; i < N_DUTIES; i++) {
    if (!hayDato[i]) continue;
    if (reposoG[i] < bLo) bLo = reposoG[i];
    if (reposoG[i] > bHi) bHi = reposoG[i];
  }
  Serial.print(F("\nDeriva de la linea de base durante la corrida: "));
  Serial.print(bHi - bLo, 0); Serial.println(F(" g"));
  Serial.println(F("  Si es del orden de F, la medicion no vale: revisar que"));
  Serial.println(F("  el hilo no se reacomode y que la balanza no derive."));

  Serial.println(F("\n--- Que falta para K_U ---"));
  Serial.println(F("1. Repetir con el otro motor y SUMAR los dos tau."));
  Serial.println(F("2. Medir m (masa total) y l (eje de ruedas -> CG)."));
  Serial.println(F("3. K_U = tau_total * w0^2 / (m * g * l),  w0 = 7.4 rad/s"));
  Serial.println(F("   Criterio: K_U >= 0.030"));
  Serial.println(F("==================================="));
}

// --- Menu ----------------------------------------------------------------
void menu() {
  Serial.println(F("\n=== TEST 7 - tau(u) con balanza ==="));
  Serial.print(F("pesa = "));
  if (pesaG > 0) { Serial.print(pesaG, 0); Serial.print(F(" g")); }
  else Serial.print(F("SIN CARGAR"));
  Serial.print(F("   radio = "));
  if (radioMM > 0) { Serial.print(radioMM, 1); Serial.print(F(" mm")); }
  else Serial.print(F("SIN CARGAR"));
  Serial.print(F("   motor = ")); Serial.print(motor);
  Serial.print(F("   sentido = ")); Serial.println(sentido > 0 ? F("FWD") : F("REV"));
  Serial.println(F("w=pesa  r=radio(eje->hilo)  1/2=motor  i=invertir  t=prueba  g=barrido  x=parar"));
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  ledcAttach(ENA, PWM_FREQ_HZ, PWM_RES_BITS);
  ledcAttach(ENB, PWM_FREQ_HZ, PWM_RES_BITS);
  pararTodo();   // estado seguro antes que nada

  Serial.println(F("\n\n########################################"));
  Serial.println(F("TEST 7 - torque con balanza de cocina"));
  Serial.println(F("########################################"));
  Serial.println(F("Antes de arrancar:"));
  Serial.println(F("  [ ] Chasis amarrado firme. Que no se mueva nada."));
  Serial.println(F("  [ ] Hilo tangente al borde de la rueda y VERTICAL."));
  Serial.println(F("  [ ] Pesa apoyada en la balanza, hilo sin flojedad."));
  Serial.println(F("  [ ] Bateria + USB (no USB solo)."));
  Serial.println(F("  [ ] Nada fragil debajo de la pesa."));
  menu();
}

void loop() {
  if (!Serial.available()) { delay(10); return; }
  char c = Serial.read();
  switch (c) {
    case 'w': { float v = leerNumero("Peso de la pesa (g): ");
                if (!isnan(v) && v > 0) pesaG = v; menu(); break; }
    case 'r': { float v = leerNumero("RADIO eje->hilo (mm): ");
                if (!isnan(v) && v > 0) radioMM = v; menu(); break; }
    case '1': motor = 1; menu(); break;
    case '2': motor = 2; menu(); break;
    case 'i': sentido = -sentido; menu(); break;
    case 't': pruebaSentido(); menu(); break;
    case 'g': barrido(); menu(); break;
    case 'x': abortar = true; pararTodo();
              Serial.println(F("\nPARADO.")); menu(); break;
    default: break;
  }
}
