#include <Arduino.h>
#include "DFRobotDFPlayerMini.h"

#define RX_PIN 17 
#define TX_PIN 18 

HardwareSerial mySerial(1); 
DFRobotDFPlayerMini myDFPlayer;

void setup() {
  Serial.begin(115200);
  mySerial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  
  Serial.println(F("DFPlayer starting..."));

  if (!myDFPlayer.begin(mySerial)) {
    Serial.println(F("Error: Check your connection!"));
    while(true);
  }
  
  Serial.println(F("Connection OK. Waiting three seconds..."));
  delay(3000);

  myDFPlayer.volume(20);
  
  Serial.println(F("Try to play Song 1..."));
  myDFPlayer.play(1); 
}

void loop() {
}
