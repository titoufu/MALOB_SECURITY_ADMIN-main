// web_server_handlers.h

#ifndef WEB_SERVER_HANDLERS_H
#define WEB_SERVER_HANDLERS_H

#include <Arduino.h>
#include <ESP8266WebServer.h>

// Prototipagem dos handlers HTTP
void handleStatus();
void handleHistorico();
void handleArmar();
void handleDesarmar();
void handleModo();
void handleLogin();
void handleVerificaLogin();
void handleGetUsuarios();
void handlePostUsuarios();
void handleGetHorarios();
void handlePostHorarios();
void handleRecarregarDados();
void handleIndex();
void handleAdmin();
void handleConfigSensoresPage();
void handleGetSensores();
void handlePostSensores();
int resolverPino(const String& pinoStr);


#endif
