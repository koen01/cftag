# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**SpoolID** is an Android app (Java, package `dngsoftware.spoolid`) for reading and writing Creality K1/K2/Hi/CFS RFID filament spool tags using MIFARE Classic 1K NFC tags. The working directory for this project is `SpoolID/` — the git root also contains sibling directories (`../Android/`, `../Arduino/`, `../Windows/`) that hold related tooling, but this Claude session focuses on the Android app.

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

### NFC Flow

- `LaunchActivity` is the NFC intent target (transparent, `singleInstance`) — it forwards to `MainActivity` and finishes
- `MainActivity` implements `NfcAdapter.ReaderCallback`; `onTagDiscovered()` runs off the main thread
- `currentTag` holds the last detected tag; `ReadTag()` and `WriteTag()` operate on it synchronously (read) or via `ExecutorService` (write)
- Encryption state (`encrypted`, `encKey`) is derived from the tag UID via `createKey()`

### Settings (SharedPreferences keys)

`printer`, `enabledm` (dark mode), `enablesm` (Spoolman), `smhost`, `smport`, `sm_spool_id` (queued spool ID), `autoread`, `newformat`, plus manual-entry field defaults (`mon`, `day`, `yr`, `ven`, `bat`, `mat`, `col`, `len`, `ser`, `res`).

### Network Security

`res/xml/nsc.xml` configures `android:networkSecurityConfig` — likely allows cleartext for local Spoolman and printer SSH targets. The app uses JSch for SSH to the printer and plain `HttpURLConnection` for Spoolman REST.

## Dependencies (from `gradle/libs.versions.toml`)

- `androidx.room` 2.6.1 — local filament database
- `com.github.mwiede:jsch` 2.27.0 — SSH to Creality printer
- `com.google.android.flexbox` 3.0.0 — color picker UI
- `androidx.documentfile` 1.1.0 — file import/export via Storage Access Framework
- Material Components, ConstraintLayout, AppCompat, Preference (standard AndroidX)
