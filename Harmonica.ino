#include <GxEPD2_BW.h>
#include <GxEPD2_7C.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <vector>
#include <WiFi.h>
#include <WebServer.h>

// --- INCLUDE SECRETS ---
#include "secrets.h" 

// --- CONFIGURATION ---
#define SLEEP_TIMEOUT_MS  300000  // 5 Minutes

// --- RTC MEMORY (Survives Deep Sleep) ---
RTC_DATA_ATTR int savedSongIndex = 0;

// --- HARDWARE PINS ---
#define SD_EN_PIN   16  
#define SD_DET_PIN  15  
#define SD_CS_PIN   14  
#define SD_MOSI_PIN 9   
#define SD_MISO_PIN 8
#define SD_SCK_PIN  7   

#define EPD_CS_PIN   10
#define EPD_DC_PIN   11
#define EPD_RES_PIN  12
#define EPD_BUSY_PIN 13

#define NEXT_BTN_PIN 4  
#define PREV_BTN_PIN 3  

// --- DISPLAY DRIVER ---
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>

#define GxEPD2_DISPLAY_CLASS GxEPD2_7C
#define GxEPD2_DRIVER_CLASS GxEPD2_730c_GDEP073E01 
#define MAX_DISPLAY_BUFFER_SIZE 16000
#define MAX_HEIGHT(EPD) (EPD::HEIGHT <= MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8) ? EPD::HEIGHT : MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8))

GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)>
    display(GxEPD2_DRIVER_CLASS(/*CS=*/EPD_CS_PIN, /*DC=*/EPD_DC_PIN, /*RST=*/EPD_RES_PIN, /*BUSY=*/EPD_BUSY_PIN));

SPIClass spiSD(HSPI);
WebServer server(80);

struct Song {
  String title;
  String tabs;
};

std::vector<Song> songs;
int currentSongIndex = 0;
String ipAddress = "Connecting...";
unsigned long lastActivityTime = 0; 
bool wifiActive = false; // Flag to track if we have web access

// Forward Declarations
void drawSong(int index);
void loadSetlistFromSD();
void handleRoot();
void handleSave();
void goToSleep();

void setup() {
  Serial.begin(115200);

  // 1. Hardware Setup
  pinMode(SD_EN_PIN, OUTPUT);
  pinMode(SD_DET_PIN, INPUT_PULLUP);
  pinMode(SD_CS_PIN, OUTPUT);
  pinMode(EPD_CS_PIN, OUTPUT);
  pinMode(EPD_RES_PIN, OUTPUT);
  pinMode(EPD_DC_PIN, OUTPUT);
  pinMode(NEXT_BTN_PIN, INPUT_PULLUP); 
  pinMode(PREV_BTN_PIN, INPUT_PULLUP); 

  // 2. Wakeup Logic
  if (esp_reset_reason() == ESP_RST_DEEPSLEEP) {
    Serial.println("Woke from Sleep");
    currentSongIndex = savedSongIndex;
  } else {
    Serial.println("Fresh Boot");
    currentSongIndex = 0;
  }

  // 3. Initialize SD Card
  digitalWrite(SD_EN_PIN, HIGH);
  delay(10);
  digitalWrite(EPD_CS_PIN, HIGH); 
  digitalWrite(SD_CS_PIN, HIGH);  

  spiSD.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  
  // Try to mount SD
  if (SD.begin(SD_CS_PIN, spiSD)) {
    loadSetlistFromSD();
  } else {
    Serial.println("SD Failed/Missing");
  }

  // --- GAME OVER LOGIC ---
  // If no songs loaded (SD missing, corrupted, or file empty)
  if (songs.empty()) {
    songs.push_back({
      "Game Over",          // Title
      "6 -5 5 -5 -4 4 -4"   // The Fail Riff
    });
  }

  // 4. Initialize Display
  display.epd2.selectSPI(spiSD, SPISettings(2000000, MSBFIRST, SPI_MODE0));
  display.init(115200); 
  display.setRotation(0);

  // 5. Connect to Wi-Fi (With Timeout Fallback)
  Serial.print("Connecting WiFi...");
  WiFi.begin(SECRET_SSID, SECRET_PASS);
  
  int retries = 0;
  // Wait max 10 seconds (20 * 500ms)
  while (WiFi.status() != WL_CONNECTED && retries < 20) { 
    delay(500);
    Serial.print(".");
    retries++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    ipAddress = WiFi.localIP().toString();
    wifiActive = true;
    server.on("/", HTTP_GET, handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.begin();
  } else {
    ipAddress = "Offline";
    wifiActive = false;
  }

  // 6. Splash Screen (Only on Fresh Boot)
  if (esp_reset_reason() != ESP_RST_DEEPSLEEP) {
     display.setFullWindow();
     display.firstPage();
     do {
       display.fillScreen(GxEPD_WHITE);
       
       // Header
       display.setFont(&FreeMonoBold12pt7b);
       display.setTextColor(GxEPD_BLACK);
       display.setCursor(10, 30); 
       display.print("Harmonica Jukebox");

       // IP Address
       display.setFont(&FreeMonoBold9pt7b);
       display.setTextColor(GxEPD_RED);
       display.setCursor(10, 60); 
       display.print("IP: " + ipAddress);

       // Line
       display.setTextColor(GxEPD_BLACK);
       display.fillRect(10, 70, 780, 2, GxEPD_BLACK);

       // List Songs
       display.setCursor(10, 95);
       display.print("Loaded Songs:");
       
       int y = 120;
       for (int i = 0; i < songs.size(); i++) {
         if (y > display.height() - 10) break;
         display.setCursor(20, y);
         display.print(String(i + 1) + ". " + songs[i].title);
         y += 25; 
       }

       // Instruction
       display.setTextColor(GxEPD_RED);
       display.setCursor(500, 60);
       display.print("Press Button ->");

     } while (display.nextPage());

     // Wait for start
     while(digitalRead(NEXT_BTN_PIN) == HIGH && digitalRead(PREV_BTN_PIN) == HIGH) {
       if (wifiActive) server.handleClient();
       delay(10);
     }
  }

  // Ensure index is valid
  if (currentSongIndex >= songs.size()) currentSongIndex = 0;
  
  drawSong(currentSongIndex);
  lastActivityTime = millis(); 
}

void loop() {
  // Only check web server if WiFi is actually on
  if (wifiActive) server.handleClient();

  // Sleep Timer
  if (millis() - lastActivityTime > SLEEP_TIMEOUT_MS) {
    goToSleep();
  }

  // Buttons
  if (digitalRead(NEXT_BTN_PIN) == LOW) {
    currentSongIndex++;
    if (currentSongIndex >= songs.size()) currentSongIndex = 0;
    savedSongIndex = currentSongIndex; 
    drawSong(currentSongIndex);
    lastActivityTime = millis(); 
    delay(500); 
    while(digitalRead(NEXT_BTN_PIN) == LOW) { if(wifiActive) server.handleClient(); }
  }

  if (digitalRead(PREV_BTN_PIN) == LOW) {
    currentSongIndex--;
    if (currentSongIndex < 0) currentSongIndex = songs.size() - 1;
    savedSongIndex = currentSongIndex;
    drawSong(currentSongIndex);
    lastActivityTime = millis(); 
    delay(500); 
    while(digitalRead(PREV_BTN_PIN) == LOW) { if(wifiActive) server.handleClient(); }
  }
}

// --- SLEEP ---
void goToSleep() {
  Serial.println("Sleep...");
  digitalWrite(SD_EN_PIN, LOW); 
  uint64_t pinMask = (1ULL << NEXT_BTN_PIN) | (1ULL << PREV_BTN_PIN);
  esp_sleep_enable_ext1_wakeup(pinMask, ESP_EXT1_WAKEUP_ANY_LOW);
  esp_deep_sleep_start();
}

// --- WEB & DRAW ---
void handleRoot() {
  File file = SD.open("/HarmonicaTab/SETLIST.txt");
  String fileContent = "";
  if (file) {
    while(file.available()) fileContent += (char)file.read();
    file.close();
  } else {
    // If we are editing via web but file is missing, start clean
    fileContent = "# New Song\nTabs here...";
  }
  String html = R"(<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width, initial-scale=1"><style>body{font-family:sans-serif;padding:20px;background:#f0f0f0}textarea{width:100%;height:60vh;font-family:monospace;font-size:16px;padding:10px}input[type=submit]{background:#0078d7;color:white;border:none;padding:15px 30px;font-size:18px;cursor:pointer;border-radius:5px}h2{color:#333}</style></head><body><h2>Harmonica Setlist Editor</h2><form action="/save" method="POST"><textarea name="content">)" + fileContent + R"(</textarea><br><br><input type="submit" value="Save & Update Device"></form></body></html>)";
  server.send(200, "text/html", html);
  lastActivityTime = millis(); 
}

void handleSave() {
  if (server.hasArg("content")) {
    File file = SD.open("/HarmonicaTab/SETLIST.txt", FILE_WRITE);
    if (file) {
      file.print(server.arg("content"));
      file.close();
      loadSetlistFromSD();
      currentSongIndex = 0; 
      drawSong(currentSongIndex);
      server.send(200, "text/html", "<h1>Saved!</h1><a href='/'>Go Back</a>");
    } else {
      server.send(500, "text/plain", "Failed to save.");
    }
  }
  lastActivityTime = millis();
}

void loadSetlistFromSD() {
  File file = SD.open("/HarmonicaTab/SETLIST.txt");
  if (!file) return;
  
  Song currentSong;
  bool isParsingSong = false;
  songs.clear();
  
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim(); 
    if (line.startsWith("#")) {
      if (isParsingSong) songs.push_back(currentSong);
      currentSong.title = line.substring(1); 
      currentSong.title.trim();
      currentSong.tabs = "";
      isParsingSong = true;
    } else if (!line.startsWith("---") && isParsingSong && line.length() > 0) {
      currentSong.tabs += line + "\n";
    }
  }
  if (isParsingSong) songs.push_back(currentSong);
  file.close();
}

void drawSong(int index) {
  String title = songs[index].title;
  String tabs = songs[index].tabs;
  const GFXfont* selectedFont;
  int16_t tbx, tby; uint16_t tbw, tbh;
  
  display.setFont(&FreeMonoBold18pt7b);
  display.getTextBounds(tabs.c_str(), 0, 0, &tbx, &tby, &tbw, &tbh);
  if (tbw < (display.width() - 20)) selectedFont = &FreeMonoBold18pt7b;
  else {
    display.setFont(&FreeMonoBold12pt7b);
    display.getTextBounds(tabs.c_str(), 0, 0, &tbx, &tby, &tbw, &tbh);
    if (tbw < (display.width() - 20)) selectedFont = &FreeMonoBold12pt7b;
    else selectedFont = &FreeMonoBold9pt7b;
  }
  
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setFont(&FreeMonoBold18pt7b);
    display.setTextColor(GxEPD_RED);
    display.getTextBounds(title.c_str(), 0, 0, &tbx, &tby, &tbw, &tbh);
    uint16_t x = ((display.width() - tbw) / 2) - tbx;
    display.setCursor(x, 50); 
    display.print(title);
    display.setFont(selectedFont);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(10, 100); 
    display.print(tabs);
  } while (display.nextPage());
}