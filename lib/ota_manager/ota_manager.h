#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <ESP8266WebServer.h>

void setup_ota(ESP8266WebServer& server, const char* mdnsHost, const char* user, const char* password);

#endif
