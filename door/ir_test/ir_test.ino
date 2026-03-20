#include <IRremote.h>

void setup() {
  Serial.begin(115200);
  IrReceiver.begin(18, ENABLE_LED_FEEDBACK); 
  Serial.println("--- IR DECODER START (Pin 18) ---");
  Serial.println("Press any button on your remote...");
}

void loop() {
  if (IrReceiver.decode()) {
    if (IrReceiver.decodedIRData.decodedRawData != 0) {
      Serial.print("IR_RAW: 0x"); Serial.println(IrReceiver.decodedIRData.decodedRawData, HEX);
      Serial.print("IR_CMD: 0x"); Serial.println(IrReceiver.decodedIRData.command, HEX);
      Serial.println("----");
    }
    IrReceiver.resume();
  }
}
