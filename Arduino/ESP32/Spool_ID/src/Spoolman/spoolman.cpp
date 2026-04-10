#include "spoolman.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

String smLastError = "";  // last non-2xx code + body for diagnostics

static String smRequest(const String& host, int port, const String& path,
                        const String& method, const String& body = "")
{
  HTTPClient http;
  String url = "http://" + host + ":" + String(port) + path;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Accept", "application/json");
  http.setTimeout(10000);

  int code = -1;
  if (method == "GET")       code = http.GET();
  else if (method == "POST") code = http.POST(body);
  else if (method == "PATCH") {
    http.addHeader("Content-Length", String(body.length()));
    code = http.sendRequest("PATCH", body);
  }

  String result = "";
  if (code >= 200 && code < 300) {
    result = http.getString();
  } else {
    smLastError = String(code) + " " + http.getString();
    smLastError = smLastError.substring(0, 200);
  }

  http.end();
  return result;
}

String smFetchSpools(const String& host, int port)
{
  return smRequest(host, port, "/api/v1/spool?allow_archived=false", "GET");
}

// Returns vendor id or -1
static int smFindOrCreateVendor(const String& host, int port, const String& name)
{
  String resp = smRequest(host, port, "/api/v1/vendor", "GET");
  if (resp.length() > 0) {
    JsonDocument doc;
    if (deserializeJson(doc, resp) == DeserializationError::Ok) {
      for (JsonObject v : doc.as<JsonArray>()) {
        if (v["name"].as<String>() == name)
          return v["id"].as<int>();
      }
    }
  }
  // create
  String body = "{\"name\":\"" + name + "\"}";
  resp = smRequest(host, port, "/api/v1/vendor", "POST", body);
  if (resp.length() > 0) {
    JsonDocument doc;
    if (deserializeJson(doc, resp) == DeserializationError::Ok)
      return doc["id"].as<int>();
  }
  return -1;
}

// Returns filament id or -1
// Matches Android: GET all filaments, search by vendor id + name (case-insensitive)
static int smFindOrCreateFilament(const String& host, int port, int vendorId,
                                   const String& material, const String& colorName,
                                   const String& colorHex, int weightG)
{
  String filamentName = material + " " + colorName;

  // Search existing filaments — no query params, same as Android app
  String resp = smRequest(host, port, "/api/v1/filament", "GET");
  if (resp.length() > 0) {
    JsonDocument doc;
    if (deserializeJson(doc, resp) == DeserializationError::Ok) {
      for (JsonObject f : doc.as<JsonArray>()) {
        // Match by vendor id + filament name, case-insensitive (same as Android)
        int fVendorId = -1;
        if (!f["vendor"].isNull())
          fVendorId = f["vendor"]["id"].as<int>();
        String fName = f["name"].as<String>();
        fName.toLowerCase();
        String matchName = filamentName;
        matchName.toLowerCase();
        if (fVendorId == vendorId && fName == matchName)
          return f["id"].as<int>();
      }
    }
  }

  // Create new filament
  String chx = colorHex; chx.replace("#", "");
  String body = "{\"vendor_id\":" + String(vendorId) +
                ",\"name\":\"" + filamentName + "\"" +
                ",\"material\":\"" + material + "\"" +
                ",\"color_hex\":\"" + chx + "\"" +
                ",\"weight\":" + String(weightG) +
                ",\"density\":1.24" +
                ",\"diameter\":1.75}";
  resp = smRequest(host, port, "/api/v1/filament", "POST", body);
  if (resp.length() > 0) {
    JsonDocument doc;
    if (deserializeJson(doc, resp) == DeserializationError::Ok)
      return doc["id"].as<int>();
  }
  return -1;
}

int smCreateSpool(const String& host, int port,
                  const String& vendorName, const String& material,
                  const String& colorName, const String& colorHex,
                  int weightG)
{
  int vendorId = smFindOrCreateVendor(host, port, vendorName);
  if (vendorId < 0) { smLastError = "vendor step: " + smLastError; return -1; }

  int filamentId = smFindOrCreateFilament(host, port, vendorId, material,
                                           colorName, colorHex, weightG);
  if (filamentId < 0) { smLastError = "filament step: " + smLastError; return -1; }

  String body = "{\"filament_id\":" + String(filamentId) +
                ",\"initial_weight\":" + String(weightG) + "}";
  String resp = smRequest(host, port, "/api/v1/spool", "POST", body);
  if (resp.length() > 0) {
    JsonDocument doc;
    if (deserializeJson(doc, resp) == DeserializationError::Ok)
      return doc["id"].as<int>();
    smLastError = "spool step: parse failed, resp=" + resp.substring(0, 100);
  } else {
    smLastError = "spool step: " + smLastError;
  }
  return -1;
}

bool smPatchRfidTag(const String& host, int port, int spoolId,
                    const String& uid, int index)
{
  String fieldKey = (index == 1) ? "rfid_tag_id_1" : "rfid_tag_id_2";
  String cleanUid = uid; cleanUid.toUpperCase(); cleanUid.replace(" ", "");
  String body = "{\"extra\":{\"" + fieldKey + "\":\"" + cleanUid + "\"}}";
  String resp = smRequest(host, port, "/api/v1/spool/" + String(spoolId), "PATCH", body);
  return resp.length() > 0;
}

void smEnsureRfidFields(const String& host, int port)
{
  String resp = smRequest(host, port, "/api/v1/field/spool", "GET");
  bool has1 = resp.indexOf("rfid_tag_id_1") >= 0;
  bool has2 = resp.indexOf("rfid_tag_id_2") >= 0;

  auto createField = [&](const String& key, const String& label) {
    String body = "{\"name\":\"" + key + "\",\"field_type\":\"text\",\"default_value\":\"\","
                  "\"multi_choice\":false,\"choices\":[]}";
    smRequest(host, port, "/api/v1/field/spool/" + key, "POST", body);
  };
  if (!has1) createField("rfid_tag_id_1", "RFID Tag ID 1");
  if (!has2) createField("rfid_tag_id_2", "RFID Tag ID 2");
}
