/*
  ========================================================================
  TEST 11 — Bring-up del driver BTS7960 (reemplazo del L298N)
  ========================================================================

  POR QUE EXISTE

  Se compraron 2 modulos BTS7960 para reemplazar el L298N, bajo la hipotesis
  (7.20, sin confirmar) de que la caida de torque del 31-36% al conducir los
  dos motores a la vez es interna al L298N -- que tiene los dos puentes en
  un mismo die. El BTS7960 es un puente H SIMPLE: cada modulo mueve UN motor,
  asi que la arquitectura queda con dos chips separados por construccion.
  Hacen falta los 2 modulos para operacion basica -- no hay uno de repuesto
  con lo comprado.

  Este sketch es SOLO bring-up: verificar que el cableado nuevo esta bien y
  que los motores responden en el sentido correcto, antes de pasar a medir
  nada con la balanza. Es el mismo paso que test2_motores_esp32 cumplio para
  el L298N -- saltearlo ya costo horas de diagnostico tres veces en este
  proyecto (ver CLAUDE.md, "Checklist antes de cualquier prueba que mueva
  motores").

  ------------------------------------------------------------------------
  ⚠️⚠️ PINES: ASUNCION, NO DATO CONFIRMADO -- REVISAR ANTES DE FLASHEAR

  Se reutilizan los mismos 3 GPIO por motor que tenia el L298N, reinterpre-
  tados para el BTS7960:

      L298N          ->  BTS7960 (este sketch)
      ENA (M1)       ->  EN  (R_EN + L_EN del modulo, atados juntos)
      IN1 (M1)       ->  RPWM
      IN2 (M1)       ->  LPWM
      (mismo patron para M2 con ENB/IN3/IN4)

  Si el cableado real de mañana usa otros pines, CAMBIAR los #define de
  abajo antes de compilar -- no asumir que esto ya esta bien.

      M1: EN=GPIO27  RPWM=GPIO19  LPWM=GPIO13   <-- RPWM es 19, NO 14
      M2: EN=GPIO4   RPWM=GPIO16  LPWM=GPIO17

  R_IS/L_IS (salidas analogicas de corriente de cada modulo) NO estan
  cableadas en este test. Podrian conectarse a pines de solo-entrada (34,
  35, 36, 39 -- libres segun CLAUDE.md) en una version futura para medir
  corriente en vivo, pero eso queda para despues del bring-up.

  ------------------------------------------------------------------------
  ⚠️⚠️ SEGURIDAD: EL CAP DE 160 DEL L298N NO APLICA ACA

  El L298N (BJTs) tenia una caida de conduccion importante, ya caracteriza-
  da en 7.19/7.20. El BTS7960 (MOSFETs, RDSon en el orden de mOhm) va a
  tener MUCHA menos caida -- el mismo duty entrega mas tension real al
  motor de lo que entregaba el L298N.

  ⚠️ Version anterior de este comentario tenia un ERROR: proponia 130
  llamandolo "conservador" (130/255*12.6V = 6.42V, YA arriba del nominal
  de 6V -- no era conservador, estaba mal calculado). Con el L298N el 160
  seguia dando menos de 6V reales porque el driver se comia varios volts;
  con el BTS7960 esa caida es chica, asi que la cuenta ingenua SI aplica:

      MAX_DUTY_ABSOLUTO = 121   (6.0V / 12.6V bateria llena x 255 = 121.4)

  Es, sin buscarlo, el mismo 121 que fue el primer calculo del L298N (misma
  fisica; ahi resulto ser demasiado conservador porque el driver comia
  varios volts que este calculo ignoraba -- aca no hay esa caida que
  compense el error, asi que el numero SI es el limite real).

  Subir este tope es una decision aparte, para despues de: (1) confirmar
  sentido y comportamiento correctos a duty bajo, (2) medir la relacion
  duty-tension real de este driver (multimetro) o el torque con la balanza
  (como el Test 9) -- recien ahi, con la caida de conduccion real medida
  (que no es exactamente cero), se puede subir el cap con criterio.

  ------------------------------------------------------------------------
  ⚠️⚠️ SIN PULL-DOWNS DE SEGURIDAD EN LOS PINES NUEVOS TODAVIA

  Plan propuesto (mas simple que el del L298N, ver "PLAN DE CABLEADO" abajo):
  UN pull-down de 10k en cada linea EN (2 en total, contra los 4 que tenia
  el L298N en IN1-4). En el BTS7960, EN en bajo deshabilita el puente
  completo sin importar el estado de RPWM/LPWM -- alcanza con protegerlo a
  el. ⚠️ Asuncion basada en el comportamiento tipico de estos drivers
  (IR2104-like); confirmar contra el datasheet del modulo especifico
  comprado antes de confiar en esto como unica proteccion.

  ESTE test corre SIN esa proteccion todavia instalada. Mano en el switch
  de V+, no dejar el robot andando sin supervision.

  ------------------------------------------------------------------------
  PLAN DE CABLEADO PROPUESTO (a confirmar antes de armar)

  Opcion elegida: R_EN y L_EN de cada modulo ATADOS JUNTOS a un solo pin
  "EN" del ESP32 (mismo patron que ENA/ENB del L298N) -- mas simple de
  cablear y de programar que manejarlos por separado, y no hay necesidad
  identificada de controlarlos de forma independiente para este proyecto.

  Reusa exactamente los mismos 6 GPIO que manejaban el L298N -- las mismas
  seis lineas del protoboard, solo cambia a que pin del modulo nuevo llega
  cada una:

      Motor 1 (derecho)              Motor 2 (izquierdo)
      ESP32 GPIO27 -> EN (R_EN+L_EN) ESP32 GPIO4  -> EN (R_EN+L_EN)
      ESP32 GPIO19 -> RPWM           ESP32 GPIO16 -> RPWM
      ESP32 GPIO13 -> LPWM           ESP32 GPIO17 -> LPWM

  Ventaja de reusar los mismos pines: el cambio de cableado es "desconectar
  del L298N, conectar al BTS7960", sin tocar filas del protoboard del lado
  del ESP32.

  CONFIRMADO CONTRA docs/bts7960_datasheet.pdf (modulo HW-039, 2026-08-19):

  1. Alimentacion logica: pin VCC pide +5V APARTE, no se deriva de la
     potencia. Sale del mismo riel del Buck que ya alimenta el ESP32 y el
     GY-521 -- mismo patron que el "5V logico" del L298N (ver CLAUDE.md).
     GND del modulo comun con el ESP32 (mismo bug-class que "GND comun
     ESP32-L298N" en CLAUDE.md: confirmar continuidad, no asumir).
  2. R_EN + L_EN atados juntos es la practica estandar segun el propio
     fabricante del modulo -- el plan de EN unico por motor queda
     respaldado, no es solo una suposicion mia.
  3. PWM hasta 25 kHz soportado -- los 20 kHz que ya usa el proyecto entran
     sin cambios.
  4. Corriente continua recomendada: 20A. Muy por encima de lo que este
     motor va a pedir -- la duda de dimensionamiento por corriente que
     tenia el TB6612FNG no aplica aca.

  CONFIRMADO CONTRA docs/BTS7960.pdf (datasheet OFICIAL Infineon del chip
  BTS7960B, no la ficha del modulo, 2026-08-19):

  5. EN/INH en bajo apaga los dos MOSFETs -- textual: "To deactivate both
     switches, the INH pin has to be set to low. No external driver is
     needed." El plan de pull-down en EN queda CONFIRMADO, no es mas una
     suposicion.
  6. R_IS: factor de conversion EXACTO, k_ILIS = I_L / I_IS = 8500 (tipico).
     Con resistencia externa de 1k en IS: V_IS = (I_L / 8.5A) V. Verificar
     si el modulo HW-039 ya trae esa resistencia instalada (continuidad/
     resistencia con multimetro) antes de asumir el valor de R_IS.
     Candidatos de pin: GPIO34 (M1), GPIO35 (M2) -- libres, solo-entrada,
     con ADC.

  ------------------------------------------------------------------------
  ⚠️ HALLAZGO SIN CONFIRMAR: el esquema RPWM/LPWM=0 estandar PODRIA YA SER
  "BRAKE" por diseño del chip, no "coast" como el L298N

  El datasheet describe "active freewheeling": el lado modulado por PWM
  enciende activamente su MOSFET inferior en el tramo OFF (no depende del
  diodo), y el lado fijo en IN=0 mantiene su MOSFET inferior encendido de
  forma continua (IN no es tri-state por si solo, solo INH lo es). Con
  RPWM modulado y LPWM fijo en 0: durante el tramo OFF de RPWM, LOS DOS
  terminales del motor (M+ y M-) quedan atados a GND al mismo tiempo --
  eso es la definicion de brake (motor en cortocircuito), no coast.

  Si esto se confirma, el BTS7960 daria de entrada la ganancia que en el
  L298N hizo falta ganar moviendo el PWM de ENABLE a IN (Test 9, 7.19) --
  gratis, sin truco de firmware.

  ⚠️⚠️ ES UNA INFERENCIA DEL DATASHEET, NO UNA MEDICION. Este proyecto ya
  aprendio (dos veces con ω₀, dos veces con K_U, ver CLAUDE.md "Lecciones
  de metodo") a no confiarle a una cadena de razonamiento lo que solo mide
  la balanza. Se verifica con el mismo metodo del Test 9 apenas este driver
  este cableado -- no asumir coast ni brake de antemano en ningun calculo.

  ------------------------------------------------------------------------
  ⚠️⚠️ COAST vs BRAKE EN ESTE DRIVER: TODAVIA NO SE SABE

  El Test 9 (7.19) encontro que el L298N hacia coast (fast decay) con el
  PWM sobre ENABLE, y que pasar a brake (PWM sobre IN, EN fijo) multiplico
  el torque 2.67x. El comportamiento del BTS7960 durante el tramo OFF del
  PWM (que hacen internamente sus drivers IR2104-like cuando RPWM baja)
  NO esta confirmado -- varia segun el modulo especifico. Esto se mide
  despues, con la balanza, igual que se hizo con el L298N. Este sketch NO
  responde esa pregunta.

  ------------------------------------------------------------------------
  QUE HACE ESTE SKETCH

  Aplica un pulso CORTO (2 s, no continuo) al motor elegido, a un duty bajo
  por defecto, y se detiene solo. Pensado para: confirmar sentido de giro,
  confirmar que no hay comportamiento raro (chirridos, tironeos, olor),
  antes de subir el duty. Incluye un modo "los dos juntos" para empezar a
  escuchar/sentir de forma cualitativa si aparece algo parecido a la caida
  de carga simultanea del L298N (7.20) -- esto es solo un chequeo auditivo
  de arranque, NO reemplaza medir con la balanza.

  CABLEADO: ver el bloque de arriba. Motor 1 = derecho, Motor 2 = izquierdo
  (misma convencion que el resto del proyecto).
  ========================================================================
*/

#include <Arduino.h>

// --- Pines -- CONFIRMADOS contra el cableado real (2026-09-12) ------------
// M1_RPWM es GPIO19, NO GPIO14: el 14 tiene salida activa durante el boot
// (reportado, no en el datasheet oficial) y cae bajo la misma regla que GPIO5
// de este proyecto -- no usarlo para nada que mueva un motor. Analisis en
// docs/plano-conexiones-esp32.html seccion 3; filas en 3.1b del registro.
const uint8_t M1_EN = 27, M1_RPWM = 19, M1_LPWM = 13;
const uint8_t M2_EN = 4,  M2_RPWM = 16, M2_LPWM = 17;

// --- PWM -------------------------------------------------------------------
const uint32_t PWM_FREQ_HZ  = 20000;
const uint8_t  PWM_RES_BITS = 8;

// Tope derivado para ESTE driver -- no es el 160 del L298N (ver cabecera).
// 6.0V / 12.6V bateria llena x 255 = 121.4 -- corregido, no es "conservador
// de mas": con MOSFETs la caida de conduccion es chica, esta cuenta ingenua
// SI es aproximadamente el limite real.
const uint8_t MAX_DUTY_ABSOLUTO = 121;
uint8_t duty = 40;     // arranque bien bajo, primer encendido de driver sin verificar

const uint16_t PULSO_MS = 2000;   // pulso corto, no continuo, para bring-up

uint8_t motor   = 1;
int8_t  sentido = 1;

void configurarPines() {
  pinMode(M1_EN, OUTPUT);   pinMode(M2_EN, OUTPUT);
  digitalWrite(M1_EN, LOW); digitalWrite(M2_EN, LOW);   // arranca deshabilitado
  ledcAttach(M1_RPWM, PWM_FREQ_HZ, PWM_RES_BITS); ledcWrite(M1_RPWM, 0);
  ledcAttach(M1_LPWM, PWM_FREQ_HZ, PWM_RES_BITS); ledcWrite(M1_LPWM, 0);
  ledcAttach(M2_RPWM, PWM_FREQ_HZ, PWM_RES_BITS); ledcWrite(M2_RPWM, 0);
  ledcAttach(M2_LPWM, PWM_FREQ_HZ, PWM_RES_BITS); ledcWrite(M2_LPWM, 0);
}

// Nunca PWM en RPWM y LPWM a la vez -- shoot-through / freno no intencional.
void aplicarMotor(uint8_t m, int8_t dir, uint8_t d) {
  d = constrain(d, 0, MAX_DUTY_ABSOLUTO);
  uint8_t en = (m == 1) ? M1_EN : M2_EN;
  uint8_t rp = (m == 1) ? M1_RPWM : M2_RPWM;
  uint8_t lp = (m == 1) ? M1_LPWM : M2_LPWM;

  digitalWrite(en, HIGH);
  if (dir > 0)      { ledcWrite(rp, d); ledcWrite(lp, 0); }
  else if (dir < 0) { ledcWrite(rp, 0); ledcWrite(lp, d); }
  else              { ledcWrite(rp, 0); ledcWrite(lp, 0); }
}

void pararMotor(uint8_t m) {
  uint8_t en = (m == 1) ? M1_EN : M2_EN;
  uint8_t rp = (m == 1) ? M1_RPWM : M2_RPWM;
  uint8_t lp = (m == 1) ? M1_LPWM : M2_LPWM;
  ledcWrite(rp, 0); ledcWrite(lp, 0);
  digitalWrite(en, LOW);
}

void pararTodo() { pararMotor(1); pararMotor(2); }

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

void pulso(uint8_t m) {
  uint8_t d = constrain(duty, 0, MAX_DUTY_ABSOLUTO);
  Serial.print(F("\n--- Pulso: motor ")); Serial.print(m);
  Serial.print(F(", ")); Serial.print(sentido > 0 ? F("FWD") : F("REV"));
  Serial.print(F(", duty ")); Serial.print(d);
  Serial.print(F(" (")); Serial.print(100.0f * d / 255.0f, 1);
  Serial.println(F("%), 2 s"));
  Serial.println(F("Mirar/escuchar: sentido correcto, sin ruidos raros."));

  aplicarMotor(m, sentido, d);
  delay(PULSO_MS);
  pararMotor(m);
  Serial.println(F("listo, motor detenido."));
}

void pulsoAmbos() {
  uint8_t d = constrain(duty, 0, MAX_DUTY_ABSOLUTO);
  Serial.print(F("\n--- Pulso AMBOS motores, duty ")); Serial.print(d);
  Serial.println(F(", 2 s ---"));
  Serial.println(F("Chequeo cualitativo: algun motor suena/vibra distinto"));
  Serial.println(F("que corriendo solo? (comparar con pulso individual)"));

  aplicarMotor(1, sentido, d);
  aplicarMotor(2, sentido, d);
  delay(PULSO_MS);
  pararTodo();
  Serial.println(F("listo, motores detenidos."));
}

void menu() {
  Serial.println(F("\n=== TEST 11 - bring-up BTS7960 ==="));
  Serial.print(F("motor = ")); Serial.print(motor);
  Serial.print(F("   sentido = ")); Serial.print(sentido > 0 ? F("FWD") : F("REV"));
  Serial.print(F("   duty = ")); Serial.print(duty);
  Serial.print(F(" (tope absoluto ")); Serial.print(MAX_DUTY_ABSOLUTO); Serial.println(F(")"));
  Serial.println(F("1/2=motor  i=invertir  d=duty  p=pulso 2s  a=pulso AMBOS  x=parar"));
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  configurarPines();

  Serial.println(F("\n\n########################################"));
  Serial.println(F("TEST 11 - bring-up del BTS7960"));
  Serial.println(F("########################################"));
  Serial.println(F("SOLO verificacion de cableado y sentido. No mide torque."));
  Serial.println(F("\n⚠️ PINES: asuncion (reuso de los del L298N). Verificar"));
  Serial.println(F("   contra el cableado real antes de confiar en el sentido."));
  Serial.println(F("⚠️ SIN pull-downs de seguridad en estos pines todavia."));
  Serial.println(F("   Mano en el switch de V+."));
  Serial.println(F("⚠️ Tope de duty conservador (130), NO el 160 del L298N --"));
  Serial.println(F("   este driver cae mucho menos, el mismo duty da mas tension."));
  Serial.println(F("\nOrden sugerido:"));
  Serial.println(F("  1. Motor 1 solo, duty bajo (60), FWD -> confirmar sentido"));
  Serial.println(F("  2. Motor 1 solo, REV ('i') -> confirmar que invierte bien"));
  Serial.println(F("  3. Repetir con motor 2"));
  Serial.println(F("  4. Subir duty de a poco si todo se ve normal"));
  Serial.println(F("  5. 'a' -> los dos juntos, chequeo auditivo cualitativo"));
  menu();
}

void loop() {
  if (!Serial.available()) { delay(10); return; }
  char c = Serial.read();
  switch (c) {
    case '1': motor = 1; menu(); break;
    case '2': motor = 2; menu(); break;
    case 'i': sentido = -sentido; menu(); break;
    case 'd': { float v = leerNumero("Duty: ");
                if (!isnan(v) && v > 0) duty = constrain((int)v, 1, MAX_DUTY_ABSOLUTO);
                menu(); break; }
    case 'p': pulso(motor); menu(); break;
    case 'a': pulsoAmbos(); menu(); break;
    case 'x': pararTodo(); Serial.println(F("\nPARADO.")); menu(); break;
    default: break;
  }
}
