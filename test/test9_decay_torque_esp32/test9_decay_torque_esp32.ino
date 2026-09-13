/*
  ========================================================================
  TEST 9 — Torque con balanza: COAST vs BRAKE (modo de decaimiento)
  ========================================================================

  POR QUE EXISTE

  Los Tests 8 y 8b intentaron medir la caida del L298N con multimetro y
  fallaron por tres motivos independientes:

    1. El Test 8 asumia 0 V en el tramo OFF del PWM. Falso.
    2. El Test 8b asumia V_off CONSTANTE. Tambien falso: los 4 barridos
       (2 motores x 2 repeticiones) dan el mismo patron de residuos en U.
    3. La lectura no es estable: arranca alta y baja durante la ventana
       -> el L298N se degrada al calentarse. Nada de eso era permanente.

  Es el mismo patron que ya paso dos veces en este proyecto (ω₀ y K_U
  medidos mal dos veces cada uno). Lo que lo cerro las dos veces fue dejar
  la cadena inferencial y MEDIR DIRECTO. Eso hace este test.

  ------------------------------------------------------------------------
  LA HIPOTESIS QUE PRUEBA

  En los Tests 2/3/5/6/7/8 el PWM va sobre ENA/ENB con los IN fijos.
  Cuando ENA baja, las dos salidas del puente quedan en alta impedancia y
  la corriente inductiva del motor vuelve al bus por los diodos: las
  pestanas quedan a tension NEGATIVA durante todo el tramo OFF.
  Eso es COAST (fast decay), y es el V_off negativo que aparecio en los
  cuatro ajustes del Test 8b.

  Si en cambio se conmutan los pines IN dejando EN siempre alto:

      COAST   EN = PWM,  IN_a = 1,    IN_b = 0
      BRAKE   EN = 1,    IN_a = PWM,  IN_b = 0

  en el tramo OFF quedan IN_a = IN_b = 0 con EN = 1, o sea AMBAS salidas
  forzadas a bajo: el motor queda cortocircuitado. Eso es BRAKE (slow
  decay), V_off ~ 0, y la corriente media sube al mismo duty.

  ⚠️ V_off NO depende de la tecnologia del transistor sino del modo de
  decaimiento. Un puente de MOSFETs cableado igual tendria el mismo V_off
  negativo. Por eso esto se prueba ANTES de comprar nada.

  ------------------------------------------------------------------------
  QUE MIDE

  Torque directo con la balanza, igual que el Test 7: sin multimetro, sin
  modelo, sin suposiciones sobre el tramo OFF.

      F = linea_de_base - lectura        τ = F · r

  ⚠️ r ES DEL MONTAJE, NO DEL ROBOT. Medirlo con calibre y cargarlo con 'r'
  en cada remontaje. El 2026-08-17 dio 33 mm, no los 35 que asumia el Test 7.
  La reproducibilidad entre remontajes resulto ser de ~±30%, mucho peor que
  el margen que se busca resolver — de ahi que el radio se mida y no se asuma.

  La comparacion que decide es la RAZON entre modos al mismo duty y mismo
  motor. El sketch la calcula solo con 'k'.

  Linea de base del Test 7 (duty 160, coast): M1 72.0 gf, M2 58.3 gf.

  ------------------------------------------------------------------------
  MONTAJE — identico al Test 7 (ver CLAUDE.md, "Medicion de τ(u) con balanza")

        chasis amarrado a la mesa
   ┌──────────────────────┐
   │                      │   ( O )  <- rueda, sobresale del borde
   ├──────────────────────┤     │
   │////  mesa  //////////│     │  hilo tangente, VERTICAL
   │                            │
   │                          ┌─┴─┐
   │                          │ W │  <- pesa (~860 g)
   │                        ┌─┴───┴─┐
   │                        │balanza│

  ⚠️ La pesa tiene que pesar mas que la fuerza maxima, o el motor la
     levanta entera y sigue enrollando.
  ⚠️ El hilo sale por el costado y baja VERTICAL. En diagonal el brazo no
     es r y τ queda sobrestimado.
  ⚠️ La linea de base se toma ANTES DE CADA PULSO: deriva ~10 g por corrida
     y ese corrimiento se le sumaria entero a F.

  ------------------------------------------------------------------------
  PROTOCOLO TERMICO — igual al Test 7, y no es un detalle

  Pulsos de 3.5 s con 12 s de descanso FORZADO. El Test 8b uso 8 s seguidos
  y por eso midio el chip mucho mas caliente; sus numeros no son comparables
  con los del Test 7. Para que la razon COAST/BRAKE signifique algo, los dos
  modos tienen que medirse con el mismo protocolo.

  ⚠️ Leer la balanza SIEMPRE EN EL MISMO INSTANTE del pulso (~2 s adentro).
  Hoy se confirmo que la salida del L298N decae mientras se calienta, asi
  que "cuando miraste" cambia el numero.

  ------------------------------------------------------------------------
  ⚠️⚠️ SEGURIDAD: EL TOPE DE DUTY NO SIGNIFICA LO MISMO EN LOS DOS MODOS

  En BRAKE, el mismo duty entrega bastante mas corriente y mas tension media
  que en COAST. El tope de COAST fue calibrado en coast y NO es conservador
  en BRAKE.

      COAST  -> 160   (el cap vigente del proyecto)
      BRAKE  -> 160   SOLO PARA MEDIR (subido de 127 el 2026-08-17)

  El 127 original salia de: frac 0.50 sobre un bus de ~12.1 V => ~6.0 V
  medios, el nominal del motor. Pero ese calculo IGNORA la caida del driver,
  asi que la tension real a duty 127 esta por debajo del nominal y el tope
  quedaba mas conservador de lo necesario.

  ⚠️⚠️ ARRIBA DE duty 127 EN BRAKE EL MOTOR PASA SU NOMINAL DE 6 V.
  Es aceptable para PULSOS DE MEDICION de 3.5 s con 12 s de descanso (ciclo
  ~22%; verificado el 2026-08-17: el L298N apenas entibia). NO es un permiso
  para operar ahi de forma continua: balanceando, la carga termica es ~4.5x
  la de este protocolo.

  ⚠️⚠️ EL CAP OPERATIVO DEL FIRMWARE ES OTRA DECISION, y mas conservadora.
  Hay que fijarlo con un ensayo termico de ciclo continuo, no con este test.
  Subir el tope para medir NO autoriza subirlo para operar.

  ------------------------------------------------------------------------
  MODO 'e' — ENSAYO SOSTENIDO (termico)

  Los pulsos de 3.5s/12s ('m') miden torque en frio y no dicen si el driver
  aguanta el uso REAL: en balanceo el motor trabaja mucho mas seguido, y el
  7.18 ya mostro que este chip degrada su salida al calentarse.

  Diferencias con 'm':
    - Los DOS motores giran a la vez (como en balanceo real: los dos canales
      del L298N conducen simultaneamente). El motor NO medido necesita su
      rueda trabada por otro medio (mordaza/cuna), no por la balanza.
    - Sin pausas: se lee la balanza y V_bus cada 30 s CON el motor girando.
    - Se compara cada lectura contra un UMBRAL DE FALLA (no solo se mira el
      numero): sale del margen del 8.6% que dejo este mismo test (7.19)
      contra el criterio de 5°. Si F cae por debajo, ese duty no es sostenible.

  ⚠️ Anotar V_bus en cada lectura no es opcional: en carga sostenida la
  bateria tambien cae, y esa caida se confunde con degradacion termica si no
  se registra por separado.

  Protocolo sugerido: empezar en duty bajo (130) y verificar 5 min. Si pasa,
  subir (145, despues 160). El primer duty que NO sostenga el umbral marca
  el techo — el cap operativo real queda por debajo de ese.

  CABLEADO: identico al Test 3/5/7/8 (docs/conexiones-registro-pruebas.md 3.1)
    Motor 1: ENA=GPIO27, IN1=GPIO14, IN2=GPIO13
    Motor 2: ENB=GPIO4,  IN3=GPIO16, IN4=GPIO17
  ========================================================================
*/

#include <Arduino.h>

// --- Pines ---------------------------------------------------------------
const uint8_t ENA = 27, IN1 = 14, IN2 = 13;
const uint8_t ENB = 4,  IN3 = 16, IN4 = 17;
const uint8_t PINES[6] = {ENA, IN1, IN2, ENB, IN3, IN4};

// --- PWM -----------------------------------------------------------------
const uint32_t PWM_FREQ_HZ  = 20000;
const uint8_t  PWM_RES_BITS = 8;

const uint8_t CAP_COAST = 160;
const uint8_t CAP_BRAKE = 160;   // subido de 127 el 2026-08-17 — SOLO PARA MEDIR
const uint8_t BRAKE_NOMINAL = 127;  // arriba de esto el motor pasa su nominal

// --- Protocolo (igual al Test 7) -----------------------------------------
const uint16_t PULSO_MS    = 3500;
const uint16_t DESCANSO_MS = 12000;

// ⚠️ EL RADIO NO ES UNA CONSTANTE DEL ROBOT: es del MONTAJE.
// Depende de donde apoya el hilo, y cambia al pasarlo de una rueda a la otra.
// El 2026-08-17 se midio 33 mm con el hilo ajustado, contra los 35 mm que asumia
// el Test 7 — 6% de error directo sobre tau. MEDIR CON CALIBRE EN CADA REMONTAJE
// y cargarlo con 'r' antes de medir.
const float R_DEFECTO = 0.033f;  // m
float rRueda = R_DEFECTO;

const float G = 9.81f;

// Referencia del Test 7 (duty 160, coast) para contexto
const float T7_M1_GF = 72.0f, T7_M2_GF = 58.3f;

// --- Ensayo sostenido (termico) -------------------------------------------
// Umbrales de falla: por debajo de esto, el duty NO es sostenible.
// Salen del margen del Test 9 (7.19): tau_total medido 0.1184 N·m contra
// 0.1082 N·m que exige el criterio de 5° -> margen 8.6%.
// F_umbral = F_medida_a_160 * (0.1082/0.1184). Asume que M1 y M2 se degradan
// en la misma proporcion — no verificado, es la mejor suposicion disponible.
const float UMBRAL_M1_GF = 195.0f;   // de 213.4 gf medidos
const float UMBRAL_M2_GF = 139.0f;   // de 152.2 gf medidos

const uint32_t SOST_INTERVALO_MS = 30000;   // lectura cada 30 s
const uint32_t SOST_DURACION_MS  = 300000;  // 5 min por corrida; repetir 'e' para seguir

// --- Modos ---------------------------------------------------------------
enum Modo { COAST, BRAKE };
Modo modo = COAST;

const __FlashStringHelper* nombreModo(Modo m) {
  return (m == COAST) ? F("COAST") : F("BRAKE");
}
uint8_t capDe(Modo m) { return (m == COAST) ? CAP_COAST : CAP_BRAKE; }

// --- Estado --------------------------------------------------------------
uint8_t duty    = 120;
uint8_t motor   = 1;
int8_t  sentido = 1;

// --- Set de resultados ---------------------------------------------------
const uint8_t N_MAX = 24;
uint8_t nRes = 0;
Modo    rModo[N_MAX];
uint8_t rMotor[N_MAX], rDuty[N_MAX];
float   rFuerza[N_MAX];   // gf

// --- Configuracion de pines segun modo -----------------------------------
void liberarTodos() {
  for (uint8_t i = 0; i < 6; i++) {
    ledcDetach(PINES[i]);
    pinMode(PINES[i], OUTPUT);
    digitalWrite(PINES[i], LOW);
  }
}

// Deja los pines listos para el modo y motor actuales, con salida en cero.
void configurar() {
  liberarTodos();
  uint8_t en = (motor == 1) ? ENA : ENB;
  uint8_t a  = (motor == 1) ? IN1 : IN3;
  uint8_t b  = (motor == 1) ? IN2 : IN4;

  if (modo == COAST) {
    ledcAttach(en, PWM_FREQ_HZ, PWM_RES_BITS);
    ledcWrite(en, 0);
    digitalWrite(a, LOW); digitalWrite(b, LOW);
  } else {
    // EN fijo en alto (no por LEDC: con 8 bits, 255 no es 100% exacto)
    digitalWrite(en, HIGH);
    ledcAttach(a, PWM_FREQ_HZ, PWM_RES_BITS); ledcWrite(a, 0);
    ledcAttach(b, PWM_FREQ_HZ, PWM_RES_BITS); ledcWrite(b, 0);
  }
}

void aplicar(uint8_t d) {
  d = constrain(d, 0, capDe(modo));
  uint8_t en = (motor == 1) ? ENA : ENB;
  uint8_t a  = (motor == 1) ? IN1 : IN3;
  uint8_t b  = (motor == 1) ? IN2 : IN4;

  if (modo == COAST) {
    if (sentido > 0) { digitalWrite(a, HIGH); digitalWrite(b, LOW);  }
    else             { digitalWrite(a, LOW);  digitalWrite(b, HIGH); }
    ledcWrite(en, d);
  } else {
    if (sentido > 0) { ledcWrite(a, d); ledcWrite(b, 0); }
    else             { ledcWrite(a, 0); ledcWrite(b, d); }
  }
}

void parar() { configurar(); }   // reconfigurar deja todo en cero

// --- Ambos motores en BRAKE, para el ensayo sostenido ---------------------
// Independiente de 'modo'/'motor' globales: el ensayo termico necesita los
// DOS motores girando (asi se calienta el L298N como en balanceo real), sin
// importar cual este seleccionado en el menu.
void configurarAmbosBrake() {
  liberarTodos();
  digitalWrite(ENA, HIGH); digitalWrite(ENB, HIGH);
  ledcAttach(IN1, PWM_FREQ_HZ, PWM_RES_BITS); ledcWrite(IN1, 0);
  ledcAttach(IN2, PWM_FREQ_HZ, PWM_RES_BITS); ledcWrite(IN2, 0);
  ledcAttach(IN3, PWM_FREQ_HZ, PWM_RES_BITS); ledcWrite(IN3, 0);
  ledcAttach(IN4, PWM_FREQ_HZ, PWM_RES_BITS); ledcWrite(IN4, 0);
}

void aplicarAmbosBrake(uint8_t d) {
  d = constrain(d, 0, CAP_BRAKE);
  ledcWrite(IN1, d); ledcWrite(IN2, 0);   // M1 FWD
  ledcWrite(IN3, d); ledcWrite(IN4, 0);   // M2 FWD
}

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

// --- Una corrida ---------------------------------------------------------
void corrida() {
  if (nRes >= N_MAX) { Serial.println(F("\nSet lleno. Usar 'r' para borrar.")); return; }

  uint8_t d = constrain(duty, 0, capDe(modo));
  Serial.println(F("\n--- Corrida ---"));
  Serial.print(F("Modo ")); Serial.print(nombreModo(modo));
  Serial.print(F(", motor ")); Serial.print(motor);
  Serial.print(F(", ")); Serial.print(sentido > 0 ? F("FWD") : F("REV"));
  Serial.print(F(", duty ")); Serial.println(d);
  if (d != duty) Serial.println(F("  (recortado al tope de este modo)"));
  if (modo == BRAKE && d > BRAKE_NOMINAL) {
    Serial.print(F("⚠️ duty > ")); Serial.print(BRAKE_NOMINAL);
    Serial.println(F(" en BRAKE: el motor pasa su nominal de 6 V."));
    Serial.println(F("   OK para pulsos de medicion. NO para operacion continua."));
  }

  float base = leerNumero("Linea de base: lectura de la balanza SIN motor (g): ");
  if (isnan(base)) return;

  if (!esperarEnter("ENTER para el pulso de 3.5 s (o 'x'): ")) return;
  for (uint8_t i = 3; i > 0; i--) { Serial.print(i); Serial.print(F("... ")); Serial.flush(); delay(1000); }
  Serial.println(F("PULSO - LEE LA BALANZA A LOS ~2 s"));
  Serial.flush();

  configurar();
  aplicar(d);
  delay(PULSO_MS);
  parar();
  Serial.println(F("listo."));

  float lect = leerNumero("Lectura de la balanza CON motor (g): ");
  if (isnan(lect)) return;

  float F   = base - lect;           // gf
  float tau = (F / 1000.0f) * G * rRueda;

  Serial.print(F("\nF = ")); Serial.print(F, 1); Serial.print(F(" gf     τ = "));
  Serial.print(tau, 5); Serial.println(F(" N·m"));
  if (F <= 0) Serial.println(F("⚠️ Fuerza <= 0: revisar hilo, pesa y sentido de giro."));

  rModo[nRes] = modo; rMotor[nRes] = motor; rDuty[nRes] = d; rFuerza[nRes] = F;
  nRes++;
  Serial.print(F("Guardado. Total: ")); Serial.println(nRes);

  Serial.print(F("\nDescanso forzado de ")); Serial.print(DESCANSO_MS / 1000);
  Serial.println(F(" s (protocolo del Test 7 - no saltearlo)."));
  for (uint8_t i = DESCANSO_MS / 1000; i > 0; i--) {
    Serial.print(i); Serial.print(F(" ")); Serial.flush(); delay(1000);
  }
  Serial.println(F("\nListo para la proxima."));
}

// El umbral absoluto (UMBRAL_M1_GF/M2_GF) se calibro contra el torque medido
// a duty 160 (7.19) y SOLO significa algo ahi. A duty mas bajo el torque en
// frio ya esta por debajo, y comparar contra el umbral dispara falsas
// alarmas sin relacion con temperatura. Por eso el chequeo pass/fail solo se
// activa cerca de ese punto de calibracion (d >= UMBRAL_DUTY_MIN).
const uint8_t UMBRAL_DUTY_MIN = 150;

// --- Ensayo sostenido (termico) --------------------------------------------
// A diferencia de 'corrida()', NO para el motor para leer: el punto es medir
// bajo carga continua. Los DOS motores giran todo el ensayo. Duracion max
// SOST_DURACION_MS; 'x' en cualquier momento aborta y frena todo.
void sostenido() {
  uint8_t d = constrain(duty, 0, CAP_BRAKE);
  uint8_t motorMedido = motor;
  float umbral = (motorMedido == 1) ? UMBRAL_M1_GF : UMBRAL_M2_GF;

  Serial.println(F("\n########## ENSAYO SOSTENIDO (termico) ##########"));
  Serial.print(F("Motor MEDIDO: ")); Serial.print(motorMedido);
  Serial.print(F("   Duty: ")); Serial.print(d);
  Serial.print(F("   Umbral de falla: ")); Serial.print(umbral, 0);
  Serial.println(F(" gf"));
  Serial.println(F("\nRequisitos del montaje (leer antes de arrancar):"));
  Serial.println(F("  - Motor medido: balanza + hilo, como siempre."));
  Serial.println(F("  - El OTRO motor: rueda TRABADA por otro medio (mordaza,"));
  Serial.println(F("    cuna, tope de madera) - NO por la balanza. Los DOS"));
  Serial.println(F("    motores van a girar en BRAKE durante todo el ensayo,"));
  Serial.println(F("    para calentar el L298N como en balanceo real."));
  Serial.println(F("  - Bateria LLENA: si se descarga durante el ensayo, esa"));
  Serial.println(F("    caida se confunde con degradacion termica."));
  Serial.println(F("  - Corte de seguridad: 'x' en cualquier momento. Tambien"));
  Serial.println(F("    cortar ante olor a quemado o algo que no se pueda"));
  Serial.println(F("    tocar 1 s (~60°C) — driver, motor O BATERIA."));

  if (!esperarEnter("\nENTER cuando el montaje este listo (o 'x'): ")) return;

  float base = leerNumero("Linea de base ANTES de arrancar, SIN motor (g): ");
  if (isnan(base)) return;

  float vBusIni = leerNumero("V_bus en VIN+ ANTES de arrancar (V): ");
  if (isnan(vBusIni)) return;
  float tempIni = leerNumero("Temp. del driver ANTES de arrancar (multimetro de lapiz,"
                              " sin calibrar, mismo punto de contacto siempre) - 'x' si no medis: ");
  bool hayTemp = !isnan(tempIni);

  Serial.println(F("\nArrancando. Lectura cada 30 s. 'x' para abortar YA."));
  for (uint8_t i = 3; i > 0; i--) { Serial.print(i); Serial.print(F("... ")); Serial.flush(); delay(1000); }
  Serial.println(F("EN MARCHA."));
  Serial.flush();

  configurarAmbosBrake();
  aplicarAmbosBrake(d);

  uint32_t inicio = millis();
  uint32_t proxima = SOST_INTERVALO_MS;
  float primeraF = NAN;
  bool abortado = false;

  while (true) {
    uint32_t transcurrido = millis() - inicio;
    if (transcurrido >= SOST_DURACION_MS) {
      Serial.println(F("\nDuracion maxima de esta corrida alcanzada."));
      break;
    }
    if (Serial.available() && Serial.peek() == 'x') {
      Serial.read();
      Serial.println(F("\n⚠️ ABORTADO POR EL OPERADOR."));
      abortado = true;
      break;
    }
    if (transcurrido >= proxima) {
      Serial.print(F("\n--- t = ")); Serial.print(transcurrido / 1000);
      Serial.println(F(" s (motores SIGUEN girando durante la lectura) ---"));

      float lect = leerNumero("Lectura de la balanza (g): ");
      if (isnan(lect)) { abortado = true; break; }
      float Fnow = base - lect;
      if (isnan(primeraF)) primeraF = Fnow;
      float caidaPct = (primeraF > 0.1f) ? (100.0f * (primeraF - Fnow) / primeraF) : 0;

      Serial.print(F("F = ")); Serial.print(Fnow, 1);
      Serial.print(F(" gf   (")); Serial.print(caidaPct, 1);
      Serial.println(F("% desde el primer punto de esta corrida)"));

      if (d < UMBRAL_DUTY_MIN) {
        Serial.println(F("(umbral de falla no aplica por debajo de duty 150 —"));
        Serial.println(F(" se calibro contra el torque a duty 160. Solo mirar la"));
        Serial.println(F(" tendencia de F en esta corrida.)"));
      } else if (Fnow < umbral) {
        Serial.print(F("⚠️⚠️ POR DEBAJO DEL UMBRAL DE FALLA (")); Serial.print(umbral, 0);
        Serial.println(F(" gf): este duty NO es sostenible a este tiempo."));
      }

      float vB = leerNumero("V_bus en VIN+ (V): ");
      if (!isnan(vB)) {
        Serial.print(F("V_bus = ")); Serial.print(vB, 2);
        Serial.print(F(" V   (")); Serial.print(vB - vBusIni, 2);
        Serial.println(F(" V desde el inicio — si cae mucho, la bateria"));
        Serial.println(F("   se mete en la lectura de F, no solo la temperatura)"));
      }

      if (hayTemp) {
        float temp = leerNumero("Temp. del driver (mismo punto de contacto, 'x' para saltear): ");
        if (!isnan(temp)) {
          Serial.print(F("Temp = ")); Serial.print(temp, 1);
          Serial.print(F("   (")); Serial.print(temp - tempIni, 1);
          Serial.println(F(" desde el inicio, unidades sin calibrar)"));
        } else {
          hayTemp = false;   // 'x': dejar de pedirla el resto de esta corrida
        }
      }

      proxima += SOST_INTERVALO_MS;
    }
    delay(50);
  }

  liberarTodos();   // frenado seguro: detach + salidas en LOW
  configurar();      // vuelve al modo/motor del menu principal
  Serial.println(abortado ? F("\nMotores detenidos.") : F("\nCorrida terminada. Motores detenidos."));
  Serial.println(F("Repetir 'e' para seguir el ensayo o subir el duty (con 'd')."));
}

// --- Ensayo intermitente (termico, ciclo ON/OFF) ---------------------------
// El ensayo 'e' es el peor caso (duty fijo, continuo) y ya establecio que el
// driver NO aguanta eso indefinidamente. Pero balanceando el robot no opera
// asi: da pulsos cortos y descansa cerca del equilibrio. Este modo repite un
// ciclo ON/OFF con los DOS motores, para ver si ESE patron llega a un
// regimen termico estable o sigue degradandose sin limite.
// ⚠️ El patron ON/OFF es una SUPOSICION sobre el uso real, no un dato medido
// -- no hay firmware de balanceo todavia que diga la frecuencia real de
// correccion. Se pide por Serial para poder variarlo entre corridas.
void intermitente() {
  uint8_t d = constrain(duty, 0, CAP_BRAKE);
  uint8_t motorMedido = motor;
  float umbral = (motorMedido == 1) ? UMBRAL_M1_GF : UMBRAL_M2_GF;

  Serial.println(F("\n########## ENSAYO INTERMITENTE (ciclo ON/OFF) ##########"));
  Serial.print(F("Motor MEDIDO: ")); Serial.print(motorMedido);
  Serial.print(F("   Duty durante ON: ")); Serial.println(d);
  Serial.println(F("Mismo montaje que el ensayo sostenido: motor NO medido"));
  Serial.println(F("trabado aparte, los DOS motores ciclan juntos."));

  float tOnS  = leerNumero("Tiempo ON por ciclo (s), ej. 0.5: ");
  if (isnan(tOnS) || tOnS <= 0) return;
  float tOffS = leerNumero("Tiempo OFF por ciclo (s), ej. 2.0: ");
  if (isnan(tOffS) || tOffS < 0) return;
  uint32_t onMs  = (uint32_t)(tOnS  * 1000);
  uint32_t offMs = (uint32_t)(tOffS * 1000);
  uint32_t cicloMs = onMs + offMs;
  Serial.print(F("Duty cycle: ")); Serial.print(100.0f * onMs / cicloMs, 0);
  Serial.println(F("%"));

  if (!esperarEnter("\nENTER cuando el montaje este listo (o 'x'): ")) return;

  float base = leerNumero("Linea de base ANTES de arrancar, SIN motor (g): ");
  if (isnan(base)) return;
  float vBusIni = leerNumero("V_bus en VIN+ ANTES de arrancar (V): ");
  if (isnan(vBusIni)) return;
  float tempIni = leerNumero("Temp. del driver ANTES de arrancar (multimetro de lapiz,"
                              " sin calibrar, mismo punto de contacto siempre) - 'x' si no medis: ");
  bool hayTemp = !isnan(tempIni);

  Serial.println(F("\nArrancando ciclo. Lectura cada ~30 s (al final de un ON)."));
  Serial.println(F("'x' para abortar YA."));
  for (uint8_t i = 3; i > 0; i--) { Serial.print(i); Serial.print(F("... ")); Serial.flush(); delay(1000); }
  Serial.println(F("EN MARCHA."));
  Serial.flush();

  configurarAmbosBrake();
  uint32_t inicio = millis();
  uint32_t proxima = SOST_INTERVALO_MS;
  float primeraF = NAN;
  bool abortado = false;
  bool faseOnAnterior = false;

  while (true) {
    uint32_t transcurrido = millis() - inicio;
    if (transcurrido >= SOST_DURACION_MS) {
      Serial.println(F("\nDuracion maxima de esta corrida alcanzada."));
      break;
    }
    if (Serial.available() && Serial.peek() == 'x') {
      Serial.read();
      Serial.println(F("\n⚠️ ABORTADO POR EL OPERADOR."));
      abortado = true;
      break;
    }

    bool faseOn = (transcurrido % cicloMs) < onMs;
    if (faseOn != faseOnAnterior) {
      aplicarAmbosBrake(faseOn ? d : 0);
      faseOnAnterior = faseOn;
    }

    if (transcurrido >= proxima) {
      // Forzar fase ON para la lectura: torque de pico es lo que importa.
      aplicarAmbosBrake(d);
      delay(80);   // asentar antes de leer
      Serial.print(F("\n--- t = ")); Serial.print(transcurrido / 1000);
      Serial.println(F(" s (forzado a ON para la lectura — torque de pico) ---"));

      float lect = leerNumero("Lectura de la balanza (g): ");
      if (isnan(lect)) { abortado = true; break; }
      float Fnow = base - lect;
      if (isnan(primeraF)) primeraF = Fnow;
      float caidaPct = (primeraF > 0.1f) ? (100.0f * (primeraF - Fnow) / primeraF) : 0;

      Serial.print(F("F = ")); Serial.print(Fnow, 1);
      Serial.print(F(" gf   (")); Serial.print(caidaPct, 1);
      Serial.println(F("% desde el primer punto de esta corrida)"));

      if (d >= UMBRAL_DUTY_MIN && Fnow < umbral) {
        Serial.print(F("⚠️⚠️ POR DEBAJO DEL UMBRAL DE FALLA (")); Serial.print(umbral, 0);
        Serial.println(F(" gf) incluso en pico: este ciclo NO es sostenible."));
      }

      float vB = leerNumero("V_bus en VIN+ (V): ");
      if (!isnan(vB)) {
        Serial.print(F("V_bus = ")); Serial.print(vB, 2);
        Serial.print(F(" V   (")); Serial.print(vB - vBusIni, 2);
        Serial.println(F(" V desde el inicio)"));
      }

      if (hayTemp) {
        float temp = leerNumero("Temp. del driver (mismo punto de contacto, 'x' para saltear): ");
        if (!isnan(temp)) {
          Serial.print(F("Temp = ")); Serial.print(temp, 1);
          Serial.print(F("   (")); Serial.print(temp - tempIni, 1);
          Serial.println(F(" desde el inicio, unidades sin calibrar)"));
        } else {
          hayTemp = false;   // 'x': dejar de pedirla el resto de esta corrida
        }
      }

      proxima += SOST_INTERVALO_MS;
      faseOnAnterior = true;   // ya quedo aplicado el duty de ON
    }
    delay(20);
  }

  liberarTodos();
  configurar();
  Serial.println(abortado ? F("\nMotores detenidos.") : F("\nCorrida terminada. Motores detenidos."));
  Serial.println(F("Repetir 'w' para seguir, con el mismo ciclo o uno distinto."));
}

// --- Listado -------------------------------------------------------------
void listar() {
  Serial.println(F("\n--- Corridas ---"));
  if (nRes == 0) { Serial.println(F("(ninguna)")); return; }
  Serial.println(F("  modo    motor  duty   F(gf)    τ(N·m)"));
  for (uint8_t i = 0; i < nRes; i++) {
    Serial.print(F("  "));   Serial.print(nombreModo(rModo[i]));
    Serial.print(F("\t"));   Serial.print(rMotor[i]);
    Serial.print(F("\t"));   Serial.print(rDuty[i]);
    Serial.print(F("\t"));   Serial.print(rFuerza[i], 1);
    Serial.print(F("\t"));   Serial.print((rFuerza[i] / 1000.0f) * G * rRueda, 5);
    Serial.println();
  }
}

// Promedio de fuerza para una combinacion; n devuelve cuantas corridas entraron.
float promedio(Modo m, uint8_t mot, uint8_t d, uint8_t &n) {
  float s = 0; n = 0;
  for (uint8_t i = 0; i < nRes; i++)
    if (rModo[i] == m && rMotor[i] == mot && rDuty[i] == d) { s += rFuerza[i]; n++; }
  return (n > 0) ? s / n : NAN;
}

// --- Comparacion ---------------------------------------------------------
void comparar() {
  if (nRes == 0) { Serial.println(F("\nNo hay corridas.")); return; }

  Serial.println(F("\n============ COMPARACION ============"));
  bool alguna = false;

  for (uint8_t mot = 1; mot <= 2; mot++) {
    for (uint8_t i = 0; i < nRes; i++) {
      if (rMotor[i] != mot) continue;
      uint8_t d = rDuty[i];

      // procesar cada duty una sola vez
      bool yaVisto = false;
      for (uint8_t j = 0; j < i; j++)
        if (rMotor[j] == mot && rDuty[j] == d) { yaVisto = true; break; }
      if (yaVisto) continue;

      uint8_t nC, nB;
      float fC = promedio(COAST, mot, d, nC);
      float fB = promedio(BRAKE, mot, d, nB);
      if (nC == 0 || nB == 0) continue;

      alguna = true;
      Serial.print(F("\nMotor ")); Serial.print(mot);
      Serial.print(F(", duty ")); Serial.println(d);
      Serial.print(F("  COAST: ")); Serial.print(fC, 1);
      Serial.print(F(" gf  (n=")); Serial.print(nC); Serial.println(F(")"));
      Serial.print(F("  BRAKE: ")); Serial.print(fB, 1);
      Serial.print(F(" gf  (n=")); Serial.print(nB); Serial.println(F(")"));

      if (fC > 0.1f) {
        float g = fB / fC;
        Serial.print(F("  >>> GANANCIA BRAKE/COAST = ")); Serial.print(g, 2);
        Serial.println(F("x <<<"));
        if (nC < 2 || nB < 2)
          Serial.println(F("  ⚠️ Menos de 2 corridas por modo: la razon no tiene respaldo."));
      } else {
        Serial.println(F("  COAST ~ 0: no se puede formar la razon."));
      }
    }
  }

  if (!alguna) {
    Serial.println(F("\nNo hay ninguna combinacion motor+duty medida en LOS DOS"));
    Serial.println(F("modos. Para comparar hace falta el mismo duty en ambos"));
    Serial.println(F("(recordar el tope de BRAKE: 127)."));
  }

  Serial.println(F("\n--- Como leer esto ---"));
  Serial.println(F("La ganancia es a duty IGUAL. El deficit de torque de 7.17"));
  Serial.println(F("es 2.42x, pero medido a duty 160 en COAST; si aca mediste a"));
  Serial.println(F("120, no se compara directo: sirve para saber si el modo de"));
  Serial.println(F("decaimiento mueve la aguja, no para cerrar el deficit."));
  Serial.print(F("Referencia Test 7 (duty 160, COAST): M1 "));
  Serial.print(T7_M1_GF, 1); Serial.print(F(" gf, M2 "));
  Serial.print(T7_M2_GF, 1); Serial.println(F(" gf."));
  Serial.println(F("=====================================\n"));
}

// --- Menu ----------------------------------------------------------------
void menu() {
  Serial.println(F("\n=== TEST 9 - torque COAST vs BRAKE ==="));
  Serial.print(F("modo = ")); Serial.print(nombreModo(modo));
  Serial.print(F(" (tope ")); Serial.print(capDe(modo)); Serial.print(F(")"));
  Serial.print(F("   motor = ")); Serial.print(motor);
  Serial.print(F("   sentido = ")); Serial.print(sentido > 0 ? F("FWD") : F("REV"));
  Serial.print(F("   duty = ")); Serial.print(duty);
  Serial.print(F("   corridas = ")); Serial.println(nRes);
  Serial.print(F("radio del hilo = ")); Serial.print(rRueda * 1000.0f, 1);
  Serial.println(F(" mm   <-- verificar con calibre en cada remontaje"));
  Serial.println(F("c=COAST  k=BRAKE  1/2=motor  i=invertir  d=duty  r=radio"));
  Serial.println(F("m=corrida (pulso 3.5s)   e=ENSAYO SOSTENIDO (continuo)"));
  Serial.println(F("w=ENSAYO INTERMITENTE (ciclo ON/OFF, mas realista)"));
  Serial.println(F("t=comparar  l=listar  z=borrar  x=parar"));
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  configurar();

  Serial.println(F("\n\n########################################"));
  Serial.println(F("TEST 9 - torque con balanza: COAST vs BRAKE"));
  Serial.println(F("########################################"));
  Serial.println(F("Mide el torque DIRECTO con la balanza y compara los dos"));
  Serial.println(F("modos de decaimiento. Sin multimetro y sin modelo: la"));
  Serial.println(F("razon entre modos ES la ganancia."));
  Serial.println(F("\nOrden sugerido:"));
  Serial.println(F("  0. MEDIR EL RADIO CON CALIBRE y cargarlo con 'r'"));
  Serial.println(F("     (cambia al pasar el hilo de una rueda a la otra)"));
  Serial.println(F("  1. Montaje del Test 7 (hilo tangente + pesa ~900 g)"));
  Serial.println(F("  2. Elegir motor (1/2) y duty seguro para ambos modos (120)"));
  Serial.println(F("  3. 'c' -> 2 o 3 corridas en COAST"));
  Serial.println(F("  4. 'k' -> 2 o 3 corridas en BRAKE, mismo motor y duty"));
  Serial.println(F("  5. 't' -> comparar"));
  Serial.println(F("\n⚠️ El tope de duty NO significa lo mismo en los dos modos:"));
  Serial.println(F("   en BRAKE el mismo duty entrega bastante mas."));
  Serial.println(F("⚠️ BRAKE arriba de 127 pasa el nominal de 6 V del motor."));
  Serial.println(F("   OK para pulsos de medicion, NO para operacion continua."));
  Serial.println(F("⚠️ Leer la balanza siempre en el mismo instante (~2 s)."));
  Serial.println(F("⚠️ Bateria + USB. No saltear el descanso de 12 s."));
  Serial.println(F("\nPara el ensayo TERMICO ('e'): ver la seccion dedicada"));
  Serial.println(F("arriba en el codigo fuente. Motor NO medido trabado aparte,"));
  Serial.println(F("los dos giran juntos, bateria llena, cortar ante cualquier"));
  Serial.println(F("señal termica anomala."));
  menu();
}

void loop() {
  if (!Serial.available()) { delay(10); return; }
  char c = Serial.read();
  switch (c) {
    case 'c': modo = COAST; configurar();
              if (duty > capDe(modo)) duty = capDe(modo);
              menu(); break;
    case 'k': modo = BRAKE; configurar();
              if (duty > capDe(modo)) {
                duty = capDe(modo);
                Serial.println(F("\nDuty recortado al tope de BRAKE."));
              }
              menu(); break;
    case '1': motor = 1; configurar(); menu(); break;
    case '2': motor = 2; configurar(); menu(); break;
    case 'i': sentido = -sentido; menu(); break;
    case 'd': { float v = leerNumero("Duty: ");
                if (!isnan(v) && v > 0) duty = constrain((int)v, 1, capDe(modo));
                menu(); break; }
    case 'm': corrida(); menu(); break;
    case 'e': sostenido(); menu(); break;
    case 'w': intermitente(); menu(); break;
    case 't': comparar(); menu(); break;
    case 'l': listar(); menu(); break;
    case 'r': { float v = leerNumero("Radio donde apoya el hilo, con CALIBRE (mm): ");
                if (!isnan(v) && v > 5 && v < 100) {
                  rRueda = v / 1000.0f;
                  if (nRes > 0)
                    Serial.println(F("⚠️ Hay corridas cargadas: sus τ se recalculan con el radio nuevo.\n"
                                     "   Si el remontaje cambio, borrar con 'z' y volver a medir."));
                } else Serial.println(F("Fuera de rango (5-100 mm), sin cambios."));
                menu(); break; }
    case 'z': nRes = 0; Serial.println(F("\nCorridas borradas.")); menu(); break;
    case 'x': parar(); Serial.println(F("\nPARADO.")); menu(); break;
    default: break;
  }
}
