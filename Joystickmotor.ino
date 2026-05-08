// L298N Motorsteuerung mit KY-023 Joystick und Arduino Nano
// Roboterfahrzeug mit Differentialantrieb und Schlepprad
//
// Joystick (KY-023):
//   VCC -> 5V
//   GND -> GND
//   VRx -> A1  (X-Achse: Lenken)
//   VRy -> A0  (Y-Achse: Vorwärts/Rückwärts)
//   SW  -> D2  (Joystick-Taste: Sofortstopp)
//
// L298N:
//   IN1 -> D5   Motor Links
//   IN2 -> D6   Motor Links
//   IN3 -> D9   Motor Rechts
//   IN4 -> D10  Motor Rechts
//   ENA -> D3   (PWM, Jumper entfernen)
//   ENB -> D11  (PWM, Jumper entfernen)
//   5V EN Jumper -> gesetzt lassen (versorgt Arduino Nano)
//
// Verdrahtung Motoren:
//   OUT1/OUT2 -> Motor Links  (normal)
//   OUT3/OUT4 -> Motor Rechts (+ und - tauschen wegen Einbaulage)
//
// Joystick-Kalibrierung wird per Joystick_Test (Hardwaretesting) ins
// EEPROM geschrieben (Magic 0xCA01) und hier nur gelesen.

#include <EEPROM.h>

// --- Pin-Definitionen ---
const int JOY_X  = A1;
const int JOY_Y  = A0;
const int JOY_SW = 2;

// Motor Links (Motor A)
const int ENA = 3;
const int IN1 = 5;
const int IN2 = 6;

// Motor Rechts (Motor B)
const int ENB = 11;
const int IN3 = 9;
const int IN4 = 10;

// Totzone um die Joystick-Mitte (in ADC-Counts)
const int JOY_DEADZONE = 50;

// Dämpfung: 0.0 = sofort, 1.0 = nie – Werte zwischen 0.05 und 0.3 empfohlen
const float ALPHA = 0.15;

// EEPROM-Kalibrierung (Layout muss zu Joystick_Test passen)
const int      EEPROM_ADDR  = 0;
const uint16_t EEPROM_MAGIC = 0xCA01;

struct Kalibrierung {
  uint16_t magic;
  int16_t  xCenter, xMin, xMax;
  int16_t  yCenter, yMin, yMax;
};

Kalibrierung kal;

// Geglättete Motorleistung
float smoothLeft  = 0;
float smoothRight = 0;

bool ladeKalibrierung() {
  EEPROM.get(EEPROM_ADDR, kal);
  return kal.magic == EEPROM_MAGIC;
}

void setzeKalibrierungDefault() {
  kal.xCenter = 512; kal.xMin = 0; kal.xMax = 1023;
  kal.yCenter = 512; kal.yMin = 0; kal.yMax = 1023;
}

// Kalibrierung + Deadzone, linear ueber den ganzen Bereich.
int applyLinearKal(int raw, int center, int minV, int maxV,
                   int outMin, int outMax) {
  int outMid = (outMin + outMax) / 2;
  int delta  = raw - center;
  if (delta > -JOY_DEADZONE && delta < JOY_DEADZONE) return outMid;

  if (delta > 0) {
    long out = (long)(delta - JOY_DEADZONE) * (outMax - outMid)
               / max(1, (maxV - center - JOY_DEADZONE));
    return constrain(outMid + (int)out, outMin, outMax);
  } else {
    long out = (long)(delta + JOY_DEADZONE) * (outMid - outMin)
               / max(1, (center - minV - JOY_DEADZONE));
    return constrain(outMid + (int)out, outMin, outMax);
  }
}

void setup() {
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);
  pinMode(JOY_SW, INPUT_PULLUP);

  Serial.begin(9600);

  if (ladeKalibrierung()) {
    Serial.print(F(">> EEPROM-Kalibrierung geladen: X-Mitte="));
    Serial.print(kal.xCenter);
    Serial.print(F(" ("));    Serial.print(kal.xMin);
    Serial.print(F(".."));    Serial.print(kal.xMax);
    Serial.print(F(") Y-Mitte=")); Serial.print(kal.yCenter);
    Serial.print(F(" ("));    Serial.print(kal.yMin);
    Serial.print(F(".."));    Serial.print(kal.yMax);
    Serial.println(F(")"));
  } else {
    Serial.println(F(">> Keine EEPROM-Kalibrierung -> Default 0..1023, Mitte 512"));
    Serial.println(F("   Hinweis: Joystick_Test (Hardwaretesting) zum Kalibrieren"));
    setzeKalibrierungDefault();
  }
}

void loop() {
  int rawX = analogRead(JOY_X);
  int rawY = analogRead(JOY_Y);

  // Joystick-Taste: alle Motoren stoppen
  if (digitalRead(JOY_SW) == LOW) {
    motorStop(ENA, IN1, IN2);
    motorStop(ENB, IN3, IN4);
    smoothLeft = smoothRight = 0;
    delay(200);
    return;
  }

  // Kalibriert + Deadzone, vorzeichenbehaftet -255..+255
  int drive = applyLinearKal(rawY, kal.yCenter, kal.yMin, kal.yMax, -255, 255);
  int steer = applyLinearKal(rawX, kal.xCenter, kal.xMin, kal.xMax, -255, 255);

  int leftPower  = constrain(drive + steer, -255, 255);
  int rightPower = constrain(drive - steer, -255, 255);

  // Tiefpassfilter: neuen Zielwert schrittweise annähern
  smoothLeft  += ALPHA * (leftPower  - smoothLeft);
  smoothRight += ALPHA * (rightPower - smoothRight);

  int outLeft  = (int)smoothLeft;
  int outRight = (int)smoothRight;

  // Motorsteuerung anhand des Vorzeichens
  if (outLeft > 0)       motorForward (ENA, IN1, IN2,  outLeft);
  else if (outLeft < 0)  motorBackward(ENA, IN1, IN2, -outLeft);
  else                   motorStop    (ENA, IN1, IN2);

  if (outRight > 0)      motorForward (ENB, IN3, IN4,  outRight);
  else if (outRight < 0) motorBackward(ENB, IN3, IN4, -outRight);
  else                   motorStop    (ENB, IN3, IN4);

  String richtung;
  if (drive == 0 && steer == 0)               richtung = "STOP";
  else if (drive == 0)                         richtung = (steer > 0) ? "DREHEN RECHTS" : "DREHEN LINKS";
  else if (leftPower >= 0 && rightPower >= 0)  richtung = "VORWAERTS";
  else if (leftPower <= 0 && rightPower <= 0)  richtung = "RUECKWAERTS";
  else                                         richtung = "KURVE";

  Serial.print("X:"); Serial.print(steer);
  Serial.print("  Y:"); Serial.print(drive);
  Serial.print("  "); Serial.println(richtung);

  delay(20);
}

// --- Hilfsfunktionen ---

void motorForward(int en, int in1, int in2, int speed) {
  analogWrite(en, speed);
  digitalWrite(in1, HIGH);
  digitalWrite(in2, LOW);
}

void motorBackward(int en, int in1, int in2, int speed) {
  analogWrite(en, speed);
  digitalWrite(in1, LOW);
  digitalWrite(in2, HIGH);
}

void motorStop(int en, int in1, int in2) {
  analogWrite(en, 0);
  digitalWrite(in1, LOW);
  digitalWrite(in2, LOW);
}
