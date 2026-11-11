# GPS viewer: ESP32-S3 N16R8 + GY-GPS6MV2 (u-blox NEO-6M)

| GY-GPS6MV2 | ESP32-S3 |
|---|---|
| VCC | 3V3, or an external 3.3–5 V supply (see Power) |
| GND | GND |
| TX | GPIO 18 |
| RX | GPIO 17 |

Don't use the board's labeled RX/TX pins (GPIO 44/43). They are hard-wired to the onboard USB-serial chip behind the second USB-C port, which holds RX high. Tested 2026-11-11: GPS on RX gave 0 bytes and a steady-high line.
The module's onboard regulator takes 3.3 to 5 V. On 3V3 its TX swings 0 to 3.3 V, which is safe for the S3.
Factory baud is 9600. The firmware auto-detects 9600, 38400, 115200, 57600 and 4800.

GPS module LED: steady = no fix yet, blinking once a second = fix.
S3 RGB LED: red blink = no NMEA from the GPS, amber blink = NMEA but no fix, green = fix.

## Power

On the first bring-up (2026-11-11), the module was dead when fed from the S3 board: GPS TX sat low and no bytes arrived. It started streaming as soon as it was put on an external supply. If you use an external supply, its ground must be tied to the S3 GND.

## Wi-Fi

- Joins the network in `wifi_secrets.h` (copy `wifi_secrets.example.h`), then the saved network. If that fails, it opens the `GPS-Setup` portal for 3 minutes. Join that from a phone and pick a network.
- It always runs its own access point too: `ESP32-GPS`, password `gps12345`, app at http://192.168.4.1. There is no map there, because the map tiles need internet.
- On a shared network, the app is at http://gps.local or at the IP printed on the serial port.
- To forget the saved network, hold BOOT and tap RST.

## Build

```
TMPDIR=<scratch> arduino-cli compile --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB,CDCOnBoot=cdc" gps_viewer
TMPDIR=<scratch> arduino-cli upload -p /dev/cu.usbmodem12201 --fqbn <same> gps_viewer
```

Requires the WiFiManager library (already installed).
