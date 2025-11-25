#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include "DFRobotDFPlayerMini.h"

#define RX_PIN 17
#define TX_PIN 18

TFT_eSPI tft = TFT_eSPI();
HardwareSerial mySerial(1);
DFRobotDFPlayerMini myDFPlayer;

int currentSongIndex = 1;
int volume = 20;  // 0-30
bool isPlaying = false;
bool isPaused = false;
int recordAngle = 0;

unsigned long lastTouchCheck = 0;
unsigned long lastAnimTime = 0;

void drawInterface();
void drawPlayPauseButton();
void updateSongInfo();
void playSong(int index);
void togglePlayPause();
void handleTouch(int x, int y);
void animateRecord();

void setup() {
  Serial.begin(115200);

  mySerial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);

  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);

  uint16_t calData[5] = { 418, 3418, 322, 3348, 1 };
  tft.setTouch(calData);

  //  touch_calibrate();

  Serial.println(F("DFPlayer Mini Starting..."));

  if (!myDFPlayer.begin(mySerial)) {
    tft.setTextColor(TFT_RED);
    tft.setTextSize(2);
    tft.setCursor(10, 50);
    tft.println("Connection Error!");
  } else {
    Serial.println(F("DFPlayer Online."));
    myDFPlayer.volume(volume);
  }

  drawInterface();
}

void loop() {
  if (millis() - lastTouchCheck > 50) {
    lastTouchCheck = millis();

    uint16_t t_x, t_y;
    if (tft.getTouch(&t_x, &t_y)) {
      handleTouch(t_x, t_y);
      delay(200);
    }
  }
  if (isPlaying && !isPaused && (millis() - lastAnimTime > 50)) {
    animateRecord();
    lastAnimTime = millis();
  }
}

void drawInterface() {
  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.print("ESP32 Player");

  tft.fillCircle(110, 120, 75, TFT_DARKGREY);
  tft.fillCircle(110, 120, 70, TFT_BLACK);
  tft.fillCircle(110, 120, 25, TFT_RED);
  tft.fillCircle(110, 120, 5, TFT_BLACK);

  tft.fillRect(240, 40, 70, 50, TFT_BLUE);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.drawString("VOL+", 255, 55);

  tft.fillRect(240, 110, 70, 50, TFT_BLUE);
  tft.drawString("VOL-", 255, 125);

  tft.drawRect(5, 195, 230, 45, TFT_DARKGREY);
  tft.fillRect(10, 200, 60, 35, TFT_DARKGREY);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("<<", 30, 208);

  tft.fillRect(170, 200, 60, 35, TFT_DARKGREY);
  tft.drawString(">>", 190, 208);

  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setCursor(250, 10);
  tft.print("Vol:");

  drawPlayPauseButton();
  updateVolInfo();
  updateSongInfo();
}

void drawPlayPauseButton() {
  tft.fillRect(90, 200, 60, 35, TFT_BLACK);
  tft.drawRect(90, 200, 60, 35, TFT_WHITE);

  if (isPlaying && !isPaused) {
    tft.fillRect(108, 208, 8, 20, TFT_WHITE);
    tft.fillRect(124, 208, 8, 20, TFT_WHITE);
  } else {
    tft.fillTriangle(110, 208, 110, 228, 135, 218, TFT_GREEN);
  }
}

void updateVolInfo() {
  tft.fillRect(289, 9, 30, 20, TFT_BLACK);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(290, 10);
  tft.print(volume);
}

void updateSongInfo() {
  tft.fillRect(230, 180, 90, 50, TFT_BLACK);

  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(240, 190);
  tft.print("Song");

  tft.setCursor(250, 215);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.print("#");
  tft.print(currentSongIndex);
}

void animateRecord() {
  float rad = recordAngle * 0.0174532925;

  int x = 110 + (int)(15 * cos(rad));
  int y = 120 + (int)(15 * sin(rad));
  int oldX = 110 + (int)(15 * cos((rad - 0.2)));
  int oldY = 120 + (int)(15 * sin((rad - 0.2)));

  tft.fillCircle(oldX, oldY, 4, TFT_RED);
  tft.fillCircle(x, y, 4, TFT_WHITE);

  recordAngle += 12;
  if (recordAngle >= 360) recordAngle = 0;
}

void handleTouch(int x, int y) {
  if (x > 240 && x < 310 && y > 40 && y < 90) {
    if (volume < 30) volume++;
    myDFPlayer.volume(volume);
    Serial.print("Vol: ");
    Serial.println(volume);
    updateVolInfo();
  }
  if (x > 240 && x < 310 && y > 110 && y < 160) {
    if (volume > 0) volume--;
    myDFPlayer.volume(volume);
    Serial.print("Vol: ");
    Serial.println(volume);
    updateVolInfo();
  }
  if (x > 10 && x < 70 && y > 200 && y < 235) {
    if (currentSongIndex > 1) {
      currentSongIndex--;
      playSong(currentSongIndex);
    } else {
      Serial.println("Start of list");
    }
  }
  if (x > 90 && x < 150 && y > 200 && y < 235) {
    togglePlayPause();
  }
  if (x > 170 && x < 230 && y > 200 && y < 235) {
    currentSongIndex++;
    playSong(currentSongIndex);
  }
}

void togglePlayPause() {
  if (isPlaying && !isPaused) {
    myDFPlayer.pause();
    isPaused = true;
    Serial.println("Action: PAUSE");
  } else if (isPlaying && isPaused) {
    myDFPlayer.start();
    isPaused = false;
    Serial.println("Action: RESUME");
  } else {
    playSong(currentSongIndex);
  }
  drawPlayPauseButton();
}

void playSong(int index) {
  currentSongIndex = index;

  isPlaying = true;
  isPaused = false;

  Serial.print("Playing Index: ");
  Serial.println(index);

  myDFPlayer.play(index);

  updateSongInfo();
  drawPlayPauseButton();  // Will show Pause icon now
}

void touch_calibrate() {
  uint16_t calData[5];
  uint8_t calDataOK = 0;

  tft.fillScreen(TFT_BLACK);
  tft.setCursor(20, 0);
  tft.setTextFont(2);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.println("Touch corners as indicated");

  tft.setTextFont(1);
  tft.println();

  tft.calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 15);

  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.println("Calibration complete!");
  tft.println("Copy these numbers to code:");

  Serial.print("uint16_t calData[5] = { ");
  for (uint8_t i = 0; i < 5; i++) {
    Serial.print(calData[i]);
    if (i < 4) Serial.print(", ");
  }
  Serial.println(" };");

  tft.print("{ ");
  for (uint8_t i = 0; i < 5; i++) {
    tft.print(calData[i]);
    if (i < 4) tft.print(", ");
  }
  tft.println(" }");

  delay(4000);
}
