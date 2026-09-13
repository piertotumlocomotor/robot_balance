/*
  ========================================================================
  TEST 5b — K_U por DESVIACION ESTATICA (eje trabado)
  ========================================================================

  POR QUE EXISTE:
  El Test 5 estima K_U derivando el giroscopio durante un transitorio de
  500 ms. Con el eje del motor TRABADO al soporte ese metodo no sirve, por
  dos razones independientes:

    1. La gravedad deja de ser despreciable dentro de la ventana. La
       respuesta al escalon es w(t) = (K_U*u/w0)*sin(w0*t), cuyo pico cae
       en t = pi/(2*w0) = 0.67 s -- FUERA de los 500 ms que mide el Test 5.
    2. No hace falta derivar nada. El mismo sistema tiene solucion estatica.

  METODO:
  Con el eje trabado y el robot colgando, un duty sostenido lleva al cuerpo
  a un angulo de equilibrio donde el torque del motor iguala al de gravedad:

      K_U * u  =  w0^2 * sin(theta_ss)

  Se mide theta_ss con el ACELEROMETRO (vector gravedad, medicion estatica,
  sin integrar ni derivar nada) para varios duties, y K_U sale de la
  PENDIENTE:

      K_U = w0^2 * d[sin(theta_ss)] / du

  ⚠️ PENDIENTE, NO COCIENTE. La friccion estatica de la reductora agrega un
  offset constante en duty: por debajo de cierto comando el cuerpo no se
  mueve nada. Ese offset se le suma entero al cociente theta/u (que daria
  K_U subestimado y distinto en cada duty), pero la pendiente lo ignora. De
  yapa, la ordenada al origen de la recta da ese umbral de friccion en
  unidades de duty -- un dato util por si solo.

  Se reporta el cociente por duty EN PARALELO, para poder ver la diferencia
  entre los dos estimadores sobre los mismos datos.

  Se usa sin(theta), no theta: el equilibrio real es tau = m*g*l*sin(theta).
  Asi la medicion no depende de que el angulo sea chico.

  ------------------------------------------------------------------------
  MONTAJE FISICO

  El mismo soporte de brackets del que ya cuelga el robot, con UN cambio:

  1. APRETAR el bulon que une la cupla de bronce al bracket, hasta que la
     cara de la cupla muerda la chapa. Pasa de pasador flojo a abrazadera.

  2. ⚠️ MARCA TESTIGO — sin esto la medicion no es verificable. Raya de
     fibron cruzando cupla<->eje y cupla<->chapa. Si despues de una corrida
     las rayas no coinciden, algo patino y ESA CORRIDA NO VALE. Son las dos
     interfaces de friccion que pueden ceder cerca de stall, y las dos
     inflan K_U (direccion peligrosa: te haria creer que el robot puede
     balancear cuando no).

  3. Chequeo de que el bulon agarra. ⚠️ NO es "que el cuerpo no se mueva":
     con el eje trabado, la unica forma que tiene el cuerpo de rotar es
     RETRO-ACCIONANDO LA REDUCTORA, y eso es justamente el mecanismo por el
     que el motor lo va a mover durante la medicion. Que se mueva es normal.

     Lo que hay que mirar es QUIEN se mueve:
       - Marcas quietas y el cuerpo se frena casi enseguida, pesado
         -> gira la reductora. CORRECTO.
       - Oscila varias veces y se apaga de a poco, liviano
         -> esta pivotando sobre el bulon flojo. NO esta trabado.
       - Las marcas se corrieron -> patino. Dato invalido.

     La diferencia se nota a mano: un buje flojo suena varios ciclos (asi se
     midio w0), retro-accionar la reductora mata el movimiento en menos de uno.

  4. Cuerda de seguridad floja, sin rozar.

  ⚠️ LIMITACION CONOCIDA, dejarla registrada con el resultado: la chapa del
  bracket se tuerce bajo el torque, y el IMU lee esa torsion como si fuera
  inclinacion del cuerpo. Infla K_U. La medicion por balanza (torque directo
  con el chasis amarrado) no tiene ese problema; esta si. Sirve como cota
  superior y para el criterio de pasa/no pasa, no como valor de precision.

  ------------------------------------------------------------------------
  ⚠️ TERMICO: con el eje trabado el motor gira solo unos grados, o sea que
  trabaja casi en STALL: corriente alta en el motor y en los BJTs del L298N.
  Por eso los pulsos son cortos y el descanso entre pulsos es largo. Tocar
  los motores y el disipador del L298N cada tanto; si queman, parar con 'x'.

  REPORTE: Serial a 115200. NO usa WiFi a proposito -- el AP que se cae con
  bateria sola sigue siendo un problema abierto (CLAUDE.md), y un pulso de
  corriente cerca de stall es exactamente lo que lo tira. Alimentar con
  bateria + USB conectado (la configuracion de debug correcta del proyecto).

  COMANDOS por Serial:  s = arrancar barrido    x = abortar

  CABLEADO: identico al Test 3/5 (docs/conexiones-registro-pruebas.md 3.1)
    I2C:     SDA=GPIO21, SCL=GPIO22
    Motor 1: ENA=GPIO27, IN1=GPIO14, IN2=GPIO13
    Motor 2: ENB=GPIO4,  IN3=GPIO16, IN4=GPIO17
  ========================================================================
*/

#include <Arduino.h>
#include <Wire.h>
#include <esp_system.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

/*
  DIAGNOSTICO DE REINICIOS (agregado 2026-08-15).

  La primera corrida devolvio 71432 repeticiones de la ultima linea del banner
  de arranque y nada mas: el ESP32 se reseteaba en bucle y nunca llegaba a
  aceptar el comando. Como el banner solo se imprime en setup(), repetirlo N
  veces significa que setup() corrio N veces.

  Estas dos cosas convierten esa adivinanza en dato:
    - `esp_reset_reason()` dice POR QUE se reinicio (brownout / panic /
      watchdog / power-on). Es la diferencia entre "se cae la alimentacion" y
      "el firmware crashea".
    - `bootCount` vive en memoria RTC, que SOBREVIVE a un reset pero no a un
      corte de alimentacion real. Si crece solo, hay bucle de reinicio; si
      queda en 1, cada arranque es un power-on genuino.

  ⚠️ `motivoReset()` esta definida abajo, junto a setup(), y NO aca. El IDE de
  Arduino genera los prototipos de todas las funciones y los inserta antes de
  la primera definicion de funcion del archivo: si esta quedara arriba, los
  prototipos caerian antes de los `struct` y no compilaria.
*/
RTC_DATA_ATTR uint32_t bootCount = 0;

// --- Pines ---------------------------------------------------------------
const uint8_t I2C_SDA = 21, I2C_SCL = 22;
const uint8_t ENA = 27, IN1 = 14, IN2 = 13;
const uint8_t ENB = 4,  IN3 = 16, IN4 = 17;

// --- PWM (LEDC, API core 3.x) --------------------------------------------
const uint32_t PWM_FREQ_HZ  = 20000;
const uint8_t  PWM_RES_BITS = 8;
const uint8_t  MAX_PWM_DUTY = 160;   // tope absoluto, ninguna ruta lo cruza

/*
  w0 medido el 2026-08-15 por video sobre el montaje de brackets (5 sueltas,
  13 periodos completos): T = 0.834..0.855 s -> w0 = 7.4 rad/s.

  ⚠️ REEMPLAZA al valor viejo de 2.33 rad/s (T = 2.699 s), que era 3.2x mas
  chico y estaba MAL: correspondia a un pendulo simple equivalente de 1.8 m,
  imposible para un robot de ~30 cm. El valor nuevo da L_eq = 17 cm, del
  orden del tamaño real del robot.

  K_U escala con w0^2, asi que ese error viejo valia un factor 10 en K_U.
*/
const float OMEGA0 = 7.4f;

// --- Parametros del experimento ------------------------------------------
/*
  El 70 es CONTROL NULO: esta por debajo de la zona muerta de M2 (~90), asi
  que ahi no puede haber señal fisica. Lo que mida es el piso de ruido del
  metodo, y es la referencia contra la cual se juzga si el resto es real.
  Queda fuera del ajuste de la recta.
*/
/*
  Duties revisados tras la corrida del 2026-08-15 (ver 7.16). Los duties 100,
  110 y 121 dieron 0.03-0.04 grados, indistinguible del control nulo: por
  debajo de ~135 la friccion estatica de la reductora no se rompe y no hay
  señal que medir. Se sacaron y el tiempo se reinvierte donde SI pasa algo.
*/
const uint8_t  DUTIES[]   = {70, 100, 115, 130, 145};
const uint8_t  N_DUTIES   = sizeof(DUTIES) / sizeof(DUTIES[0]);
const uint8_t  DUTY_NULO  = 70;
const uint8_t  N_REPS     = 3;

const uint16_t ASENTAR_MS  = 4000;   // quedarse quieto antes de leer el reposo
const uint16_t REPOSO_MS   = 1000;   // promediado del angulo de reposo
const uint16_t PULSO_MS    = 3000;   // duracion del escalon (casi stall: corto)
const uint16_t VENTANA_MS  = 900;    // ultimos ms del pulso que se promedian
const uint16_t DESCANSO_MS = 10000;  // enfriamiento entre pulsos

/*
  DITHER -- la correccion que hizo falta (2026-08-15, tras dos corridas).

  El problema: la banda de friccion estatica de la reductora es de +-3 grados,
  del mismo orden que la señal (3.7 grados a duty maximo). Con eso, cualquier
  metodo que dependa de que el cuerpo ENCUENTRE su equilibrio por si solo mide
  de donde venia, no la fisica. Evidencia directa de la corrida abortada:

    - Con motor apagado el cuerpo NO vuelve a la vertical: queda clavado a
      2.5-3.7 grados de donde lo dejo el pulso anterior.
    - El intento de acotar por arriba (pre-pulso a MAX y despues bajar a u)
      dio 7.06 grados en el CONTROL NULO, donde el motor ni siquiera gira:
      medía la posicion de estacionamiento del pre-pulso, no un equilibrio.
    - `th_arriba` BAJABA al subir el duty (7.06 -> 3.91 -> 3.37), al reves de
      lo que exige la fisica.

  La solucion: sacudir el comando para que la friccion estatica no agarre. Se
  superpone una oscilacion de +-DITHER_AMP puntos de duty durante todo el
  pulso, INCLUIDA la ventana de medicion. A 12.5 Hz el cuerpo casi no se mueve
  (el pendulo es de 1.2 Hz): solo se despega, y se asienta en el equilibrio
  real en vez de donde la friccion lo agarro.

  Que el duty tope sea 145 y no 160 es por esto: 145+15 = 160 deja al dither
  entero por debajo del tope absoluto. K_U sale de la pendiente en la zona
  limpia y la desviacion a 160 se EXTRAPOLA. De paso desaparece el punto
  degenerado de duty 160, donde no habia margen por encima para acotar.
*/
const uint8_t  DITHER_AMP      = 15;   // puntos de duty
const uint16_t DITHER_HALF_MS  = 40;   // medio ciclo -> 12.5 Hz

const int8_t FWD = 1, REV = -1, STOP = 0;

// --- Estado --------------------------------------------------------------
Adafruit_MPU6050 mpu;
float gyroBiasX = 0;
volatile bool abortar = false;

struct Lectura {
  float thetaRad;   // angulo del vector gravedad en el plano Y-Z
  float giroMax;    // |giroX| maximo durante la ventana (control de que asento)
};

struct Punto {
  uint8_t duty;
  float   s[N_REPS];        // (sin(dTheta_fwd) - sin(dTheta_rev)) / 2
  float   giroMax[N_REPS];
  float   reposo[N_REPS];   // grados: si deriva entre reps, algo patino
  uint8_t n;
};

Punto datos[N_DUTIES];

// --- Control de motores --------------------------------------------------
void setMotor(uint8_t enPin, uint8_t inA, uint8_t inB, int8_t dir, uint8_t duty) {
  duty = constrain(duty, 0, MAX_PWM_DUTY);
  if (dir > 0)      { digitalWrite(inA, HIGH); digitalWrite(inB, LOW);  }
  else if (dir < 0) { digitalWrite(inA, LOW);  digitalWrite(inB, HIGH); }
  else              { digitalWrite(inA, LOW);  digitalWrite(inB, LOW); duty = 0; }
  ledcWrite(enPin, duty);
}

// Los dos motores juntos: es la condicion real de operacion y duplica el
// torque, que con la gravedad restituyendo es lo que da angulo medible.
void ambos(int8_t dir, uint8_t duty) {
  setMotor(ENA, IN1, IN2, dir, duty);
  setMotor(ENB, IN3, IN4, dir, duty);
}
void stopAll() { ambos(STOP, 0); }

// --- Sensor --------------------------------------------------------------
/*
  Promedia las COMPONENTES del vector gravedad y recien despues calcula el
  angulo. Promediar angulos daria mal cerca de +-180 grados, que es
  exactamente donde cae el robot colgado boca abajo.
*/
Lectura leerPromedio(uint16_t ms) {
  double sy = 0, sz = 0;
  float  gmax = 0;
  uint32_t n = 0;
  uint32_t t0 = millis();
  while (millis() - t0 < ms) {
    sensors_event_t a, g, t;
    mpu.getEvent(&a, &g, &t);
    sy += a.acceleration.y;
    sz += a.acceleration.z;
    float gx = fabsf(g.gyro.x - gyroBiasX);
    if (gx > gmax) gmax = gx;
    n++;
    delay(2);
  }
  Lectura r;
  r.thetaRad = n ? atan2f((float)(sy / n), (float)(sz / n)) : 0;
  r.giroMax  = gmax;
  return r;
}

void calibrarGiro(uint16_t muestras = 400) {
  Serial.println(F("Calibrando giroscopio (mantener quieto)..."));
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

// Diferencia angular envuelta a (-pi, pi]
float difAng(float a, float b) {
  float d = a - b;
  while (d >  PI) d -= 2.0f * PI;
  while (d < -PI) d += 2.0f * PI;
  return d;
}

// Espera cortada por 'x', para no quedar 10 s sin poder abortar
void esperar(uint32_t ms) {
  uint32_t t0 = millis();
  while (millis() - t0 < ms && !abortar) {
    if (Serial.available() && Serial.read() == 'x') { abortar = true; stopAll(); }
    delay(20);
  }
}

// --- Medicion ------------------------------------------------------------
/*
  Un escalon: parte del reposo, aplica el duty, y promedia el angulo en los
  ULTIMOS `VENTANA_MS` del pulso -- para entonces el transitorio ya paso y
  lo que queda es el equilibrio.

  Devuelve la desviacion respecto del reposo, en radianes y con signo.
*/
/*
  Muestrea el acelerometro MIENTRAS mantiene el dither. El dither tiene que
  seguir corriendo durante la ventana de medicion, no solo antes: si se apaga,
  la friccion estatica vuelve a agarrar y el cuerpo se queda donde quedo.
*/
Lectura leerConDither(uint16_t ms, int8_t dir, uint8_t duty, uint8_t amp) {
  double sy = 0, sz = 0;
  float  gmax = 0;
  uint32_t n = 0;
  uint32_t t0 = millis(), tCambio = 0;
  bool alto = false;

  while (millis() - t0 < ms && !abortar) {
    if (millis() - tCambio >= DITHER_HALF_MS) {
      tCambio = millis();
      alto = !alto;
      int d = alto ? (int)duty + amp : (int)duty - amp;
      ambos(dir, (uint8_t)constrain(d, 0, (int)MAX_PWM_DUTY));
    }
    sensors_event_t a, g, t;
    mpu.getEvent(&a, &g, &t);
    sy += a.acceleration.y;
    sz += a.acceleration.z;
    float gx = fabsf(g.gyro.x - gyroBiasX);
    if (gx > gmax) gmax = gx;
    n++;
    if (Serial.available() && Serial.read() == 'x') abortar = true;
    delay(2);
  }
  Lectura r;
  r.thetaRad = n ? atan2f((float)(sy / n), (float)(sz / n)) : 0;
  r.giroMax  = gmax;
  return r;
}

/*
  Un escalon: parte del reposo, aplica el duty CON DITHER, y promedia el angulo
  en los ultimos VENTANA_MS -- para entonces el transitorio ya paso y lo que
  queda es el equilibrio.
*/
float medirDesviacion(uint8_t duty, int8_t dir, Lectura& reposo, float& giroMax) {
  stopAll();
  esperar(ASENTAR_MS);
  if (abortar) { giroMax = 0; return 0; }

  reposo = leerPromedio(REPOSO_MS);

  leerConDither(PULSO_MS > VENTANA_MS ? PULSO_MS - VENTANA_MS : 0,
                dir, duty, DITHER_AMP);              // asentar, se descarta
  Lectura bajo = leerConDither(VENTANA_MS, dir, duty, DITHER_AMP);
  stopAll();

  giroMax = bajo.giroMax;
  return difAng(bajo.thetaRad, reposo.thetaRad);
}

// s simetrico: restar las dos direcciones cancela cualquier offset constante
float simetrico(float dFwd, float dRev) {
  return 0.5f * (sinf(dFwd) - sinf(dRev));
}

void correrBarrido() {
  Serial.println(F("\n=== Barrido de desviacion estatica (con dither) ==="));
  Serial.println(F("duty rep  th_fwd   th_rev     s       giroMax  reposo"));
  Serial.println(F("          (deg)    (deg)              (rad/s)  (deg)"));

  for (uint8_t d = 0; d < N_DUTIES && !abortar; d++) {
    datos[d].duty = DUTIES[d];
    datos[d].n = 0;

    for (uint8_t r = 0; r < N_REPS && !abortar; r++) {
      Lectura rep;
      float gm = 0, gmPeor = 0;

      float dF = medirDesviacion(DUTIES[d], FWD, rep, gm);
      float reposo0 = rep.thetaRad;
      gmPeor = max(gmPeor, gm);
      if (abortar) break;
      esperar(DESCANSO_MS); if (abortar) break;

      float dR = medirDesviacion(DUTIES[d], REV, rep, gm);
      gmPeor = max(gmPeor, gm);
      if (abortar) break;
      esperar(DESCANSO_MS);

      datos[d].s[r]       = simetrico(dF, dR);
      datos[d].giroMax[r] = gmPeor;
      datos[d].reposo[r]  = reposo0 * 180.0f / PI;
      datos[d].n++;

      Serial.print(F("  ")); Serial.print(DUTIES[d]);
      Serial.print(F("   ")); Serial.print(r + 1);
      Serial.print(F("    ")); Serial.print(dF * 180.0f / PI, 2);
      Serial.print(F("    ")); Serial.print(dR * 180.0f / PI, 2);
      Serial.print(F("    ")); Serial.print(datos[d].s[r], 5);
      Serial.print(F("   ")); Serial.print(gmPeor, 3);
      Serial.print(F("   ")); Serial.println(datos[d].reposo[r], 2);
    }
  }
  stopAll();
}

/*
  Se devuelve en VALOR ABSOLUTO. El signo de `s` solo dice hacia que lado
  inclina FWD -- es una convencion de montaje, no fisica. La corrida del
  2026-08-15 salio con s negativo y el criterio de pasa/no pasa, que comparaba
  `ku >= kuMin` sin abs(), dijo "NO PASA" con un |K_U| casi el doble del umbral.
*/
float promedioS(const Punto& p) {
  if (p.n == 0) return 0;
  float s = 0;
  for (uint8_t i = 0; i < p.n; i++) s += fabsf(p.s[i]);
  return s / p.n;
}

// dispersion entre repeticiones: si es grande, la friccion sigue mandando
float sdS(const Punto& p) {
  if (p.n < 2) return 0;
  float m = promedioS(p), acc = 0;
  for (uint8_t i = 0; i < p.n; i++) {
    float e = fabsf(p.s[i]) - m;
    acc += e * e;
  }
  return sqrtf(acc / (p.n - 1));
}

float maxGiro(const Punto& p) {
  float g = 0;
  for (uint8_t i = 0; i < p.n; i++) if (p.giroMax[i] > g) g = p.giroMax[i];
  return g;
}

/*
  Deriva del angulo de reposo dentro de un duty: sintoma de que algo patino.

  ⚠️ Se compara SIEMPRE contra la primera repeticion y envolviendo a (-180,180].
  Un max-min sobre los grados crudos falla justo en este montaje: el robot
  cuelga boca abajo, con el reposo medido en ~177 grados, o sea pegado al borde
  del envoltorio. Si entre repeticiones cruza de +179 a -179, el max-min
  reporta 358 grados de deriva cuando en realidad fueron 2.
*/
float derivaReposo(const Punto& p) {
  if (p.n < 2) return 0;
  float peor = 0;
  for (uint8_t i = 1; i < p.n; i++) {
    float d = fabsf(difAng(p.reposo[i] * PI / 180.0f,
                           p.reposo[0] * PI / 180.0f)) * 180.0f / PI;
    if (d > peor) peor = d;
  }
  return peor;
}

void informe() {
  Serial.println(F("\n\n================ RESULTADO ================"));
  Serial.print(F("w0 usado: ")); Serial.print(OMEGA0, 3);
  Serial.print(F(" rad/s   (w0^2 = ")); Serial.print(OMEGA0 * OMEGA0, 4);
  Serial.println(F(")"));

  Serial.println(F("\nduty   theta_eq   sd     K_U(cociente)  giroMax  deriva"));
  Serial.println(F("        (deg)     (deg)                   (rad/s)   (deg)"));

  double su = 0, ss = 0, suu = 0, sus = 0;
  uint8_t nFit = 0;

  for (uint8_t d = 0; d < N_DUTIES; d++) {
    if (datos[d].n == 0) continue;
    float s  = promedioS(datos[d]);
    float th = asinf(constrain(s, 0.0f, 1.0f)) * 180.0f / PI;
    float sd = asinf(constrain(sdS(datos[d]), 0.0f, 1.0f)) * 180.0f / PI;
    float kuCoc = (DUTIES[d] > 0) ? (OMEGA0 * OMEGA0 * s / DUTIES[d]) : 0;

    Serial.print(F("  ")); Serial.print(DUTIES[d]);
    Serial.print(F("    ")); Serial.print(th, 3);
    Serial.print(F("    ")); Serial.print(sd, 3);
    Serial.print(F("     ")); Serial.print(kuCoc, 5);
    Serial.print(F("      ")); Serial.print(maxGiro(datos[d]), 3);
    Serial.print(F("    ")); Serial.print(derivaReposo(datos[d]), 2);
    if (DUTIES[d] == DUTY_NULO) Serial.print(F("   <- control nulo"));
    Serial.println();

    if (DUTIES[d] != DUTY_NULO) {
      su += DUTIES[d]; ss += s;
      suu += (double)DUTIES[d] * DUTIES[d];
      sus += (double)DUTIES[d] * s;
      nFit++;
    }
  }

  Serial.println(F("\n--- Ajuste lineal  s = A*u + B  (excluye control nulo) ---"));
  if (nFit < 2) { Serial.println(F("Datos insuficientes.")); return; }

  double den = nFit * suu - su * su;
  if (den == 0) { Serial.println(F("Ajuste degenerado.")); return; }
  double A = (nFit * sus - su * ss) / den;
  double B = (ss - A * su) / nFit;

  float ku = fabsf((float)(OMEGA0 * OMEGA0 * A));

  Serial.print(F("A = ")); Serial.print(A, 7); Serial.println(F(" (1/duty)"));
  Serial.print(F("B = ")); Serial.println(B, 6);

  Serial.print(F("\n>>> K_U = w0^2 * |A| = ")); Serial.print(ku, 5);
  Serial.println(F("  <<<"));

  if (A != 0) {
    double u0 = -B / A;
    Serial.print(F("Umbral de friccion (ordenada al origen): duty ~ "));
    Serial.println(u0, 1);
    Serial.println(F("  = el comando por debajo del cual el cuerpo no se mueve."));
  }

  /*
    El barrido llega a duty 145, no a 160, para que el dither entre bajo el
    tope. La desviacion a duty maximo se EXTRAPOLA con el ajuste -- que es
    legitimo justamente porque el dither devuelve una relacion lineal.
  */
  float thExtrap = fabsf((float)(A * MAX_PWM_DUTY + B));
  Serial.print(F("\nExtrapolado a duty ")); Serial.print(MAX_PWM_DUTY);
  Serial.print(F(": desviacion = "));
  Serial.print(asinf(constrain(thExtrap, 0.0f, 1.0f)) * 180.0f / PI, 2);
  Serial.println(F(" deg"));
  Serial.println(F("  Ese angulo ES el maximo del que puede recuperarse parado."));

  /*
    Criterio de pasa/no pasa.

    ⚠️ NO se usa K_U * u_max / w0^2. Esa formula asume que TODO el duty produce
    torque, y en este sistema no es cierto: la friccion estatica se come el
    duty hasta la ordenada al origen del ajuste (medido: ~118 de 160). K_U es
    la PENDIENTE -- cuanto angulo por punto de duty POR ENCIMA de ese umbral.

    Ignorar la zona muerta hacia que la corrida del 2026-08-15 informara 7.28
    grados y "PASA" cuando el ajuste sobre los mismos datos daba 4.0 grados.

    El veredicto sale del angulo EXTRAPOLADO, que ya lleva la ordenada al
    origen adentro (A*u_max + B) y es directamente la cantidad fisica: de
    cuantos grados puede recuperarse el robot con el duty maximo disponible.
  */
  const float THETA_MIN_DEG = 5.0f;
  float thExtrapDeg = asinf(constrain(thExtrap, 0.0f, 1.0f)) * 180.0f / PI;

  Serial.print(F("\nCriterio: recuperarse de "));
  Serial.print(THETA_MIN_DEG, 1);
  Serial.print(F(" deg con duty ")); Serial.println(MAX_PWM_DUTY);
  Serial.print(F("Angulo maximo recuperable (extrapolado): "));
  Serial.print(thExtrapDeg, 2); Serial.println(F(" deg"));
  Serial.print(F("Margen: ")); Serial.print(thExtrapDeg / THETA_MIN_DEG, 2);
  Serial.println(F("x del criterio"));
  Serial.println(thExtrapDeg >= THETA_MIN_DEG ? F(">>> PASA") : F(">>> NO PASA"));
  Serial.println(F("(K_U de la pendiente sirve para el modelo de PSO; el"));
  Serial.println(F(" veredicto sale del angulo, que ya descuenta la friccion.)"));

  Serial.println(F("\n--- Validez, en orden de importancia ---"));
  Serial.println(F("1. DERIVA del reposo. Si crece monotonamente con el duty,"));
  Serial.println(F("   algo esta patinando y la señal en los duties altos es"));
  Serial.println(F("   deslizamiento, no inclinacion. Contrastar con las marcas"));
  Serial.println(F("   testigo. Es lo que invalido la corrida del 2026-08-15."));
  Serial.println(F("2. LINEALIDAD. th_medio tiene que crecer de forma pareja con"));
  Serial.println(F("   el duty. Si esta clavado en cero y de golpe salta, domina"));
  Serial.println(F("   la friccion estatica y la pendiente no significa nada:"));
  Serial.println(F("   usar el dato directo al duty maximo, no el ajuste."));
  Serial.println(F("3. DISPERSION (sd) entre repeticiones. Si es del orden de"));
  Serial.println(F("   theta_eq, el dither no alcanzo a romper la friccion y el"));
  Serial.println(F("   numero vale poco. Subir DITHER_AMP y repetir."));
  Serial.println(F("4. giroMax alto (>0.05) = no habia asentado: subir PULSO_MS."));
  Serial.println(F("5. El control nulo debe dar th ~ 0: es el piso de ruido."));
  Serial.println(F("6. La flexion del bracket infla K_U. Todo esto es COTA"));
  Serial.println(F("   SUPERIOR, no un valor de precision."));
  Serial.println(F("==========================================="));
}

/*
  ------------------------------------------------------------------------
  VERIFICACION RAPIDA ('v') -- 20 s en vez de 17 min.

  Contesta la unica pregunta que decide si este montaje sirve:

      ¿el cuerpo se queda desviado mientras el motor esta encendido?

  Si se queda, el eje esta anclado al mundo y el torque de reaccion empuja al
  cuerpo: la medicion tiene sentido. Si pega un tiron y vuelve con el motor
  todavia encendido, el eje gira libre y no hay nada que medir.

  Es preferible al test de empujar con la mano, que no distingue entre "gira
  la reductora" (correcto) y "gira el eje en el bracket" (inservible): las dos
  cosas se sienten parecido. Aca no hay que interpretar nada -- o el angulo se
  sostiene, o no.
*/
void faseVerif(const char* etiqueta, int8_t dir, uint8_t duty,
               uint16_t ms, float ref) {
  ambos(dir, duty);
  uint32_t t0 = millis();
  while (millis() - t0 < ms && !abortar) {
    Lectura l = leerPromedio(250);
    Serial.print(F("  ")); Serial.print(etiqueta);
    Serial.print(F("  t=")); Serial.print((millis() - t0) / 1000.0f, 1);
    Serial.print(F("s   desviacion = "));
    Serial.print(difAng(l.thetaRad, ref) * 180.0f / PI, 2);
    Serial.println(F(" deg"));
    if (Serial.available() && Serial.read() == 'x') abortar = true;
  }
  if (duty == 0) stopAll();
}

void verificacionRapida() {
  Serial.println(F("\n\n=== VERIFICACION RAPIDA ==="));
  Serial.println(F("MIRA EL ROBOT. La pregunta es una sola:"));
  Serial.println(F("  ¿el cuerpo se QUEDA desviado mientras el motor esta ON?"));
  Serial.println(F("Referencia = angulo de reposo. 'x' aborta.\n"));

  stopAll();
  esperar(2500);
  if (abortar) { abortar = false; return; }
  float ref = leerPromedio(800).thetaRad;

  faseVerif("OFF      ", STOP, 0,             1500, ref);
  faseVerif("FWD @160 ", FWD,  MAX_PWM_DUTY,  4000, ref);
  stopAll();
  faseVerif("OFF      ", STOP, 0,             3000, ref);
  faseVerif("REV @160 ", REV,  MAX_PWM_DUTY,  4000, ref);
  stopAll();
  faseVerif("OFF      ", STOP, 0,             3000, ref);
  stopAll();

  Serial.println(F("\n--- Como leerlo ---"));
  Serial.println(F("SIRVE:    con el motor ON la desviacion sube y se MANTIENE"));
  Serial.println(F("          (unos grados, estable), y vuelve a ~0 al apagar."));
  Serial.println(F("          FWD y REV con signos opuestos y tamaño parecido."));
  Serial.println(F("NO SIRVE: pico y vuelta a ~0 con el motor todavia ON, o"));
  Serial.println(F("          desviacion que no supera el ruido (~0.05 deg)."));
  Serial.println(F("NO SIRVE: no vuelve a ~0 al apagar -> patino, revisar marcas."));
  Serial.println(F("\nSi SIRVE, correr el barrido completo con 's'."));
  Serial.println(F("==========================="));
  abortar = false;
}

const char* motivoReset() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:   return "POWERON - arranque normal por alimentacion";
    case ESP_RST_EXT:       return "EXT - pin de reset externo";
    case ESP_RST_SW:        return "SW - reinicio pedido por software";
    case ESP_RST_PANIC:     return "PANIC - excepcion del firmware (BUG EN EL CODIGO)";
    case ESP_RST_INT_WDT:   return "INT_WDT - watchdog de interrupciones";
    case ESP_RST_TASK_WDT:  return "TASK_WDT - watchdog de tarea";
    case ESP_RST_WDT:       return "WDT - otro watchdog";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:  return "BROWNOUT - SE CAYO LA TENSION DE ALIMENTACION";
    case ESP_RST_SDIO:      return "SDIO";
    default:                return "DESCONOCIDO";
  }
}

void setup() {
  Serial.begin(115200);
  /*
    1.5 s antes de imprimir nada. El monitor serie tarda en abrir el puerto
    tras el reset, y si el banner sale antes se pierde. Con un bucle de
    reinicio esa perdida es justo lo que oculta el diagnostico.
  */
  delay(1500);

  bootCount++;
  Serial.println(F("\n\n########################################"));
  Serial.print(F("ARRANQUE #")); Serial.println(bootCount);
  Serial.print(F("Motivo del reset: ")); Serial.println(motivoReset());
  if (bootCount > 1) {
    Serial.println(F("!!! Este NO es el primer arranque desde el ultimo corte"));
    Serial.println(F("!!! de alimentacion. Si este numero sigue creciendo solo,"));
    Serial.println(F("!!! hay un bucle de reinicio: NO medir hasta resolverlo."));
  }
  Serial.println(F("########################################"));

  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  ledcAttach(ENA, PWM_FREQ_HZ, PWM_RES_BITS);
  ledcAttach(ENB, PWM_FREQ_HZ, PWM_RES_BITS);
  stopAll();   // estado seguro antes que nada

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);
  if (!mpu.begin()) {
    Serial.println(F("ERROR: MPU6050 no responde."));
    Serial.println(F("Ojo: que conteste por I2C no prueba que este alimentado."));
    Serial.println(F("Verificar VCC con multimetro / LED del modulo, y correr"));
    Serial.println(F("test0_i2c_scanner. Ver CLAUDE.md 'Alimentacion parasita'."));
    while (1) delay(1000);
  }
  /*
    Rango y filtro distintos del Test 5, y a proposito: aca la medicion es
    ESTATICA. 2G da la mejor resolucion sobre el vector gravedad (que vale
    1G), y 10 Hz de ancho de banda rechaza la vibracion del motor en vez de
    dejarla pasar. El Test 5 usaba 8G/94 Hz porque medía un transitorio.
  */
  mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
  mpu.setGyroRange(MPU6050_RANGE_250_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_10_HZ);

  for (uint8_t d = 0; d < N_DUTIES; d++) datos[d].n = 0;

  // 2 pulsos por repeticion: FWD y REV
  uint32_t seg = (uint32_t)N_DUTIES * N_REPS * 2 *
                 (ASENTAR_MS + REPOSO_MS + PULSO_MS + DESCANSO_MS) / 1000;

  Serial.println(F("\n=== TEST 5b - K_U por desviacion estatica ==="));
  Serial.println(F("MONTAJE: eje TRABADO al bracket (bulon apretado)."));
  Serial.println(F("Antes de arrancar:"));
  Serial.println(F("  [ ] Marcas testigo en cupla<->eje y cupla<->chapa."));
  Serial.println(F("  [ ] Empujar el cuerpo: que se mueva es NORMAL (gira la"));
  Serial.println(F("      reductora). Lo que importa: marcas quietas, y que se"));
  Serial.println(F("      frene en menos de un ciclo. Si oscila varias veces,"));
  Serial.println(F("      esta pivotando sobre el bulon flojo -> no medir."));
  Serial.println(F("  [ ] Cuerda de seguridad floja, sin rozar."));
  Serial.println(F("  [ ] Bateria + USB (no USB solo)."));
  Serial.print(F("Duracion estimada: ~")); Serial.print(seg / 60);
  Serial.println(F(" min. Los motores trabajan cerca de stall: tocarlos cada tanto."));
  Serial.println(F("\n'v' = VERIFICACION RAPIDA (20 s) - correr esto PRIMERO"));
  Serial.println(F("'s' = barrido completo   'x' = abortar"));
}

void loop() {
  /*
    Latido cada 5 s con el uptime. Resuelve de un vistazo la pregunta que la
    primera corrida no pudo contestar: si el uptime CRECE, la placa esta viva
    esperando el comando y el problema es el envio de teclas; si vuelve a
    cero, se esta reseteando.
  */
  static uint32_t ultimoLatido = 0;
  if (millis() - ultimoLatido > 5000) {
    ultimoLatido = millis();
    Serial.print(F("[esperando 's']  uptime = "));
    Serial.print(millis() / 1000);
    Serial.print(F(" s   arranque #"));
    Serial.println(bootCount);
  }

  if (Serial.available()) {
    char c = Serial.read();
    if (c == 's') {
      abortar = false;
      calibrarGiro();
      correrBarrido();
      if (abortar) { Serial.println(F("\nABORTADO.")); }
      informe();
      Serial.println(F("\n's' para repetir."));
    } else if (c == 'v') {
      abortar = false;
      calibrarGiro(150);          // corta: aca solo se usa el acelerometro
      verificacionRapida();
      Serial.println(F("\n'v' repetir verificacion   's' barrido completo"));
    } else if (c == 'x') {
      abortar = true;
      stopAll();
    }
  }
  delay(10);
}
