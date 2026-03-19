#include <LiquidCrystal.h>

// PIN CONFIG
LiquidCrystal lcd(7, 8, 9, 10, 11, 12);
const int trig1 = 3; const int echo1 = 4;
const int trig2 = 5; const int echo2 = 6;
const int BUZZER_PIN = A0;
const int ALERT_LED  = A1;

// SPEED ALERT THRESHOLD
const float SPEED_LIMIT_KMH = 5.0; 

// STABILITY SETTINGS
const float DIST_M = 0.13;     // Fixed gap between sensors
const int TRIGGER_CM = 25;     // Detection threshold
const int MSG_DELAY = 1500;    // How long to show "Ready" screen

enum State { WAITING, TIMING };
State sysState = WAITING;
unsigned long t1 = 0;

float measureDist(int t, int e) {
  // Clear the trigger pin
  digitalWrite(t, LOW);
  delayMicroseconds(5);
  
  // Trigger the sensor
  digitalWrite(t, HIGH);
  delayMicroseconds(10);
  digitalWrite(t, LOW);
  
  // Measure the echo pulse
  // timeout set to 30ms (longer than the ~23ms max of the sensor)
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
  
  // -- SENSOR HEALTH CHECK --
  delay(1000);
  lcd.clear(); 
  lcd.print("S1 check: ");
  float s1 = measureDist(trig1, echo1);
  if (s1 < 500) { lcd.print("OK"); Serial.println("CHECK:S1_OK"); }
  else { lcd.print("FAIL!"); Serial.println("CHECK:S1_FAIL"); }
  
  lcd.setCursor(0, 1);
  lcd.print("S2 check: ");
  float s2 = measureDist(trig2, echo2);
  if (s2 < 500) { lcd.print("OK"); Serial.println("CHECK:S2_OK"); }
  else { lcd.print("FAIL!"); Serial.println("CHECK:S2_FAIL"); }
  
  delay(2000);
  lcd.clear(); lcd.print("Ready [STABLE]");
  Serial.println("STATE:WAITING");
}

void loop() {
  if (sysState == WAITING) {
    float d1 = measureDist(trig1, echo1);
    
    // Output for live web dash
    Serial.print("DEBUG_DIST_1:");
    Serial.println(d1);
    
    // Valid trigger: must be within range and NOT noise
    if (d1 > 2.0 && d1 < TRIGGER_CM) {
        // Double-confirmation for extreme stability
        delay(15);
        if (measureDist(trig1, echo1) < TRIGGER_CM) {
          t1 = micros();
          sysState = TIMING;
          Serial.println("STATE:P1_CROSSED");
          lcd.clear(); lcd.print("TIMING...");
        }
    }
    delay(100); // Prevents rapid re-triggering
  } 
  else if (sysState == TIMING) {
    float d2 = measureDist(trig2, echo2);
    
    Serial.print("DEBUG_DIST_2:");
    Serial.println(d2);

    if (d2 > 2.0 && d2 < TRIGGER_CM) {
      unsigned long t2 = micros();
      
      // Calculate speed
      float dt = (float)(t2 - t1) / 1000000.0;
      if (dt > 0.001) { // Prevents div by zero
        float speedMS = DIST_M / dt;
        float speedKMH = speedMS * 3.6;

        Serial.print("SPEED_MS:"); Serial.println(speedMS, 3);
        Serial.print("SPEED_KMH:"); Serial.println(speedKMH, 2);
        Serial.println("STATE:P2_CROSSED");

        lcd.clear();
        lcd.print(speedMS, 2); lcd.print(" m/s");
        lcd.setCursor(0, 1);
        lcd.print(speedKMH, 1); lcd.print(" km/h");

        if (speedKMH > SPEED_LIMIT_KMH) {
          Serial.println("ALERT:SPEED_EXCEEDED");
          lcd.setCursor(8, 1);
          lcd.print("!! FAST");
          for (int i = 0; i < 5; i++) {
            digitalWrite(ALERT_LED, HIGH);
            tone(BUZZER_PIN, 1200, 80);
            delay(100);
            digitalWrite(ALERT_LED, LOW);
            delay(100);
          }
        }
        
        delay(3000); // Show results
      }
      
      resetToWaiting();
    }

    // Safety Timeout: 4 seconds
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
