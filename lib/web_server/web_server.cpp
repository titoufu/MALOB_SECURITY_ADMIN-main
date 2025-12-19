#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <algorithm>

#include "system_config.h"
#include "web_server.h"
#include "sensor.h"
#include "sirene.h"
#include "alarme.h"
#include "zona.h"

ESP8266WebServer server(80);
Alarme* alarmePtr = nullptr;

int ARM_HOUR_WEEKDAY = 18;
int DISARM_HOUR_WEEKDAY = 6;
int ARM_HOUR_WEEKEND = 0;
int DISARM_HOUR_WEEKEND = 0;

extern std::vector<String> todasZonas;
extern int ultimoDiaReinicio;

// ------------------------------------
// Salva o dia do último reinício no FS
// ------------------------------------
void salvarUltimoDiaReinicio(int diaId) {
  // 1) Lê o JSON atual
  File file = LittleFS.open("/horarios.json", "r");
  if (!file) return;

  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, file);
  file.close();
  if (err) return;

  // 2) Atualiza campo
  doc["ultimoDiaReinicio"] = diaId;

  // 3) Escrita atômica
  File tmp = LittleFS.open("/horarios.tmp", "w");
  if (!tmp) return;

  if (serializeJson(doc, tmp) == 0) {
    tmp.close();
    LittleFS.remove("/horarios.tmp");
    return;
  }
  tmp.close();

  LittleFS.remove("/horarios.json");
  LittleFS.rename("/horarios.tmp", "/horarios.json");
}

// ------------------------------------
// Carrega horários do arquivo JSON
// ------------------------------------
void loadHorariosFromFS() {
  File file = LittleFS.open("/horarios.json", "r");
  if (!file) {
    Serial.println("[CONFIG] Arquivo de horários não encontrado. Usando valores padrão.");
    return;
  }

  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, file)) {
    Serial.println("[CONFIG] Erro ao ler horários. Usando valores padrão.");
    file.close();
    return;
  }
  file.close();

  ARM_HOUR_WEEKDAY = doc["ARM_HOUR_WEEKDAY"] | 18;
  DISARM_HOUR_WEEKDAY = doc["DISARM_HOUR_WEEKDAY"] | 6;
  ARM_HOUR_WEEKEND = doc["ARM_HOUR_WEEKEND"] | 0;
  DISARM_HOUR_WEEKEND = doc["DISARM_HOUR_WEEKEND"] | 0;
  HORA_RESTART = doc["horaRestart"] | 23;
  RESTART_CONFIG = doc["restartConfig"] | false;
  ultimoDiaReinicio = doc["ultimoDiaReinicio"] | -1;

/*   Serial.println("[CONFIG] Horários carregados da LittleFS:");
  Serial.printf("  ARM_HOUR_WEEKDAY: %d\n", ARM_HOUR_WEEKDAY);
  Serial.printf("  DISARM_HOUR_WEEKDAY: %d\n", DISARM_HOUR_WEEKDAY);
  Serial.printf("  ARM_HOUR_WEEKEND: %d\n", ARM_HOUR_WEEKEND);
  Serial.printf("  DISARM_HOUR_WEEKEND: %d\n", DISARM_HOUR_WEEKEND);
  Serial.printf("  HORA_RESTART: %d\n", HORA_RESTART);
  Serial.printf("  RESTART_CONFIG: %s\n", RESTART_CONFIG ? "true" : "false");
  Serial.printf("  ultimoDiaReinicio: %d\n", ultimoDiaReinicio); */
}

// ------------------------------------
// Validação de login
// ------------------------------------
String normalize(String s) {
  s.trim();
  s.toLowerCase();
  return s;
}

bool credenciais_validas(String usuario, String senha) {
  if (!LittleFS.exists(USUARIOS_PATH)) return false;
  File file = LittleFS.open(USUARIOS_PATH, "r");
  if (!file) return false;

  DynamicJsonDocument doc(1024);
  if (deserializeJson(doc, file)) return false;
  file.close();

  usuario = normalize(usuario);
  senha.trim();

  for (JsonObject obj : doc.as<JsonArray>()) {
    String u = normalize(obj["usuario"].as<String>());
    String s = obj["senha"].as<String>();
    s.trim();
    if (u == usuario && s == senha) return true;
  }
  return false;
}

// ------------------------------------
// Registra evento no histórico
// ------------------------------------
void registrarEvento(const String &mensagem) {
  DynamicJsonDocument doc(4096);
  File file = LittleFS.open(HISTORICO_PATH, "r");
  if (file) {
    if (deserializeJson(doc, file)) doc.clear();
    file.close();
  }

  if (!doc.is<JsonArray>()) {
    doc.clear();
    doc.to<JsonArray>();
  }

  JsonArray arr = doc.as<JsonArray>();
  JsonObject novo = arr.createNestedObject();
  novo["timestamp"] = time(nullptr);
  novo["evento"] = mensagem;

  while (arr.size() > HISTORICO_MAX_REGISTROS) arr.remove(0);

  file = LittleFS.open(HISTORICO_PATH, "w");
  if (file) {
    serializeJson(doc, file);
    file.close();
  }
}

// ------------------------------------
// Retorna JSON com estado completo do sistema
// ------------------------------------
String getEstadoAtualJson() {
  StaticJsonDocument<2048> doc;

  doc["estado_alarme"] = alarmePtr->getEstado() == Alarme::Estado::ARMADO ? "ARMADO" : "DESARMADO";
  doc["modo_operacao"] = alarmePtr->getModo() == Alarme::Modo::MANUAL ? "MANUAL" : "AUTOMATICO";
  doc["tempo_online"] = millis() / 1000;

  JsonArray zonasAtivas = doc.createNestedArray("zonas_ativas");
  for (const String &z : alarmePtr->getZonasAtivas()) zonasAtivas.add(z);

  JsonArray zonas = doc.createNestedArray("zonas");
  for (auto zona : alarmePtr->getZonas()) {
    JsonObject z = zonas.createNestedObject();
    z["nome"] = zona->getNome();
    z["estado"] = zona->estaViolada() ? "VIOLADA" : "OK";

    JsonArray sensores = z.createNestedArray("sensores");
    for (auto sensor : zona->getSensores()) {
      JsonObject s = sensores.createNestedObject();
      s["nome"] = sensor->getNome();
      s["estado"] = sensor->getEstado() == Sensor::Estado::VIOLADO ? "VIOLADO" : "OK";
      s["ativo"] = sensor->getSituacao() == Sensor::Situacao::ATIVO;
      s["isolado"] = sensor->estaIsolado();
    }
  }

  String out;
  serializeJson(doc, out);
  return out;
}

// ------------------------------------
// Converte string CSV para vetor
// ------------------------------------
std::vector<String> splitZonas(const String &zonasStr) {
  std::vector<String> zonas;
  int start = 0, end;
  while ((end = zonasStr.indexOf(',', start)) != -1) {
    zonas.push_back(zonasStr.substring(start, end));
    start = end + 1;
  }
  zonas.push_back(zonasStr.substring(start));
  return zonas;
}

// ------------------------------------
// Setup das rotas HTTP
// ------------------------------------
#include "web_server_handlers.h"

void web_server_setup(Alarme *alarme) {
  alarmePtr = alarme;

  server.on("/", handleIndex);
  server.on("/index", handleIndex);
  server.on("/admin", handleAdmin);

  server.on("/status.json", HTTP_GET, handleStatus);
  server.on("/historico.json", HTTP_GET, handleHistorico);
  server.on("/arma", HTTP_POST, handleArmar);
  server.on("/desarma", HTTP_POST, handleDesarmar);
  server.on("/modo", HTTP_POST, handleModo);

  server.on("/login", HTTP_POST, handleLogin);
  server.on("/verifica_login", HTTP_POST, handleVerificaLogin);

  server.on("/usuarios.json", HTTP_GET, handleGetUsuarios);
  server.on("/usuarios.json", HTTP_POST, handlePostUsuarios);

  server.on("/horarios.json", HTTP_GET, handleGetHorarios);
  server.on("/horarios.json", HTTP_POST, handlePostHorarios);
  server.on("/recarregar_dados", HTTP_POST, handleRecarregarDados);

  server.on("/config_sensores", HTTP_GET, handleConfigSensoresPage);
  server.on("/sensores.json", HTTP_GET, handleGetSensores);
  server.on("/sensores.json", HTTP_POST, handlePostSensores);

  server.serveStatic("/", LittleFS, "/");

  server.begin();
  Serial.println("[WEB] Servidor HTTP iniciado");
}
