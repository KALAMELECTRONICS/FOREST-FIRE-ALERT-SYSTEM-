// FUELGUARD -   Real-Time Forest Fire Monitoring System
// Developed by KALAM ELECTRONICS
// Date: 29.09.2026



#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <espnow.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFiClientSecureBearSSL.h>
#include <ESP8266HTTPClient.h>

const char* TELEGRAM_BOT_TOKEN = "8546912170:AAGx4XMjSNpWHfEF84IAko3nbBZmpyO81zk";
const char* TELEGRAM_CHAT_ID   = "7125131809";

const char* WIFI_SSID     = "Kalam_004";
const char* WIFI_PASSWORD = "kalam@202606";

#define GREEN_LED  D8
#define YELLOW_LED D3
#define RED_LED    D7
#define BUZZER_PIN D6

#define WARNING_TEMP  35.0
#define FIRE_TEMP     38.0
#define WARNING_SMOKE 1800
#define FIRE_SMOKE    2500

#define DEFAULT_GPS_LATITUDE  11.1004301
#define DEFAULT_GPS_LONGITUDE 77.0266116
const char* DEFAULT_GPS_NAME = "SNS College of Technology, Coimbatore";

bool telegramEnabled = true;
bool telegramPending = false;
uint8_t telegramPendingStatus = 0;
String telegramPendingMessage = "";
uint8_t lastTelegramStatus = 0;
bool telegramStatusInitialized = false;
bool telegramConnected = false;

const unsigned long TELEGRAM_RETRY_INTERVAL = 10000UL;
const unsigned long TELEGRAM_ALERT_COOLDOWN = 60000UL;
const uint8_t TELEGRAM_MAX_RETRIES = 5;

unsigned long telegramNextRetry = 0;
unsigned long telegramLastSentWarning = 0;
unsigned long telegramLastSentFire = 0;
uint8_t telegramRetryCount = 0;
String telegramLastResult = "NOT TESTED";
unsigned long telegramLastAttempt = 0;
unsigned long telegramLastSuccess = 0;

LiquidCrystal_I2C lcd(0x27, 16, 2);
ESP8266WebServer server(80);

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

FireData receivedData;

bool transmitterConnected = false;
unsigned long lastPacketTime = 0;
unsigned long lastLCDUpdate = 0;
unsigned long lastWarningBeep = 0;
bool warningBuzzerState = false;
uint8_t lcdPage = 0;

void queueTelegramAlert(uint8_t status);
String buildTelegramAlert(uint8_t status);

float getDisplayLatitude() {
  return receivedData.gpsValid ? receivedData.latitude : DEFAULT_GPS_LATITUDE;
}

float getDisplayLongitude() {
  return receivedData.gpsValid ? receivedData.longitude : DEFAULT_GPS_LONGITUDE;
}

String getGoogleMapsUrl() {
  return "https://www.google.com/maps/search/?api=1&query=" +
         String(getDisplayLatitude(), 6) + "," +
         String(getDisplayLongitude(), 6);
}

uint8_t getCurrentStatus() {
  if (receivedData.temperature >= FIRE_TEMP ||
      receivedData.smokeValue >= FIRE_SMOKE) {
    return 2;
  }
  if (receivedData.temperature >= WARNING_TEMP ||
      receivedData.smokeValue >= WARNING_SMOKE) {
    return 1;
  }
  return 0;
}

String getStatusName(uint8_t status) {
  if (status == 0) return "NORMAL";
  if (status == 1) return "WARNING";
  if (status == 2) return "FIRE ALERT";
  return "UNKNOWN";
}

bool isTelegramConfigured() {
  String token = String(TELEGRAM_BOT_TOKEN);
  String chat  = String(TELEGRAM_CHAT_ID);
  if (token.startsWith("PASTE_")) return false;
  if (token.length() < 30) return false;
  if (token.indexOf(':') < 0) return false;
  if (chat.startsWith("PASTE_")) return false;
  if (chat.length() < 3) return false;
  return true;
}

String jsonEscape(String s) {
  s.replace("\\", "\\\\");
  s.replace("\"", "\\\"");
  s.replace("\n", " ");
  s.replace("\r", " ");
  return s;
}

void onDataReceive(uint8_t* mac, uint8_t* data, uint8_t len) {
  if (len != sizeof(FireData)) {
    Serial.print("Invalid packet size: ");
    Serial.println(len);
    return;
  }

  memcpy(&receivedData, data, sizeof(FireData));
  lastPacketTime = millis();
  transmitterConnected = true;

  uint8_t newStatus = getCurrentStatus();

  if (!telegramStatusInitialized) {
    lastTelegramStatus = newStatus;
    telegramStatusInitialized = true;
    if (newStatus == 1 || newStatus == 2) {
      queueTelegramAlert(newStatus);
    }
  } else if (newStatus != lastTelegramStatus) {
    if (newStatus == 1 || newStatus == 2) {
      queueTelegramAlert(newStatus);
    }
    lastTelegramStatus = newStatus;
  }

  Serial.println();
  Serial.println("==============================");
  Serial.println("ESP-NOW DATA RECEIVED");
  Serial.println("==============================");
  Serial.print("Temperature: ");
  Serial.print(receivedData.temperature);
  Serial.println(" C");
  Serial.print("Humidity: ");
  Serial.print(receivedData.humidity);
  Serial.println(" %");
  Serial.print("Smoke: ");
  Serial.println(receivedData.smokeValue);
  Serial.print("Status: ");
  Serial.println(getStatusName(getCurrentStatus()));
  Serial.print("Packet: ");
  Serial.println(receivedData.packetNumber);

  if (receivedData.gpsValid) {
    Serial.print("Latitude: ");
    Serial.println(receivedData.latitude, 6);
    Serial.print("Longitude: ");
    Serial.println(receivedData.longitude, 6);
  } else {
    Serial.println("GPS: NO FIX");
  }
  Serial.println("==============================");
}

void updateOutputs() {
  if (!transmitterConnected || millis() - lastPacketTime > 20000) {
    transmitterConnected = false;
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(YELLOW_LED, HIGH);
    digitalWrite(RED_LED, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    warningBuzzerState = false;
    return;
  }

  uint8_t status = getCurrentStatus();

  if (status == 2) {
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(YELLOW_LED, LOW);
    digitalWrite(RED_LED, HIGH);
    digitalWrite(BUZZER_PIN, HIGH);
    warningBuzzerState = false;
    return;
  }

  if (status == 1) {
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(YELLOW_LED, HIGH);
    digitalWrite(RED_LED, LOW);
    if (millis() - lastWarningBeep >= 500) {
      lastWarningBeep = millis();
      warningBuzzerState = !warningBuzzerState;
      digitalWrite(BUZZER_PIN, warningBuzzerState);
    }
    return;
  }

  digitalWrite(GREEN_LED, HIGH);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(RED_LED, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  warningBuzzerState = false;
}

void showTemperatureLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("TEMP:");
  lcd.print(receivedData.temperature, 1);
  lcd.print("C");
  lcd.setCursor(0, 1);
  lcd.print("HUM:");
  lcd.print(receivedData.humidity, 1);
  lcd.print("%");
}

void showSmokeLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SMOKE:");
  lcd.print(receivedData.smokeValue);
  lcd.setCursor(0, 1);
  lcd.print(getStatusName(getCurrentStatus()));
}

void showPacketLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("STATUS:");
  lcd.print(getStatusName(getCurrentStatus()));
  lcd.setCursor(0, 1);
  lcd.print("PKT:");
  lcd.print(receivedData.packetNumber);
}

void showGPSLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("LAT:");
  lcd.print(getDisplayLatitude(), 3);
  lcd.setCursor(0, 1);
  lcd.print("LON:");
  lcd.print(getDisplayLongitude(), 3);
}

void updateLCD() {
  if (millis() - lastLCDUpdate < 3000) return;
  lastLCDUpdate = millis();

  if (!transmitterConnected) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("TRANSMITTER");
    lcd.setCursor(0, 1);
    lcd.print("DISCONNECTED");
    return;
  }

  if (lcdPage == 0)      showTemperatureLCD();
  else if (lcdPage == 1) showSmokeLCD();
  else if (lcdPage == 2) showPacketLCD();
  else                   showGPSLCD();

  lcdPage++;
  if (lcdPage > 3) lcdPage = 0;
}

String telegramUrlEncode(const String &text) {
  String encoded = "";
  encoded.reserve(text.length() * 2);

  for (unsigned int i = 0; i < text.length(); i++) {
    char c = text.charAt(i);

    if ((c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') ||
        c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
    } else if (c == ' ') {
      encoded += "%20";
    } else {
      char hex[4];
      sprintf(hex, "%%%02X", (unsigned char)c);
      encoded += hex;
    }
  }
  return encoded;
}

bool isTelegramStatusOnCooldown(uint8_t status) {
  unsigned long now = millis();

  if (status == 1 && telegramLastSentWarning != 0 &&
      now - telegramLastSentWarning < TELEGRAM_ALERT_COOLDOWN) {
    return true;
  }
  if (status == 2 && telegramLastSentFire != 0 &&
      now - telegramLastSentFire < TELEGRAM_ALERT_COOLDOWN) {
    return true;
  }
  return false;
}

String buildTelegramAlert(uint8_t status) {
  String message = "FOREST GUARD ALERT\n\n";

  if (status == 1)      message += "STATUS: WARNING\n";
  else if (status == 2) message += "STATUS: FIRE ALERT\n";
  else                  message += "STATUS: NORMAL\n";

  message += "Temperature: " + String(receivedData.temperature, 1) + " C\n";
  message += "Humidity: " + String(receivedData.humidity, 1) + " %\n";
  message += "Smoke: " + String(receivedData.smokeValue) + "\n";
  message += "Packet: " + String(receivedData.packetNumber) + "\n";
  message += "Location: " + String(DEFAULT_GPS_NAME) + "\n";
  message += "Latitude: " + String(getDisplayLatitude(), 6) + "\n";
  message += "Longitude: " + String(getDisplayLongitude(), 6) + "\n";
  message += "Maps: " + getGoogleMapsUrl() + "\n";
  return message;
}

void queueTelegramAlert(uint8_t status) {
  if (!telegramEnabled || (status != 1 && status != 2)) return;

  if (isTelegramStatusOnCooldown(status)) {
    Serial.print("TELEGRAM: ");
    Serial.print(getStatusName(status));
    Serial.println(" alert suppressed by cooldown");
    return;
  }

  telegramPendingStatus = status;
  telegramPendingMessage = buildTelegramAlert(status);
  telegramPending = true;
  telegramRetryCount = 0;
  telegramNextRetry = 0;

  Serial.print("TELEGRAM: ALERT QUEUED -> ");
  Serial.println(getStatusName(status));
}

bool sendTelegramMessage(const String &message) {
  telegramLastAttempt = millis();

  if (!isTelegramConfigured()) {
    telegramConnected = false;
    telegramLastResult = "CONFIGURATION MISSING (set new bot token)";
    Serial.println("TELEGRAM: BOT TOKEN/CHAT ID NOT CONFIGURED");
    return false;
  }

  if (WiFi.status() != WL_CONNECTED) {
    telegramConnected = false;
    telegramLastResult = "WIFI OFFLINE";
    Serial.println("TELEGRAM: WIFI OFFLINE");
    return false;
  }

  uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t maxBlock = ESP.getMaxFreeBlockSize();
  Serial.print("TELEGRAM: free heap ");
  Serial.print(freeHeap);
  Serial.print(", max block ");
  Serial.println(maxBlock);

  BearSSL::WiFiClientSecure client;
  client.setInsecure();
  client.setBufferSizes(1024, 512);
  client.setTimeout(8000);

  HTTPClient https;
  https.setTimeout(8000);
  https.setReuse(false);

  String url = "https://api.telegram.org/bot";
  url += TELEGRAM_BOT_TOKEN;
  url += "/sendMessage?chat_id=";
  url += TELEGRAM_CHAT_ID;
  url += "&text=";
  url += telegramUrlEncode(message);

  Serial.println("TELEGRAM: CONNECTING...");

  if (!https.begin(client, url)) {
    telegramConnected = false;
    telegramLastResult = "HTTPS BEGIN FAILED";
    Serial.println("TELEGRAM: HTTPS BEGIN FAILED");
    return false;
  }

  int httpCode = https.GET();

  if (httpCode > 0) {
    String response = https.getString();

    Serial.print("TELEGRAM HTTP CODE: ");
    Serial.println(httpCode);

    if (httpCode == HTTP_CODE_OK && response.indexOf("\"ok\":true") >= 0) {
      telegramConnected = true;
      telegramLastSuccess = millis();
      telegramLastResult = "CONNECTED / MESSAGE SENT";
      Serial.println("TELEGRAM: MESSAGE SENT SUCCESSFULLY");
      https.end();
      return true;
    }

    telegramConnected = false;
    String hint = "";
    if (httpCode == 401) hint = " (wrong/revoked token)";
    else if (httpCode == 400) hint = " (check chat id)";
    else if (httpCode == 403) hint = " (press START on the bot)";
    else if (httpCode == 404) hint = " (token format wrong)";
    telegramLastResult = "HTTP " + String(httpCode) + hint + ": " + response;

    Serial.print("TELEGRAM RESPONSE: ");
    Serial.println(response);
  } else {
    telegramConnected = false;
    int sslErr = client.getLastSSLError();
    telegramLastResult = "HTTPS ERROR " + https.errorToString(httpCode) +
                         " | SSL " + String(sslErr) +
                         " | MAXBLOCK " + String(maxBlock);

    Serial.print("TELEGRAM HTTPS ERROR: ");
    Serial.println(https.errorToString(httpCode));
    Serial.print("TELEGRAM SSL ERROR: ");
    Serial.println(sslErr);
  }

  https.end();
  return false;
}

void processTelegram() {
  if (!telegramEnabled || !telegramPending) return;

  unsigned long now = millis();
  if ((long)(now - telegramNextRetry) < 0) return;

  uint8_t statusToSend = telegramPendingStatus;

  if (statusToSend != 1 && statusToSend != 2) {
    telegramPending = false;
    telegramPendingMessage = "";
    return;
  }

  uint8_t currentStatus = getCurrentStatus();
  if (currentStatus == 2 && statusToSend == 1) {
    statusToSend = 2;
    telegramPendingStatus = 2;
    telegramPendingMessage = buildTelegramAlert(2);
  }

  Serial.print("TELEGRAM: SEND ATTEMPT ");
  Serial.print(telegramRetryCount + 1);
  Serial.print("/");
  Serial.println(TELEGRAM_MAX_RETRIES);

  bool ok = sendTelegramMessage(telegramPendingMessage);

  if (ok) {
    telegramPending = false;
    telegramPendingMessage = "";
    telegramRetryCount = 0;
    telegramNextRetry = 0;

    if (statusToSend == 1) telegramLastSentWarning = millis();
    else                   telegramLastSentFire = millis();

    Serial.println("TELEGRAM: ALERT DELIVERED");
    return;
  }

  telegramRetryCount++;

  if (telegramRetryCount >= TELEGRAM_MAX_RETRIES) {
    telegramRetryCount = 0;
    telegramNextRetry = millis() + 60000UL;
    Serial.println("TELEGRAM: MAX RETRIES - RETRYING IN 60 SECONDS");
  } else {
    telegramNextRetry = millis() + TELEGRAM_RETRY_INTERVAL;
    Serial.println("TELEGRAM: SEND FAILED - WILL RETRY");
  }
}

void handleTelegramTest() {
  String message =
    "FOREST GUARD TEST MESSAGE\n\n"
    "Telegram connection test successful.\n"
    "ESP8266 Receiver is online.\n"
    "IP: " + WiFi.localIP().toString();

  bool ok = sendTelegramMessage(message);

  if (ok) {
    server.send(200, "application/json",
                "{\"ok\":true,\"message\":\"Test message sent successfully\"}");
  } else {
    String json = "{\"ok\":false,\"message\":\"" + jsonEscape(telegramLastResult) + "\"}";
    server.send(200, "application/json", json);
  }
}

void handleTelegramStatus() {
  String json = "{";
  json += "\"connected\":";
  json += telegramConnected ? "true" : "false";
  json += ",\"enabled\":";
  json += telegramEnabled ? "true" : "false";
  json += ",\"result\":\"" + jsonEscape(telegramLastResult) + "\"";
  json += ",\"lastSuccess\":" + String(telegramLastSuccess);
  json += ",\"pending\":";
  json += telegramPending ? "true" : "false";
  json += ",\"configured\":";
  json += isTelegramConfigured() ? "true" : "false";
  json += "}";
  server.send(200, "application/json", json);
}

const char PAGE_HEAD[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Forest Guard | Monitoring Dashboard</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
:root{--blue:#1769e0;--ink:#172033;--line:#e5eaf1;--green:#16a36a;--yellow:#e09b00;--red:#dc3545}
body{font-family:Inter,Segoe UI,Arial,sans-serif;background:#f4f7fb;color:var(--ink);min-height:100vh}
.header{background:#fff;border-bottom:1px solid var(--line);padding:18px 5%;display:flex;align-items:center;justify-content:space-between;position:sticky;top:0;z-index:20;box-shadow:0 2px 14px rgba(22,35,60,.05)}
.brand{display:flex;align-items:center;gap:13px}
.logo{width:44px;height:44px;border-radius:12px;display:grid;place-items:center;background:linear-gradient(135deg,#1769e0,#08a7c9);color:#fff;font-weight:800;box-shadow:0 7px 18px rgba(23,105,224,.22)}
.brand h1{font-size:19px;letter-spacing:1px}
.brand p{color:#8390a3;font-size:10px;margin-top:4px;letter-spacing:1.2px}
.system-pill{display:flex;align-items:center;gap:8px;border:1px solid #dce5ef;background:#f8fbff;color:#516176;border-radius:999px;padding:9px 14px;font-size:11px;font-weight:700}
.dot{width:8px;height:8px;border-radius:50%;background:#16a36a;box-shadow:0 0 0 5px rgba(22,163,106,.10);animation:pulse 2s infinite}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:.45}}
.container{width:90%;max-width:1450px;margin:auto;padding:28px 0 45px}
.status{position:relative;overflow:hidden;border-radius:24px;padding:28px;margin-bottom:20px;border:1px solid #dfe6ef;background:#fff;box-shadow:0 8px 28px rgba(28,43,70,.07)}
.status.normal{border-left:6px solid var(--green)}
.status.warning{border-left:6px solid var(--yellow)}
.status.fire{border-left:6px solid var(--red)}
.status-inner{display:flex;align-items:center;justify-content:space-between;gap:20px}
.status-title{display:flex;align-items:center;gap:15px}
.status-icon{width:58px;height:58px;border-radius:17px;display:grid;place-items:center;font-weight:900;font-size:21px;background:#edf5ff;color:var(--blue)}
.status.warning .status-icon{background:#fff7df;color:#c88900}
.status.fire .status-icon{background:#fff0f1;color:var(--red);animation:alertPulse 1s infinite}
@keyframes alertPulse{50%{transform:scale(1.06);box-shadow:0 0 0 9px rgba(220,53,69,.08)}}
.status h2{font-size:30px;letter-spacing:1px}
.status p{color:#78869a;font-size:11px;margin-top:6px;letter-spacing:.7px}
.status-time{text-align:right;color:#8a96a8;font-size:10px;letter-spacing:1px}
.status-time strong{display:block;color:#26354b;font-size:13px;margin-top:5px}
.grid{display:grid;grid-template-columns:repeat(3,1fr);gap:17px;margin-bottom:20px}
.card{background:#fff;border:1px solid var(--line);border-radius:19px;padding:21px;box-shadow:0 6px 20px rgba(28,43,70,.055)}
.card-top{display:flex;align-items:center;justify-content:space-between}
.label{color:#748197;font-size:10px;font-weight:800;text-transform:uppercase;letter-spacing:1.2px}
.icon{width:38px;height:38px;border-radius:11px;display:grid;place-items:center;background:#edf5ff;color:var(--blue);font-weight:900}
.value{margin-top:19px;font-size:37px;font-weight:800;letter-spacing:-1px}
.unit{color:#7b8798;font-size:13px;font-weight:600}
.badge{display:inline-block;margin-top:12px;padding:5px 9px;border-radius:7px;background:#f2f6fa;color:#647286;font-size:9px;font-weight:800;letter-spacing:.8px}
.main-grid{display:grid;grid-template-columns:1.15fr .85fr;gap:20px;margin-bottom:20px}
.panel{background:#fff;border:1px solid var(--line);border-radius:20px;overflow:hidden;box-shadow:0 6px 20px rgba(28,43,70,.055)}
.panel-head{padding:18px 21px;border-bottom:1px solid #edf0f4;display:flex;align-items:center;justify-content:space-between}
.panel-title{font-size:13px;font-weight:800;letter-spacing:.4px}
.panel-sub{color:#8995a6;font-size:9px;margin-top:4px;letter-spacing:.8px}
.live-tag{padding:6px 9px;border-radius:7px;background:#eaf8f2;color:#13945f;font-size:9px;font-weight:800;letter-spacing:.8px}
.rows{padding:5px 21px 14px}
.row{min-height:52px;display:flex;align-items:center;justify-content:space-between;border-bottom:1px solid #edf0f4;font-size:12px}
.row:last-child{border-bottom:0}
.row span{color:#7a8799}
.row strong{font-size:12px}
.good{color:var(--green)}
.bad{color:var(--red)}
.gps{padding-bottom:20px;margin-bottom:20px}
.gps-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:13px;padding:20px}
.gps-box{border:1px solid #e5ebf2;border-radius:14px;background:#f8fafd;padding:17px}
.gps-box span{display:block;color:#8390a2;font-size:9px;font-weight:800;letter-spacing:1px;text-transform:uppercase;margin-bottom:8px}
.gps-box strong{font-size:15px}
.maplink{display:block;margin:0 20px;text-align:center;text-decoration:none;background:linear-gradient(135deg,#1769e0,#0ea5c9);color:#fff;padding:12px 16px;border-radius:10px;font-size:11px;font-weight:800;letter-spacing:.5px}
.telegram{margin-bottom:20px}
.telegram-body{padding:20px 21px}
.telegram-row{display:flex;align-items:center;justify-content:space-between;gap:15px;flex-wrap:wrap}
.telegram-status{display:flex;align-items:center;gap:10px;font-size:12px;font-weight:800}
.telegram-dot{width:11px;height:11px;border-radius:50%;background:#b7c0cb}
.telegram-dot.connected{background:#16a36a;box-shadow:0 0 0 6px rgba(22,163,106,.10)}
.telegram-dot.disconnected{background:#dc3545;box-shadow:0 0 0 6px rgba(220,53,69,.08)}
button{border:0;border-radius:10px;padding:11px 16px;background:linear-gradient(135deg,#1769e0,#0ea5c9);color:#fff;font-size:10px;font-weight:800;letter-spacing:.5px;cursor:pointer}
button:disabled{opacity:.55;cursor:wait}
.telegram-result{margin-top:12px;color:#7d899b;font-size:10px;word-break:break-word}
.threshold{padding:21px;margin-bottom:20px}
.threshold-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:13px;margin-top:17px}
.threshold-box{border:1px solid #e5ebf2;border-radius:14px;padding:17px;background:#fafbfd}
.threshold-box span{font-size:9px;color:#7e8b9d;font-weight:800;letter-spacing:1px}
.threshold-box h3{margin-top:8px;font-size:21px}
.threshold-box p{margin-top:5px;color:#8995a6;font-size:9px}
.footer{text-align:center;color:#9aa5b4;font-size:9px;letter-spacing:1px;padding-top:5px}
.credits{text-align:center;color:#7e8b9d;font-size:9px;letter-spacing:1px;margin-top:8px;line-height:1.7}
@media(max-width:900px){.grid,.main-grid,.gps-grid,.threshold-grid{grid-template-columns:1fr}.status-inner{align-items:flex-start;flex-direction:column}.status-time{text-align:left}}
@media(max-width:600px){.header{padding:15px 20px}.container{width:94%;padding-top:20px}.system-pill{display:none}.brand h1{font-size:16px}.brand p{font-size:8px}.status h2{font-size:25px}.value{font-size:32px}}
</style>
</head>
<body>
<header class="header">
  <div class="brand">
    <div class="logo">FG</div>
    <div>
      <h1>FOREST GUARD</h1>
      <p>REAL-TIME ENVIRONMENTAL MONITORING SYSTEM</p>
    </div>
  </div>
  <div class="system-pill"><span class="dot"></span>ESP8266 RECEIVER ONLINE</div>
</header>
<main class="container">
)rawliteral";

const char PAGE_TELEGRAM[] PROGMEM = R"rawliteral(
<section class="panel telegram">
  <div class="panel-head">
    <div>
      <div class="panel-title">TELEGRAM ALERT SYSTEM</div>
      <div class="panel-sub">REMOTE NOTIFICATION &amp; CONNECTION TEST</div>
    </div>
    <div class="live-tag">SECURE ALERT</div>
  </div>
  <div class="telegram-body">
    <div class="telegram-row">
      <div class="telegram-status">
        <span id="telegramDot" class="telegram-dot disconnected"></span>
        <span id="telegramText">TELEGRAM: NOT CONNECTED</span>
      </div>
      <button id="telegramTestButton" onclick="testTelegram()">SEND TEST MESSAGE</button>
    </div>
    <div id="telegramResult" class="telegram-result">STATUS: NOT TESTED</div>
  </div>
</section>
)rawliteral";

const char PAGE_SCRIPT[] PROGMEM = R"rawliteral(
<div class="footer">FOREST GUARD &bull; REAL-TIME FOREST FIRE MONITORING</div>
<div class="credits">
  Developed by [YOUR NAME / TEAM NAME]<br>
  Guided by [GUIDE / MENTOR NAME] &bull; SNS College of Technology, Coimbatore
</div>
</main>
<script>
function $(i){return document.getElementById(i);}
function updateLiveData(){
  fetch('/live-data').then(function(r){return r.json();}).then(function(d){
    if($('tempValue')) $('tempValue').innerHTML=d.temperature.toFixed(1)+'<span class="unit"> &deg;C</span>';
    if($('humidityValue')) $('humidityValue').innerHTML=d.humidity.toFixed(1)+'<span class="unit"> %</span>';
    if($('smokeValue')) $('smokeValue').innerText=d.smoke;
    if($('statusText')) $('statusText').innerText=d.statusText;
    if($('statusBox')){
      var c=['normal','warning','fire'][d.status]||'normal';
      $('statusBox').className='status '+c;
    }
    if($('transmitterStatus')){
      $('transmitterStatus').innerText=d.transmitter;
      $('transmitterStatus').className=(d.transmitter==='ONLINE')?'good':'bad';
    }
    if($('packetValue')) $('packetValue').innerText='#'+d.packet;
  }).catch(function(){});
}
function updateTelegramStatus(){
  fetch('/telegram-status').then(function(r){return r.json();}).then(function(data){
    var dot=$('telegramDot'), txt=$('telegramText'), res=$('telegramResult');
    if(data.connected){
      dot.className='telegram-dot connected';
      txt.innerText='TELEGRAM: CONNECTED';
    }else{
      dot.className='telegram-dot disconnected';
      txt.innerText=data.configured?'TELEGRAM: NOT CONNECTED':'TELEGRAM: TOKEN NOT SET';
    }
    res.innerText='STATUS: '+data.result+(data.pending?' (alert pending)':'');
  }).catch(function(){
    $('telegramDot').className='telegram-dot disconnected';
    $('telegramText').innerText='TELEGRAM: STATUS ERROR';
    $('telegramResult').innerText='STATUS: ESP8266 WEB SERVER NOT RESPONDING';
  });
}
function testTelegram(){
  var b=$('telegramTestButton'), r=$('telegramResult');
  b.disabled=true; b.innerText='SENDING...';
  r.innerText='STATUS: SENDING TEST MESSAGE (can take up to 10 seconds)...';
  fetch('/telegram-test').then(function(x){return x.json();}).then(function(data){
    r.innerText='STATUS: '+data.message;
    setTimeout(updateTelegramStatus,500);
  }).catch(function(){
    r.innerText='STATUS: TEST REQUEST FAILED';
  }).finally(function(){
    b.disabled=false; b.innerText='SEND TEST MESSAGE';
  });
}
updateLiveData(); setInterval(updateLiveData,2000);
updateTelegramStatus(); setInterval(updateTelegramStatus,5000);
</script>
</body>
</html>
)rawliteral";

void handleRoot() {
  uint8_t status = getCurrentStatus();

  String statusText = "NORMAL";
  String statusClass = "normal";
  String statusDescription = "ENVIRONMENTAL CONDITIONS STABLE";
  String statusIcon = "OK";

  if (status == 1) {
    statusText = "WARNING";
    statusClass = "warning";
    statusDescription = "UNUSUAL ENVIRONMENTAL CONDITIONS DETECTED";
    statusIcon = "!";
  } else if (status == 2) {
    statusText = "FIRE ALERT";
    statusClass = "fire";
    statusDescription = "IMMEDIATE ATTENTION REQUIRED";
    statusIcon = "!!";
  }

  String connection = transmitterConnected ? "ONLINE" : "OFFLINE";
  String connectionClass = transmitterConnected ? "good" : "bad";

  String gpsText = receivedData.gpsValid ? "GPS FIX" : "DEFAULT LOCATION";
  String gpsClass = receivedData.gpsValid ? "good" : "bad";

  String tempState = "NORMAL";
  if (receivedData.temperature >= FIRE_TEMP) tempState = "CRITICAL";
  else if (receivedData.temperature >= WARNING_TEMP) tempState = "WARNING";

  String smokeState = "NORMAL";
  if (receivedData.smokeValue >= FIRE_SMOKE) smokeState = "CRITICAL";
  else if (receivedData.smokeValue >= WARNING_SMOKE) smokeState = "WARNING";

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");
  server.sendContent_P(PAGE_HEAD);

  String s;
  s.reserve(900);

  s  = F("<section id=\"statusBox\" class=\"status ");
  s += statusClass;
  s += F("\"><div class=\"status-inner\"><div class=\"status-title\"><div class=\"status-icon\">");
  s += statusIcon;
  s += F("</div><div><h2 id=\"statusText\">");
  s += statusText;
  s += F("</h2><p>");
  s += statusDescription;
  s += F("</p></div></div><div class=\"status-time\">SYSTEM STATUS<strong>LIVE MONITORING</strong></div></div></section>");
  server.sendContent(s);

  s  = F("<section class=\"grid\">");
  s += F("<div class=\"card\"><div class=\"card-top\"><div class=\"label\">Temperature</div><div class=\"icon\">T</div></div>");
  s += F("<div id=\"tempValue\" class=\"value\">");
  s += String(receivedData.temperature, 1);
  s += F("<span class=\"unit\"> &deg;C</span></div><div class=\"badge\">");
  s += tempState;
  s += F("</div></div>");

  s += F("<div class=\"card\"><div class=\"card-top\"><div class=\"label\">Humidity</div><div class=\"icon\">H</div></div>");
  s += F("<div id=\"humidityValue\" class=\"value\">");
  s += String(receivedData.humidity, 1);
  s += F("<span class=\"unit\"> %</span></div><div class=\"badge\">ENVIRONMENT</div></div>");

  s += F("<div class=\"card\"><div class=\"card-top\"><div class=\"label\">Smoke Level</div><div class=\"icon\">S</div></div>");
  s += F("<div id=\"smokeValue\" class=\"value\">");
  s += String(receivedData.smokeValue);
  s += F("</div><div class=\"badge\">");
  s += smokeState;
  s += F("</div></div></section>");
  server.sendContent(s);

  s  = F("<section class=\"main-grid\"><div class=\"panel\"><div class=\"panel-head\"><div>");
  s += F("<div class=\"panel-title\">SYSTEM TELEMETRY</div><div class=\"panel-sub\">COMMUNICATION &amp; DEVICE HEALTH</div></div>");
  s += F("<div class=\"live-tag\">&bull; LIVE</div></div><div class=\"rows\">");

  s += F("<div class=\"row\"><span>ESP32 Transmitter</span><strong id=\"transmitterStatus\" class=\"");
  s += connectionClass;
  s += F("\">");
  s += connection;
  s += F("</strong></div>");

  s += F("<div class=\"row\"><span>Packet Number</span><strong id=\"packetValue\">#");
  s += String(receivedData.packetNumber);
  s += F("</strong></div>");

  s += F("<div class=\"row\"><span>ESP-NOW / Wi-Fi Channel</span><strong>");
  s += String(WiFi.channel());
  s += F("</strong></div>");

  s += F("<div class=\"row\"><span>Receiver IP</span><strong>");
  s += WiFi.localIP().toString();
  s += F("</strong></div>");

  s += F("<div class=\"row\"><span>GPS Status</span><strong class=\"");
  s += gpsClass;
  s += F("\">");
  s += gpsText;
  s += F("</strong></div>");

  s += F("<div class=\"row\"><span>Free Heap</span><strong>");
  s += String(ESP.getFreeHeap());
  s += F(" B</strong></div>");
  s += F("</div></div>");
  server.sendContent(s);

  s  = F("<div class=\"panel\"><div class=\"panel-head\"><div>");
  s += F("<div class=\"panel-title\">ALERT ENGINE</div><div class=\"panel-sub\">AUTOMATED FIRE DETECTION</div></div></div><div class=\"rows\">");

  s += F("<div class=\"row\"><span>Current State</span><strong>");
  s += statusText;
  s += F("</strong></div>");

  s += F("<div class=\"row\"><span>Temperature Rule</span><strong>&ge; ");
  s += String(FIRE_TEMP, 0);
  s += F(" &deg;C</strong></div>");

  s += F("<div class=\"row\"><span>Smoke Rule</span><strong>&ge; ");
  s += String(FIRE_SMOKE);
  s += F("</strong></div>");

  s += F("<div class=\"row\"><span>Warning Temperature</span><strong>");
  s += String(WARNING_TEMP, 0);
  s += F(" &deg;C</strong></div>");

  s += F("<div class=\"row\"><span>Warning Smoke</span><strong>");
  s += String(WARNING_SMOKE);
  s += F("</strong></div></div></div></section>");
  server.sendContent(s);

  s  = F("<section class=\"panel gps\"><div class=\"panel-head\"><div>");
  s += F("<div class=\"panel-title\">LOCATION INTELLIGENCE</div>");
  s += F("<div class=\"panel-sub\">NEO-6M GPS POSITION &bull; DEFAULT: SNS COLLEGE OF TECHNOLOGY</div></div>");
  s += F("<div class=\"live-tag\">GPS</div></div><div class=\"gps-grid\">");

  s += F("<div class=\"gps-box\"><span>Latitude</span><strong>");
  s += String(getDisplayLatitude(), 6);
  s += F("</strong></div>");

  s += F("<div class=\"gps-box\"><span>Longitude</span><strong>");
  s += String(getDisplayLongitude(), 6);
  s += F("</strong></div>");

  s += F("<div class=\"gps-box\"><span>Position</span><strong>");
  s += receivedData.gpsValid ? "GPS AVAILABLE" : "DEFAULT LOCATION";
  s += F("</strong></div></div>");

  s += F("<a class=\"maplink\" href=\"");
  s += getGoogleMapsUrl();
  s += F("\" target=\"_blank\" rel=\"noopener\">OPEN IN GOOGLE MAPS</a></section>");
  server.sendContent(s);

  server.sendContent_P(PAGE_TELEGRAM);

  s  = F("<section class=\"panel threshold\"><div class=\"panel-title\">ALERT CONFIGURATION</div>");
  s += F("<div class=\"panel-sub\">CURRENT ENVIRONMENTAL DETECTION LEVELS</div><div class=\"threshold-grid\">");

  s += F("<div class=\"threshold-box\"><span>NORMAL</span><h3>&lt; ");
  s += String(WARNING_TEMP, 0);
  s += F(" &deg;C</h3><p>Temperature and smoke below warning levels</p></div>");

  s += F("<div class=\"threshold-box\"><span>WARNING</span><h3>");
  s += String(WARNING_TEMP, 0);
  s += F(" &deg;C / ");
  s += String(WARNING_SMOKE);
  s += F("</h3><p>Environmental condition requires attention</p></div>");

  s += F("<div class=\"threshold-box\"><span>FIRE ALERT</span><h3>");
  s += String(FIRE_TEMP, 0);
  s += F(" &deg;C / ");
  s += String(FIRE_SMOKE);
  s += F("</h3><p>High temperature or smoke detected</p></div></div></section>");
  server.sendContent(s);

  server.sendContent_P(PAGE_SCRIPT);
  server.sendContent("");
}

void handleLiveData() {
  uint8_t status = getCurrentStatus();

  String json = "{";
  json += "\"temperature\":" + String(receivedData.temperature, 1);
  json += ",\"humidity\":" + String(receivedData.humidity, 1);
  json += ",\"smoke\":" + String(receivedData.smokeValue);
  json += ",\"status\":" + String(status);
  json += ",\"statusText\":\"" + getStatusName(status) + "\"";
  json += ",\"transmitter\":\"" + String(transmitterConnected ? "ONLINE" : "OFFLINE") + "\"";
  json += ",\"packet\":" + String(receivedData.packetNumber);
  json += ",\"gpsValid\":" + String(receivedData.gpsValid ? "true" : "false");
  json += ",\"latitude\":" + String(getDisplayLatitude(), 6);
  json += ",\"longitude\":" + String(getDisplayLongitude(), 6);
  json += ",\"locationName\":\"" + jsonEscape(String(DEFAULT_GPS_NAME)) + "\"";
  json += ",\"mapsUrl\":\"" + getGoogleMapsUrl() + "\"";
  json += "}";

  server.send(200, "application/json", json);
}

void maintainWiFi() {
  static unsigned long lastCheck = 0;

  if (WiFi.status() == WL_CONNECTED) return;
  if (millis() - lastCheck < 10000UL) return;

  lastCheck = millis();
  Serial.println("WiFi lost - reconnecting...");
  WiFi.reconnect();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  memset(&receivedData, 0, sizeof(receivedData));

  Serial.println();
  Serial.println("================================");
  Serial.println(" FOREST GUARD ESP8266 RECEIVER");
  Serial.println(" Developed by: [YOUR NAME / TEAM NAME]");
  Serial.println(" Guide: [GUIDE / MENTOR NAME]");
  Serial.println(" SNS College of Technology");
  Serial.println("================================");

  pinMode(GREEN_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(GREEN_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(RED_LED, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  Wire.begin(D2, D1);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("FOREST GUARD");
  lcd.setCursor(0, 1);
  lcd.print("STARTING...");
  delay(2000);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("DEVELOPED BY");
  lcd.setCursor(0, 1);
  lcd.print("[YOUR NAME]");
  delay(2000);

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting WiFi");
  unsigned long wifiStart = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 30000UL) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi Connected");
  } else {
    Serial.println("WiFi NOT connected (continuing offline, will keep retrying)");
  }

  Serial.print("ESP8266 MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.print("WiFi Channel: ");
  Serial.println(WiFi.channel());
  Serial.println(">> The ESP32 transmitter MUST use this same channel <<");

  if (esp_now_init() != 0) {
    Serial.println("ESP-NOW INIT FAILED");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("ESP-NOW ERROR");
    while (true) {
      delay(1000);
    }
  }

  Serial.println("ESP-NOW INITIALIZED");
  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(onDataReceive);

  server.on("/", handleRoot);
  server.on("/live-data", handleLiveData);
  server.on("/telegram-test", handleTelegramTest);
  server.on("/telegram-status", handleTelegramStatus);
  server.begin();

  Serial.println("WEB SERVER STARTED");
  Serial.println();
  Serial.println("================================");
  Serial.print("Dashboard: http://");
  Serial.println(WiFi.localIP());
  Serial.println("WAITING FOR ESP32...");
  Serial.println("================================");

  if (!isTelegramConfigured()) {
    Serial.println("WARNING: Telegram bot token is not set!");
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(WiFi.status() == WL_CONNECTED ? "WiFi READY" : "WiFi OFFLINE");
  lcd.setCursor(0, 1);
  lcd.print("CH:");
  lcd.print(WiFi.channel());
  lcd.print(" WAIT DATA");
  delay(2500);
}

void loop() {
  server.handleClient();
  processTelegram();
  updateOutputs();
  updateLCD();
  maintainWiFi();
  delay(10);
}
