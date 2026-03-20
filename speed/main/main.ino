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

unsigned long tS1 = 0;
unsigned long tS2 = 0;

float measureDist(int t, int e) {
  digitalWrite(t, LOW);
  delayMicroseconds(2);
  digitalWrite(t, HIGH);
  delayMicroseconds(10);
  digitalWrite(t, LOW);
  long dur = pulseIn(e, HIGH, 6000); 
  if (dur == 0) return 999.0;
  return (dur * 0.034) / 2.0;
}

void setup() {
  Serial.begin(9600);
  lcd.begin(16, 2);
  lcd.print("SPEED TRAP V11");
  lcd.setCursor(0, 1);
  lcd.print("STRICT-FILTER");

  pinMode(trig1, OUTPUT); pinMode(echo1, INPUT);
  pinMode(trig2, OUTPUT); pinMode(echo2, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(ALERT_LED,  OUTPUT);

  delay(2000);
  lcd.clear();
  lcd.print("Ready [A] <-> [B]");
}

void loop() {
  float d1 = measureDist(trig1, echo1);
  float d2 = measureDist(trig2, echo2);
  unsigned long now = micros();

  // NOISE FILTER: Track consecutive hits
  static int hits1 = 0;
  static int hits2 = 0;

  // 1. MONITOR SENSOR A (Requires 3 consecutive samples)
  if (d1 < TRIGGER_CM && d1 > 1.0) hits1++; else hits1 = 0;
  
  if (hits1 >= 3 && tS1 == 0) {
    tS1 = now;
    Serial.println("STATE:P1_CROSSED");
    lcd.setCursor(15, 0); lcd.print("1");
  }

  // MONITOR SENSOR B (Requires 3 consecutive samples)
  if (d2 < TRIGGER_CM && d2 > 1.0) hits2++; else hits2 = 0;

  if (hits2 >= 3 && tS2 == 0) {
    tS2 = now;
    if (tS1 > 0) Serial.println("STATE:P2_CROSSED");
    lcd.setCursor(15, 1); lcd.print("2");
  }

  // 2. CALC SPEED (Differential)
  if (tS1 > 0 && tS2 > 0) {
    unsigned long dt_us = (tS1 > tS2) ? (tS1 - tS2) : (tS2 - tS1);
    float dt_s = (float)dt_us / 1000000.0;

    // VALIDATION: Min 20ms, Max 2 seconds
    if (dt_s >= 0.02 && dt_s < 2.0) { 
       float speedMS  = DIST_M / dt_s;
       float speedKMH = speedMS * 3.6;

       Serial.print("SPEED:KMH:"); Serial.print(speedKMH, 2);
       Serial.print(":MS:"); Serial.println(speedMS, 3);

       lcd.clear();
       lcd.print(speedMS, 1); lcd.print("m/s ");
       lcd.print(tS1 < tS2 ? ">>" : "<<");
       lcd.setCursor(0, 1);
       lcd.print(speedKMH, 1); lcd.print("km/h");

       if (speedKMH > SPEED_LIMIT_KMH) {
         Serial.println("ALERT:OVERSPEED");
         lcd.setCursor(11, 1); lcd.print("!FAST");
         digitalWrite(ALERT_LED, HIGH);
         tone(BUZZER_PIN, 1000, 300);
         delay(1000);
         digitalWrite(ALERT_LED, LOW);
       }
       delay(4000); // Hold the data longer for better visibility
    }
    tS1 = 0; tS2 = 0; hits1 = 0; hits2 = 0; // RESET ALL
    Serial.println("STATE:WAITING");
    lcd.clear(); lcd.print("Ready [A] <-> [B]");
  }

  // 3. TIMEOUT
  if (tS1 > 0 && (micros() - tS1 > 2000000)) { tS1 = 0; hits1 = 0; Serial.println("STATE:TIMEOUT"); lcd.setCursor(15, 0); lcd.print(" "); }
  if (tS2 > 0 && (micros() - tS2 > 2000000)) { tS2 = 0; hits2 = 0; Serial.println("STATE:TIMEOUT"); lcd.setCursor(15, 1); lcd.print(" "); }

  // 4. LIVE CALIBRATION
  static unsigned long lastDebug = 0;
  if (millis() - lastDebug > 1000) {
    String debugMsg = "DEBUG_DIST:S1:" + String(d1, 1) + "_S2:" + String(d2, 1);
    Serial.println(debugMsg);
    lastDebug = millis();
    lcd.setCursor(15, 1); lcd.print(millis() % 2000 < 1000 ? "." : " ");
  }
}