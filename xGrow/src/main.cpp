#include <Arduino.h>
#include <TFT_eSPI.h>
#include <DHT20.h>
#include <Wire.h>
#include <ESP32Servo.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <SPI.h>
#include <HttpClient.h>
#include <WiFi.h>
#include <inttypes.h>
#include <stdio.h>
#include <string>
#include "esp_system.h"

#define SERVO_PIN 1
#define LED_PIN 2
#define BUZZER_PIN 3
#define PHOTO_PIN 10
#define DHT_SCL_PIN 11
#define DHT_SDA_PIN 12
#define SMS_PIN 13
#define LIGHT_PIN 16
//S3 SCREEN PINS
#define PIN_POWER_ON 15
#define PIN_LCD_BL 38

DHT20 dht;
Servo myservo;
TFT_eSPI tft = TFT_eSPI();

// Thresholds
const float SOIL_MOISTURE_THRESHOLD = 3680; // Soil moisture threshold
const int MAX_WATER_CYCLES = 10;           // Max watering cycles before alerting
int waterCycleCount = 0;                   // Tracks number of watering cycles
int LIGHT_THRESHOLD = 5;                          // Detect min light
unsigned long lastWateringTime = 0;        // Last watering timestamp
const unsigned long WATERING_INTERVAL = 172800000; // 2 days(48 hours) in milliseconds
unsigned long deviceStartTime = 0; // Time when the device was turned on
unsigned long lightStartTime = 0;   // Start time when light is detected
unsigned long totalLightTime = 0;  // Total light time in milliseconds
unsigned long artificialLightStart = 0; // Start time for artificial light
unsigned long lastResetTime = 0;     // Last time totalLightTime was reset
bool artificialLightOn = false;    // Track if artificial light is ON
bool userControlled = false;         // Tracks if the light is user-controlled
const unsigned long ONE_HOUR = 3600000; // One hour in milliseconds
const unsigned long TWO_HOURS = 2 * ONE_HOUR; // 2 hours in milliseconds
const unsigned long FOUR_HOURS = 4 * ONE_HOUR; // 4 hours in milliseconds
const unsigned long ONE_DAY = 24 * 60 * 60 * 1000;   // 24 hours in milliseconds
// Comfort thresholds
const float TEMP_COMFORT_MIN = 20;
const float TEMP_COMFORT_MAX = 30;
const float HUMIDITY_COMFORT_MIN = 40;
const float HUMIDITY_COMFORT_MAX = 60;

// WIFI
char ssid[50] = "S&K Router"; // your network SSID (name)
char password[50] = "88888888"; // your network password
// Number of milliseconds to wait without receiving any data before we give up
const int kNetworkTimeout = 30 * 1000;
// Number of milliseconds to wait if no data is available before trying again
const int kNetworkDelay = 1000;

// Bluetooth
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CONTROL_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a7"
#define NOTIFICATION_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

BLECharacteristic *waterNotificationCharacteristic;

class PLANTCallbacks : public BLECharacteristicCallbacks {
public:
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    
    if (value == "on") {
      digitalWrite(LIGHT_PIN, HIGH);
      artificialLightOn = true;
      userControlled = true;
      lightStartTime = millis(); // Start timing light
      Serial.println("Artificial light ON (via Bluetooth).");
    } else if (value == "off") {
      digitalWrite(LIGHT_PIN, LOW);
      Serial.println("Artificial light OFF (via Bluetooth).");
      artificialLightOn = false;
      userControlled = true;
    } else if (value == "water"){
      myservo.write(0);
      delay(2000);
      myservo.write(180);
      waterCycleCount++;
      Serial.println("Manual watering triggered via BLE.");
    } else if (value == "refill") {
      waterCycleCount = 0;
      Serial.println("Water container refilled. Cycle count reset.");
    } else if (value == "no water"){
      //FOR TEST ONLY
      waterCycleCount = 10;
    }
  }
};

// Function to map temperature to color
uint16_t getTemperatureColor(float temperature) {
  if (temperature < TEMP_COMFORT_MIN)
    return TFT_BLUE;
  else if (temperature > TEMP_COMFORT_MAX)
    return TFT_RED;
  else
    return TFT_GREEN;
}

// Function to map humidity to color
uint16_t getHumidityColor(float humidity) {
  if (humidity < HUMIDITY_COMFORT_MIN || humidity > HUMIDITY_COMFORT_MAX)
    return TFT_YELLOW;
  else
    return TFT_GREEN;
}

// Function to map soil moisture to color
uint16_t getSoilMoistureColor(int soilMoisture) {
  if (soilMoisture > SOIL_MOISTURE_THRESHOLD)
    return TFT_RED;
  else
    return TFT_GREEN;
}

// Function to draw a status circle
void drawStatusCircle(int x, int y, const char *value, const char *label, uint16_t color) {
  tft.fillCircle(x, y, 30, color); // Draw filled circle
  tft.drawCircle(x, y, 30, TFT_WHITE); // Draw circle outline

  // Display the string value inside the circle
  tft.setCursor(x - 20, y - 10);
  tft.setTextColor(TFT_WHITE, color);
  tft.setTextSize(2);
  tft.print(value);

  // Display the label below the circle
  tft.setCursor(x - 20, y + 20);
  tft.setTextSize(1);
  tft.print(label);
}

// Function to draw the water level bar
void drawWaterLevelBar(int waterLevel) {
  int barWidth = map(waterLevel, 0, MAX_WATER_CYCLES, 0, 170); // Map water level to bar width
  tft.fillRect(0, 130, 170, 20, TFT_BLACK); // Clear previous bar
  tft.fillRect(0, 130, barWidth, 20, TFT_BLUE); // Draw the blue bar
  tft.drawRect(0, 130, 170, 20, TFT_WHITE); // Draw the bar outline

  // Display water level percentage
  tft.setCursor(10, 155);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.print("Water Level: ");
  tft.print(map(waterLevel, 0, MAX_WATER_CYCLES, 0, 100));
  tft.println("%");
}

void connectToWiFi() {
  const unsigned long WIFI_TIMEOUT = 10000; // 10 seconds timeout
  unsigned long startAttemptTime = millis();

  // Start WiFi connection
  WiFi.begin(ssid, password);
  Serial.println();
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  // Wait until WiFi is connected or timeout
  while (WiFi.status() != WL_CONNECTED && (millis() - startAttemptTime) < WIFI_TIMEOUT) {
    delay(500);
    Serial.print(".");
  }

  // Check if connected
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("MAC address: ");
    Serial.println(WiFi.macAddress());
  } else {
    Serial.println("\nWiFi connection failed. Continuing without WiFi...");
  }
}

// Automatic Watering Function
bool initialCheckDone = false; // Flag to ensure the initial check is performed only once

void manageWatering(int soilMoisture, int waterLevel, unsigned long currentMillis) {
  const unsigned long TWO_MINUTES = 120000; // 2 minutes in milliseconds

  // Ensure the device has been running for at least 2 minutes
  if (currentMillis - deviceStartTime < TWO_MINUTES) {
    Serial.println("Skipping watering: Device just started.");
    return; // Do nothing if the device is within the 2-minute startup period
  }

  // Only water if soil moisture is above the threshold and water is available
  if (soilMoisture > SOIL_MOISTURE_THRESHOLD && waterLevel > 0 &&
      (currentMillis - lastWateringTime >= WATERING_INTERVAL)) {
    myservo.write(90);    // Open valve
    delay(2000);          // Simulate watering for 2 seconds
    myservo.write(0);     // Close valve
    lastWateringTime = currentMillis; // Update last watering time
    waterCycleCount++;    // Increment water cycle count
    Serial.println("Scheduled watering triggered.");
  } else if (soilMoisture <= SOIL_MOISTURE_THRESHOLD) {
    Serial.println("Soil moisture is sufficient. No watering needed.");
  } else if (waterLevel <= 0) {
    Serial.println("Water level is too low. Unable to water the plant.");
  }
}

void manageLight() {
  unsigned long currentMillis = millis();

  // Reset totalLightTime every 24 hours
  if (currentMillis - lastResetTime >= ONE_DAY) {
    lastResetTime = currentMillis;
    totalLightTime = 0;
    Serial.println("Reset total light time for the past 24 hours.");
  }

  // Check natural light levels
  int lightLevel = analogRead(PHOTO_PIN); // Read light level from sensor
  if (lightLevel > LIGHT_THRESHOLD) {
    if (!artificialLightOn) {
      totalLightTime += currentMillis - lightStartTime;
    }
    lightStartTime = currentMillis; // Update light tracking
  }

  if (userControlled) {
    Serial.println("Manual override active. Skipping automatic light control.");
    return;
  }

  // Automatic light control
  if (!userControlled && totalLightTime < FOUR_HOURS) {
    if (!artificialLightOn) {
      digitalWrite(LIGHT_PIN, HIGH);
      artificialLightOn = true;
      lightStartTime = currentMillis; // Start timing artificial light
      Serial.println("Artificial light ON automatically (not enough light).");
    }
  }

  // Turn off artificial light after 4 hours (auto mode)
  if (artificialLightOn && (currentMillis - lightStartTime >= FOUR_HOURS)) {
    digitalWrite(LIGHT_PIN, LOW);
    artificialLightOn = false;
    userControlled = false; // Reset control mode
    totalLightTime += FOUR_HOURS;
    Serial.println("Artificial light OFF after 4 hours (automatic mode).");
  }
}

void sendDataToCloud(float temperature, float humidity, HttpClient &http) {
  // Convert temperature and humidity to strings
  std::string temp = std::to_string(temperature);
  std::string hum = std::to_string(humidity);

  // Create the request URL
  std::string request = "/update?hum=" + hum + "&temp=" + temp;

  // Send the HTTP GET request
  int err = http.get("54.152.93.88", 5000, request.c_str(), NULL);
  if (err == 0) {
    Serial.println("Started request successfully.");
    err = http.responseStatusCode();

    if (err >= 0) {
      Serial.print("Got status code: ");
      Serial.println(err);

      // Check for response headers
      err = http.skipResponseHeaders();
      if (err >= 0) {
        int bodyLen = http.contentLength();
        Serial.print("Content length is: ");
        Serial.println(bodyLen);
        Serial.println("Body returned follows:");

        // Print the response body
        unsigned long timeoutStart = millis();
        char c;
        while ((http.connected() || http.available()) &&
               ((millis() - timeoutStart) < kNetworkTimeout)) {
          if (http.available()) {
            c = http.read();
            Serial.print(c);
            bodyLen--;
            timeoutStart = millis(); // Reset the timeout counter
          } else {
            delay(kNetworkDelay); // Wait for data to arrive
          }
        }
      } else {
        Serial.print("Failed to skip response headers: ");
        Serial.println(err);
      }
    } else {
      Serial.print("Getting response failed: ");
      Serial.println(err);
    }
  } else {
    Serial.print("Connect failed: ");
    Serial.println(err);
  }

  // Stop the HTTP client
  http.stop();
}

bool notificationSent = false; // Flag to track if BLE notification has been sent

void checkWaterLevelAndNotify(int waterLevel) {
  // Send BLE notification when water level is low (<= 2)
  if (waterLevel <= 2 && !notificationSent) {
    Serial.println("Water level is low. Sending BLE notification...");
    std::string notificationMessage = "Water level is critically low. Please refill.";
    waterNotificationCharacteristic->setValue(notificationMessage);
    waterNotificationCharacteristic->notify(); // Notify via BLE
    notificationSent = true;                   // Mark notification as sent
  }

  // Reset notification flag if water level rises above 2
  if (waterLevel > 2) {
    notificationSent = false;
  }

  // Activate LED and buzzer when water is empty (level = 0)
  if (waterLevel == 0) {
    tone(BUZZER_PIN, 262);       // Buzzer ON
    digitalWrite(LED_PIN, HIGH); // LED ON
    tft.setCursor(0, 260);
    tft.println("REFILL WATER!");
    delay(1000);                 // Buzzer duration
    noTone(BUZZER_PIN);          // Turn off buzzer
  } else {
    digitalWrite(LED_PIN, LOW);  // Turn off LED when water is sufficient
  }
}

void setup() {
  Serial.begin(9600);

  // S3 TFT display setup
  // REMOVE THESE LINES IF USE TTGO
  pinMode(PIN_POWER_ON, OUTPUT);
  pinMode(PIN_LCD_BL, OUTPUT);
  digitalWrite(PIN_POWER_ON, HIGH);
  digitalWrite(PIN_LCD_BL, HIGH);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  // Sensors and actuators
  myservo.attach(SERVO_PIN);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(LIGHT_PIN, OUTPUT);
  // Initialize the DHT sensor
  Wire.begin(DHT_SDA_PIN, DHT_SCL_PIN);
  if(dht.begin() )
    Serial.println("Ready.");
  else { 
    Serial.println("Could not connect to Temperature and Humidity Sensor.");
    Serial.println("Freezing");
  }

  // Initialize BLE
  BLEDevice::init("Plant Controller");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);
  // Create LED control characteristic
  BLECharacteristic *PlantCharacteristic = pService->createCharacteristic(
      CONTROL_CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_WRITE
  );
  PlantCharacteristic->setCallbacks(new PLANTCallbacks());

  waterNotificationCharacteristic = pService->createCharacteristic(
    NOTIFICATION_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ
);

  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  BLEDevice::startAdvertising();
  Serial.println("BLE Control Server is Running");

  // Wi-Fi setup
  connectToWiFi();
}

PLANTCallbacks plantCallbacks;

void loop() {
  int err = 0;
  WiFiClient c;
  HttpClient http(c);

  dht.read();
  float temperature = dht.getTemperature();
  float humidity = dht.getHumidity();
  int soilMoisture = analogRead(SMS_PIN);
  unsigned long currentMillis = millis();

  // Calculate estimated water level
  int waterLevel = MAX_WATER_CYCLES - waterCycleCount;

  tft.fillScreen(TFT_BLACK);

  // Get colors for each parameter
  uint16_t tempColor = getTemperatureColor(temperature);
  uint16_t humidityColor = getHumidityColor(humidity);
  uint16_t soilMoistureColor = getSoilMoistureColor(soilMoisture);

  char tempBuffer[10];
  char humidBuffer[10];

  sprintf(tempBuffer, "%.1f", temperature); // Convert temperature to string
  sprintf(humidBuffer, "%.1f", humidity);   // Convert humidity to string
  const char *soilStatus = soilMoisture < SOIL_MOISTURE_THRESHOLD ? "Good" : "Dry";

  drawStatusCircle(50, 80, tempBuffer, "Temp", getTemperatureColor(temperature));
  drawStatusCircle(120, 80, humidBuffer, "Humid", getHumidityColor(humidity));
  drawStatusCircle(190, 80, soilStatus, "Soil", soilMoistureColor);

  // Draw the water level bar
  drawWaterLevelBar(waterLevel);

  // Check if readings are valid
  if (isnan(temperature) || isnan(humidity)) {
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0);
    tft.println("DHT sensor failed!");
    return;
  }

  // Collecting temperature and humidity data for Cloud
  sendDataToCloud(temperature, humidity, http);

  // Automatic light control
  manageLight();

  // Automatic watering
  manageWatering(soilMoisture, waterLevel, currentMillis);

  // Check if water container needs refilling
  checkWaterLevelAndNotify(waterLevel);

  delay(1000); // Wait before next reading
}
