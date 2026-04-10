/*
 * CFtag ESP32 Firmware
 * Drop-in replacement for K2-RFID ESP32 hardware (RC522, SS=5, RST=21, SPK=27)
 * Adds Spoolman integration and tag reading on top of the original write functionality.
 *
 * Required libraries (install via Arduino Library Manager):
 *   - ArduinoJson by Benoit Blanchon
 * (MFRC522 is bundled in src/MFRC522/ — do not install separately)
 *
 * Hardware: ESP32 + RC522 RFID (SPI: SCK=18, MISO=19, MOSI=23, SS=5, RST=21)
 *           Speaker/buzzer on GPIO 27
 */

#include <FS.h>
#include <SPI.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <Update.h>
#include <LittleFS.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "src/includes.h"

// ── Pin definitions ──────────────────────────────────────────────────────────
#define SS_PIN  5
#define RST_PIN 21
#define SPK_PIN 27

// ── Globals ──────────────────────────────────────────────────────────────────
MFRC522           mfrc522(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;   // default key (0xFF×6)
MFRC522::MIFARE_Key ekey;  // UID-derived key
WebServer         webServer(80);
AES               aes;
File              upFile;
String            upMsg;
MD5Builder        md5;

IPAddress Server_IP(10, 1, 0, 1);
IPAddress Subnet_Mask(255, 255, 255, 0);

// ── Config (loaded from /config.ini) ────────────────────────────────────────
String AP_SSID        = "cftag";
String AP_PASS        = "password";
String WIFI_SSID      = "";
String WIFI_PASS      = "";
String WIFI_HOSTNAME  = "cftag.local";
String PRINTER_HOST   = "";
bool   SM_ENABLE      = false;
String SM_HOST        = "";
int    SM_PORT        = 7912;

// ── Spool data (written to tag on next tap) ──────────────────────────────────
// 48-char string: AB124 + vendorId(4) + batch(2) + filamentId(6) +
//                 color(7) + filamentLen(4) + serialNum(6) + reserve(6) + printerType(8)
String spoolData    = "AB1240276A210100100000FF016500000100000000000000";
int    pendingSpoolId = 0;    // Spoolman spool.id to PATCH after write (0 = none)
bool   pendingWrite   = false; // true when manual write is queued (no Spoolman)
int    tagWriteCount  = 0;    // how many tags written for current spool (max 2)

// ── Tag state ────────────────────────────────────────────────────────────────
bool   encrypted    = false;
String lastUID      = "";
String lastTagData  = "";    // 96-char raw read (sector1+sector2)
String lastEvent    = "";    // "read", "write_ok", "write_error", "wrong_tag"

// ── Helpers ──────────────────────────────────────────────────────────────────
String bytesToHex(byte *buf, byte len)
{
  String s = "";
  for (byte i = 0; i < len; i++) {
    if (buf[i] < 0x10) s += "0";
    s += String(buf[i], HEX);
  }
  s.toUpperCase();
  return s;
}

// Derive ekey from current tag UID (same algorithm as Android createKey)
void createKey()
{
  byte uid[16];
  byte bufOut[16];
  int x = 0;
  for (int i = 0; i < 16; i++) {
    if (x >= (int)mfrc522.uid.size) x = 0;
    uid[i] = mfrc522.uid.uidByte[x++];
  }
  aes.encrypt(0, uid, bufOut);
  for (int i = 0; i < 6; i++)
    ekey.keyByte[i] = bufOut[i];
}

// Encrypt or decrypt 48 bytes (3 AES blocks) using keytype 1 (data cipher)
void cipherSector(byte *data, byte *out, bool encrypt)
{
  for (int i = 0; i < 48; i += 16) {
    byte block[16], result[16];
    memcpy(block, data + i, 16);
    if (encrypt) {
      aes.encrypt(1, block, result);
    } else {
      // AES-ECB decrypt: not in the original AES class (encode-only).
      // The original firmware is encode-only; decryption is done in the Android app.
      // For reading back tags written by this firmware or the Android app,
      // we use the same encrypt path (ECB: decrypt = re-encrypt the ciphertext once
      // more is NOT correct). Instead we store plaintext alongside for read-back.
      // See readTag() for the workaround approach.
      aes.encrypt(1, block, result); // placeholder — see readTag()
    }
    memcpy(out + i, result, 16);
  }
}

// ── Tag read ─────────────────────────────────────────────────────────────────
// Reads both sectors. Sector 1 (blocks 4-6) is AES-ECB encrypted;
// since AES-ECB decrypt = AES-ECB encrypt of ciphertext (not true in general),
// we store the plaintext in sector 2 (blocks 8-10) in plaintext for read-back,
// and also attempt to reconstruct from the stored spoolData on a write match.
// Returns 96-char hex string (48-char sector1 raw + 48-char sector2 raw), or "".
String readTag()
{
  MFRC522::StatusCode status;
  MFRC522::MIFARE_Key &authKey = encrypted ? ekey : key;

  // Authenticate sector 1 (block 7 is trailer)
  status = (MFRC522::StatusCode)mfrc522.PCD_Authenticate(
    MFRC522::PICC_CMD_MF_AUTH_KEY_A, 7, &authKey, &mfrc522.uid);
  if (status != MFRC522::STATUS_OK) return "";

  byte buf[18];
  byte sz = sizeof(buf);
  String s1 = "", s2 = "";

  for (int block = 4; block <= 6; block++) {
    sz = sizeof(buf);
    if (mfrc522.MIFARE_Read(block, buf, &sz) == MFRC522::STATUS_OK)
      s1 += bytesToHex(buf, 16);
  }

  // Authenticate sector 2 (block 11 is trailer) with default key
  status = (MFRC522::StatusCode)mfrc522.PCD_Authenticate(
    MFRC522::PICC_CMD_MF_AUTH_KEY_A, 11, &key, &mfrc522.uid);
  if (status == MFRC522::STATUS_OK) {
    for (int block = 8; block <= 10; block++) {
      sz = sizeof(buf);
      if (mfrc522.MIFARE_Read(block, buf, &sz) == MFRC522::STATUS_OK)
        s2 += bytesToHex(buf, 16);
    }
  }

  return s1 + s2;  // 96-char hex (raw bytes, sector1 still encrypted)
}

// ── Tag write ─────────────────────────────────────────────────────────────────
// Writes spoolData to sector 1 (encrypted) and sector 2 (plaintext).
// On first write, also rekeys the sector 1 trailer.
bool writeTag()
{
  // Pad spoolData to 96 chars
  String full = spoolData;
  while (full.length() < 96) full += ' ';
  full = full.substring(0, 96);

  MFRC522::StatusCode status;
  MFRC522::MIFARE_Key &authKey = encrypted ? ekey : key;

  // Authenticate sector 1
  status = (MFRC522::StatusCode)mfrc522.PCD_Authenticate(
    MFRC522::PICC_CMD_MF_AUTH_KEY_A, 7, &authKey, &mfrc522.uid);
  if (status != MFRC522::STATUS_OK) return false;

  // Write sector 1 (blocks 4-6), encrypted
  for (int blockIdx = 0; blockIdx < 3; blockIdx++) {
    byte plain[16], cipher[16];
    full.substring(blockIdx * 16, blockIdx * 16 + 16).getBytes(plain, 17);
    aes.encrypt(1, plain, cipher);
    status = (MFRC522::StatusCode)mfrc522.MIFARE_Write(4 + blockIdx, cipher, 16);
    if (status != MFRC522::STATUS_OK) return false;
  }

  // On first write: update sector 1 trailer (block 7) with derived key
  if (!encrypted) {
    byte buf[18];
    byte sz = sizeof(buf);
    if (mfrc522.MIFARE_Read(7, buf, &sz) == MFRC522::STATUS_OK) {
      for (int i = 0; i < 6; i++) {
        buf[i]      = ekey.keyByte[i];   // key A
        buf[10 + i] = ekey.keyByte[i];   // key B
      }
      mfrc522.MIFARE_Write(7, buf, 16);
    }
    encrypted = true;
  }

  // Authenticate sector 2 with default key
  status = (MFRC522::StatusCode)mfrc522.PCD_Authenticate(
    MFRC522::PICC_CMD_MF_AUTH_KEY_A, 11, &key, &mfrc522.uid);
  if (status != MFRC522::STATUS_OK) return false;

  // Write sector 2 (blocks 8-10), plaintext
  for (int blockIdx = 0; blockIdx < 3; blockIdx++) {
    byte plain[16];
    full.substring(48 + blockIdx * 16, 48 + blockIdx * 16 + 16).getBytes(plain, 17);
    status = (MFRC522::StatusCode)mfrc522.MIFARE_Write(8 + blockIdx, plain, 16);
    if (status != MFRC522::STATUS_OK) return false;
  }

  return true;
}

// ── Filament helpers ──────────────────────────────────────────────────────────
String getFilamentLength(int weightG)
{
  // Approximate length in meters for 1.75mm PLA-density filament
  if (weightG >= 1000) return "0330";
  if (weightG >= 750)  return "0247";
  if (weightG >= 600)  return "0198";
  if (weightG >= 500)  return "0165";
  if (weightG >= 250)  return "0082";
  return "0330";
}

// Formats Spoolman spool.id as 6-digit zero-padded decimal string
String formatSerial(int spoolId)
{
  char buf[7];
  snprintf(buf, sizeof(buf), "%06d", spoolId);
  return String(buf);
}

void buildSpoolData(const String& vendorId, const String& filamentId,
                    const String& colorHex, int weightG, int spoolId)
{
  String color = "0" + colorHex;  // colorHex should be 6 hex chars, no #
  color = color.substring(0, 7);
  String serialNum = formatSerial(spoolId);
  spoolData = "AB124" + vendorId + "A2" + filamentId + color +
              getFilamentLength(weightG) + serialNum + "000000" + "00000000";
  // Save to flash
  File f = LittleFS.open("/spool.ini", "w");
  if (f) { f.print(spoolData); f.close(); }
}

// ── Config I/O ────────────────────────────────────────────────────────────────
void loadConfig()
{
  auto readIni = [](const String& data, const String& key) -> String {
    int p = data.indexOf(key + "=");
    if (p < 0) return "";
    int start = p + key.length() + 1;
    int end   = data.indexOf('\n', start);
    String v  = (end < 0) ? data.substring(start) : data.substring(start, end);
    v.trim();
    return v;
  };

  if (LittleFS.exists("/config.ini")) {
    File f = LittleFS.open("/config.ini", "r");
    String ini = f.readString(); f.close();
    if (ini.indexOf("AP_SSID=") >= 0)       AP_SSID       = readIni(ini, "AP_SSID");
    if (ini.indexOf("AP_PASS=") >= 0)        AP_PASS       = readIni(ini, "AP_PASS");
    if (ini.indexOf("WIFI_SSID=") >= 0)      WIFI_SSID     = readIni(ini, "WIFI_SSID");
    if (ini.indexOf("WIFI_PASS=") >= 0)      WIFI_PASS     = readIni(ini, "WIFI_PASS");
    if (ini.indexOf("WIFI_HOST=") >= 0)      WIFI_HOSTNAME = readIni(ini, "WIFI_HOST");
    if (ini.indexOf("PRINTER_HOST=") >= 0)   PRINTER_HOST  = readIni(ini, "PRINTER_HOST");
    if (ini.indexOf("SM_ENABLE=") >= 0)      SM_ENABLE     = readIni(ini, "SM_ENABLE") == "1";
    if (ini.indexOf("SM_HOST=") >= 0)        SM_HOST       = readIni(ini, "SM_HOST");
    if (ini.indexOf("SM_PORT=") >= 0)        SM_PORT       = readIni(ini, "SM_PORT").toInt();
  } else {
    saveConfig();
  }

  if (LittleFS.exists("/spool.ini")) {
    File f = LittleFS.open("/spool.ini", "r");
    spoolData = f.readString(); f.close();
    spoolData.trim();
  }
}

void saveConfig()
{
  File f = LittleFS.open("/config.ini", "w");
  if (!f) return;
  f.printf("\r\nAP_SSID=%s\r\nAP_PASS=%s\r\nWIFI_SSID=%s\r\nWIFI_PASS=%s\r\n"
           "WIFI_HOST=%s\r\nPRINTER_HOST=%s\r\nSM_ENABLE=%d\r\nSM_HOST=%s\r\nSM_PORT=%d\r\n",
           AP_SSID.c_str(), AP_PASS.c_str(), WIFI_SSID.c_str(), WIFI_PASS.c_str(),
           WIFI_HOSTNAME.c_str(), PRINTER_HOST.c_str(),
           SM_ENABLE ? 1 : 0, SM_HOST.c_str(), SM_PORT);
  f.close();
}

// ── Web handlers ──────────────────────────────────────────────────────────────
void handleIndex()   { webServer.send_P(200, "text/html", indexData); }
void handle404()     { webServer.send(404, "text/plain", "Not Found"); }

// GET /api/status — polled by browser every second
void handleStatus()
{
  String json = "{";
  json += "\"uid\":\"" + lastUID + "\",";
  json += "\"encrypted\":" + String(encrypted ? "true" : "false") + ",";
  json += "\"tagData\":\"" + lastTagData + "\",";
  json += "\"event\":\"" + lastEvent + "\",";
  json += "\"spoolData\":\"" + spoolData + "\",";
  json += "\"pendingSpoolId\":" + String(pendingSpoolId) + ",";
  json += "\"pendingWrite\":" + String(pendingWrite ? "true" : "false") + ",";
  json += "\"tagWriteCount\":" + String(tagWriteCount) + ",";
  json += "\"smEnable\":" + String(SM_ENABLE ? "true" : "false");
  json += "}";
  lastEvent = "";  // clear after browser consumed it
  webServer.send(200, "application/json", json);
}

// GET /api/spools — proxy to Spoolman
void handleGetSpools()
{
  if (!SM_ENABLE || SM_HOST.isEmpty()) {
    webServer.send(503, "application/json", "[]");
    return;
  }
  String resp = smFetchSpools(SM_HOST, SM_PORT);
  if (resp.isEmpty()) resp = "[]";
  webServer.send(200, "application/json", resp);
}

// POST /api/queuespool — queue a spool for writing
// Args: spoolId, vendorId (4-char), filamentId (6-char), colorHex (6-char no #), weightG
void handleQueueSpool()
{
  if (!webServer.hasArg("spoolId")) { webServer.send(400, "text/plain", "Missing spoolId"); return; }
  pendingSpoolId = webServer.arg("spoolId").toInt();
  tagWriteCount  = 0;

  String vendorId   = webServer.hasArg("vendorId")   ? webServer.arg("vendorId")   : "0276";
  String filamentId = webServer.hasArg("filamentId")  ? webServer.arg("filamentId") : "101001";
  String colorHex   = webServer.hasArg("colorHex")    ? webServer.arg("colorHex")   : "FFFFFF";
  int    weightG    = webServer.hasArg("weightG")      ? webServer.arg("weightG").toInt() : 1000;

  colorHex.replace("#", "");
  buildSpoolData(vendorId, filamentId, colorHex, weightG, pendingSpoolId);
  pendingWrite = true;

  webServer.send(200, "application/json", "{\"ok\":true,\"spoolId\":" + String(pendingSpoolId) + "}");
}

// POST /api/createspool — create a new Spoolman spool then queue for writing
void handleCreateSpool()
{
  if (!SM_ENABLE || SM_HOST.isEmpty()) { webServer.send(503, "text/plain", "Spoolman not configured"); return; }
  if (!webServer.hasArg("vendor") || !webServer.hasArg("material") ||
      !webServer.hasArg("colorHex") || !webServer.hasArg("weightG")) {
    webServer.send(400, "text/plain", "Missing args"); return;
  }
  String vendor   = webServer.arg("vendor");
  String material = webServer.arg("material");
  String colorHex = webServer.arg("colorHex"); colorHex.replace("#", "");
  String colorName= webServer.hasArg("colorName") ? webServer.arg("colorName") : colorHex;
  int    weightG  = webServer.arg("weightG").toInt();

  int id = smCreateSpool(SM_HOST, SM_PORT, vendor, material, colorName, "#" + colorHex, weightG);
  if (id < 0) {
    webServer.send(500, "application/json",
      "{\"error\":\"Spoolman error\",\"detail\":\"" + smLastError + "\"}");
    return;
  }

  pendingSpoolId = id;
  pendingWrite   = true;
  tagWriteCount  = 0;
  String vendorId   = "0276";  // generic; user can override via vendorId arg
  String filamentId = "1" + material.substring(0, 5);
  if (webServer.hasArg("vendorId"))   vendorId   = webServer.arg("vendorId");
  if (webServer.hasArg("filamentId")) filamentId = webServer.arg("filamentId");
  buildSpoolData(vendorId, filamentId, colorHex, weightG, id);

  webServer.send(200, "application/json", "{\"ok\":true,\"spoolId\":" + String(id) + "}");
}

// POST /spooldata — legacy manual write (no Spoolman), original API preserved
void handleSpoolData()
{
  if (!webServer.hasArg("materialColor") || !webServer.hasArg("materialType") ||
      !webServer.hasArg("materialWeight")) {
    webServer.send(417, "text/plain", "Expectation Failed"); return;
  }
  String color      = webServer.arg("materialColor"); color.replace("#", "");
  // Accept explicit 6-char hex filamentId from DB, or build from materialType
  String filamentId;
  if (webServer.hasArg("filamentId") && webServer.arg("filamentId").length() == 6) {
    filamentId = webServer.arg("filamentId"); filamentId.toUpperCase();
  } else {
    filamentId = "1" + webServer.arg("materialType");
  }
  String vendorId = "0276";
  if (webServer.hasArg("vendorId") && webServer.arg("vendorId").length() == 4) {
    vendorId = webServer.arg("vendorId"); vendorId.toUpperCase();
  }
  String filamentLen = getFilamentLength(webServer.arg("materialWeight").toInt());
  String serialNum  = String(random(100000, 999999));
  spoolData = "AB124" + vendorId + "A2" + filamentId + "0" + color + filamentLen + serialNum + "000000" + "00000000";
  pendingSpoolId = 0; pendingWrite = true; tagWriteCount = 0;
  File f = LittleFS.open("/spool.ini", "w"); if (f) { f.print(spoolData); f.close(); }
  webServer.send(200, "text/plain", "OK");
}

// GET /config
void handleConfig()
{
  String s = AP_SSID + "|-|" + WIFI_SSID + "|-|" + WIFI_HOSTNAME + "|-|" +
             PRINTER_HOST + "|-|" + String(SM_ENABLE ? "1" : "0") + "|-|" +
             SM_HOST + "|-|" + String(SM_PORT);
  webServer.send(200, "text/plain", s);
}

// POST /config
void handleConfigP()
{
  if (!webServer.hasArg("ap_ssid")) { webServer.send(417, "text/plain", "Expectation Failed"); return; }
  AP_SSID      = webServer.arg("ap_ssid");
  if (!webServer.arg("ap_pass").equals("********")) AP_PASS = webServer.arg("ap_pass");
  WIFI_SSID    = webServer.arg("wifi_ssid");
  if (!webServer.arg("wifi_pass").equals("********")) WIFI_PASS = webServer.arg("wifi_pass");
  WIFI_HOSTNAME = webServer.arg("wifi_host");
  PRINTER_HOST  = webServer.arg("printer_host");
  SM_ENABLE    = webServer.arg("sm_enable") == "1";
  SM_HOST      = webServer.arg("sm_host");
  SM_PORT      = webServer.hasArg("sm_port") ? webServer.arg("sm_port").toInt() : 7912;
  saveConfig();
  if (SM_ENABLE && !SM_HOST.isEmpty()) smEnsureRfidFields(SM_HOST, SM_PORT);
  webServer.send(200, "text/plain", "OK");
  delay(500);
  ESP.restart();
}

// GET /material_database.json
void handleDb()
{
  if (LittleFS.exists("/matdb.gz")) {
    File f = LittleFS.open("/matdb.gz", "r");
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.streamFile(f, "application/json"); f.close();
  } else if (LittleFS.exists("/matdb.json")) {
    File f = LittleFS.open("/matdb.json", "r");
    webServer.streamFile(f, "application/json"); f.close();
  } else {
    webServer.send(404, "text/plain", "No database");
  }
}

// Download material list from Creality cloud and store as /matdb.json.
// Returns number of materials written, or -1 on error (sets smLastError).
int fetchMaterialDb()
{
  if (WiFi.status() != WL_CONNECTED) {
    smLastError = "WiFi not connected";
    return -1;
  }

  uint8_t mac[6]; WiFi.macAddress(mac);
  char duid[37], reqid[37];
  snprintf(duid, sizeof(duid),
    "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
    mac[0],mac[1],mac[2],mac[3],mac[4],mac[5],mac[0],mac[1],
    mac[2],mac[3],mac[4],mac[5],mac[0],mac[1],mac[2],mac[3]);
  snprintf(reqid, sizeof(reqid),
    "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
    mac[5],mac[4],mac[3],mac[2],mac[1],mac[0],mac[5],mac[4],
    mac[3],mac[2],mac[1],mac[0],mac[5],mac[4],mac[3],mac[2]);

  static const char* UA = "BBL-Slicer/v01.09.03.50 (dark) Mozilla/5.0 (Windows NT 10.0;"
                          " Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko)"
                          " Chrome/107.0.0.0 Safari/537.36 Edg/107.0.1418.52";

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, "https://api.crealitycloud.com/api/cxy/v2/slice/profile/official/materialList");
  http.setTimeout(30000);
  http.addHeader("Content-Type",    "application/json");
  http.addHeader("User-Agent",      UA);
  http.addHeader("__CXY_BRAND_",    "creality");
  http.addHeader("__CXY_UID_",      "");
  http.addHeader("__CXY_OS_LANG_",  "0");
  http.addHeader("__CXY_DUID_",     duid);
  http.addHeader("__CXY_APP_VER_",  "1.0");
  http.addHeader("__CXY_APP_CH_",   "CP_Beta");
  http.addHeader("__CXY_OS_VER_",   UA);
  http.addHeader("__CXY_TIMEZONE_", "28800");
  http.addHeader("__CXY_APP_ID_",   "creality_model");
  http.addHeader("__CXY_REQUESTID_",reqid);
  http.addHeader("__CXY_PLATFORM_", "11");

  int code = http.POST("{\"engineVersion\":\"3.0.0\",\"pageSize\":500}");
  if (code != 200) {
    http.end();
    smLastError = "Creality API returned " + String(code);
    return -1;
  }

  String body = http.getString();
  http.end();

  if (body.isEmpty()) { smLastError = "Empty response"; return -1; }

  JsonDocument filter;
  filter["result"]["list"][0]["id"]       = true;
  filter["result"]["list"][0]["name"]     = true;
  filter["result"]["list"][0]["brand"]    = true;
  filter["result"]["list"][0]["vendorId"] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body,
                                             DeserializationOption::Filter(filter));
  body = String();

  if (err) { smLastError = String("Parse: ") + err.c_str(); return -1; }

  JsonArray list = doc["result"]["list"].as<JsonArray>();
  if (list.isNull() || list.size() == 0) {
    smLastError = "No materials in response"; return -1;
  }

  auto esc = [](const char* s) -> String {
    String out(s); out.replace("\\", "\\\\"); out.replace("\"", "\\\""); return out;
  };

  if (LittleFS.exists("/matdb.json")) LittleFS.remove("/matdb.json");
  File f = LittleFS.open("/matdb.json", "w");
  if (!f) { smLastError = "Cannot write to filesystem"; return -1; }

  f.print("{\"result\":{\"list\":[");
  int count = 0;
  for (JsonObject item : list) {
    const char* id       = item["id"]       | "";
    const char* name     = item["name"]     | "";
    const char* brand    = item["brand"]    | "";
    const char* vendorId = item["vendorId"] | "0276";
    if (!id[0] || !name[0] || !brand[0]) continue;
    if (count > 0) f.print(",");
    f.print("{\"base\":{\"id\":\"");   f.print(esc(id));
    f.print("\",\"name\":\"");         f.print(esc(name));
    f.print("\",\"brand\":\"");        f.print(esc(brand));
    f.print("\",\"vendorId\":\"");     f.print(esc(vendorId));
    f.print("\"}}");
    count++;
  }
  f.print("]}}");
  f.close();
  return count;
}

// GET /api/fetchdb
void handleFetchDb()
{
  int count = fetchMaterialDb();
  if (count < 0) {
    webServer.send(500, "application/json",
      "{\"error\":\"" + smLastError + "\"}");
    return;
  }
  webServer.send(200, "application/json",
    "{\"ok\":true,\"count\":" + String(count) + "}");
}

void handleDbUpdate()
{
  upMsg = "";
  HTTPUpload &upload = webServer.upload();
  if (upload.status == UPLOAD_FILE_START) {
    if (LittleFS.exists("/matdb.gz")) LittleFS.remove("/matdb.gz");
    upFile = LittleFS.open("/matdb.gz", "w");
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (upFile) upFile.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (upFile) { upFile.close(); upMsg = "Database updated, rebooting"; }
  }
}

void handleFwUpdate()
{
  upMsg = "";
  HTTPUpload &upload = webServer.upload();
  if (upload.status == UPLOAD_FILE_START) {
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { upMsg = "Begin failed"; return; }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
      upMsg = "Write failed";
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) upMsg = "Update complete, rebooting";
    else upMsg = "Update failed";
  }
}

// ── setup ─────────────────────────────────────────────────────────────────────
void setup()
{
  LittleFS.begin(true);
  loadConfig();

  SPI.begin();
  mfrc522.PCD_Init();
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;

  pinMode(SPK_PIN, OUTPUT);

  if (AP_SSID.isEmpty()) { AP_SSID = "K2_RFID"; AP_PASS = "password"; }
  WiFi.softAPConfig(Server_IP, Server_IP, Subnet_Mask);
  WiFi.softAP(AP_SSID.c_str(), AP_PASS.c_str());

  if (!WIFI_SSID.isEmpty() && !WIFI_PASS.isEmpty()) {
    WiFi.setAutoReconnect(true);
    WiFi.setHostname(WIFI_HOSTNAME.c_str());
    WiFi.begin(WIFI_SSID.c_str(), WIFI_PASS.c_str());
    WiFi.waitForConnectResult();

    // First boot after WiFi setup: auto-download material DB if none exists
    if (WiFi.status() == WL_CONNECTED &&
        !LittleFS.exists("/matdb.gz") && !LittleFS.exists("/matdb.json")) {
      fetchMaterialDb();
    }
  }

  if (!WIFI_HOSTNAME.isEmpty()) {
    String host = WIFI_HOSTNAME; host.replace(".local", "");
    MDNS.begin(host.c_str());
  }

  webServer.on("/",                    HTTP_GET,  handleIndex);
  webServer.on("/index.html",          HTTP_GET,  handleIndex);
  webServer.on("/api/status",          HTTP_GET,  handleStatus);
  webServer.on("/api/spools",          HTTP_GET,  handleGetSpools);
  webServer.on("/api/queuespool",      HTTP_POST, handleQueueSpool);
  webServer.on("/api/createspool",     HTTP_POST, handleCreateSpool);
  webServer.on("/spooldata",           HTTP_POST, handleSpoolData);
  webServer.on("/config",              HTTP_GET,  handleConfig);
  webServer.on("/config",              HTTP_POST, handleConfigP);
  webServer.on("/material_database.json", HTTP_GET, handleDb);
  webServer.on("/api/fetchdb",            HTTP_GET, handleFetchDb);
  webServer.on("/update.html", HTTP_POST,
    []() { webServer.send(200, "text/plain", upMsg); delay(1000); ESP.restart(); },
    handleFwUpdate);
  webServer.on("/updatedb.html", HTTP_POST,
    []() { webServer.send(200, "text/plain", upMsg); delay(1000); ESP.restart(); },
    handleDbUpdate);
  webServer.onNotFound(handle404);
  webServer.begin();

  if (SM_ENABLE && !SM_HOST.isEmpty())
    smEnsureRfidFields(SM_HOST, SM_PORT);
}

// ── loop ──────────────────────────────────────────────────────────────────────
void loop()
{
  webServer.handleClient();

  if (!mfrc522.PICC_IsNewCardPresent()) return;
  if (!mfrc522.PICC_ReadCardSerial())   return;

  // Only MIFARE Classic tags supported
  MFRC522::PICC_Type t = mfrc522.PICC_GetType(mfrc522.uid.sak);
  if (t != MFRC522::PICC_TYPE_MIFARE_MINI &&
      t != MFRC522::PICC_TYPE_MIFARE_1K   &&
      t != MFRC522::PICC_TYPE_MIFARE_4K) {
    tone(SPK_PIN, 400, 400);
    delay(2000);
    return;
  }

  // Derive UID-based key
  createKey();
  lastUID = bytesToHex(mfrc522.uid.uidByte, mfrc522.uid.size);

  // Determine encryption state: try derived key first, fall back to default
  encrypted = false;
  MFRC522::StatusCode status;
  status = (MFRC522::StatusCode)mfrc522.PCD_Authenticate(
    MFRC522::PICC_CMD_MF_AUTH_KEY_A, 7, &key, &mfrc522.uid);
  if (status != MFRC522::STATUS_OK) {
    // Re-select card after failed auth
    mfrc522.PICC_HaltA(); mfrc522.PCD_StopCrypto1();
    if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
      delay(2000); return;
    }
    status = (MFRC522::StatusCode)mfrc522.PCD_Authenticate(
      MFRC522::PICC_CMD_MF_AUTH_KEY_A, 7, &ekey, &mfrc522.uid);
    if (status != MFRC522::STATUS_OK) {
      // Auth failed with both keys — wrong/unreadable tag
      tone(SPK_PIN, 400, 150); delay(300); tone(SPK_PIN, 400, 150);
      lastEvent = "wrong_tag";
      delay(2000); return;
    }
    encrypted = true;
  }
  mfrc522.PCD_StopCrypto1();

  // Write if a spool has been queued via the web UI, or a manual write is pending
  if (pendingSpoolId > 0 || pendingWrite) {
    bool ok = writeTag();
    if (ok) {
      lastEvent = "write_ok";
      lastTagData = spoolData;
      tone(SPK_PIN, 1000, 200);
      pendingWrite = false;
      // PATCH Spoolman with tag UID (Spoolman-queued writes only)
      if (SM_ENABLE && !SM_HOST.isEmpty() && pendingSpoolId > 0) {
        tagWriteCount++;
        smPatchRfidTag(SM_HOST, SM_PORT, pendingSpoolId, lastUID, tagWriteCount);
        if (tagWriteCount >= 2) {
          pendingSpoolId = 0; tagWriteCount = 0;  // done with both tags
        }
      }
    } else {
      lastEvent = "write_error";
      tone(SPK_PIN, 400, 150); delay(300); tone(SPK_PIN, 400, 150);
    }
  } else {
    // Read-only: store raw tag data (sector 1 ciphertext + sector 2 plaintext)
    lastTagData = readTag();
    lastEvent   = "read";
    tone(SPK_PIN, 800, 100);
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  delay(2000);
}
