/*
  ========================================================================
  TEST 8 — Caída de tensión del L298N en stall
  ========================================================================

  ⛔⛔ SUPERADO — NO USAR SUS RESULTADOS (corrido y descartado 2026-08-17)
  ⛔⛔ Ver docs/conexiones-registro-pruebas.md 7.18

  Su modelo es FALSO. Asume que en el tramo OFF del PWM el motor ve 0 V, y no
  es cierto: el PWM va sobre ENABLE, asi que al apagar, las dos salidas del
  puente quedan en alta impedancia y la corriente inductiva vuelve al bus por
  los diodos, poniendo las pestanas a tension NEGATIVA (modo coast).

  Se descarta con sus propios datos: dos duties sobre la misma condicion
  tienen que dar la misma caida y no la dan.

      duty 150 -> V=1.8 V -> caida implicita 9.08 V
      duty 160 -> V=2.3 V -> caida implicita 8.45 V

  Ademas la lectura ARRANCA ALTA Y BAJA durante la ventana: el L298N degrada
  su salida al calentarse. Ningun voltaje en stall medido asi es regimen
  permanente. (El bus NO es el culpable: monitoreado en vivo, cayo solo de
  12.12 a 12.07 V.)

  El intento de arreglarlo ajustando V_off (test8b_caida_barrido_esp32)
  TAMBIEN fallo. La via de caracterizar este driver con multimetro no sirve.

  => Lo que se hace en su lugar: medir el torque DIRECTO con la balanza.
     Ver test9_decay_torque_esp32/.

  Se conserva como historial del metodo. Todo lo que imprima ---sobre todo
  el veredicto de "cambiar el driver alcanza/no alcanza"--- carece de
  respaldo.

  ------------------------------------------------------------------------

  POR QUE EXISTE:
  El Test 7 (7.17) midió que al robot le falta un factor 2.42 de torque para
  poder balancear. La pregunta que decide QUE COMPRAR es de donde sale ese
  deficit:

    - Si el L298N se esta comiendo varios volts, cambiar el driver por uno de
      MOSFETs recupera una fraccion grande del deficit sin tocar los motores.
    - Si el driver entrega casi todo el bus, el deficit es mecanico y la unica
      salida es cambiar la reduccion de los motores.

  El L298N usa BJTs, no MOSFETs, y su caida CRECE CON LA CORRIENTE. Por eso hay
  que medir EN STALL: es la condicion de corriente maxima, y es exactamente la
  condicion en la que se midio el torque del Test 7.

  ------------------------------------------------------------------------
  LA CUENTA

  El motor no ve DC: ve PWM. Un multimetro en modo DC promedia, y eso es
  justamente lo que queremos. Durante el tramo ON el motor ve (V_bus - V_caida)
  y durante el OFF ve ~0, asi que:

      V_medido = (duty/255) * (V_bus - V_caida)

      -->   V_caida = V_bus - V_medido * 255 / duty

  De ahi sale lo unico que importa para la decision: cuanto torque se
  recuperaria con un driver de MOSFETs (caida ~0.2 V en vez de la medida). En
  stall el torque es proporcional a la tension que llega al motor, asi que:

      ganancia = (V_bus - 0.2) / (V_bus - V_caida)

  ------------------------------------------------------------------------
  COMO SE USA

    b  -> cargar V_bus, medido en VIN+ del L298N contra GND de potencia
    d  -> cambiar el duty (por defecto 160, el cap vigente)
    1  -> motor 1        2 -> motor 2        i -> invertir sentido
    m  -> aplicar el duty durante 8 s y pedir la tension medida
    x  -> parar

  ⚠️ ENGANCHA LAS PUNTAS DEL MULTIMETRO ANTES DE ARRANCAR. La ventana es de
  8 s a proposito: en stall el motor y los BJTs del L298N disipan mucho. Con
  pinzas cocodrilo en OUT1/OUT2 solo hay que leer el display.

  ⚠️ COMO TRABAR LA RUEDA: sirve cualquier cosa que impida que gire — el
  montaje del Test 7 (hilo + pesa) es el mas comodo porque ya esta probado que
  clava el motor, pero una morsa o una cuña alcanzan. No hace falta que el
  bloqueo sea calibrado: solo que la rueda NO gire.

  ⚠️ MEDIR TAMBIEN CON LA RUEDA LIBRE, como control. Girando, la corriente es
  baja y la caida deberia ser mucho menor. La diferencia entre las dos
  condiciones ES la parte de la caida que depende de la corriente, que es la
  que un driver de MOSFETs elimina.

  CABLEADO: identico al Test 3/5/7 (docs/conexiones-registro-pruebas.md 3.1)
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

const uint16_t VENTANA_MS = 8000;    // en stall: corto, pero alcanza para leer
const float    V_MOSFET   = 0.2f;    // caida tipica de un puente de MOSFETs

// --- Estado --------------------------------------------------------------
float   vBus   = 0;
uint8_t duty   = 160;
uint8_t motor  = 1;
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

// --- Medicion ------------------------------------------------------------
void medir() {
  if (vBus <= 0) {
    Serial.println(F("\nFalta V_bus: medir en VIN+ del L298N y cargarlo con 'b'."));
    return;
  }
  Serial.println(F("\n--- Medicion de caida ---"));
  Serial.print(F("Motor ")); Serial.print(motor);
  Serial.print(F(", duty ")); Serial.print(duty);
  Serial.print(F(", ventana ")); Serial.print(VENTANA_MS / 1000);
  Serial.println(F(" s."));
  Serial.println(F("Puntas del multimetro en OUT del motor, en DC."));
  Serial.println(F("Anota tambien si la rueda esta TRABADA o LIBRE."));

  if (!esperarEnter("ENTER cuando las puntas esten puestas (o 'x'): ")) return;

  for (uint8_t i = 3; i > 0; i--) { Serial.print(i); Serial.print(F("... ")); Serial.flush(); delay(1000); }
  Serial.println(F("APLICANDO - LEE EL MULTIMETRO"));
  Serial.flush();

  aplicar(motor, sentido, duty);
  delay(VENTANA_MS);
  pararTodo();
  Serial.println(F("listo."));

  float vMot = leerNumero("Tension medida en el motor (V): ");
  if (isnan(vMot)) return;

  float frac    = (float)duty / 255.0f;
  float vIdeal  = vBus * frac;                 // si el driver no cayera nada
  float vCaida  = vBus - vMot / frac;          // caida real del puente
  float perdido = vIdeal - vMot;

  Serial.println(F("\n===== RESULTADO ====="));
  Serial.print(F("V_bus        = ")); Serial.print(vBus, 2);  Serial.println(F(" V"));
  Serial.print(F("duty         = ")); Serial.print(duty);
  Serial.print(F(" / 255  (")); Serial.print(frac * 100, 1); Serial.println(F("%)"));
  Serial.print(F("V ideal      = ")); Serial.print(vIdeal, 2);
  Serial.println(F(" V   (lo que veria el motor sin caida)"));
  Serial.print(F("V medido     = ")); Serial.print(vMot, 2);  Serial.println(F(" V"));
  Serial.print(F("Perdido      = ")); Serial.print(perdido, 2);
  Serial.print(F(" V  (")); Serial.print(100.0f * perdido / vIdeal, 0);
  Serial.println(F("% de lo disponible)"));
  Serial.print(F("\nCAIDA DEL PUENTE = ")); Serial.print(vCaida, 2);
  Serial.println(F(" V"));

  if (vCaida > 0 && vCaida < vBus) {
    float gan = (vBus - V_MOSFET) / (vBus - vCaida);
    Serial.print(F("\n>>> Con un puente de MOSFETs (caida ~"));
    Serial.print(V_MOSFET, 1); Serial.print(F(" V) el torque se multiplicaria por "));
    Serial.print(gan, 2); Serial.println(F(" <<<"));
    Serial.println(F("    (en stall el torque es proporcional a la tension)"));

    Serial.println(F("\n--- Que hacer con esto ---"));
    Serial.print(F("Deficit medido en 7.17: 2.42x.  Con este driver nuevo"));
    Serial.print(F(" quedaria: ")); Serial.print(2.42f / gan, 2); Serial.println(F("x"));
    if (gan >= 2.42f) {
      Serial.println(F("=> El driver SOLO alcanzaria. No hace falta cambiar motores."));
    } else if (gan >= 1.3f) {
      Serial.println(F("=> Cambiar el driver vale la pena, pero NO alcanza solo."));
      Serial.print(F("   Reduccion necesaria despues del cambio: ~"));
      Serial.print(793.0f * gan / 2.42f, 0); Serial.println(F(" RPM"));
    } else {
      Serial.println(F("=> El driver no es el problema. El deficit es mecanico:"));
      Serial.println(F("   la salida es cambiar la reduccion (~330 RPM)."));
    }
  } else {
    Serial.println(F("\n⚠️ Caida fuera de rango. Revisar V_bus y la lectura."));
  }
  Serial.println(F("=====================\n"));
}

void menu() {
  Serial.println(F("\n=== TEST 8 - caida del L298N ==="));
  Serial.print(F("V_bus = "));
  if (vBus > 0) { Serial.print(vBus, 2); Serial.print(F(" V")); } else Serial.print(F("SIN CARGAR"));
  Serial.print(F("   duty = ")); Serial.print(duty);
  Serial.print(F("   motor = ")); Serial.print(motor);
  Serial.print(F("   sentido = ")); Serial.println(sentido > 0 ? F("FWD") : F("REV"));
  Serial.println(F("b=V_bus  d=duty  1/2=motor  i=invertir  m=medir  x=parar"));
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
  Serial.println(F("TEST 8 - caida de tension del L298N"));
  Serial.println(F("########################################"));
  Serial.println(F("Orden sugerido:"));
  Serial.println(F("  1. Medir VIN+ del L298N con el multimetro -> cargar con 'b'"));
  Serial.println(F("  2. Trabar la rueda (hilo+pesa del Test 7, morsa, cuña...)"));
  Serial.println(F("  3. Pinzas en los OUT del motor, multimetro en DC"));
  Serial.println(F("  4. 'm' -> medir. Repetir con la rueda LIBRE como control."));
  Serial.println(F("\n⚠️ Bateria + USB. En stall el L298N calienta: no encadenar"));
  Serial.println(F("   mediciones sin dejarlo enfriar."));
  menu();
}

void loop() {
  if (!Serial.available()) { delay(10); return; }
  char c = Serial.read();
  switch (c) {
    case 'b': { float v = leerNumero("V_bus medido en VIN+ del L298N (V): ");
                if (!isnan(v) && v > 0) vBus = v; menu(); break; }
    case 'd': { float v = leerNumero("Duty (0-160): ");
                if (!isnan(v) && v > 0) duty = constrain((int)v, 1, MAX_PWM_DUTY);
                menu(); break; }
    case '1': motor = 1; menu(); break;
    case '2': motor = 2; menu(); break;
    case 'i': sentido = -sentido; menu(); break;
    case 'm': medir(); menu(); break;
    case 'x': pararTodo(); Serial.println(F("\nPARADO.")); menu(); break;
    default: break;
  }
}
