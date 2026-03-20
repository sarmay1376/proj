// MEGA 2560 - BROAD SCAN IR MONITOR
const int scanPins[] = {2, 3, 18, 19, 20, 21};
const int numPins = 6;

void setup() {
  Serial.begin(115200);
  for (int i = 0; i < numPins; i++) {
    pinMode(scanPins[i], INPUT_PULLUP);
  }
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.println("--- MEGA IR BROAD-SCAN START ---");
  Serial.println("Checking Pins: 2, 3, 18, 19, 20, 21...");
}

void loop() {
  bool anyPulse = false;
  
  for (int i = 0; i < numPins; i++) {
    if (digitalRead(scanPins[i]) == LOW) {
      Serial.print("  >>> SIGNAL DETECTED ON PIN: ");
      Serial.print(scanPins[i]);
      Serial.println(" <<<  ");
      anyPulse = true;
    }
  }

  digitalWrite(LED_BUILTIN, anyPulse);
  
  if (anyPulse) delay(20); // Small debounce for readability
  
  static unsigned long lastBeat = 0;
  if (millis() - lastBeat > 1000) {
    Serial.println("SEARCHING...");
    lastBeat = millis();
  }
}
