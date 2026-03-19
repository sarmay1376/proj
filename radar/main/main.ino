#include <Servo.h>
#include <LiquidCrystal.h>

const int trigPin = 10;
const int echoPin = 11;
const int BUZZER_PIN = 8;
const int ALERT_LED  = A5;
const int MAX_RANGE  = 400; // cm — max reliable HC-SR04 range

long duration;
int distance;

Servo myServo;

// LCD: All on a clean consecutive block: RS=2, E=3, D4=4, D5=5, D6=6, D7=7
LiquidCrystal lcd(2, 3, 4, 5, 6, 7);

void setup() {
  pinMode(trigPin, OUTPUT); 
  pinMode(echoPin, INPUT); 
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(ALERT_LED, OUTPUT);
  Serial.begin(9600);
  myServo.attach(9);

  lcd.begin(16, 2);
  lcd.print("RADAR v4.0");
  lcd.setCursor(0, 1);
  lcd.print("PINS: 2-7 MODE"); // Diagnostic hint for user
  delay(2500);
  lcd.clear();
}

void loop() {
  // Stability Sweep: 1-degree steps, 40ms settling delay
  for(int i=15; i<=165; i++){  
    myServo.write(i);
    delay(40); // Increased delay for LCD and Servo stability
    
    distance = calculateDistance();
    alertCheck(distance);
    
    // THREAD-SAFE LCD: Only update every 10 degrees to prevent corruption
    if (i % 10 == 0) updateLCD(i, distance);
    
    Serial.print(i);
    Serial.print(",");
    Serial.println(distance);
  }
  
  for(int i=165; i>15; i--){  
    myServo.write(i);
    delay(40);
    
    distance = calculateDistance();
    alertCheck(distance);
    
    if (i % 10 == 0) updateLCD(i, distance);
    
    Serial.print(i);
    Serial.print(",");
    Serial.println(distance);
  }
}

void updateLCD(int angle, int dist) {
  lcd.setCursor(0, 0);
  lcd.print("ANGL: ");
  lcd.print(angle);
  lcd.print((char)223);
  lcd.print("   ");     

  lcd.setCursor(0, 1);
  if (dist > 0 && dist < 400) {
    lcd.print("DIST: ");
    lcd.print(dist);
    lcd.print("cm       ");
  } else {
    lcd.print("SCANNING...     ");
  }
}

void alertCheck(int dist) {
  // Only alert on stable close objects (under 100cm)
  if (dist > 1 && dist < 100) {
    digitalWrite(ALERT_LED, HIGH);
    tone(BUZZER_PIN, 1200, 20);
  } else {
    digitalWrite(ALERT_LED, LOW);
    noTone(BUZZER_PIN);
  }
}

int calculateDistance(){ 
  digitalWrite(trigPin, LOW); 
  delayMicroseconds(5);
  digitalWrite(trigPin, HIGH); 
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  // High Stability: 30000us timeout (~5m)
  long dur = pulseIn(echoPin, HIGH, 30000); 
  if (dur == 0) return 999;
  
  int d = dur * 0.034 / 2;
  return (d > 0) ? d : 999;
}