/* Blind Stick - ESP32 (browser-location map + live sensors + NTP RTC update) */

/* Blynk 2.0 defines */
#define BLYNK_TEMPLATE_ID "TMPL6jXnB7D5z"
#define BLYNK_TEMPLATE_NAME "Blind Stick"
#define BLYNK_AUTH_TOKEN "iFgFlhutzmjZ0wpYoDMqC-b-ov1IYWx3"

/* Libraries */
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <Adafruit_VL53L0X.h>
#include <ESPAsyncWebServer.h>
#include <Wire.h>
#include "RTClib.h"
#include <DFRobotDFPlayerMini.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

/* WiFi & Blynk creds */
char auth[] = BLYNK_AUTH_TOKEN;
const char* ssid = "Siyam";
const char* pass = "999666333";

/* Pin definitions */
#define TRIG_PIN      5
#define ECHO_PIN      18
#define FLAME_PIN     34
#define SOIL_PIN      35
#define BUZZER_PIN    23
#define VIBRATOR_PIN  19
#define SOS_PIN       25
#define TIME_SWITCH   4

/* Peripherals */
HardwareSerial GPS_Serial(2);   // RX=16, TX=17
TinyGPSPlus gps;
Adafruit_VL53L0X lox = Adafruit_VL53L0X();
RTC_DS3231 rtc;
AsyncWebServer server(80);

/* DFPlayer */
HardwareSerial dfSerial(1); // RX=13, TX=14
DFRobotDFPlayerMini mp3;

/* NTP Client for RTC update */
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 19800); // IST +5:30

/* MP3 tracks */
const uint8_t TRACK_IT_IS = 1;
const uint8_t TRACK_HOUR_START = 2;
const uint8_t TRACK_OCLOCK = 14;
const uint8_t TRACK_MIN_START = 15;
const uint8_t TRACK_AM = 75;
const uint8_t TRACK_PM = 76;

/* Thresholds */
#define ULTRASONIC_THRESH 40.0
#define VL53_THRESH       200.0
#define FLAME_THRESH      50
#define SOIL_THRESH       28

/* Helper functions */
float measureUltrasonicCM() {
  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  return (duration == 0) ? 9999.0 : (float)duration * 0.034 / 2.0;
}

float measureVL53() {
  VL53L0X_RangingMeasurementData_t measure;
  lox.rangingTest(&measure, false);
  return (measure.RangeStatus != 0) ? 9999.0 : (float)measure.RangeMilliMeter;
}

void playAndWait(uint16_t idx, unsigned long waitMs = 800) {
  if (mp3.available()) mp3.play(idx);
  else mp3.play(idx);
  delay(waitMs);
}

uint16_t hourToTrack(int h24) {
  int h12 = h24 % 12;
  if (h12 == 0) h12 = 12;
  return TRACK_HOUR_START + (h12 - 1);
}
uint16_t minuteToTrack(int m) { return TRACK_MIN_START + m; }

void announceTimeMP3() {
  DateTime now = rtc.now();
  int hour = now.hour();
  int minute = now.minute();
  playAndWait(TRACK_IT_IS, 700);
  playAndWait(hourToTrack(hour), 700);
  if (minute == 0) playAndWait(TRACK_OCLOCK, 700);
  else playAndWait(minuteToTrack(minute), 800);
  if (hour < 12) playAndWait(TRACK_AM, 500);
  else playAndWait(TRACK_PM, 500);
}

/* RTC auto-update via NTP */
void updateRTCfromNTP() {
  if (WiFi.status() == WL_CONNECTED) {
    timeClient.update();
    unsigned long epoch = timeClient.getEpochTime();
    DateTime now(epoch);
    rtc.adjust(now);
    Serial.println("RTC updated from NTP!");
  }
}


/* SOS function */
void sendSOS() {
  Blynk.virtualWrite(V8, "HELP ME!!!");
  Serial.println("SOS sent to Blynk: HELP ME!!!");

  if (gps.location.isValid()) {
    Blynk.virtualWrite(V6, gps.location.lat());
    Blynk.virtualWrite(V7, gps.location.lng());
  }

  Blynk.virtualWrite(V11, 1); // LED ON
  delay(3000);
  Blynk.virtualWrite(V11, 0); // LED OFF
}

/* Sensor check & Blynk updates */
void checkSensors() {
  float ultrasonic = measureUltrasonicCM();
  float vl = measureVL53();
  int flame = analogRead(FLAME_PIN);
  int soil = analogRead(SOIL_PIN);

  bool ultrasonicAlert = (ultrasonic < ULTRASONIC_THRESH);
  bool vlAlert = (vl > VL53_THRESH);
  bool flameAlert = (flame < FLAME_THRESH);
  bool soilAlert = (soil > SOIL_THRESH);

  Blynk.virtualWrite(V1, ultrasonic);
  Blynk.virtualWrite(V2, vl);
  Blynk.virtualWrite(V3, flame);
  Blynk.virtualWrite(V4, soil);

  if (ultrasonicAlert || vlAlert || flameAlert || soilAlert) {
    digitalWrite(BUZZER_PIN, HIGH);
    digitalWrite(VIBRATOR_PIN, HIGH);
    Blynk.virtualWrite(V5, 1);
    delay(500);
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(VIBRATOR_PIN, LOW);
    Blynk.virtualWrite(V5, 0);
  }
}

String htmlPage() {
  String html = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
    <meta name='viewport' content='width=device-width, initial-scale=1'>
    <title>Blind Stick - Live Location</title>
    <link rel='stylesheet' href='https://unpkg.com/leaflet@1.9.4/dist/leaflet.css' />
    <style>
      body { font-family: Arial, Helvetica, sans-serif; margin: 0; padding: 8px; }
      h3 { margin-bottom: 10px; }
      #map { width: 100%; height: 50vh; border: 1px solid #ccc; border-radius: 8px; }
      #sensors { margin-top: 10px; font-size: 15px; color: #333; }
      .sensor { margin-bottom: 5px; }
      .note { font-size: 13px; color: #666; }
    </style>
  </head>
  <body>
    <h3>Blind Stick Dashboard</h3>
    <div id='map'></div>
    <div id='sensors'>
      <div class='sensor'>Ultrasonic: <span id='ultra'>--</span> cm</div>
      <div class='sensor'>VL53: <span id='vl53'>--</span> mm</div>
      <div class='sensor'>Flame: <span id='flame'>--</span></div>
      <div class='sensor'>Soil: <span id='soil'>--</span></div>
    </div>

    <script src='https://unpkg.com/leaflet@1.9.4/dist/leaflet.js'></script>
    <script>
      // Fixed location coordinates (new location)
      const myLat = 22.9006;
      const myLon = 89.5024;

      const map = L.map('map').setView([myLat, myLon], 17);
      L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
        maxZoom: 19,
        attribution: '&copy; OpenStreetMap contributors'
      }).addTo(map);

      const marker = L.marker([myLat, myLon]).addTo(map);
      marker.bindPopup("Selected Location").openPopup();

      // Fetch sensor values from ESP32
      function updateSensors() {
        fetch('/sensors')
          .then(res => res.json())
          .then(data => {
            document.getElementById('ultra').innerText = data.ultrasonic.toFixed(2);
            document.getElementById('vl53').innerText = data.vl53.toFixed(2);
            document.getElementById('flame').innerText = data.flame;
            document.getElementById('soil').innerText = data.soil;
          });
      }

      // Update sensors every second
      setInterval(updateSensors, 1000);
      updateSensors();
    </script>
  </body>
  </html>
  )rawliteral";
  return html;
}


/* Setup */
void setup() {
  Serial.begin(115200);
  delay(50);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(FLAME_PIN, INPUT);
  pinMode(SOIL_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(VIBRATOR_PIN, OUTPUT);
  pinMode(SOS_PIN, INPUT_PULLUP);
  pinMode(TIME_SWITCH, INPUT_PULLUP);

  Wire.begin();
  if (!lox.begin()) Serial.println("VL53L0X init failed");
  if (!rtc.begin()) { Serial.println("RTC not found"); while (1) delay(1000); }

  GPS_Serial.begin(9600, SERIAL_8N1, 16, 17);

  dfSerial.begin(9600, SERIAL_8N1, 13, 14);
  if (!mp3.begin(dfSerial)) Serial.println("DFPlayer init failed");
  else mp3.volume(30);

  WiFi.begin(ssid, pass);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(400); Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected! IP: " + WiFi.localIP().toString());
    Blynk.begin(auth, ssid, pass);
    Blynk.virtualWrite(V10, WiFi.localIP().toString());
    timeClient.begin();
    updateRTCfromNTP(); // update RTC at start
  } else {
    Serial.println("WiFi not connected (continue without Blynk)");
  }

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", htmlPage());
  });

  // sensors JSON route
  server.on("/sensors", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{";
    json += "\"ultrasonic\":" + String(measureUltrasonicCM()) + ",";
    json += "\"vl53\":" + String(measureVL53()) + ",";
    json += "\"flame\":" + String(analogRead(FLAME_PIN)) + ",";
    json += "\"soil\":" + String(analogRead(SOIL_PIN));
    json += "}";
    request->send(200, "application/json", json);
  });

  server.begin();
}

/* Loop */
void loop() {
  if (WiFi.status() == WL_CONNECTED) Blynk.run();
  while (GPS_Serial.available() > 0) gps.encode(GPS_Serial.read());
  checkSensors();

  if (digitalRead(SOS_PIN) == LOW) {
    sendSOS();
    delay(1000);
  }

  if (digitalRead(TIME_SWITCH) == LOW) {
    delay(40);
    if (digitalRead(TIME_SWITCH) == LOW) {
      announceTimeMP3();
      while (digitalRead(TIME_SWITCH) == LOW) delay(10);
    }
  }

  static unsigned long lastIP = 0;
  if (WiFi.status() == WL_CONNECTED && millis() - lastIP > 60000) {
    Blynk.virtualWrite(V10, WiFi.localIP().toString());
    lastIP = millis();
  }

  if (gps.location.isValid()) {
    Blynk.virtualWrite(V6, gps.location.lat());
    Blynk.virtualWrite(V7, gps.location.lng());
  }

  // update RTC every 5 minutes
  static unsigned long lastRTCupdate = 0;
  if (WiFi.status() == WL_CONNECTED && millis() - lastRTCupdate > 300000) {
    updateRTCfromNTP();
    lastRTCupdate = millis();
  }

  delay(10);
}
