// =================================================================
// IOT CLIKS TEAM - BOOTCAMP TETI 2025
// =================================================================
#define BLYNK_TEMPLATE_ID "TMPL69JUbbPLg"
#define BLYNK_TEMPLATE_NAME "SMART HEALTH MONITORING"
#define BLYNK_AUTH_TOKEN "KQpYvRbGu-YrqTalhF-4TEBOpx8mFHTn"
// =================================================================

#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <TimeLib.h>

// OLED Setup
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// MPU6050 Setup
const int MPU_ADDR = 0x68;
int16_t accelX, accelY, accelZ;

// DS18B20 Setup
OneWire oneWire(4);
DallasTemperature tempSensor(&oneWire);
bool tempSensorConnected = false;
float currentTempC = 0.0;

// Pulse Sensor Simulation
const int pulsePin = 34;
int heartRate = 0;

// Alarm Pins
const int ledPin = 15;
const int buzzerPin = 2;

// Blynk Auth and WiFi Credentials
char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "Wokwi-GUEST";
char pass[] = "";

// Blynk Virtual Pins
#define BLYNK_HEART_RATE V1
#define BLYNK_TEMPERATURE V2
#define BLYNK_FALL_ALERT V3
#define BLYNK_SWITCH V0      
#define BLYNK_TERMINAL V4    

// System State
enum SystemState {
  STATE_NORMAL,
  STATE_FALL_EMERGENCY,
  STATE_HR_WARNING,
  STATE_TEMP_WARNING,
  STATE_SYSTEM_OFF
};
SystemState currentState = STATE_NORMAL;
SystemState lastKnownState = STATE_NORMAL;

// Global Control Variables
bool systemActive = true;

// Thresholds
const float TEMP_LOW_THRESHOLD = 35.5;
const float TEMP_HIGH_THRESHOLD = 38.0;
const int HR_LOW_THRESHOLD = 60;
const int HR_HIGH_THRESHOLD = 100;
const long FALL_THRESHOLD = 30000;

// Timing Variables
unsigned long lastMPURead = 0;
unsigned long lastHRRead = 0;
unsigned long lastTempRead = 0;
unsigned long lastDisplayUpdate = 0;


BLYNK_WRITE(V0) {
  systemActive = param.asInt();
  String msg = systemActive ? "--- Sistem DIAKTIFKAN ---" : "--- Sistem DIMATIKAN ---";
  Blynk.virtualWrite(BLYNK_TERMINAL, "clr\n" + msg + "\n");
  Serial.println(msg);
  if (!systemActive) {
    digitalWrite(ledPin, LOW);
    digitalWrite(buzzerPin, LOW);
  }
}


BLYNK_CONNECTED() {
  Blynk.sendInternal("rtc", "sync"); 
}

// RTC CLOK
BLYNK_WRITE(InternalPinRTC) {
  setTime(param.asLong()); 
}


// Fungsi bantuan untuk mendapatkan string waktu yang diformat
String getTimeString() {
  
  if (year() > 2020) {
    char timeStr[9];
    // Format: HH:MM:SS
    sprintf(timeStr, "%02d:%02d:%02d", hour(), minute(), second());
    return String(timeStr);
  } else {
    return "Clock Sync...";
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED failed!");
    while(1);
  }

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);

  tempSensor.begin();
  tempSensorConnected = (tempSensor.getDeviceCount() > 0);
  Serial.println(tempSensorConnected ? "Conecting sucessfully....." : "WARNING: connecting eror.....");

  pinMode(ledPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  digitalWrite(buzzerPin, LOW);

  Blynk.begin(auth, ssid, pass);
  Blynk.virtualWrite(BLYNK_TERMINAL, "clr\n--- Sistem Online ---\nMenunggu sinkronisasi waktu...\n");
}

void loop() {
  Blynk.run();
  unsigned long currentMillis = millis();

  if (!systemActive) {
    currentState = STATE_SYSTEM_OFF;
    if (currentMillis - lastDisplayUpdate >= 500) {
      updateOLED();
      sendBlynkData();
    }
    return;
  }

  if (currentMillis - lastMPURead >= 100) {
    readMPU6050();
    lastMPURead = currentMillis;
  }

  if (currentMillis - lastHRRead >= 300) {
    heartRate = map(analogRead(pulsePin), 0, 4095, 50, 120);
    lastHRRead = currentMillis;
  }

  if (currentMillis - lastTempRead >= 2000) {
    if (tempSensorConnected) {
      tempSensor.requestTemperatures();
      currentTempC = tempSensor.getTempCByIndex(0);
      if (currentTempC == -127.00) tempSensorConnected = false;
    }
    lastTempRead = currentMillis;
  }

  determineSystemState();
  handleAlarms();
  
  if (currentMillis - lastDisplayUpdate >= 500) {
    updateOLED();
    sendBlynkData();
    lastDisplayUpdate = currentMillis;
  }
}

void readMPU6050() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 6, true);
  accelX = Wire.read() << 8 | Wire.read();
  accelY = Wire.read() << 8 | Wire.read();
  accelZ = Wire.read() << 8 | Wire.read();
}

void determineSystemState() {
  long totalAccel = sqrt(pow(accelX, 2) + pow(accelY, 2) + pow(accelZ, 2));
  if (totalAccel > FALL_THRESHOLD) {
    currentState = STATE_FALL_EMERGENCY;
    return;
  }
  
  if (tempSensorConnected && (currentTempC < TEMP_LOW_THRESHOLD || currentTempC > TEMP_HIGH_THRESHOLD)) {
    currentState = STATE_TEMP_WARNING;
    return;
  }
  
  if (heartRate < HR_LOW_THRESHOLD || heartRate > HR_HIGH_THRESHOLD) {
    currentState = STATE_HR_WARNING;
    return;
  }
  
  currentState = STATE_NORMAL;
}

void handleAlarms() {
  if (currentState == STATE_FALL_EMERGENCY) {
    digitalWrite(ledPin, HIGH);
    digitalWrite(buzzerPin, HIGH);
  } else if (currentState == STATE_TEMP_WARNING || currentState == STATE_HR_WARNING) {
    digitalWrite(ledPin, HIGH);
    digitalWrite(buzzerPin, LOW);
  } else {
    digitalWrite(ledPin, LOW);
    digitalWrite(buzzerPin, LOW);
  }
}

void sendBlynkData() {
  if(systemActive){
    Blynk.virtualWrite(BLYNK_HEART_RATE, heartRate);
    if (tempSensorConnected) {
      Blynk.virtualWrite(BLYNK_TEMPERATURE, currentTempC);
    }
  }

  if (currentState != lastKnownState) {
    String logMessage;
    String timestamp = "[" + getTimeString() + "] ";

    switch (currentState) {
        case STATE_FALL_EMERGENCY:
            logMessage = timestamp + "DARURAT: JATUH TERDETEKSI!";
            Blynk.logEvent("fall_alert", logMessage);
            Blynk.virtualWrite(BLYNK_FALL_ALERT, 1);
            break;
        case STATE_HR_WARNING:
            logMessage = timestamp + "PERINGATAN: Detak jantung abnormal: " + String(heartRate) + " BPM";
            Blynk.logEvent("health_warning", logMessage);
            break;
        case STATE_TEMP_WARNING:
            logMessage = timestamp + "PERINGATAN: Suhu tubuh abnormal: " + String(currentTempC, 1) + " C";
            Blynk.logEvent("health_warning", logMessage);
            break;
        case STATE_NORMAL:
            if(lastKnownState != STATE_NORMAL && lastKnownState != STATE_SYSTEM_OFF) {
              logMessage = timestamp + "Status kembali NORMAL.";
            }
            Blynk.virtualWrite(BLYNK_FALL_ALERT, 0);
            break;
        case STATE_SYSTEM_OFF:
            
            break;
    }

    if (logMessage.length() > 0) {
      Blynk.virtualWrite(BLYNK_TERMINAL, logMessage + "\n");
    }
    
    lastKnownState = currentState;
  }
}

void drawCenteredString(const String &text, int y, int textSize = 1) {
  display.setTextSize(textSize);
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, y);
  display.print(text);
}

void updateOLED() {
  display.clearDisplay();
  display.setTextColor(WHITE);

  switch (currentState) {
    case STATE_SYSTEM_OFF:
      drawCenteredString("SISTEM", 15, 2);
      drawCenteredString("DIMATIKAN", 35, 2);
      break;

    case STATE_NORMAL:
      drawCenteredString("HEALTH MONITOR", 0);
      display.drawLine(0, 10, 127, 10, WHITE);
      display.setCursor(0, 20);
      display.print("HR: "); display.print(heartRate); display.print(" BPM");
      display.setCursor(0, 35);
      display.print("Temp: ");
      if (tempSensorConnected) {
        display.print(currentTempC, 1); display.print((char)247); display.print("C");
      } else {
        display.print("N/A");
      }
      display.setCursor(0, 50);
      display.print("Status: NORMAL");
      break;

    case STATE_FALL_EMERGENCY:
      drawCenteredString("HEALTH MONITOR", 0);
      display.drawLine(0, 10, 127, 10, WHITE);
      drawCenteredString("! EMERGENCY !", 20, 1);
      drawCenteredString("FALL DETECTED", 40, 1);
      break;

    case STATE_HR_WARNING:
      drawCenteredString("HEALTH MONITOR", 0);
      display.drawLine(0, 10, 127, 10, WHITE);
      drawCenteredString("HR WARNING", 20, 1);
      display.setCursor(0, 35);
      display.print("Current: "); display.print(heartRate); display.print(" BPM");
      display.setCursor(0, 50);
      display.print("Normal: 60-100 BPM");
      break;

    case STATE_TEMP_WARNING:
      drawCenteredString("HEALTH MONITOR", 0);
      display.drawLine(0, 10, 127, 10, WHITE);
      drawCenteredString("TEMP WARNING", 20, 1);
      display.setCursor(0, 35);
      display.print("Current: "); display.print(currentTempC, 1); display.print((char)247); display.print("C");
      display.setCursor(0, 50);
      display.print("Normal: 35.5-38.0");display.print((char)247); display.print("C");
      break;
  }
  
  display.display();
}
