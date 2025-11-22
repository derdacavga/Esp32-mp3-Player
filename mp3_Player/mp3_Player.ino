#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>
#include <TFT_eSPI.h>
#include "Audio.h"

#define SD_SCK 17
#define SD_MISO 18
#define SD_MOSI 16
#define SD_CS 10

#define I2S_DOUT 6
#define I2S_BCLK 7
#define I2S_LRC 5

TFT_eSPI tft = TFT_eSPI();
Audio audio;
SPIClass sdSPI = SPIClass(VSPI);

std::vector<String> songList;
int currentSongIndex = -1;
int volume = 10;
bool isPlaying = false;
bool isPaused = false;

enum ScreenMode { SCREEN_PLAYER,
                  SCREEN_PLAYLIST };
ScreenMode currentScreen = SCREEN_PLAYER;

int playlistScrollOffset = 0;
int songsPerPage = 5;

float vinylAngle = 0;
unsigned long lastVinylUpdate = 0;
int vinylCenterX = 160;
int vinylCenterY = 100;
int vinylRadius = 70;

unsigned long songDuration = 0;
unsigned long currentTime = 0;

void loadSongs();
void drawPlayerScreen();
void drawPlaylistScreen();
void drawVinyl(float angle);
void drawButton(int x, int y, int w, int h, String text, uint16_t color);
void drawMusicIcon(int x, int y, uint16_t color);
void drawVolumeControl();
void updateSongCounter();
void handleTouch(int x, int y);
void playSong(int index);
void formatTime(unsigned long ms, char* buffer);
void logo();

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== MP3 Player Starting ===");

  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  logo();
  delay(1000);
  Serial.println("Display OK");

  uint16_t calData[5] = { 418, 3418, 322, 3348, 1 };
  tft.setTouch(calData);

  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

  if (!SD.begin(SD_CS, sdSPI, 4000000)) {
    Serial.println("SD Card ERROR!");
    tft.setTextColor(TFT_RED);
    tft.setTextSize(2);
    tft.setCursor(60, 100);
    tft.println("SD ERROR!");
    while (1) delay(1000);
  }
  Serial.println("SD Card OK");

  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(volume);

  loadSongs();
  drawPlayerScreen();

  Serial.println("Setup Complete");
}

unsigned long lastTouchCheck = 0;
bool touched = false;

void loop() {
  audio.loop();

  if (isPlaying && !isPaused && millis() - lastVinylUpdate > 50) {
    lastVinylUpdate = millis();
    vinylAngle += 3.0;
    if (vinylAngle >= 360) vinylAngle = 0;

    if (currentScreen == SCREEN_PLAYER) {
      drawVinyl(vinylAngle);

      currentTime = audio.getAudioCurrentTime();

      tft.fillRect(20, 210, 280, 2, TFT_DARKGREY);
      if (songDuration > 0) {
        int progress = map(currentTime, 0, songDuration, 0, 280);
        tft.fillRect(20, 210, progress, 2, TFT_RED);
      }

      char timeStr[20];
      tft.setTextSize(1);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);

      formatTime(currentTime * 1000, timeStr);
      tft.setCursor(20, 215);
      tft.print(timeStr);

      tft.fillRect(260, 215, 50, 10, TFT_BLACK);
      formatTime(songDuration * 1000, timeStr);
      tft.setCursor(270, 215);
      tft.print(timeStr);
    }
  }

  if (millis() - lastTouchCheck > 100) {
    lastTouchCheck = millis();

    uint16_t t_x = 0, t_y = 0;
    bool isTouched = tft.getTouch(&t_x, &t_y);

    if (isTouched && !touched) {
      touched = true;
      Serial.printf("Touch: x=%d y=%d\n", t_x, t_y);
      handleTouch(t_x, t_y);
      delay(200);
    } else if (!isTouched) {
      touched = false;
    }
  }
}

void loadSongs() {
  songList.clear();
  File root = SD.open("/");
  if (!root) {
    Serial.println("SD Root Failed");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    String fileName = String(file.name());
    if (!file.isDirectory()) {
      if (fileName.endsWith(".mp3") || fileName.endsWith(".MP3") || fileName.endsWith(".wav") || fileName.endsWith(".WAV")) {
        if (fileName.startsWith("/")) fileName = fileName.substring(1);
        songList.push_back(fileName);
        Serial.println("Found: " + fileName);
      }
    }
    file = root.openNextFile();
  }
  root.close();
  Serial.printf("Total songs: %d\n", songList.size());
}

void drawPlayerScreen() {
  tft.fillScreen(TFT_BLACK);

  updateSongCounter();

  tft.drawRect(295, 8, 20, 12, TFT_WHITE);
  tft.fillRect(315, 11, 3, 6, TFT_WHITE);
  tft.fillRect(297, 10, 16, 8, TFT_GREEN);

  drawVinyl(vinylAngle);

  tft.drawRect(20, 175, 25, 25, TFT_DARKGREY);
  tft.setTextSize(2);
  tft.setTextColor(TFT_GREEN);
  tft.setCursor(24, 180);
  tft.print("S");

  tft.fillRect(20, 210, 280, 2, TFT_DARKGREY);

  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(20, 215);
  tft.print("0:00:00");
  tft.setCursor(270, 215);
  tft.print("0:00:00");

  int btnY = 185;

  drawButton(90, btnY, 40, 40, "<<", TFT_DARKGREY);

  if (isPaused || !isPlaying) {
    drawButton(145, btnY - 10, 50, 50, "||", TFT_WHITE);
  } else {
    drawButton(145, btnY - 10, 50, 50, "||", TFT_RED);
  }
  drawButton(210, btnY, 40, 40, ">>", TFT_DARKGREY);
  drawMusicIcon(280, 185, TFT_WHITE);
  drawVolumeControl();
}

void drawPlaylistScreen() {
  tft.fillScreen(TFT_BLACK);

  tft.fillRect(0, 0, 320, 30, TFT_DARKGREY);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(10, 7);
  tft.print("PLAYLIST");

  tft.fillRect(270, 5, 40, 20, TFT_RED);
  tft.setTextSize(1);
  tft.setCursor(278, 9);
  tft.print("BACK");

  if (playlistScrollOffset > 0) {
    tft.fillTriangle(160, 35, 150, 45, 170, 45, TFT_GREEN);
  }

  int yPos = 50;
  int displayCount = min(songsPerPage, (int)songList.size() - playlistScrollOffset);

  for (int i = 0; i < displayCount; i++) {
    int songIndex = i + playlistScrollOffset;

    if (songIndex == currentSongIndex) {
      tft.fillRect(5, yPos - 2, 310, 32, TFT_DARKGREY);
    }

    tft.setTextSize(2);
    if (songIndex == currentSongIndex) {
      tft.setTextColor(TFT_GREEN, TFT_DARKGREY);
    } else {
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
    }

    String displayName = songList[songIndex];
    if (displayName.endsWith(".mp3") || displayName.endsWith(".MP3")) {
      displayName = displayName.substring(0, displayName.length() - 4);
    }

    if (displayName.length() > 22) {
      displayName = displayName.substring(0, 19) + "...";
    }

    tft.setCursor(10, yPos);
    tft.print(displayName);

    yPos += 35;
  }

  if (playlistScrollOffset + songsPerPage < songList.size()) {
    tft.fillTriangle(160, 225, 150, 215, 170, 215, TFT_GREEN);
  }
}

void drawVinyl(float angle) {
  tft.fillCircle(vinylCenterX, vinylCenterY, vinylRadius, TFT_BLACK);

  for (int i = 0; i < 3; i++) {
    tft.drawCircle(vinylCenterX, vinylCenterY, vinylRadius - i, TFT_DARKGREY);
  }

  for (int r = 20; r < vinylRadius - 5; r += 8) {
    tft.drawCircle(vinylCenterX, vinylCenterY, r, 0x2104);
  }

  tft.fillCircle(vinylCenterX, vinylCenterY, 18, TFT_RED);
  tft.fillCircle(vinylCenterX - 3, vinylCenterY - 3, 6, 0xF800);

  if (isPlaying && !isPaused) {
    float rad = angle * PI / 180.0;
    for (int i = 0; i < 4; i++) {
      float a = rad + (i * PI / 2);
      int x1 = vinylCenterX + cos(a) * 25;
      int y1 = vinylCenterY + sin(a) * 25;
      int x2 = vinylCenterX + cos(a) * 65;
      int y2 = vinylCenterY + sin(a) * 65;
      tft.drawLine(x1, y1, x2, y2, 0x4208);
    }
  }
}

void drawButton(int x, int y, int w, int h, String text, uint16_t color) {
  tft.fillRoundRect(x, y, w, h, 5, color);
  tft.drawRoundRect(x, y, w, h, 5, TFT_WHITE);

  tft.setTextSize(2);
  tft.setTextColor(TFT_BLACK);
  int textX = x + (w - text.length() * 12) / 2;
  int textY = y + (h - 16) / 2;
  tft.setCursor(textX, textY);
  tft.print(text);
}

void drawMusicIcon(int x, int y, uint16_t color) {
  tft.fillRect(x, y, 4, 20, color);
  tft.fillRect(x + 10, y + 5, 4, 15, color);
  tft.fillCircle(x + 2, y + 20, 4, color);
  tft.fillCircle(x + 12, y + 20, 4, color);
  tft.drawLine(x + 4, y, x + 14, y + 5, color);
}

void drawVolumeControl() {
  int volX = 265;
  int volY = 80;

  tft.fillRoundRect(volX, volY, 40, 30, 5, TFT_DARKGREEN);
  tft.drawRoundRect(volX, volY, 40, 30, 5, TFT_WHITE);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(volX + 12, volY + 7);
  tft.print("+");

  tft.fillRoundRect(volX, volY + 35, 40, 30, 5, TFT_MAROON);
  tft.drawRoundRect(volX, volY + 35, 40, 30, 5, TFT_WHITE);
  tft.setCursor(volX + 15, volY + 42);
  tft.print("-");

  tft.fillRect(volX, volY + 70, 50, 10, TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setCursor(volX + 8, volY + 70);
  tft.printf("VOL:%d", volume);
}

void updateSongCounter() {
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(10, 10);
  if (currentSongIndex >= 0) {
    tft.printf("%04d/%04d", currentSongIndex + 1, songList.size());
  } else {
    tft.printf("0000/%04d", songList.size());
  }
}

void handleTouch(int x, int y) {
  if (currentScreen == SCREEN_PLAYER) {

    if (x > 145 && x < 195 && y > 175 && y < 225) {
      if (!isPlaying && currentSongIndex >= 0) {
        audio.pauseResume();
        isPlaying = true;
        isPaused = false;
      } else if (isPlaying) {
        audio.pauseResume();
        isPaused = !isPaused;
      } else if (songList.size() > 0) {
        playSong(0);
      }
      drawPlayerScreen();
      Serial.println(isPaused ? "Paused" : "Playing");
    }

    if (x > 90 && x < 130 && y > 185 && y < 225) {
      if (currentSongIndex > 0) {
        playSong(currentSongIndex - 1);
      }
    }

    if (x > 210 && x < 250 && y > 185 && y < 225) {
      if (currentSongIndex < songList.size() - 1) {
        playSong(currentSongIndex + 1);
      }
    }

    if (x > 270 && y > 180) {
      currentScreen = SCREEN_PLAYLIST;
      drawPlaylistScreen();
      Serial.println("Playlist opened");
    }

    if (x > 265 && x < 305 && y > 80 && y < 110) {
      if (volume < 21) {
        volume++;
        audio.setVolume(volume);
        Serial.printf("Volume: %d\n", volume);
        drawVolumeControl();
      }
    }

    if (x > 265 && x < 305 && y > 115 && y < 145) {
      if (volume > 0) {
        volume--;
        audio.setVolume(volume);
        Serial.printf("Volume: %d\n", volume);
        drawVolumeControl();
      }
    }
  } else if (currentScreen == SCREEN_PLAYLIST) {

    if (x > 270 && y < 30) {
      currentScreen = SCREEN_PLAYER;
      drawPlayerScreen();
      Serial.println("Back to player");
    }

    if (y > 30 && y < 50 && playlistScrollOffset > 0) {
      playlistScrollOffset--;
      drawPlaylistScreen();
    }

    if (y > 210 && y < 230 && playlistScrollOffset + songsPerPage < songList.size()) {
      playlistScrollOffset++;
      drawPlaylistScreen();
    }

    if (y > 50 && y < 215) {
      int index = (y - 50) / 35 + playlistScrollOffset;
      if (index < songList.size()) {
        playSong(index);
        currentScreen = SCREEN_PLAYER;
        drawPlayerScreen();
      }
    }
  }
}

void playSong(int index) {
  if (index >= songList.size() || index < 0) return;

  currentSongIndex = index;
  String path = "/" + songList[index];

  Serial.printf("Playing: %s\n", path.c_str());

  bool success = audio.connecttoFS(SD, path.c_str());

  if (success) {
    isPlaying = true;
    isPaused = false;
    vinylAngle = 0;
    currentTime = 0;
    songDuration = 0;

    if (currentScreen == SCREEN_PLAYER) {
      updateSongCounter();
    }

    Serial.println("Playback started");
  } else {
    Serial.println("Playback FAILED");
    isPlaying = false;
  }
}

void formatTime(unsigned long ms, char* buffer) {
  int seconds = (ms / 1000) % 60;
  int minutes = (ms / 60000) % 60;
  int hours = ms / 3600000;
  sprintf(buffer, "%d:%02d:%02d", hours, minutes, seconds);
}

void audio_info(const char* info) {
  Serial.printf("Info: %s\n", info);

  String infoStr = String(info);

  if (infoStr.indexOf("Duration:") >= 0) {
    int idx = infoStr.indexOf("Duration:");
    songDuration = infoStr.substring(idx + 10).toInt();

    if (currentScreen == SCREEN_PLAYER && songDuration > 0) {
      tft.fillRect(260, 215, 50, 10, TFT_BLACK);
      char timeStr[20];
      formatTime(songDuration * 1000, timeStr);
      tft.setTextSize(1);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.setCursor(270, 215);
      tft.print(timeStr);
    }
  }
}

void audio_eof_mp3(const char* info) {
  Serial.println("Song ended");

  currentSongIndex++;
  if (currentSongIndex < songList.size()) {
    playSong(currentSongIndex);
  } else {
    currentSongIndex = 0;
    playSong(0);
  }
}

void logo() {
  tft.fillScreen(TFT_GREEN);
  int centerX = 120;
  int centerY = 160;
  int radius = 70;
  int thickness = 7;

  tft.fillCircle(centerX, centerY, radius, TFT_RED);
  for (int r = radius - thickness / 2; r <= radius + thickness / 2; r++) {
    tft.drawCircle(centerX, centerY, r, TFT_BLACK);
  }
  tft.setTextColor(TFT_BLACK, TFT_RED);
  tft.setFreeFont(&FreeSansBold24pt7b);
  tft.drawString("DSN", 70, 142);
  tft.setTextColor(TFT_BLACK, TFT_GREEN);
  tft.setFreeFont(&FreeSansBold9pt7b);
  tft.drawString("Mp3 PLAYER", 20, 300);
}
