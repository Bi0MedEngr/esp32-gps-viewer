# ESP32 GPS Viewer

**Launcher: https://bi0medengr.github.io/esp32-gps-viewer/**. Bookmark it; it opens the live page on the ESP32 (gps.local, a remembered IP, or the ESP32-GPS hotspot).

Live GPS coordinates in a browser, from an ESP32-S3 and a GY-GPS6MV2 (u-blox NEO-6M) module. The ESP32 parses the GPS data itself and serves a web app over Wi-Fi. Nothing else needs to be installed.

## Features

- Latitude and longitude in decimal degrees, degrees + decimal minutes, or degrees/minutes/seconds, with Copy and Google Maps buttons
- Altitude, speed, heading, satellites used and seen, HDOP/PDOP, estimated accuracy, UTC time, fix age
- A live map (OpenStreetMap or Esri satellite) with an accuracy circle, a follow mode and a track that downloads as GPX or CSV
- A sky plot and signal-strength bars per satellite, with the satellites used in the fix highlighted
- A raw NMEA console, plus diagnostics that say whether the problem is wiring, baud rate or lack of sky
- Works on phones (tested at 320 and 375 px wide), with light and dark modes
- RGB LED: blinking red = no GPS data, blinking amber = searching, solid green = fix

## Hardware

| GY-GPS6MV2 | ESP32-S3 |
|---|---|
| TX | GPIO 18 |
| RX | GPIO 17 |
| VCC | 3V3 or an external 3.3–5 V supply (common ground) |
| GND | GND |

Don't use the board's labeled RX/TX pins (GPIO 44/43). They're shared with the onboard USB-serial chip. See [gps_viewer/WIRING.md](gps_viewer/WIRING.md).

## Wi-Fi

1. If `wifi_secrets.h` exists (copy it from `wifi_secrets.example.h`), the board joins that network.
2. Otherwise it joins the network saved in flash, or opens a **GPS-Setup** portal for 3 minutes so you can pick one from a phone.
3. It always runs its own access point too: **ESP32-GPS** / `gps12345`, at http://192.168.4.1. There is no map there, because the tiles need internet.

On your network the app is at **http://gps.local**, or at the IP printed on the USB serial port.

## Build

Requires arduino-cli, the esp32 core 3.x and the WiFiManager library. Board: ESP32-S3 N16R8.

```
arduino-cli compile -u -p <port> --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB,CDCOnBoot=cdc" gps_viewer
```

## API

- `GET /api/gps`: JSON with the fix, DOPs, satellites `[prn, elevation, azimuth, snr, usedInFix]` and UART diagnostics
- `GET /api/nmea?since=N`: the latest raw NMEA lines (first line is the sequence number)
