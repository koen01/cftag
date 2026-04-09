#pragma once
#include <Arduino.h>

// Returns JSON array string of active spools, or "" on error.
String smFetchSpools(const String& host, int port);

// Creates a filament record, then a spool. Returns new spool id, or -1 on error.
// vendorName, material, colorName, colorHex (#RRGGBB), weightG (e.g. 1000)
int smCreateSpool(const String& host, int port,
                  const String& vendorName, const String& material,
                  const String& colorName, const String& colorHex,
                  int weightG);

// PATCHes the spool's extra field with rfid_tag_id_1 or rfid_tag_id_2.
// index = 1 or 2
bool smPatchRfidTag(const String& host, int port, int spoolId,
                    const String& uid, int index);

// Ensures rfid_tag_id_1 and rfid_tag_id_2 custom fields exist on Spoolman.
void smEnsureRfidFields(const String& host, int port);
