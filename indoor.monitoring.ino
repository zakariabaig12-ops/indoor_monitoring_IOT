/*
  Project: Indoor Temperature & Humidity Monitor + Motion 
  Board: Arduino UNO R4 WiFi
  Sensors: DHT11 (Temp/Humidity) + PIR (Motion)
  Cloud: ThingSpeak 

  What it does:
  - Reads temperature & humidity from DHT11
  - Reads motion from PIR (0/1)
  - Makes a simple on-device decision : Normal / High Humidity
  - Sends readings to ThingSpeak over WiFi
*/

#include <WiFiS3.h>
#include <ThingSpeak.h>
#include <DHT.h>


// WiFi details
const char* homeNetwork = "ssid";
const char* homePassword = "password";

// ThingSpeak info
const unsigned long myChannelId = 3224454;
const char* myWriteKey = "YOSHKL2YFWHCYVW8";


// SENSOR SETUP

#define DHT_DATA_PIN 2     // DHT11 OUT to D2
#define DHT_SENSOR_TYPE DHT11
#define PIR_PIN 3          // PIR OUT to D3

DHT dht(DHT_DATA_PIN, DHT_SENSOR_TYPE);

// talk to ThingSpeak
WiFiClient netClient;

// humidity threshold
const float HUMIDITY_WARNING = 60.0;

// ThingSpeak rate limit 
const unsigned long uploadIntervalMs = 20000;
unsigned long lastUploadTime = 0;



void connectToWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(homeNetwork);

  WiFi.begin(homeNetwork, homePassword);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }

  Serial.println("\nWiFi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void ensureWiFiIsUp() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi lost. Reconnecting...");
    connectToWiFi();
  }
}


// READ SENSOR

bool readDht11(float &tempC, float &humPct) {
  tempC = dht.readTemperature();
  humPct = dht.readHumidity();

  if (isnan(tempC) || isnan(humPct)) {
    return false;
  }
  return true;
}

const char* classifyHumidity(float humPct) {
  if (humPct >= HUMIDITY_WARNING) {
    return "High Humidity";
  }
  return "Normal";
}


// SEND TO THINGSPEAK

int uploadToThingSpeak(float tempC, float humPct, int motionDetected) {
  ThingSpeak.setField(1, tempC);
  ThingSpeak.setField(2, humPct);
  ThingSpeak.setField(3, motionDetected);   // Field 3 = Motion (0/1)

  // edge status 
  const char* statusLabel = classifyHumidity(humPct);
  ThingSpeak.setStatus(statusLabel);

  return ThingSpeak.writeFields(myChannelId, myWriteKey);
}



void setup() {
  Serial.begin(9600);
  delay(1000);

  Serial.println("Starting DHT11 + PIR + ThingSpeak project...");
  dht.begin();

  pinMode(PIR_PIN, INPUT);

  connectToWiFi();
  ThingSpeak.begin(netClient);

  Serial.println("Ready.");
}

void loop() {
  ensureWiFiIsUp();

  if (millis() - lastUploadTime < uploadIntervalMs) {
    return;
  }
  lastUploadTime = millis();

  float temperatureC = 0;
  float humidityPct = 0;

  // Read DHT11
  if (!readDht11(temperatureC, humidityPct)) {
    Serial.println("Sensor read failed (DHT11). Check wiring/pin order.");
    return;
  }

  // Read PIR (1 = motion, 0 = no motion)
  int motionDetected = digitalRead(PIR_PIN);

  // Edge decision
  const char* statusNow = classifyHumidity(humidityPct);

  // Print to Serial
  Serial.print("Temp: ");
  Serial.print(temperatureC);
  Serial.print(" C | Hum: ");
  Serial.print(humidityPct);
  Serial.print(" % | Edge Status: ");
  Serial.print(statusNow);

  Serial.print(" | Motion: ");
  Serial.println(motionDetected ? "DETECTED" : "NONE");

  // Upload to ThingSpeak
  int httpCode = uploadToThingSpeak(temperatureC, humidityPct, motionDetected);

  Serial.print("ThingSpeak HTTP code: ");
  Serial.println(httpCode);

  if (httpCode == 200) {
    Serial.println("Upload OK");
  } else {
    Serial.println("Upload failed");
  }
}
