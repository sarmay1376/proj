#define IR_PIN 2

void setup() {
  Serial.begin(115200);
  pinMode(IR_PIN, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.println(" --- HEARTBEAT MONITOR (NON-BLOCKING) ---");
}

void loop() {
  static unsigned long lastPrint = 0;
  bool pinState = digitalRead(IR_PIN);
  unsigned long now = millis();

  // 1. HEARTBEAT: If you see this, the Arduino is NOT frozen.
  if (now - lastPrint > 500) {
    Serial.print("INFO:IR_PIN is "); 
    Serial.println(pinState == HIGH ? "HIGH (OK)" : "LOW (SHORT!)");
    lastPrint = now;
  }

  // 2. INSTANT FEEDBACK:
  digitalWrite(LED_BUILTIN, !pinState); // LED ON when pin is LOW (Seeing Light)
  
  if (pinState == LOW) {
    // If you see this, your hardware/remote is definitely working!
    Serial.println("  >>> PULSE! <<<  ");
    delay(5); // Ultra-small debounce
  }
}
