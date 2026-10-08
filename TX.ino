// FUELGUARD -   Real-Time Forest Fire Monitoring System
// Developed by KALAM ELECTRONICS
// Date: 29.09.2026

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <DHT.h>
#include <TinyGPSPlus.h>

// =====================================================
// PIN DEFINITIONS
// =====================================================

#define DHT_PIN 4
#define DHT_TYPE DHT11

#define MQ2_PIN 34

#define GPS_RX_PIN 16
#define GPS_TX_PIN 17

// =====================================================
// OBJECTS
// =====================================================

DHT dht(DHT_PIN, DHT_TYPE);
TinyGPSPlus gps;

HardwareSerial GPSserial(2);

// =====================================================
// ESP-NOW CONFIGURATION
// =====================================================

// MUST MATCH ESP8266 RECEIVER Wi-Fi CHANNEL
#define ESPNOW_CHANNEL 11

// ESP8266 RECEIVER MAC
// F8:B3:B7:87:28:60
uint8_t RECEIVER_MAC[] = {
  0xF8,
  0xB3,
  0xB7,
  0x87,
  0x28,
  0x60
};

// =====================================================
// THRESHOLDS
// =====================================================

#define WARNING_TEMP 35.0
#define FIRE_TEMP 50.0

#define WARNING_SMOKE 1800
#define FIRE_SMOKE 2500

// =====================================================
// DATA STRUCTURE
// MUST EXACTLY MATCH ESP8266
// =====================================================

typedef struct __attribute__((packed)) {

  float temperature;
  float humidity;

  int smokeValue;

  float latitude;
  float longitude;

  bool gpsValid;

  uint8_t systemStatus;

  uint32_t packetNumber;

} FireData;

FireData fireData;

// =====================================================
// VARIABLES
// =====================================================

uint32_t packetNumber = 0;

unsigned long lastSendTime = 0;

#define SEND_INTERVAL 3000

// =====================================================
// DETERMINE STATUS
// =====================================================

uint8_t determineStatus(float temperature, int smoke) {

  if (temperature >= FIRE_TEMP ||
      smoke >= FIRE_SMOKE) {

    return 2;
  }

  if (temperature >= WARNING_TEMP ||
      smoke >= WARNING_SMOKE) {

    return 1;
  }

  return 0;
}

// =====================================================
// STATUS NAME
// =====================================================

const char* getStatusName(uint8_t status) {

  if (status == 0)
    return "NORMAL";

  if (status == 1)
    return "WARNING";

  if (status == 2)
    return "FIRE ALERT";

  return "UNKNOWN";
}

// =====================================================
// GPS READING
// =====================================================

void readGPS() {

  while (GPSserial.available()) {

    gps.encode(GPSserial.read());
  }
}

// =====================================================
// ESP-NOW SEND CALLBACK
// ESP32 CORE 2.0.11
// =====================================================

void OnDataSent(
  const uint8_t *mac_addr,
  esp_now_send_status_t status
) {

  Serial.print("ESP-NOW STATUS: ");

  if (status == ESP_NOW_SEND_SUCCESS) {

    Serial.println("SUCCESS");

  } else {

    Serial.println("FAILED");
  }
}

// =====================================================
// SEND SENSOR DATA
// =====================================================

void sendSensorData() {

  // ---------------------------------------------------
  // DHT11
  // ---------------------------------------------------

  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  if (isnan(temperature) ||
      isnan(humidity)) {

    Serial.println("DHT11 READ ERROR");

    return;
  }

  // ---------------------------------------------------
  // MQ-2
  // ---------------------------------------------------

  int smokeValue = analogRead(MQ2_PIN);

  // ---------------------------------------------------
  // GPS
  // ---------------------------------------------------

  readGPS();

  float latitude = 0.0;
  float longitude = 0.0;

  bool gpsValid = false;

  if (gps.location.isValid()) {

    latitude = gps.location.lat();
    longitude = gps.location.lng();

    gpsValid = true;
  }

  // ---------------------------------------------------
  // STATUS
  // ---------------------------------------------------

  uint8_t status =
    determineStatus(
      temperature,
      smokeValue
    );

  // ---------------------------------------------------
  // CREATE PACKET
  // ---------------------------------------------------

  fireData.temperature = temperature;

  fireData.humidity = humidity;

  fireData.smokeValue = smokeValue;

  fireData.latitude = latitude;

  fireData.longitude = longitude;

  fireData.gpsValid = gpsValid;

  fireData.systemStatus = status;

  fireData.packetNumber = ++packetNumber;

  // ---------------------------------------------------
  // SEND USING ESP-NOW
  // ---------------------------------------------------

  esp_err_t result = esp_now_send(
    RECEIVER_MAC,
    (uint8_t*)&fireData,
    sizeof(fireData)
  );

  // ---------------------------------------------------
  // SERIAL MONITOR
  // ---------------------------------------------------

  Serial.println();
  Serial.println("--------------------------------");
  Serial.println("FOREST FIRE TRANSMITTER");
  Serial.println("--------------------------------");

  Serial.print("Temperature : ");
  Serial.print(temperature, 1);
  Serial.println(" C");

  Serial.print("Humidity    : ");
  Serial.print(humidity, 1);
  Serial.println(" %");

  Serial.print("Smoke       : ");
  Serial.println(smokeValue);

  Serial.print("GPS         : ");

  if (gpsValid) {

    Serial.println("VALID");

    Serial.print("Latitude    : ");
    Serial.println(latitude, 6);

    Serial.print("Longitude   : ");
    Serial.println(longitude, 6);

  } else {

    Serial.println("NOT AVAILABLE");
  }

  Serial.print("Status      : ");
  Serial.println(getStatusName(status));

  Serial.print("Packet      : ");
  Serial.println(packetNumber);

  Serial.print("Receiver MAC: ");
  Serial.println("F8:B3:B7:87:28:60");

  Serial.print("Channel     : ");
  Serial.println(ESPNOW_CHANNEL);

  Serial.print("Send Queue  : ");

  if (result == ESP_OK) {

    Serial.println("OK");

  } else {

    Serial.println("FAILED");
  }

  Serial.println("--------------------------------");
}

// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println("================================");
  Serial.println(" ESP32 FOREST FIRE TRANSMITTER");
  Serial.println("================================");

  // ---------------------------------------------------
  // DHT11
  // ---------------------------------------------------

  dht.begin();

  // ---------------------------------------------------
  // MQ2
  // ---------------------------------------------------

  pinMode(MQ2_PIN, INPUT);

  // ---------------------------------------------------
  // GPS
  // ---------------------------------------------------

  GPSserial.begin(
    9600,
    SERIAL_8N1,
    GPS_RX_PIN,
    GPS_TX_PIN
  );

  // ---------------------------------------------------
  // WIFI STATION MODE
  // ---------------------------------------------------

  WiFi.mode(WIFI_STA);

  delay(100);

  Serial.println();
  Serial.print("ESP32 MAC: ");
  Serial.println(WiFi.macAddress());

  // ---------------------------------------------------
  // SET ESP-NOW CHANNEL
  // ---------------------------------------------------

  Serial.print("Setting ESP-NOW Channel: ");
  Serial.println(ESPNOW_CHANNEL);

  esp_err_t channelResult = esp_wifi_set_channel(
    ESPNOW_CHANNEL,
    WIFI_SECOND_CHAN_NONE
  );

  if (channelResult == ESP_OK) {

    Serial.println("Channel set successfully");

  } else {

    Serial.print("Channel set failed: ");
    Serial.println(channelResult);
  }

  // ---------------------------------------------------
  // ESP-NOW INIT
  // ---------------------------------------------------

  if (esp_now_init() != ESP_OK) {

    Serial.println("ESP-NOW INIT FAILED");

    while (true) {

      delay(1000);
    }
  }

  Serial.println("ESP-NOW INITIALIZED");

  // ---------------------------------------------------
  // REGISTER SEND CALLBACK
  // ---------------------------------------------------

  esp_now_register_send_cb(OnDataSent);

  // ---------------------------------------------------
  // ADD ESP8266 RECEIVER
  // ---------------------------------------------------

  esp_now_peer_info_t peerInfo = {};

  memcpy(
    peerInfo.peer_addr,
    RECEIVER_MAC,
    6
  );

  peerInfo.channel = ESPNOW_CHANNEL;

  peerInfo.encrypt = false;

  peerInfo.ifidx = WIFI_IF_STA;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {

    Serial.println("PEER ADD FAILED");

    while (true) {

      delay(1000);
    }
  }

  Serial.println("ESP8266 PEER ADDED");

  // ---------------------------------------------------
  // SYSTEM INFORMATION
  // ---------------------------------------------------

  Serial.println();
  Serial.println("================================");
  Serial.println("SYSTEM READY");
  Serial.println("================================");

  Serial.print("ESP32 MAC     : ");
  Serial.println(WiFi.macAddress());

  Serial.println("Receiver MAC  : F8:B3:B7:87:28:60");

  Serial.print("ESP-NOW CH    : ");
  Serial.println(ESPNOW_CHANNEL);

  Serial.println("Send Interval : 3 seconds");

  Serial.println("================================");
}

// =====================================================
// LOOP
// =====================================================

void loop() {

  // ---------------------------------------------------
  // KEEP GPS PROCESSING
  // ---------------------------------------------------

  readGPS();

  // ---------------------------------------------------
  // SEND EVERY 3 SECONDS
  // ---------------------------------------------------

  if (millis() - lastSendTime >= SEND_INTERVAL) {

    lastSendTime = millis();

    sendSensorData();
  }

  delay(10);
}
