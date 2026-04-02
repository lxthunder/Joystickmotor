// L298N Motorsteuerung mit KY-023 Joystick und Arduino Nano
// Roboterfahrzeug mit Differentialantrieb und Schlepprad
//
// Joystick (KY-023):
//   VCC -> 5V
//   GND -> GND
//   VRx -> A0  (X-Achse: Lenken)
//   VRy -> A1  (Y-Achse: Vorwärts/Rückwärts)
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

// Totzone um die Joystick-Mitte
const int DEADZONE = 50;

// Dämpfung: 0.0 = sofort, 1.0 = nie – Werte zwischen 0.05 und 0.3 empfohlen
const float ALPHA = 0.15;

// Kalibrierungswerte (werden in setup() gemessen)
int centerX = 512;
int centerY = 512;

// Geglättete Motorleistung
float smoothLeft  = 0;
float smoothRight = 0;

void setup() {
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);
  pinMode(JOY_SW, INPUT_PULLUP);

  Serial.begin(9600);

  // Joystick kalibrieren: Mittelwert aus 32 Messungen
  long sumX = 0, sumY = 0;
  for (int i = 0; i < 32; i++) {
    sumX += analogRead(JOY_X);
    sumY += analogRead(JOY_Y);
    delay(5);
  }
  centerX = sumX / 32;
  centerY = sumY / 32;

  Serial.print("Kalibrierung: centerX=");
  Serial.print(centerX);
  Serial.print("  centerY=");
  Serial.println(centerY);
}

void loop() {
  int rawX = analogRead(JOY_X);  // 0 - 1023
  int rawY = analogRead(JOY_Y);

  // Kalibrierungsmitte abziehen
  int x = rawX - centerX;  // Lenken
  int y = rawY - centerY;  // Fahren

  // Joystick-Taste: alle Motoren stoppen
  if (digitalRead(JOY_SW) == LOW) {
    motorStop(ENA, IN1, IN2);
    motorStop(ENB, IN3, IN4);
    // Serial.println("STOP (Taste)");  // deaktiviert für Serial Plotter
    delay(200);
    return;
  }

  // Geschwindigkeit und Lenkanteil berechnen
  int drive = (abs(y) > DEADZONE) ? map(abs(y), DEADZONE, 512, 0, 255) : 0;
  int steer = (abs(x) > DEADZONE) ? map(abs(x), DEADZONE, 512, 0, 255) : 0;

  // Vorzeichenbehaftete Geschwindigkeit: positiv = vorwärts, negativ = rückwärts
  int driveDir = (y > 0) ? 1 : -1;
  int leftPower  = constrain(drive * driveDir + steer, -255, 255);
  int rightPower = constrain(drive * driveDir - steer, -255, 255);

  // Tiefpassfilter: neuen Zielwert schrittweise annähern
  int targetLeft  = (abs(y) <= DEADZONE && abs(x) <= DEADZONE) ? 0 : leftPower;
  int targetRight = (abs(y) <= DEADZONE && abs(x) <= DEADZONE) ? 0 : rightPower;
  smoothLeft  += ALPHA * (targetLeft  - smoothLeft);
  smoothRight += ALPHA * (targetRight - smoothRight);

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
  if (abs(y) <= DEADZONE && abs(x) <= DEADZONE) richtung = "STOP";
  else if (abs(y) <= DEADZONE)                  richtung = (x > 0) ? "DREHEN RECHTS" : "DREHEN LINKS";
  else if (leftPower >= 0 && rightPower >= 0)   richtung = "VORWAERTS";
  else if (leftPower <= 0 && rightPower <= 0)   richtung = "RUECKWAERTS";
  else                                          richtung = "KURVE";

  Serial.print("X:"); Serial.print(x);
  Serial.print("  Y:"); Serial.print(y);
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
