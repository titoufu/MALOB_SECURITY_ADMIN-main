// ===============================
//  web_server_handlers.cpp
// ===============================

#include <ArduinoJson.h>
#include <LittleFS.h>

#include <set>  // Adicionado para std::set
#include <vector>  // Para std::vector

#include "web_server.h"
#include "web_server_handlers.h"
#include "system_config.h"
#include "sensor.h"
#include "sirene.h"
#include "alarme.h"

extern ESP8266WebServer server;
extern Alarme *alarmePtr;
extern std::vector<String> todasZonas;
extern int ARM_HOUR_WEEKDAY, DISARM_HOUR_WEEKDAY, ARM_HOUR_WEEKEND, DISARM_HOUR_WEEKEND, HORA_RESTART;
extern bool RESTART_CONFIG;
extern int ultimoDiaReinicio;
extern void configurarSistema();

// Variáveis de estado do login
int tentativasLogin = 0;
unsigned long ultimaTentativa = 0;
bool sistemaBloqueado = false;

// Helpers
namespace {
    bool verificarSenhaAdmin(const String &senha) {
        if (sistemaBloqueado) return false;
        
        // Verifica se excedeu tentativas
        if (millis() - ultimaTentativa < TEMPO_BLOQUEIO_LOGIN && 
            tentativasLogin >= MAX_TENTATIVAS_LOGIN) {
            sistemaBloqueado = true;
            return false;
        }

        bool senhaCorreta = (senha == SENHA_ADMIN_PADRAO); // Simples comparação
        // Em produção, substituir por: senhaCorreta = (hashSenha(senha) == SENHA_ADMIN_HASH_ARMAZENADA);

        if (!senhaCorreta) {
            tentativasLogin++;
            ultimaTentativa = millis();
            registrarEvento("Tentativa de login admin falhou");
            return false;
        }

        // Reset contador se login bem sucedido
        tentativasLogin = 0;
        sistemaBloqueado = false;
        return true;
    }

/*     bool validarDadosUsuarios(const String &jsonStr) {
        DynamicJsonDocument doc(1024);
        if (deserializeJson(doc, jsonStr) || !doc.is<JsonArray>()) {
            return false;
        }

        JsonArray usuarios = doc.as<JsonArray>();
        std::set<std::string> nomesUnicos;  // Alterado para std::string

        for (JsonObject usuario : usuarios) {
            if (!usuario.containsKey("usuario") || !usuario.containsKey("senha")) {
                return false;
            }

            std::string nome = usuario["usuario"].as<std::string>();  // Usando std::string
            // Trim manual para std::string
            nome.erase(nome.begin(), std::find_if(nome.begin(), nome.end(), [](int ch) {
                return !std::isspace(ch);
            }));
            nome.erase(std::find_if(nome.rbegin(), nome.rend(), [](int ch) {
                return !std::isspace(ch);
            }).base(), nome.end());

            if (nome.empty() || nomesUnicos.count(nome) > 0) {
                return false;
            }
            nomesUnicos.insert(nome);
        }
        return true;
    } */
}


int resolverPino(const String &pinoStr)
{
  if (pinoStr == "D0")
    return D0;
  if (pinoStr == "D1")
    return D1;
  if (pinoStr == "D2")
    return D2;
  if (pinoStr == "D3")
    return D3;
  if (pinoStr == "D4")
    return D4;
  if (pinoStr == "D5")
    return D5;
  if (pinoStr == "D6")
    return D6;
  if (pinoStr == "D7")
    return D7;
  if (pinoStr == "D8")
    return D8;
  return -1;
}

void handleStatus()
{
  server.send(200, "application/json", getEstadoAtualJson());
}

void handleHistorico()
{
  File file = LittleFS.open(HISTORICO_PATH, "r");
  if (!file)
  {
    server.send(500, "application/json", "[]");
    return;
  }
  server.streamFile(file, "application/json");
  file.close();
}

void handleArmar()
{
  if (!server.hasArg("zonas"))
  {
    server.send(400, "text/plain", "Zonas não especificadas");
    return;
  }
  std::vector<String> zonas = splitZonas(server.arg("zonas"));
  alarmePtr->armar(zonas);
  String msg = "Alarme armado por: " + server.arg("usuario");
  registrarEvento(msg);
  server.send(200, "text/plain", "Alarme armado");
}

void handleDesarmar()
{
  alarmePtr->desarmar();
  String msg = "Alarme desarmado por: " + server.arg("usuario");
  registrarEvento(msg);
  server.send(200, "text/plain", "Alarme desarmado");
}

void handleModo()
{
  if (!server.hasArg("manual"))
  {
    server.send(400, "text/plain", "Modo não especificado");
    return;
  }
  bool modoManual = server.arg("manual") == "true";
  alarmePtr->setModo(modoManual ? Alarme::Modo::MANUAL : Alarme::Modo::AUTOMATICO);
  registrarEvento("[MODO] Modo alterado para " + String(modoManual ? "MANUAL" : "AUTOMATICO") +
                  " por " + server.arg("usuario"));
  server.send(200, "text/plain", "Modo atualizado");
}
void handleLogin() {
  if (!server.hasArg("senha")) {
    server.send(400, "application/json", "{\"erro\":\"Senha não fornecida\"}");
    return;
  }

  String senha = server.arg("senha");
  bool valido = verificarSenhaAdmin(senha);
  
  if (valido) {
    server.send(200, "application/json", "{\"ok\":true, \"msg\":\"Login realizado\"}");
  } else {
    String mensagem = sistemaBloqueado ? 
      "Sistema temporariamente bloqueado" : "Senha incorreta";
    server.send(401, "application/json", 
      String("{\"erro\":\"") + mensagem + "\"}");
  }
}

void handleVerificaLogin() {
    if (!server.hasArg("usuario") || !server.hasArg("senha")) {
        server.send(400, "application/json", "{\"erro\":\"Dados incompletos\"}");
        return;
    }

    String usuario = server.arg("usuario");
    String senha = server.arg("senha");

    if (credenciais_validas(usuario, senha)) {
        // Gera um token simples (em produção, use algo mais seguro)
        String token = String(millis());
        
        server.send(200, "application/json", 
            "{\"ok\":true, \"token\":\"" + token + "\"}");
    } else {
        server.send(401, "application/json", "{\"erro\":\"Credenciais inválidas\"}");
    }
}

void handleGetUsuarios() {
    // Verifica a senha primeiro
    if (!server.hasArg("senha") || !verificarSenhaAdmin(server.arg("senha"))) {
        server.send(401, "application/json", "{\"erro\":\"Acesso negado\"}");
        return;
    }

    if (!LittleFS.exists(USUARIOS_PATH)) {
        server.send(200, "application/json", "[]");
        return;
    }

    File file = LittleFS.open(USUARIOS_PATH, "r");
    if (!file) {
        server.send(500, "application/json", "{\"erro\":\"Erro ao ler arquivo\"}");
        return;
    }
    
    server.streamFile(file, "application/json");
    file.close();
}

void handlePostUsuarios() {
    // Verifica a senha primeiro
    if (!server.hasArg("senha") || !verificarSenhaAdmin(server.arg("senha"))) {
        server.send(401, "application/json", "{\"erro\":\"Acesso negado\"}");
        return;
    }

    // Verifica se tem dados JSON
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"erro\":\"Dados não fornecidos\"}");
        return;
    }

    // Valida o JSON recebido
    DynamicJsonDocument doc(1024);
    if (deserializeJson(doc, server.arg("plain"))) {
        server.send(400, "application/json", "{\"erro\":\"JSON inválido\"}");
        return;
    }

    // Salva os usuários
    File file = LittleFS.open(USUARIOS_PATH, "w");
    if (!file) {
        server.send(500, "application/json", "{\"erro\":\"Erro ao salvar\"}");
        return;
    }

    serializeJson(doc, file);
    file.close();
    server.send(200, "application/json", "{\"ok\":true, \"msg\":\"Usuários salvos\"}");
}
void handleGetHorarios()
{
  File file = LittleFS.open("/horarios.json", "r");
  if (!file)
  {
    server.send(500, "application/json", "{}");
    return;
  }
  server.streamFile(file, "application/json");
  file.close();
}

void handlePostHorarios()
{
  File file = LittleFS.open("/horarios.json", "w");
  if (!file)
  {
    server.send(500, "text/plain", "Erro ao salvar horários");
    return;
  }
  file.print(server.arg("plain"));
  file.close();
  server.send(200, "text/plain", "Horários atualizados");
}

void handleRecarregarDados()
{
  configurarSistema();  // Recarrega sensores, zonas, horários, etc
  server.send(200, "text/plain", "Configuração recarregada com sucesso");
}


void handleIndex()
{
  File file = LittleFS.open("/index.html", "r");
  if (!file)
  {
    server.send(404, "text/plain", "Página não encontrada");
    return;
  }
  server.streamFile(file, "text/html");
  file.close();
}

void handleAdmin()
{
  File file = LittleFS.open("/admin.html", "r");
  if (!file)
  {
    server.send(404, "text/plain", "Página não encontrada");
    return;
  }
  server.streamFile(file, "text/html");
  file.close();
}

void handleConfigSensoresPage()
{
  File file = LittleFS.open("/config_sensores.html", "r");
  if (!file)
  {
    server.send(404, "text/plain", "Página não encontrada");
    return;
  }
  server.streamFile(file, "text/html");
  file.close();
}

void handleGetSensores()
{
  File file = LittleFS.open("/sensores.json", "r");
  if (!file)
  {
    server.send(200, "application/json", "[]");
    return;
  }
  server.streamFile(file, "application/json");
  file.close();
}

void handlePostSensores()
{
  File file = LittleFS.open("/sensores.json", "w");
  if (!file)
  {
    server.send(500, "text/plain", "Erro ao salvar sensores");
    return;
  }
  file.print(server.arg("plain"));
  file.close();
  server.send(200, "text/plain", "Sensores atualizados");
}
