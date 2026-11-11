#include <Wire.h>
#include <math.h>
#include <HardwareSerial.h>
#include <TinyGPS++.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

// ==================================================
// PIN DEFINITIONS
// ==================================================

#define SDA_PIN 21
#define SCL_PIN 22
#define MPU 0x68

#define BUZZER 23

#define YELLOW_LED 32
#define GREEN_LED 33
#define RED_LED 25

#define GPS_RX 16
#define GPS_TX 17

#define SIM800_RX 19
#define SIM800_TX 18

#define SERVO1_PIN 27
#define SERVO2_PIN 26

// ==================================================
// WIFI SETTINGS
// ==================================================

const char* ssid = "Galaxy A16 5G 4068";
const char* password = "ladoogopal";

// ==================================================
// TELEGRAM SETTINGS
// ==================================================

#define BOT_TOKEN "8968437600:AAEwWJWNu9GUYr0MMnoSdD7h1X95_m4nlsY"
#define CHAT_ID "8993504044"

WiFiClientSecure telegramClient;
UniversalTelegramBot bot(BOT_TOKEN, telegramClient);

// ==================================================
// OBJECTS
// ==================================================

HardwareSerial GPS(2);
HardwareSerial SIM800(1);

TinyGPSPlus gps;

Servo servo1;
Servo servo2;

// ==================================================
// IMPACT SETTINGS
// ==================================================

// Configured location, not live GPS.
const double LOCATION_LAT = 30.681267;
const double LOCATION_LON = 76.604322;

#define IMPACT_START 19000
#define MEDIUM_LEVEL 30000
#define HIGH_LEVEL 40000

bool accidentDetected = false;

unsigned long cooldownStart = 0;
const unsigned long COOLDOWN = 5000;

// ==================================================
// MPU6050
// ==================================================

bool setupMPU() {
  Wire.beginTransmission(MPU);
  Wire.write(0x6B);
  Wire.write(0);

  return Wire.endTransmission() == 0;
}

float readMPU() {
  Wire.beginTransmission(MPU);
  Wire.write(0x3B);

  if (Wire.endTransmission(false) != 0) {
    return 0;
  }

  if (Wire.requestFrom(MPU, 6, true) != 6) {
    return 0;
  }

  int16_t ax = (Wire.read() << 8) | Wire.read();
  int16_t ay = (Wire.read() << 8) | Wire.read();
  int16_t az = (Wire.read() << 8) | Wire.read();

  return sqrt(
    (float)ax * ax +
    (float)ay * ay +
    (float)az * az
  );
}

// ==================================================
// BUZZER
// ==================================================

void buzzerOn() {
  ledcWrite(BUZZER, 128);
}

void buzzerOff() {
  ledcWrite(BUZZER, 0);
}

void soundAlarm(unsigned long duration) {
  buzzerOn();
  delay(duration);
  buzzerOff();
}

// ==================================================
// LOCATION
// ==================================================

String getLocationMessage() {
  String message = "UNIVERSITY / PROJECT LOCATION\n";
  message += "University: CGC University, Mohali\n";
  message += "Address: CGC University, Mohali, Punjab, India\n\n";

  if (gps.location.isValid() &&
      gps.location.age() < 10000) {

    double lat = gps.location.lat();
    double lon = gps.location.lng();

    message += "GPS Status: LIVE GPS FIX\n";
    message += "Latitude: " + String(lat, 6) + "\n";
    message += "Longitude: " + String(lon, 6) + "\n";

    message += "Google Maps: https://maps.google.com/?q=";
    message += String(lat, 6) + ",";
    message += String(lon, 6);

  } else {
    message += "GPS Status: NO LIVE GPS FIX\n";
    message += "Configured coordinates (NOT live GPS):\n";

    message += "Latitude: " + String(LOCATION_LAT, 6) + "\n";
    message += "Longitude: " + String(LOCATION_LON, 6) + "\n";

    message += "Google Maps: https://maps.google.com/?q=";
    message += String(LOCATION_LAT, 6) + ",";
    message += String(LOCATION_LON, 6);
  }

  return message;
}

void showLocation() {
  Serial.println("\n========== ACCIDENT LOCATION ==========");
  Serial.println(getLocationMessage());
  Serial.println("========================================");
}

// ==================================================
// TELEGRAM ALERT
// ==================================================

void sendTelegramAlert(String level) {
  Serial.println("\nSending Telegram alert...");

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("ERROR: Wi-Fi disconnected.");
    return;
  }

  String message;

  if (level == "LOW") {
    message = "LOW IMPACT ALERT";
  }
  else if (level == "MEDIUM") {
    message = "MEDIUM IMPACT ALERT";
  }
  else {
    message = "HIGH IMPACT ALERT";
  }

  message += "\n\nACCIDENT ALERT SYSTEM\n";
  message += "An impact has been detected by the ESP32.\n\n";
  message += "Impact Level: " + level + "\n";

  if (level == "LOW") {
    message += "Low-impact event detected.\n";
  }
  else if (level == "MEDIUM") {
    message += "Medium-impact event detected.\n";
    message += "Servo 1: Open command activated.\n";
  }
  else {
    message += "High-impact event detected.\n";
    message += "Both servos: Open commands activated.\n";
  }

  message += "\nPlease check the situation immediately.\n\n";
  message += "LOCATION DETAILS\n";
  message += getLocationMessage();
  message += "\n\nAutomated project alert. Verify the situation.";

  bool sent = bot.sendMessage(CHAT_ID, message, "");

  if (sent) {
    Serial.println("Telegram alert sent successfully.");
  }
  else {
    Serial.println("ERROR: Telegram message failed.");
  }
}

// ==================================================
// LOW IMPACT
// ==================================================

void lowImpact() {
  Serial.println("\n========== LOW IMPACT DETECTED ==========");

  digitalWrite(YELLOW_LED, HIGH);

  showLocation();

  soundAlarm(5000);

  digitalWrite(YELLOW_LED, LOW);

  sendTelegramAlert("LOW");

  Serial.println("Low impact alert completed.");
}

// ==================================================
// MEDIUM IMPACT
// ==================================================

void mediumImpact() {
  Serial.println("\n========== MEDIUM IMPACT DETECTED ==========");

  digitalWrite(GREEN_LED, HIGH);

  showLocation();

  Serial.println("Moving Servo 1 to 180 degrees...");

  servo1.write(180);

  soundAlarm(8000);

  digitalWrite(GREEN_LED, LOW);

  sendTelegramAlert("MEDIUM");

  Serial.println("Medium impact alert completed.");
}

// ==================================================
// HIGH IMPACT
// ==================================================

void highImpact() {
  Serial.println("\n========== HIGH IMPACT DETECTED ==========");

  digitalWrite(RED_LED, HIGH);

  showLocation();

  Serial.println("Moving Servo 1 to 180 degrees...");
  servo1.write(180);

  Serial.println("Moving Servo 2 to 180 degrees...");
  servo2.write(180);

  soundAlarm(10000);

  digitalWrite(RED_LED, LOW);

  sendTelegramAlert("HIGH");

  Serial.println("High impact alert completed.");
}

// ==================================================
// WIFI
// ==================================================

void connectWiFi() {
  Serial.println("\nConnecting to Wi-Fi...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Wi-Fi connected successfully.");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
  else {
    Serial.println("Wi-Fi connection failed.");
    Serial.println("Telegram alerts require internet.");
  }
}

// ==================================================
// SETUP
// ==================================================

void setup() {
  Serial.begin(115200);

  Wire.begin(SDA_PIN, SCL_PIN);

  // Initialize LEDs.
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);

  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, LOW);

  // Initialize both servos FIRST so they securely claim LEDC channels
  servo1.setPeriodHertz(50);
  servo2.setPeriodHertz(50);

  servo1.attach(SERVO1_PIN);
  servo2.attach(SERVO2_PIN);

  servo1.write(0);
  servo2.write(0);

  delay(1000);

  Serial.println("Servo 1 initialized on GPIO 27.");
  Serial.println("Servo 2 initialized on GPIO 26.");

  // Initialize buzzer AFTER servos
  ledcAttach(BUZZER, 4000, 8);
  buzzerOff();

  // Initialize GPS.
  GPS.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);

  // Keep SIM800L connection.
  SIM800.begin(9600, SERIAL_8N1, SIM800_RX, SIM800_TX);

  Serial.println("\n==================================");
  Serial.println(" ESP32 ACCIDENT ALERT SYSTEM");
  Serial.println(" Telegram Alert Mode");
  Serial.println("==================================");

  if (setupMPU()) {
    Serial.println("MPU6050 connected successfully.");
  }
  else {
    Serial.println("ERROR: MPU6050 not detected.");
  }

  connectWiFi();

  telegramClient.setInsecure();

  Serial.println("SIM800L UART initialized; SMS disabled.");
  Serial.println("System ready. Waiting for impact...");
}

// ==================================================
// MAIN LOOP
// ==================================================

void loop() {
  // Read incoming GPS data.
  while (GPS.available()) {
    gps.encode(GPS.read());
  }

  float acceleration = readMPU();

  // Prevent repeated alerts until the sensor returns below
  // the impact threshold after the cooldown.
  if (accidentDetected) {
    if (millis() - cooldownStart >= COOLDOWN &&
        acceleration > 0 &&
        acceleration < IMPACT_START) {

      accidentDetected = false;

      Serial.println("System rearmed. Waiting for impact...");
    }

    delay(20);
    return;
  }

  // Check highest impact first.
  if (acceleration >= HIGH_LEVEL) {
    accidentDetected = true;
    cooldownStart = millis();

    highImpact();
  }
  else if (acceleration >= MEDIUM_LEVEL) {
    accidentDetected = true;
    cooldownStart = millis();

    mediumImpact();
  }
  else if (acceleration >= IMPACT_START) {
    accidentDetected = true;
    cooldownStart = millis();

    lowImpact();
  }

  delay(50);
}


