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
static int smFindOrCreateFilament(const String& host, int port, int vendorId,
                                   const String& material, const String& colorName,
                                   const String& colorHex, int weightG, float diameter)
{
  // Try to find existing
  String path = "/api/v1/filament?vendor_id=" + String(vendorId) +
                "&material=" + material;
  String resp = smRequest(host, port, path, "GET");
  if (resp.length() > 0) {
    JsonDocument doc;
    if (deserializeJson(doc, resp) == DeserializationError::Ok) {
      for (JsonObject f : doc.as<JsonArray>()) {
        // match on color hex too
        String hex = f["color_hex"].as<String>();
        hex.toLowerCase();
        String chx = colorHex;
        chx.replace("#",""); chx.toLowerCase();
        if (hex == chx)
          return f["id"].as<int>();
      }
    }
  }

  // Create new filament
  String chx = colorHex; chx.replace("#","");
  String body = "{\"vendor_id\":" + String(vendorId) +
                ",\"name\":\"" + material + " " + colorName + "\"" +
                ",\"material\":\"" + material + "\"" +
                ",\"color_hex\":\"" + chx + "\"" +
                ",\"weight\":" + String(weightG) +
                ",\"diameter\":" + String(diameter, 2) + "}";
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
  if (vendorId < 0) return -1;

  int filamentId = smFindOrCreateFilament(host, port, vendorId, material,
                                           colorName, colorHex, weightG, 1.75f);
  if (filamentId < 0) return -1;

  String body = "{\"filament_id\":" + String(filamentId) +
                ",\"initial_weight\":" + String(weightG) + "}";
  String resp = smRequest(host, port, "/api/v1/spool", "POST", body);
  if (resp.length() > 0) {
    JsonDocument doc;
    if (deserializeJson(doc, resp) == DeserializationError::Ok)
      return doc["id"].as<int>();
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
