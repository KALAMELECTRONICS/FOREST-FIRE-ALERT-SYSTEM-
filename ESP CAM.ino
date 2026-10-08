// FUELGUARD -   Real-Time Forest Fire Monitoring System
// Developed by KALAM ELECTRONICS
// Date: 29.09.2026
#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>


 
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5

#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// =====================================================
// FLASH LED
// AI THINKER ESP32-CAM ONBOARD FLASH
// =====================================================

#define FLASH_LED_PIN 4

bool flashState = false;

// =====================================================
// LOCAL WIFI ACCESS POINT
// =====================================================

const char* AP_SSID = "FOREST_GUARD_CAM";
const char* AP_PASSWORD = "12345678";

// =====================================================
// FIXED LOCAL IP
// =====================================================

IPAddress local_IP(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

// =====================================================
// WEB SERVER
// =====================================================

WebServer server(80);

// =====================================================
// CAMERA INITIALIZATION
// =====================================================

bool initCamera() {

  camera_config_t config;

  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;

  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;

  config.pin_xclk = XCLK_GPIO_NUM;

  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;

  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;

  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  config.xclk_freq_hz = 20000000;

  config.pixel_format = PIXFORMAT_JPEG;

  // ===================================================
  // PSRAM CHECK
  // ===================================================

  if (psramFound()) {

    config.frame_size = FRAMESIZE_VGA;

    config.jpeg_quality = 10;

    config.fb_count = 2;

    config.grab_mode = CAMERA_GRAB_LATEST;

  } else {

    config.frame_size = FRAMESIZE_QVGA;

    config.jpeg_quality = 12;

    config.fb_count = 1;

    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  }

  // ===================================================
  // CAMERA START
  // ===================================================

  esp_err_t err = esp_camera_init(&config);

  if (err != ESP_OK) {

    Serial.print(
      "Camera initialization failed: 0x"
    );

    Serial.println(
      err,
      HEX
    );

    return false;
  }

  // ===================================================
  // CAMERA SENSOR SETTINGS
  // ===================================================

  sensor_t* sensor =
    esp_camera_sensor_get();

  if (sensor != NULL) {

    sensor->set_brightness(
      sensor,
      0
    );

    sensor->set_contrast(
      sensor,
      0
    );

    sensor->set_saturation(
      sensor,
      0
    );

    sensor->set_framesize(
      sensor,
      FRAMESIZE_VGA
    );
  }

  return true;
}

// =====================================================
// FLASH CONTROL
// =====================================================

void setFlash(bool state) {

  flashState = state;

  digitalWrite(
    FLASH_LED_PIN,
    flashState ? HIGH : LOW
  );

  Serial.print("Flash: ");

  Serial.println(
    flashState ? "ON" : "OFF"
  );
}

// =====================================================
// FLASH API
// =====================================================

void handleFlash() {

  if (!server.hasArg("state")) {

    server.send(
      400,
      "text/plain",
      "Missing state"
    );

    return;
  }

  String state =
    server.arg("state");

  if (state == "on") {

    setFlash(true);

  } else if (state == "off") {

    setFlash(false);

  } else {

    server.send(
      400,
      "text/plain",
      "Invalid state"
    );

    return;
  }

  server.send(
    200,
    "text/plain",
    flashState ? "ON" : "OFF"
  );
}

// =====================================================
// FLASH STATUS API
// =====================================================

void handleFlashStatus() {

  server.send(
    200,
    "text/plain",
    flashState ? "ON" : "OFF"
  );
}

// =====================================================
// MAIN CAMERA PAGE
// =====================================================

void handleRoot() {

  String html = R"rawliteral(

<!DOCTYPE html>

<html>

<head>

<meta charset="UTF-8">

<meta name="viewport"
content="width=device-width, initial-scale=1.0">

<title>Forest Guard Camera</title>

<style>

/* =====================================================
   GLOBAL
===================================================== */

* {
    margin: 0;
    padding: 0;
    box-sizing: border-box;
}

html {
    scroll-behavior: smooth;
}

body {

    min-height: 100vh;

    font-family:
    "Times New Roman",
    Times,
    serif;

    background: #f4f7fb;

    color: #17233a;

    overflow-x: hidden;
}

/* =====================================================
   ANIMATIONS
===================================================== */

@keyframes pageFade {

    from {
        opacity: 0;
    }

    to {
        opacity: 1;
    }
}

@keyframes slideDown {

    from {
        opacity: 0;
        transform: translateY(-25px);
    }

    to {
        opacity: 1;
        transform: translateY(0);
    }
}

@keyframes slideUp {

    from {
        opacity: 0;
        transform: translateY(30px);
    }

    to {
        opacity: 1;
        transform: translateY(0);
    }
}

@keyframes scaleIn {

    from {
        opacity: 0;
        transform: scale(.96);
    }

    to {
        opacity: 1;
        transform: scale(1);
    }
}

@keyframes floatLogo {

    0%, 100% {
        transform: translateY(0);
    }

    50% {
        transform: translateY(-3px);
    }
}

@keyframes bluePulse {

    0%, 100% {
        box-shadow:
        0 0 0 0
        rgba(30,110,220,.20);
    }

    50% {
        box-shadow:
        0 0 0 7px
        rgba(30,110,220,0);
    }
}

@keyframes livePulse {

    0%, 100% {
        transform: scale(1);
        opacity: 1;
    }

    50% {
        transform: scale(1.18);
        opacity: .65;
    }
}

@keyframes flashPulse {

    0%, 100% {
        box-shadow:
        0 0 0 0
        rgba(30,110,220,.20);
    }

    50% {
        box-shadow:
        0 0 0 6px
        rgba(30,110,220,0);
    }
}

body {
    animation:
    pageFade .8s ease;
}

/* =====================================================
   HEADER
===================================================== */

.header {

    width: 100%;

    min-height: 80px;

    background: #ffffff;

    border-bottom:
    1px solid #dfe6ef;

    display: flex;

    align-items: center;

    justify-content: space-between;

    padding: 14px 5%;

    position: relative;

    z-index: 10;

    animation:
    slideDown .7s ease;
}

/* =====================================================
   BRAND
===================================================== */

.brand {

    display: flex;

    align-items: center;

    gap: 14px;
}

.logo {

    width: 48px;

    height: 48px;

    border-radius: 11px;

    display: flex;

    align-items: center;

    justify-content: center;

    background:
    linear-gradient(
        135deg,
        #1769d2,
        #0b4fa8
    );

    color: #ffffff;

    font-size: 16px;

    font-weight: bold;

    letter-spacing: 1px;

    box-shadow:
    0 6px 18px
    rgba(23,105,210,.22);

    animation:
    floatLogo 3s
    ease-in-out
    infinite;
}

.brand h1 {

    font-size: 22px;

    font-weight: 700;

    letter-spacing: 1px;

    color: #16243a;
}

.brand p {

    margin-top: 3px;

    color: #718096;

    font-size: 11px;

    letter-spacing: 1px;
}

/* =====================================================
   ONLINE
===================================================== */

.online {

    display: flex;

    align-items: center;

    gap: 8px;

    padding: 9px 14px;

    border-radius: 20px;

    background: #edf5ff;

    border:
    1px solid #c9ddf7;

    color: #155db5;

    font-size: 12px;

    font-weight: 600;

    animation:
    bluePulse 2.5s
    infinite;
}

.online-dot {

    width: 8px;

    height: 8px;

    border-radius: 50%;

    background: #1d75d8;

    animation:
    livePulse 1.8s
    ease-in-out
    infinite;
}

/* =====================================================
   MAIN
===================================================== */

.container {

    width: 92%;

    max-width: 1250px;

    margin: auto;

    padding:
    36px 0 45px;
}

/* =====================================================
   TITLE
===================================================== */

.page-title {

    margin-bottom: 25px;

    animation:
    slideUp .8s
    ease .1s
    both;
}

.page-title h2 {

    font-size: 31px;

    color: #16243a;

    margin-bottom: 5px;
}

.page-title p {

    color: #69798d;

    font-size: 14px;
}

/* =====================================================
   STATUS CARDS
===================================================== */

.status-bar {

    display: grid;

    grid-template-columns:
    repeat(3,1fr);

    gap: 15px;

    margin-bottom: 20px;
}

.status-card {

    background: #ffffff;

    border:
    1px solid #dfe6ef;

    border-radius: 13px;

    padding: 17px 20px;

    display: flex;

    align-items: center;

    gap: 14px;

    transition:
    transform .3s ease,
    box-shadow .3s ease,
    border-color .3s ease;

    animation:
    slideUp .7s
    ease both;
}

.status-card:nth-child(1) {
    animation-delay: .15s;
}

.status-card:nth-child(2) {
    animation-delay: .25s;
}

.status-card:nth-child(3) {
    animation-delay: .35s;
}

.status-card:hover {

    transform:
    translateY(-5px);

    border-color:
    #a9c9ee;

    box-shadow:
    0 12px 28px
    rgba(31,65,110,.10);
}

.status-icon {

    width: 41px;

    height: 41px;

    border-radius: 10px;

    display: flex;

    align-items: center;

    justify-content: center;

    background: #edf5ff;

    color: #1769d2;

    font-weight: bold;

    font-size: 12px;

    transition:
    transform .3s ease;
}

.status-card:hover
.status-icon {

    transform:
    rotate(-4deg)
    scale(1.06);
}

.status-text span {

    display: block;

    color: #78879a;

    font-size: 11px;

    text-transform: uppercase;

    letter-spacing: .7px;
}

.status-text strong {

    display: block;

    margin-top: 4px;

    font-size: 15px;

    color: #26364d;
}

/* =====================================================
   CAMERA PANEL
===================================================== */

.camera-panel {

    background: #ffffff;

    border:
    1px solid #dbe4ee;

    border-radius: 17px;

    overflow: hidden;

    box-shadow:
    0 10px 35px
    rgba(31,50,80,.08);

    animation:
    scaleIn .9s
    ease .3s
    both;

    transition:
    box-shadow .4s ease;
}

.camera-panel:hover {

    box-shadow:
    0 16px 45px
    rgba(31,50,80,.11);
}

/* =====================================================
   CAMERA HEADER
===================================================== */

.camera-header {

    min-height: 72px;

    display: flex;

    align-items: center;

    justify-content: space-between;

    padding: 15px 20px;

    border-bottom:
    1px solid #e5ebf2;

    background: #ffffff;
}

.camera-title {

    display: flex;

    align-items: center;

    gap: 12px;
}

.camera-title-icon {

    width: 41px;

    height: 41px;

    border-radius: 10px;

    background:
    linear-gradient(
        135deg,
        #1769d2,
        #0b4fa8
    );

    color: #ffffff;

    display: flex;

    align-items: center;

    justify-content: center;

    font-size: 11px;

    font-weight: bold;

    box-shadow:
    0 5px 15px
    rgba(23,105,210,.18);

    transition:
    transform .3s ease;
}

.camera-title:hover
.camera-title-icon {

    transform:
    scale(1.07)
    rotate(-3deg);
}

.camera-title h3 {

    font-size: 19px;

    color: #1d2b40;
}

.camera-title p {

    margin-top: 3px;

    color: #7b899a;

    font-size: 10px;

    letter-spacing: .5px;
}

/* =====================================================
   LIVE BADGE
===================================================== */

.live {

    display: flex;

    align-items: center;

    gap: 7px;

    padding: 7px 13px;

    border-radius: 18px;

    background: #edf5ff;

    border:
    1px solid #c9ddf7;

    color: #155db5;

    font-size: 11px;

    font-weight: bold;

    letter-spacing: .7px;
}

.live-dot {

    width: 7px;

    height: 7px;

    border-radius: 50%;

    background: #1d75d8;

    animation:
    livePulse 1.4s
    ease-in-out
    infinite;
}

/* =====================================================
   CAMERA VIEW
===================================================== */

.camera-view {

    width: 100%;

    min-height: 500px;

    background: #10151c;

    display: flex;

    align-items: center;

    justify-content: center;

    position: relative;

    overflow: hidden;
}

.camera-view img {

    display: block;

    width: 100%;

    max-height: 680px;

    object-fit: contain;

    background: #10151c;

    animation:
    scaleIn 1s ease;
}

/* =====================================================
   CAMERA CORNERS
===================================================== */

.corner {

    position: absolute;

    width: 28px;

    height: 28px;

    border-color:
    rgba(255,255,255,.72);

    border-style: solid;

    pointer-events: none;

    transition:
    border-color .3s ease;
}

.camera-panel:hover
.corner {

    border-color:
    rgba(92,165,245,.95);
}

.corner.tl {

    top: 15px;

    left: 15px;

    border-width:
    2px 0 0 2px;
}

.corner.tr {

    top: 15px;

    right: 15px;

    border-width:
    2px 2px 0 0;
}

.corner.bl {

    bottom: 15px;

    left: 15px;

    border-width:
    0 0 2px 2px;
}

.corner.br {

    bottom: 15px;

    right: 15px;

    border-width:
    0 2px 2px 0;
}

/* =====================================================
   CAMERA FOOTER
===================================================== */

.camera-footer {

    min-height: 55px;

    padding: 12px 20px;

    display: flex;

    align-items: center;

    justify-content: space-between;

    border-top:
    1px solid #e5ebf2;

    background: #fafbfd;

    color: #68788b;

    font-size: 12px;
}

.camera-footer strong {

    color: #26364d;
}

/* =====================================================
   FLASH CONTROL
===================================================== */

.flash-control {

    display: flex;

    align-items: center;

    gap: 10px;

    margin-top: 18px;

    padding: 14px 16px;

    background: #ffffff;

    border:
    1px solid #dbe4ee;

    border-radius: 13px;

    box-shadow:
    0 6px 20px
    rgba(31,50,80,.06);

    animation:
    slideUp .8s
    ease .55s
    both;
}

.flash-label {

    flex: 1;
}

.flash-label span {

    display: block;

    color: #77879a;

    font-size: 11px;

    text-transform: uppercase;

    letter-spacing: .8px;
}

.flash-label strong {

    display: block;

    color: #26364d;

    font-size: 16px;

    margin-top: 3px;
}

.flash-status {

    color: #718096;

    font-size: 12px;

    margin-right: 5px;
}

/* =====================================================
   FLASH BUTTON
===================================================== */

.flash-btn {

    min-width: 120px;

    border: none;

    border-radius: 9px;

    padding: 11px 17px;

    background:
    linear-gradient(
        135deg,
        #1769d2,
        #0b4fa8
    );

    color: white;

    font-family:
    "Times New Roman",
    Times,
    serif;

    font-size: 14px;

    font-weight: bold;

    cursor: pointer;

    transition:
    transform .25s ease,
    box-shadow .25s ease,
    background .25s ease;
}

.flash-btn:hover {

    transform:
    translateY(-2px);

    box-shadow:
    0 8px 20px
    rgba(23,105,210,.25);
}

.flash-btn:active {

    transform:
    translateY(0)
    scale(.98);
}

.flash-btn.on {

    background:
    linear-gradient(
        135deg,
        #0b4fa8,
        #1769d2
    );

    animation:
    flashPulse 1.8s
    infinite;
}

/* =====================================================
   FOOTER
===================================================== */

.footer {

    margin-top: 35px;

    padding: 20px 0;

    text-align: center;

    border-top:
    1px solid #dce5ee;

    color: #788697;

    font-size: 11px;

    letter-spacing: .6px;

    animation:
    slideUp .8s
    ease .8s
    both;
}

.footer .blue {

    color: #1769d2;
}

/* =====================================================
   RESPONSIVE
===================================================== */

@media(max-width:850px) {

    .status-bar {

        grid-template-columns: 1fr;
    }

    .camera-view {

        min-height: 350px;
    }

    .header {

        padding:
        13px 4%;
    }

    .container {

        width: 94%;
    }
}

@media(max-width:600px) {

    .header {

        min-height: 68px;
    }

    .brand h1 {

        font-size: 17px;
    }

    .brand p {

        display: none;
    }

    .logo {

        width: 40px;

        height: 40px;

        font-size: 14px;
    }

    .online {

        padding:
        6px 9px;

        font-size: 9px;
    }

    .page-title {

        margin-bottom: 18px;
    }

    .page-title h2 {

        font-size: 25px;
    }

    .page-title p {

        font-size: 12px;
    }

    .camera-header {

        padding:
        12px;
    }

    .camera-title h3 {

        font-size: 15px;
    }

    .camera-title p {

        font-size: 9px;
    }

    .camera-title-icon {

        width: 35px;

        height: 35px;
    }

    .live {

        padding:
        6px 9px;

        font-size: 9px;
    }

    .camera-view {

        min-height: 260px;
    }

    .camera-footer {

        padding:
        10px 13px;

        font-size: 10px;
    }

    .flash-control {

        flex-wrap: wrap;
    }

    .flash-label {

        width: 100%;

        flex: auto;
    }

    .flash-status {

        margin-left: auto;
    }

    .flash-btn {

        width: 100%;
    }
}

</style>

</head>

<body>

<!-- =================================================
     HEADER
================================================= -->

<header class="header">

    <div class="brand">

        <div class="logo">
            FG
        </div>

        <div>

            <h1>
                FOREST GUARD
            </h1>

            <p>
                ESP32-CAM SURVEILLANCE SYSTEM
            </p>

        </div>

    </div>


    <div class="online">

        <div class="online-dot"></div>

        CAMERA ONLINE

    </div>

</header>


<!-- =================================================
     MAIN
================================================= -->

<main class="container">


    <!-- PAGE TITLE -->

    <section class="page-title">

        <h2>
            Live Camera Monitoring
        </h2>

        <p>
            Real-time local visual surveillance system
        </p>

    </section>


    <!-- =================================================
         STATUS CARDS
    ================================================= -->

    <section class="status-bar">


        <div class="status-card">

            <div class="status-icon">
                CAM
            </div>

            <div class="status-text">

                <span>
                    Camera Status
                </span>

                <strong>
                    Online
                </strong>

            </div>

        </div>


        <div class="status-card">

            <div class="status-icon">
                IP
            </div>

            <div class="status-text">

                <span>
                    Camera Address
                </span>

                <strong>
                    192.168.4.1
                </strong>

            </div>

        </div>


        <div class="status-card">

            <div class="status-icon">
                NET
            </div>

            <div class="status-text">

                <span>
                    Connection
                </span>

                <strong>
                    Local Network
                </strong>

            </div>

        </div>


    </section>


    <!-- =================================================
         CAMERA PANEL
    ================================================= -->

    <section class="camera-panel">


        <div class="camera-header">


            <div class="camera-title">


                <div class="camera-title-icon">
                    CAM
                </div>


                <div>

                    <h3>
                        Live Camera Feed
                    </h3>

                    <p>
                        AI THINKER ESP32-CAM • LOCAL STREAM
                    </p>

                </div>


            </div>


            <div class="live">

                <div class="live-dot"></div>

                LIVE

            </div>


        </div>


        <!-- CAMERA STREAM -->

        <div class="camera-view">


            <img
                src="/stream"
                alt="ESP32-CAM Live Stream"
            >


            <div class="corner tl"></div>

            <div class="corner tr"></div>

            <div class="corner bl"></div>

            <div class="corner br"></div>


        </div>


        <!-- CAMERA FOOTER -->

        <div class="camera-footer">

            <span>
                Stream Status:
                <strong>
                    Active
                </strong>
            </span>

            <span>
                Resolution:
                <strong>
                    VGA
                </strong>
            </span>

        </div>


    </section>


    <!-- =================================================
         FLASH CONTROL
    ================================================= -->

    <section class="flash-control">


        <div class="flash-label">

            <span>
                Camera Lighting
            </span>

            <strong>
                Flash Light
            </strong>

        </div>


        <div
            class="flash-status"
            id="flashStatus"
        >
            OFF
        </div>


        <button
            class="flash-btn"
            id="flashButton"
            onclick="toggleFlash()"
        >
            FLASH ON
        </button>


    </section>


    <!-- =================================================
         FOOTER
    ================================================= -->

    <footer class="footer">

        FOREST GUARD

        <span class="blue">
            •
        </span>

        ESP32-CAM LOCAL MONITORING

        <span class="blue">
            •
        </span>

        LIVE SURVEILLANCE

    </footer>


</main>


<!-- =================================================
     FLASH JAVASCRIPT
================================================= -->

<script>

let flashOn = false;

function toggleFlash() {

    const button =
        document.getElementById(
            "flashButton"
        );

    const status =
        document.getElementById(
            "flashStatus"
        );

    const newState =
        flashOn ? "off" : "on";


    fetch(
        "/flash?state=" + newState
    )

    .then(
        response =>
        response.text()
    )

    .then(
        result => {

            if (
                result === "ON"
            ) {

                flashOn = true;

                button.innerHTML =
                    "FLASH OFF";

                button.classList.add(
                    "on"
                );

                status.innerHTML =
                    "ON";

            } else {

                flashOn = false;

                button.innerHTML =
                    "FLASH ON";

                button.classList.remove(
                    "on"
                );

                status.innerHTML =
                    "OFF";
            }

        }
    )

    .catch(
        error => {

            console.log(
                "Flash control error:",
                error
            );

        }
    );
}


/* =====================================================
   CHECK CURRENT FLASH STATUS
===================================================== */

function checkFlashStatus() {

    fetch(
        "/flash/status"
    )

    .then(
        response =>
        response.text()
    )

    .then(
        result => {

            const button =
                document.getElementById(
                    "flashButton"
                );

            const status =
                document.getElementById(
                    "flashStatus"
                );


            if (
                result === "ON"
            ) {

                flashOn = true;

                button.innerHTML =
                    "FLASH OFF";

                button.classList.add(
                    "on"
                );

                status.innerHTML =
                    "ON";

            } else {

                flashOn = false;

                button.innerHTML =
                    "FLASH ON";

                button.classList.remove(
                    "on"
                );

                status.innerHTML =
                    "OFF";
            }

        }
    )

    .catch(
        error => {

            console.log(
                "Status error:",
                error
            );

        }
    );
}

window.onload =
    checkFlashStatus;

</script>


</body>

</html>

)rawliteral";


  server.send(
    200,
    "text/html",
    html
  );
}

// =====================================================
// LIVE STREAM
// =====================================================

void handleStream() {

  WiFiClient client =
    server.client();

  String response =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
    "Access-Control-Allow-Origin: *\r\n"
    "Cache-Control: no-cache\r\n"
    "Pragma: no-cache\r\n"
    "\r\n";

  client.print(response);

  while (client.connected()) {

    camera_fb_t* fb =
      esp_camera_fb_get();

    if (!fb) {

      Serial.println(
        "Camera capture failed"
      );

      break;
    }

    client.print(
      "--frame\r\n"
    );

    client.print(
      "Content-Type: image/jpeg\r\n"
    );

    client.print(
      "Content-Length: "
    );

    client.print(
      fb->len
    );

    client.print(
      "\r\n\r\n"
    );

    client.write(
      fb->buf,
      fb->len
    );

    client.print(
      "\r\n"
    );

    esp_camera_fb_return(
      fb
    );

    delay(30);
  }
}

// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  delay(1000);

  Serial.println();

  Serial.println(
    "======================================"
  );

  Serial.println(
    "       FOREST GUARD ESP32-CAM"
  );

  Serial.println(
    "       LOCAL CAMERA MONITOR"
  );

  Serial.println(
    "======================================"
  );


  // ===================================================
  // FLASH LED
  // ===================================================

  pinMode(
    FLASH_LED_PIN,
    OUTPUT
  );

  setFlash(false);


  // ===================================================
  // CAMERA
  // ===================================================

  Serial.println(
    "Initializing camera..."
  );

  if (!initCamera()) {

    Serial.println(
      "CAMERA INITIALIZATION FAILED"
    );

    while (true) {

      delay(1000);
    }
  }

  Serial.println(
    "Camera initialized successfully"
  );


  // ===================================================
  // WIFI ACCESS POINT
  // ===================================================

  WiFi.mode(
    WIFI_AP
  );

  WiFi.softAPConfig(
    local_IP,
    gateway,
    subnet
  );

  bool apStarted =
    WiFi.softAP(
      AP_SSID,
      AP_PASSWORD
    );

  if (!apStarted) {

    Serial.println(
      "ACCESS POINT FAILED"
    );

    while (true) {

      delay(1000);
    }
  }


  Serial.println();

  Serial.println(
    "LOCAL WIFI STARTED"
  );

  Serial.print(
    "SSID: "
  );

  Serial.println(
    AP_SSID
  );

  Serial.print(
    "PASSWORD: "
  );

  Serial.println(
    AP_PASSWORD
  );

  Serial.print(
    "LOCAL IP: "
  );

  Serial.println(
    WiFi.softAPIP()
  );


  // ===================================================
  // WEB SERVER
  // ===================================================

  server.on(
    "/",
    HTTP_GET,
    handleRoot
  );


  server.on(
    "/stream",
    HTTP_GET,
    handleStream
  );


  server.on(
    "/flash",
    HTTP_GET,
    handleFlash
  );


  server.on(
    "/flash/status",
    HTTP_GET,
    handleFlashStatus
  );


  server.begin();


  Serial.println();

  Serial.println(
    "WEB SERVER STARTED"
  );

  Serial.println();

  Serial.println(
    "======================================"
  );

  Serial.println(
    "CONNECT YOUR PHONE/LAPTOP TO:"
  );

  Serial.println(
    "FOREST_GUARD_CAM"
  );

  Serial.println();

  Serial.println(
    "PASSWORD:"
  );

  Serial.println(
    "12345678"
  );

  Serial.println();

  Serial.println(
    "OPEN:"
  );

  Serial.println(
    "http://192.168.4.1"
  );

  Serial.println();

  Serial.println(
    "FLASH CONTROL:"
  );

  Serial.println(
    "http://192.168.4.1/flash?state=on"
  );

  Serial.println(
    "http://192.168.4.1/flash?state=off"
  );

  Serial.println(
    "======================================"
  );
}

// =====================================================
// LOOP
// =====================================================

void loop() {

  server.handleClient();

  delay(1);
}
