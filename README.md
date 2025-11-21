Blind Stick (ESP32)

This project is a smart blind-assist system built around the ESP32. It combines obstacle detection, fire sensing, environmental monitoring, GPS tracking, voice time announcements, an online dashboard, and automated RTC updates through NTP. The goal is to help users navigate safely and allow caregivers to monitor them remotely.

1. Hardware Functions

Ultrasonic Sensor (HC-SR04)
Measures forward distance. If an obstacle comes closer than 40 cm, the system triggers an alert.

VL53L0X Time-of-Flight Sensor
Provides precise short-range distance readings. Alerts if the distance crosses 200 mm.

Flame Sensor
Detects fire or high heat sources.

Soil/Moisture Sensor
Reads basic moisture levels (used here as a general analog sensor).

Buzzer + Vibrating Motor
Activated when any sensor detects danger, giving a combined sound and vibration warning.

DFPlayer Mini (MP3 Module)
Plays audio messages. It also announces the current time using pre-recorded tracks.

RTC DS3231
Keeps accurate time. The clock automatically updates through NTP whenever WiFi is available.

GPS Module (using Serial2)
Provides latitude and longitude. If an SOS is triggered, the location is sent to Blynk.

2. Connectivity & Cloud Features

WiFi Connection
The device connects to a hotspot or router using stored credentials.

Blynk IoT Platform
Used to:

Upload live sensor readings (V1–V5).

Send GPS coordinates (V6, V7).

Share the ESP32’s local IP (V10).

Send an SOS message ("HELP ME!!!") through V8.

Control a status LED on V11.

3. Web Dashboard (ESP32 Local Server)

The ESP32 runs its own local webpage at port 80.
The page includes:

A Leaflet map with a fixed location view.

Live sensor values fetched from /sensors.

Anyone on the same network can open the ESP32’s IP in a browser and see real-time data.

4. Time Announcement System

Pressing the time switch makes the MP3 module announce:

“It is…”

The current hour

The minute (or “o’clock”)

AM/PM

Hour and minute values are mapped to track numbers.

5. Emergency SOS Mode

Pressing the SOS switch sends:

A text alert (“HELP ME!!!”)

GPS latitude and longitude

Visual indicator via Blynk

This helps remote guardians locate the user quickly.

6. Main Loop Behavior

The loop handles:

Blynk background processes

GPS data decoding

Sensor checking and alerts

SOS detection

Time-announcement switch

Periodic updates (IP address, RTC sync)
