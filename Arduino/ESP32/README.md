# CFtag ESP32 Firmware

Drop-in replacement for the [K2-RFID ESP32 hardware](https://github.com/DnG-Crafts/K2-RFID/tree/main/Arduino/ESP32).  
Same wiring, same pins — adds Spoolman integration and tag reading.

## Hardware

| Signal | GPIO |
|---|---|
| RC522 SPI CS (SS_PIN) | 5 |
| RC522 RST (RST_PIN) | 21 |
| Speaker / Buzzer | 27 |
| RC522 SCK / MOSI / MISO | 18 / 23 / 19 (ESP32 default SPI) |

## Required Arduino Libraries

Install via **Sketch → Include Library → Manage Libraries**:

| Library | Author |
|---|---|
| MFRC522 | GithubCommunity |
| ArduinoJson | Benoit Blanchon |

## Build & Flash

1. Open `Spool_ID/Spool_ID.ino` in Arduino IDE
2. Board: **ESP32 Dev Module** (or your specific variant)
3. Flash size: **4MB** with at least 1MB LittleFS partition  
   *(Tools → Partition Scheme → "Default 4MB with spiffs" or "4MB (1.2MB APP / 1.5MB SPIFFS)")*
4. Upload

## Web Interface

After flashing, connect to WiFi AP **K2_RFID** / password **password**, then open **http://10.1.0.1**

- **⚙ Settings**: Configure AP credentials, join your LAN WiFi, enable Spoolman
- **Link Existing Spool**: Pick from your Spoolman spool list → queue → tap tag
- **Create New Spool**: Fill vendor/material/color → creates spool in Spoolman → queue → tap tag
- **Manual Write**: Original functionality without Spoolman

## Spoolman Setup

1. Open Settings → check **Enable Spoolman** → enter host IP and port (default 7912) → Save
2. The firmware registers `rfid_tag_id_1` and `rfid_tag_id_2` custom fields on first connect
3. After writing a tag, the spool's extra fields are PATCHed with the tag UID

## Tag Format

Writes the standard Creality RFID tag format (same as CFtag Android app):

```
Sector 1 (blocks 4–6, AES-128-ECB encrypted):
  AB124 + vendorId(4) + A2 + filamentId(6) + color(7) + length(4) + serialNum(6) + reserve(6) + printerType(8)

Sector 2 (blocks 8–10, plaintext):
  padding spaces / zeros
```

`serialNum` = Spoolman `spool.id` zero-padded to 6 decimal digits.

## OTA Updates

Upload a new `.bin` firmware via **Settings → Upload Firmware**.  
Upload a new material database (gzip-compressed JSON) via **Settings → Upload Material DB**.
