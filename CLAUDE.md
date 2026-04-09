# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**CFtag** has two components:

1. **Android app** (Java, package `dngsoftware.spoolid`, applicationId `io.github.koen01.cftag`) for reading and writing Creality K1/K2/Hi/CFS RFID filament spool tags using MIFARE Classic 1K NFC tags. Integrates with [Spoolman](https://github.com/Donkie/Spoolman). Android module is under `app/`; Gradle commands run from the repo root.

2. **ESP32 firmware** (`Arduino/ESP32/Spool_ID/`) — a drop-in replacement for the [K2-RFID ESP32 hardware](https://github.com/DnG-Crafts/K2-RFID/tree/main/Arduino/ESP32) (RC522 RFID reader). Serves a web interface that mirrors the Android app's functionality, including Spoolman integration. See `Arduino/ESP32/README.md`.

## Build Commands

```bash
# Build debug APK
./gradlew assembleDebug

# Build release APK
./gradlew assembleRelease

# Install on connected device
./gradlew installDebug

# Clean build
./gradlew clean
```

There are no automated tests. Development is done in Android Studio; the app is deployed to a physical Android device with NFC.

## Architecture

### Tag Data Format

The core domain is the Creality RFID tag format — a 40-character hex string split across MIFARE sectors 1 (blocks 4–6) and 2 (blocks 8–10):

```
| M DD YY | vendorId | batch | filamentId | color   | filamentLen | serialNum | reserve |
| A B1 24 |   0276   |  A2   |   101001   | 0C12E1F |    0165     |  000001   |  000000 |
```

- **Sector 1** (blocks 4–6, 48 bytes): encrypted with AES using a tag-UID-derived key; stores the first half of tag data
- **Sector 2** (blocks 8–10, 48 bytes): plain text; stores the second half
- The sector trailer (block 7) stores the encryption key after first write, changing the tag from "writable" to "encrypted/locked" state

### Spoolman Integration (this fork's key addition)

The `serialNum` field (6 hex digits, zero-padded decimal) stores the Spoolman `spool.id`. This creates a stable cross-system identifier between the physical tag, the printer's JSON state, and the Spoolman database.

**Flow:**
1. User fills in filament details → clicks "Add (Spoolman)" → `openSpoolAdd()` creates a spool via REST API → captures `spool.id` → saves to `SharedPreferences` key `sm_spool_id`
2. User taps NFC tag → `WriteTag()` writes the queued ID into `serialNum` → `UpdateSpoolmanExtraAfterTagWrite()` PATCHes the spool's `extra` field with `creality.serial_num` and `creality.tag_uid_*` (two-way breadcrumb)
3. The queued `sm_spool_id` is sticky (survives multiple tag writes for two-tag-per-spool workflow) but cleared on filament/printer-type change

Spoolman REST calls go through `performSmRequest()` in `MainActivity.java`; settings (`smhost`, `smport`, `enablesm`) are stored in `SharedPreferences`.

### Key Files

| File | Purpose |
|------|---------|
| `MainActivity.java` | Everything: NFC dispatch, UI dialogs, tag read/write, Spoolman REST calls, SSH to printer |
| `Utils.java` | Static helpers: AES key derivation (`createKey`), `cipherData`, SSH via JSch (`sendSShCommand`, `getJsonDB`, `setJsonDB`), Spoolman HTTP (`fetchDataFromApi`), material DB helpers |
| `filamentDB.java` | Room database singleton; one DB per printer type (`material_database_<pType>`) |
| `MatDB.java` | Room DAO for the `filament_table` |
| `Filament.java` | Room entity: `filamentID`, `filamentName`, `filamentVendor`, `filamentParam` (full JSON) |
| `ColorMatcher.java` | Loads `assets/colors.db` (zipped CSV) and finds nearest CSS color name by Euclidean RGB distance |
| `PrinterManager.java` | Persists the printer list to `SharedPreferences` as a JSON array |
| `PrinterOption.java` | Model for a printer entry in the printer list |
| `SpoolItem.java` | Model for a Spoolman spool (id, name, color, remaining weight) |
| `MaterialItem.java` | Model for a filament/material entry |
| `SpoolPickerAdapter.java` | RecyclerView adapter for the searchable Spoolman spool picker dialog |
| `SpoolSpinnerAdapter.java` | Spinner adapter for Spoolman spool selection |
| `spinnerAdapter.java` / `tagAdapter.java` / `jsonAdapter.java` | Adapters for material spinner, tag list, and JSON viewer |
| `tagItem.java` / `jsonItem.java` | Data models for tag and JSON list items |

### NFC Flow

- `LaunchActivity` is the NFC intent target (transparent, `singleInstance`) — it forwards to `MainActivity` and finishes
- `MainActivity` implements `NfcAdapter.ReaderCallback`; `onTagDiscovered()` runs off the main thread
- `currentTag` holds the last detected tag; `ReadTag()` and `WriteTag()` operate on it synchronously (read) or via `ExecutorService` (write)
- Encryption state (`encrypted`, `encKey`) is derived from the tag UID via `createKey()`

### Settings (SharedPreferences keys)

`printer`, `enabledm` (dark mode), `enablesm` (Spoolman), `smhost`, `smport`, `sm_spool_id` (queued spool ID), `autoread`, `newformat`, plus manual-entry field defaults (`mon`, `day`, `yr`, `ven`, `bat`, `mat`, `col`, `len`, `ser`, `res`).

### Network Security

`res/xml/nsc.xml` configures `android:networkSecurityConfig` — allows cleartext HTTP for local Spoolman and printer targets. Uses plain `HttpURLConnection` for Spoolman REST. `Utils.java` contains JSch SSH helpers (`sendSShCommand`, `getJsonDB`, `setJsonDB`) but these are **dead code** — they are not called from `MainActivity.java`. The printer interaction is HTTP-only (downloading the filament DB as a zip).

## ESP32 Firmware (`Arduino/ESP32/`)

### Hardware

Drop-in for the K2-RFID ESP32 board: RC522 RFID reader (SPI), speaker on GPIO 27.

| Signal | GPIO |
|---|---|
| RC522 SS | 5 |
| RC522 RST | 21 |
| Speaker | 27 |

### File Structure

```
Arduino/ESP32/Spool_ID/
  Spool_ID.ino              Main sketch: NFC loop, web server, config, OTA
  src/
    includes.h
    AES/AES.h + AES.cpp     AES-128-ECB; keytype 0 = UID key derivation, keytype 1 = data cipher
    Spoolman/               REST client: fetchSpools, createSpool, patchRfidTag, ensureRfidFields
    WWW/html.h              Full web UI as PROGMEM string
```

### Key Globals (Spool_ID.ino)

- `spoolData` — 48-char tag data string queued for next write
- `pendingSpoolId` — Spoolman spool.id to PATCH after write (0 = none)
- `pendingWrite` — true when a write is queued (either Spoolman or manual)
- `tagWriteCount` — tracks how many tags written for current spool (max 2, for two-tag spools)
- `encrypted` / `ekey` — encryption state and UID-derived key for current tag

### Web API Endpoints

| Method | Path | Purpose |
|---|---|---|
| GET | `/` | Serve web UI |
| GET | `/api/status` | Polled every 1s by browser; returns tag state + event |
| GET | `/api/spools` | Proxy `GET /api/v1/spool` from Spoolman |
| POST | `/api/queuespool` | Queue existing Spoolman spool for writing |
| POST | `/api/createspool` | Create new Spoolman spool then queue |
| POST | `/spooldata` | Manual write (legacy, no Spoolman) |
| GET/POST | `/config` | Read / save config.ini |
| POST | `/update.html` | OTA firmware upload |
| POST | `/updatedb.html` | Upload gzip material DB |

### Config Keys (`/config.ini`)

`AP_SSID`, `AP_PASS`, `WIFI_SSID`, `WIFI_PASS`, `WIFI_HOST`, `PRINTER_HOST`, `SM_ENABLE`, `SM_HOST`, `SM_PORT`

### Required Arduino Libraries

- **MFRC522** by GithubCommunity (Library Manager)
- **ArduinoJson** by Benoit Blanchon (Library Manager)

### Crypto (same keys as Android app)

- `keytype 0` (UID key derivation master): `71 33 62 75 5E 74 31 6E 71 66 5A 28 70 66 24 31`
- `keytype 1` (data cipher master): `48 40 43 46 6B 52 6E 7A 40 4B 41 74 42 4A 70 32`
- Both use AES-128-ECB, no IV, no padding

---

## Android Build Notes

- ViewBinding is enabled (`buildFeatures { viewBinding true }`) — use generated binding classes rather than `findViewById`
- Release builds use ProGuard minification + resource shrinking (`app/proguard-rules.pro`)
- `compileSdk`/`targetSdk` 36, `minSdk` 21, Java 11

## Dependencies (from `gradle/libs.versions.toml`)

- `androidx.room` 2.6.1 — local filament database
- `com.github.mwiede:jsch` 2.27.0 — SSH to Creality printer
- `com.google.android.flexbox` 3.0.0 — color picker UI
- `androidx.documentfile` 1.1.0 — file import/export via Storage Access Framework
- Material Components, ConstraintLayout, AppCompat, Preference (standard AndroidX)
