#include <ESP8266WiFi.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPUpdateServer.h>
#include "ota_manager.h"

static ESP8266HTTPUpdateServer httpUpdater;

void setup_ota(ESP8266WebServer& server, const char* mdnsHost, const char* user, const char* password) {
  httpUpdater.setup(&server, "/update", user, password);

  Serial.printf("[OTA] /update protegido por usuário/senha.\n");
  Serial.printf("[OTA] URL: http://%s.local/update\n", mdnsHost);
  Serial.printf("[OTA] URL: http://%s/update\n", WiFi.localIP().toString().c_str());
}
