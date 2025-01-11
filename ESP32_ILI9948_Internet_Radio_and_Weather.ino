#include <XPT2046_Touchscreen.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Audio.h>
#include <Preferences.h>
#include "Orbitron_Medium_20.h"

// touch screen
#define SCLK_PIN 18
#define MISO_PIN 19
#define MOSI_PIN 23
#define TOUCH_CS   5
#define TIRQ_PIN  35

// Wi-Fi and Openweather Configuration
const char* ssid = "YourSSID";
const char* password = "YourPASSWORD";
const char* apiKey = "YourAPIKey";
const char* cityID = "YourCityID";

// NTP Client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

unsigned long lastTimeUpdate = 0;
unsigned long timeUpdateInterval = 1000; // Update time every second

unsigned long lastWeatherUpdate = 0;
unsigned long weatherUpdateInterval = 60000; // Update weather every 10 minutes

// Radio URLs. You can add additional URLs
const char* radioURLs[] = {
  "http://stream.otvoreni.hr/otvoreni",
  "http://live.electricfm.com/electricfm",
  "http://stream2.radiotransilvania.ro/Oradea",
  "http://icecast.luxnet.ua/lux",
  "http://nl.ah.fm/live",
  "http://s4-webradio.rockantenne.de/90er-rock/stream/mp3",
  "https://punk.stream.laut.fm/punk",
  "http://144.76.106.52:7000/chillout.mp3",
  "https://streaming.radiostreamlive.com/radionylive_devices",
  "http://stream.104.6rtl.com/dance-hits/mp3-192/",
  "https://regiocast.streamabc.net/regc-90s90ssachsen-mp3-128-3768686",
  "http://stream2.wlmm.dk:8010/wmrmp3"
};

//Constant definitions
const int numberOfChannels = sizeof(radioURLs) / sizeof(radioURLs[0]);
int currentChannel = 0;
int volume = 5; // volume level 0 to 12

XPT2046_Touchscreen ts(TOUCH_CS, TIRQ_PIN); 
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);

// I2S pin configuration for ESP32
#define I2S_DOUT 27
#define I2S_BCLK 26
#define I2S_LRC 25

// Color constants
#define TFT_DARKBLUE  0x0000
#define TFT_DARKGREEN 0x03E0

// Audio object for decoding
Audio audio;

// Preferences object for saving the last station
Preferences preferences;

void updateWeather() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "http://api.openweathermap.org/data/2.5/weather?id=" + String(cityID) + "&appid=" + String(apiKey) + "&units=metric";
    Serial.println(url);
    
    http.begin(url);
    int httpResponseCode = http.GET();
    
    if (httpResponseCode > 0) {
      String payload = http.getString();
      Serial.println(payload);
      
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, payload);
      
      String city = doc["name"];
      String country = doc["sys"]["country"];
      float temperature = doc["main"]["temp"];
      int humidity = doc["main"]["humidity"];
      float windSpeed = doc["wind"]["speed"];
      float pressure = doc["main"]["pressure"];
      float latitude = doc["coord"]["lat"];
      float longitude = doc["coord"]["lon"];
      
      Serial.print("City: ");
      Serial.println(city);
      Serial.print("Country: ");
      Serial.println(country);
      Serial.print("Temperature: ");
      Serial.println(temperature);
      Serial.print("Humidity: ");
      Serial.println(humidity);
      Serial.print("Wind Speed: ");
      Serial.println(windSpeed);
      Serial.print("Pressure: ");
      Serial.println(pressure);
      Serial.print("Latitude: ");
      Serial.println(latitude);
      Serial.print("Longitude: ");
      Serial.println(longitude);

      // Print weather information
      tft.fillRoundRect(240, 200, 230, 110, 10, TFT_BLACK);
      tft.drawRoundRect(240, 200, 230, 110, 10, TFT_YELLOW);
      tft.setTextColor(TFT_RED);
      tft.setFreeFont(&Orbitron_Medium_20);
      tft.setCursor(270, 230);
      tft.printf("%s, %s\n", city.c_str(), country.c_str());
      
      tft.setTextColor(TFT_YELLOW);
      tft.setFreeFont(&Orbitron_Medium_20);
      tft.setCursor(270, 270);
      tft.printf("Temp.: %.1f C\n", temperature);
      tft.setCursor(270, 300);
      tft.printf("Hum.: %d%%\n", humidity);
      //tft.printf("Wind Speed: %.1f m/s\n", windSpeed);
      //tft.printf("Pressure: %.2f hPa\n", pressure);
      //tft.printf("Latitude: %.2f\n", latitude);
      //tft.printf("Longitude: %.2f\n", longitude);
    } else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }
    
    http.end();
  } else {
    Serial.println("WiFi Disconnected");
  }
}

// Draw a button 
void drawRoundedButton(int x, int y, int width, int height, const char* label, uint16_t color) {
  tft.fillRoundRect(x, y, width, height, 10, color); 
  tft.setCursor(x + 25, y + 25);
  tft.setTextColor(TFT_WHITE);
  tft.setFreeFont(&FreeMonoBold9pt7b);
  tft.println(label);
}

// Draw all buttons on the screen
void drawButtons() {
  drawRoundedButton(10, 200, 100, 50, "Prev", TFT_BLUE);
  drawRoundedButton(130, 200, 100, 50, "Next", TFT_BLUE);
  drawRoundedButton(10, 260, 100, 50, "Vol -", TFT_DARKGREEN);
  drawRoundedButton(130, 260, 100, 50, "Vol +", TFT_DARKGREEN);
  
}

void saveSettings() {
  preferences.begin("radio", false);
  preferences.putInt("channel", currentChannel);
  preferences.putInt("volume", volume);
  preferences.end();
}

void loadSettings() {
  preferences.begin("radio", true);
  currentChannel = preferences.getInt("channel", 0); // Default to 0 if not set
  volume = preferences.getInt("volume", volume); 
  preferences.end();
}

// Display current volume level
void displayVolume() {
  tft.fillRect(0, 160, tft.width(), 40, TFT_BLACK);
  tft.setCursor(10, 175);
  tft.setTextColor(TFT_WHITE);
  tft.setFreeFont(&FreeMonoBold9pt7b);
  tft.printf("Volume: %d\n", volume);
}

// Connect to the current radio stream
void connectToRadio(const char* url) {
  Serial.print("Connecting to radio: ");
  Serial.println(url);
  audio.connecttohost(url);
}

// Change the radio channel
void changeChannel(int direction) {
  currentChannel = (currentChannel + direction + numberOfChannels) % numberOfChannels;  // Cycle channels
  connectToRadio(radioURLs[currentChannel]);
}

// Handle touch events
void handleTouchEvent(int x, int y) {

  if (x >= 2200 && x <= 3600 && y >= 2600 && y <= 3100) {
    changeChannel(1); // Prev channel
    saveSettings();
  } else if (x >= 450 && x <= 1850 && y >= 2600 && y <= 3100) {
    changeChannel(-1); // Next channel
    saveSettings();
  } else if (x >= 2200 && x <= 3600 && y >= 3200 && y <= 3700) {
    if (volume > 0) volume++;  // Decrease volume
    audio.setVolume(volume);
    displayVolume();
    saveSettings();
  } else if (x >= 450 && x <= 1850 && y >= 3200 && y <= 3700) {
    if (volume < 21) volume--;  // Increase volume
    audio.setVolume(volume);
    displayVolume();
    saveSettings();
  }
}

// Check for touch input and handle it
void checkTouch() {
  if (ts.touched()) {
    TS_Point p = ts.getPoint();
    // Map the touch coordinates to the screen coordinates
    int x = map(p.x, 0, 240, 0, tft.width());
    int y = map(p.y, 0, 320, 0, tft.height());

    delay(100);

    handleTouchEvent(x, y);
    drawButtons();  // Redraw buttons after feedback
  }
}


// Callback for Station Name display
void audio_showstation(const char* info) {
  Serial.print("Station: ");
  Serial.println(info);
    tft.fillRect(0, 20, tft.width(), 50, TFT_BLACK);  // Clear area
    tft.setCursor(0, 30);
    tft.setTextColor(TFT_WHITE);
    tft.setFreeFont(&FreeMonoBold9pt7b);
    tft.println(info);
  }

// Callback for Stream Title display
void audio_showstreamtitle(const char* info) {
  Serial.print("Stream Title: ");
  Serial.println(info);
    tft.fillRect(0, 80, tft.width(), 70, TFT_BLACK);  // Clear area
    tft.setCursor(0, 90);
    tft.setTextColor(TFT_YELLOW);
    tft.setFreeFont(&FreeMono9pt7b);
    tft.println(info);
  }

void setup() {
  Serial.begin(115200);

  SPI.begin(SCLK_PIN, MISO_PIN, MOSI_PIN);
  ts.begin();
  ts.setRotation(1);
  
  while (!Serial && (millis() <= 1000));

  // TFT display setup
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);

  // Wi-Fi setup
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nConnected to Wi-Fi");
  loadSettings();
  
  // I2S setup for audio
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(volume);

  // Connect to the first radio station
  connectToRadio(radioURLs[currentChannel]);

  // Draw buttons on screen
  drawButtons(); 
  displayVolume();
}

void loop() {
  audio.loop();  // Audio playback loop
  checkTouch();  // Check for touch inputs
    // Update weather every 10 minutes
   unsigned long currentMillis = millis();
  if (currentMillis - lastWeatherUpdate >= weatherUpdateInterval || lastWeatherUpdate == 0) {
    lastWeatherUpdate = currentMillis;
    updateWeather();
  }
  
  }
