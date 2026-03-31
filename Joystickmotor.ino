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
const int JOY_X  = A0;
const int JOY_Y  = A1;
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

// Kalibrierungswerte (werden in setup() gemessen)
int centerX = 512;
int centerY = 512;

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
    Serial.println("STOP (Taste)");
    delay(200);
    return;
  }

  // Geschwindigkeit und Lenkanteil berechnen
  int drive = (abs(y) > DEADZONE) ? map(abs(y), DEADZONE, 512, 0, 255) : 0;
  int steer = (abs(x) > DEADZONE) ? map(abs(x), DEADZONE, 512, 0, 255) : 0;

  int leftSpeed  = constrain(drive + steer, 0, 255);
  int rightSpeed = constrain(drive - steer, 0, 255);

  if (abs(y) > DEADZONE) {
    // Vorwärts oder Rückwärts mit Lenken
    if (y > 0) {
      motorForward(ENA,  IN1, IN2, leftSpeed);
      motorForward(ENB,  IN3, IN4, rightSpeed);
      Serial.print("VORWÄRTS");
    } else {
      motorBackward(ENA, IN1, IN2, leftSpeed);
      motorBackward(ENB, IN3, IN4, rightSpeed);
      Serial.print("RÜCKWÄRTS");
    }
  } else if (abs(x) > DEADZONE) {
    // Auf der Stelle drehen (nur X-Achse)
    if (x > 0) {
      motorForward(ENA,  IN1, IN2, steer);
      motorBackward(ENB, IN3, IN4, steer);
      Serial.print("DREHEN RECHTS");
    } else {
      motorBackward(ENA, IN1, IN2, steer);
      motorForward(ENB,  IN3, IN4, steer);
      Serial.print("DREHEN LINKS");
    }
  } else {
    // Totzone - Stopp
    motorStop(ENA, IN1, IN2);
    motorStop(ENB, IN3, IN4);
    Serial.print("STOP");
  }

  Serial.print("  X="); Serial.print(x);
  Serial.print("  Y="); Serial.println(y);

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
