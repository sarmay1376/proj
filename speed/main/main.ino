#include <LiquidCrystal.h>

// PIN CONFIG
LiquidCrystal lcd(7, 8, 9, 10, 11, 12);
const int trig1 = 3; const int echo1 = 4;
const int trig2 = 5; const int echo2 = 6;
const int BUZZER_PIN = A0;
const int ALERT_LED  = A1;

// SPEED ALERT THRESHOLD
const float SPEED_LIMIT_KMH = 5.0; 

// SETTINGS
const float DIST_M = 0.13;
const int TRIGGER_CM = 25;

enum State { WAITING, TIMING };
State sysState = WAITING;
unsigned long t1 = 0;

float measureDist(int t, int e) {
  digitalWrite(t, LOW);
  delayMicroseconds(5);
  digitalWrite(t, HIGH);
  delayMicroseconds(10);
  digitalWrite(t, LOW);

  long dur = pulseIn(e, HIGH, 30000);
  if (dur == 0 || dur > 35000) return 999.0;

  return (dur * 0.034) / 2.0;
}

void setup() {
  Serial.begin(9600);
  lcd.begin(16, 2);
  lcd.print("SYSTEM BOOTING");

  pinMode(trig1, OUTPUT); pinMode(echo1, INPUT);
  pinMode(trig2, OUTPUT); pinMode(echo2, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(ALERT_LED,  OUTPUT);

  delay(2000);

  lcd.clear();
  lcd.print("Ready [STABLE]");
  Serial.println("STATE:WAITING");
}

void loop() {

  if (sysState == WAITING) {

    float d1 = measureDist(trig1, echo1);

    if (d1 > 2.0 && d1 < TRIGGER_CM) {
      delay(15);

      if (measureDist(trig1, echo1) < TRIGGER_CM) {
        t1 = micros();
        sysState = TIMING;

        Serial.println("STATE:P1_CROSSED");

        lcd.clear();
        lcd.print("TIMING...");
      }
    }

    delay(100);
  } 

  else if (sysState == TIMING) {

    float d2 = measureDist(trig2, echo2);

    if (d2 > 2.0 && d2 < TRIGGER_CM) {

      unsigned long t2 = micros();

      float dt = (float)(t2 - t1) / 1000000.0;

      if (dt > 0.001) {

        float speedMS  = DIST_M / dt;
        float speedKMH = speedMS * 3.6;

        // -------- SEND TO PYTHON --------
        Serial.print("SPEED_MS:");
        Serial.println(speedMS, 3);

        Serial.print("SPEED_KMH:");
        Serial.println(speedKMH, 2);

        Serial.println("STATE:P2_CROSSED");

        // -------- LCD DISPLAY --------
        lcd.clear();
        lcd.print(speedMS, 2);
        lcd.print(" m/s");

        lcd.setCursor(0, 1);
        lcd.print(speedKMH, 1);
        lcd.print(" km/h");

        // -------- ALERT --------
        if (speedKMH > SPEED_LIMIT_KMH) {

          Serial.println("ALERT:SPEED_EXCEEDED");

          lcd.setCursor(8, 1);
          lcd.print("FAST");

          for (int i = 0; i < 5; i++) {
            digitalWrite(ALERT_LED, HIGH);
            tone(BUZZER_PIN, 1200, 80);
            delay(100);
            digitalWrite(ALERT_LED, LOW);
            delay(100);
          }
        }

        delay(3000);
      }

      resetToWaiting();
    }

    // TIMEOUT
    if (micros() - t1 > 4000000) {
      resetToWaiting();
    }

    delay(10);
  }
}

void resetToWaiting() {
  sysState = WAITING;

  Serial.println("STATE:WAITING");

  lcd.clear();
  lcd.print("Ready [STABLE]");
}