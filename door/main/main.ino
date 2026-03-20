#include <SPI.h>
#include <MFRC522.h>
#include <Keypad.h>
#include <IRremote.h>
#include <Servo.h>
#include <LiquidCrystal.h>

/*
  SMART DOOR LOCK SYSTEM - MEGA EDITION (v2)
  -------------------------------------------
  Features:
  - RFID Card Access (MFRC522)
  - 4x4 Keypad PIN Code Access (# = Enter, * = Clear)
  - IR Remote Control Access
  - Servo Motor Bolt Mechanism (Pin 9)
  - 16x2 LCD Status Display (Parallel, Pins 26-29, A8, A9)
  - Buzzer & LED Feedback (Pin 10, 11)
  - Serial Telemetry for Web Dashboard (115200)
*/

// --- PIN DEFINITIONS (MEGA 2560) ---
#define RFID_SS_PIN  53
#define RFID_RST_PIN  5
#define IR_PIN       18  // Restored to Pin 18 (Pin 19 was too noisy)
#define BUZZER_PIN   10
#define RED_LED_PIN  11
#define SERVO_PIN     9

// --- SERVO SETUP ---
Servo lockServo;
const int LOCKED_ANGLE = 0;
const int OPEN_ANGLE   = 90;

// --- LCD SETUP (16x2 Parallel) ---
// All on a clean consecutive block: RS=38, E=39, D4=40, D5=41, D6=42, D7=43
LiquidCrystal lcd(38, 39, 40, 41, 42, 43);

// --- RFID SETUP ---
MFRC522 rfid(RFID_SS_PIN, RFID_RST_PIN);
String masterUID = "F3400EE5";

// --- KEYPAD SETUP ---
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {30, 31, 32, 33};
byte colPins[COLS] = {34, 35, 36, 37};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);
String correctPIN    = "1234";
String inputBuffer   = "";
String webCmdBuffer  = "";

// --- NON-BLOCKING STATE ---
bool isDoorOpen = false;
unsigned long openTimer = 0;
const unsigned long AUTO_LOCK_MS = 5000;

// --- KEYPAD SAFETY ---
unsigned long lastKeyPress = 0;
const unsigned long PIN_TIMEOUT = 10000;

// --- IR SETUP ---
const unsigned long IR_OPEN_CODE  = 0xFF02FD;
const unsigned long IR_CLOSE_CODE = 0xFF22DD;

// --- HELPER: Show on LCD ---
void showLCD(String line1, String line2 = "") {
  // Use a fixed 16-char buffer for flicker-free updates (no clear() needed)
  char buf1[17]; char buf2[17];
  
  // Format line 1
  snprintf(buf1, 17, "%-16s", line1.substring(0, 16).c_str());
  lcd.setCursor(0, 0);
  lcd.print(buf1);
  
  // Format line 2
  snprintf(buf2, 17, "%-16s", line2.substring(0, 16).c_str());
  lcd.setCursor(0, 1);
  lcd.print(buf2);
}

// --- HELPER: Timer-Safe Beep (Digital Bit-bang) ---
void passiveBeep(int freq, int dur) {
  long delayUs = 1000000L / freq / 2;
  long numCycles = (long)freq * dur / 1000L;
  for (long i = 0; i < numCycles; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delayMicroseconds(delayUs);
    digitalWrite(BUZZER_PIN, LOW);
    delayMicroseconds(delayUs);
  }
}

void setup() {
  Serial.begin(115200);
  SPI.begin();
  
  // 1. RFID INIT (Final say on SPI)
  rfid.PCD_Init();
  byte v = rfid.PCD_ReadRegister(rfid.VersionReg);
  if (v == 0x00 || v == 0xFF) {
    Serial.println("WARNING:RFID Fail!");
  } else {
    Serial.print("INFO:RFID OK (v"); Serial.print(v, HEX); Serial.println(")");
    // Power-Safe: Lower gain to 33dB to avoid overloading the Mega's 5V rail
    rfid.PCD_SetAntennaGain(rfid.RxGain_33dB); 
  }

  // 2. Servo & LCD
  lockServo.attach(SERVO_PIN);
  lockServo.write(LOCKED_ANGLE);
  lcd.begin(16, 2);
  showLCD(" SYSTEM RESET ", " HARDWARE READY");
  
  // 3. IR INIT (CALLED LAST: Prevents Timer-Clash with servo)
  IrReceiver.begin(IR_PIN, ENABLE_LED_FEEDBACK); 

  // Startup Pulse (Passive Digital Pulse: Won't break IR timer)
  pinMode(BUZZER_PIN, OUTPUT);
  passiveBeep(2000, 100);
  pinMode(RED_LED_PIN, OUTPUT);
  digitalWrite(RED_LED_PIN, LOW);
  delay(1000);
  digitalWrite(RED_LED_PIN, HIGH);

  Serial.println("STATE:READY_GLOBAL");
}

void loop() {
  static unsigned long lastRfidCheck = 0;
  
  // 1. SELF-HEALING: Re-init RFID every 10s if idle to prevent SPI hangs
  if (millis() - lastRfidCheck > 10000) {
    rfid.PCD_Init();
    lastRfidCheck = millis();
  }

  // 1b. PRIORITY RFID CHECK
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    passiveBeep(2000, 50); 
    String content = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
      content.concat(String(rfid.uid.uidByte[i] < 0x10 ? "0" : ""));
      content.concat(String(rfid.uid.uidByte[i], HEX));
    }
    content.toUpperCase();
    content.trim();

    if (content.equals(masterUID)) grantAccess("RFID");
    else denyAccess("RFID");
    
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
    lastRfidCheck = millis(); // Reset healing timer
  }

  // 2. Check Keypad
  char key = keypad.getKey();
  if (key) {
    Serial.print("KEY_PRESSED: "); Serial.println(key);
    passiveBeep(1000, 50);
    lastKeyPress = millis();

    if (key == '#') {
      Serial.print("PIN_ENTERED:"); Serial.println(inputBuffer);
      if (inputBuffer == correctPIN) grantAccess("KEYPAD");
      else denyAccess("KEYPAD");
      inputBuffer = "";
    } else if (key == '*') {
      Serial.println("INFO:PIN Buffer Cleared");
      showLCD("** LOCKED **", "PIN cleared");
      inputBuffer = "";
    } else {
      inputBuffer += key;
      // Show asterisks on LCD
      String stars = "";
      for (int i = 0; i < inputBuffer.length(); i++) stars += "*";
      showLCD("Enter PIN:", stars + "_");
    }
  }

  // 2b. PIN timeout
  if (inputBuffer.length() > 0 && (millis() - lastKeyPress >= PIN_TIMEOUT)) {
    Serial.println("INFO:PIN Entry Timeout");
    showLCD("** LOCKED **", "PIN timeout");
    inputBuffer = "";
  }

  // 3. Check IR (DEBUG MODE)
  if (IrReceiver.decode()) {
    unsigned long cmd = IrReceiver.decodedIRData.command;
    unsigned long raw = IrReceiver.decodedIRData.decodedRawData;

    // All NON-ZERO data is interesting!
    if (raw != 0) {
      passiveBeep(1500, 20); 
      if (cmd == IR_OPEN_CODE || cmd == 0x45) {
        if (isDoorOpen) manualRelock();
        else grantAccess("REMOTE");
      }
      else if (cmd == IR_CLOSE_CODE) manualRelock();
      else if ((cmd & 0xF0) == 0x90) grantAccess("REMOTE"); // Backup fuzzy
      else {
        char buf[17];
        sprintf(buf, "CMD:0x%02X R:0x%lX", (int)cmd, raw);
        showLCD("IR RAW DATA:", buf);
      }
    }
    IrReceiver.resume();
  }

  // 4. HEARTBEAT (Blink bottom-right of LCD)
  static unsigned long lastBeat = 0;
  if (millis() - lastBeat > 1000) {
    lcd.setCursor(15, 1);
    lcd.print(millis() % 2000 > 1000 ? "." : " ");
    lastBeat = millis();
  }

  // 4. Check Serial Web Commands
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\n') {
      webCmdBuffer.trim();
      if (webCmdBuffer == "UNLOCK_CMD") grantAccess("WEB");
      else if (webCmdBuffer == "LOCK_CMD") manualRelock();
      webCmdBuffer = "";
    } else {
      webCmdBuffer += c;
    }
  }

  // 5. Auto-lock check (Non-blocking)
  if (isDoorOpen && (millis() - openTimer >= AUTO_LOCK_MS)) {
    Serial.println("STATE:LOCKING");
    showLCD("AUTO LOCKING", "Please wait...");
    lockServo.write(LOCKED_ANGLE);
    delay(500);
    digitalWrite(RED_LED_PIN, HIGH);
    Serial.println("STATE:LOCKED");
    showLCD("** LOCKED **", "Scan / PIN / Web");
    isDoorOpen = false;
  }

  delay(20);
}

void grantAccess(String method) {
  if (isDoorOpen) {
    manualRelock();
    return;
  }

  Serial.print("STATE:ACCESS_GRANTED_BY_"); Serial.println(method);
  showLCD("ACCESS GRANTED", method);

  digitalWrite(RED_LED_PIN, LOW);
  passiveBeep(1800, 150); delay(50); passiveBeep(2400, 150); // Slightly 'happier' tone

  Serial.println("STATE:OPEN");
  lockServo.write(OPEN_ANGLE);

  showLCD("** OPEN **", "Auto-lock: 5s");

  isDoorOpen = true;
  openTimer  = millis();
}

void denyAccess(String method) {
  Serial.println("STATE:ACCESS_DENIED");
  showLCD("ACCESS DENIED", method + " rejected");

  for (int i = 0; i < 3; i++) {
    digitalWrite(RED_LED_PIN, LOW);
    passiveBeep(400, 200);
    delay(200);
    digitalWrite(RED_LED_PIN, HIGH);
    delay(200);
  }
  showLCD("** LOCKED **", "Scan / PIN / Web");
}

void manualRelock() {
  if (!isDoorOpen) return;

  Serial.println("STATE:LOCKING_MANUAL");
  showLCD("MANUAL LOCK", "Locking...");
  lockServo.write(LOCKED_ANGLE);
  delay(500);
  digitalWrite(RED_LED_PIN, HIGH);
  Serial.println("STATE:LOCKED");
  showLCD("** LOCKED **", "Scan / PIN / Web");

  isDoorOpen = false;
}
