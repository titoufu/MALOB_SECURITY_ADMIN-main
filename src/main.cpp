#include <Arduino.h>
#include <LittleFS.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <time.h>

#include "web_server_handlers.h"
#include "system_config.h"
#include "web_server.h" // deve declarar: extern ESP8266WebServer server;
#include "ota_manager.h"
#include "alarme.h"
#include "zona.h"
#include "sensor.h"
#include "sirene.h"

// ================== CONFIGS ==================
static const char *HOSTNAME = "alarme";
static const char *WIFI_AP_SSID = "ConfigurarESP";
static const char *WIFI_AP_PASS = "12345678";
static const char *OTA_USER = "admin";
static const char *OTA_PASS = "1234";

static const unsigned long INTERVALO_ALARME_MS = 100;
static const unsigned long WIFI_RECONNECT_INTERVAL_MS = 10UL * 1000UL;  // 10s
static const unsigned long WIFI_RESTART_AFTER_MS = 5UL * 60UL * 1000UL; // 5 min
static const unsigned long NTP_RETRY_INTERVAL_MS = 5UL * 60UL * 1000UL; // 5 min

// WiFi hardening (mínimo viável #2)
static const uint8_t WIFI_RESET_SUAVE_APOS_FALHAS = 3; // após 3 tentativas, faz disconnect(false)

// ================== GLOBAIS ==================
Alarme alarme;
Sirene sirene(BUZZER_PIN, 5000, 5000, 4);
std::vector<String> todasZonas;

// Controle WiFi/NTP
static bool wifiEstavaConectado = false;
static unsigned long ultimoWifiOkMs = 0;
static unsigned long proximaTentativaWifiMs = 0;

static bool ntpOk = false;
static unsigned long proximaTentativaNtpMs = 0;

// mDNS hardening (mínimo viável #1)
static bool mdnsAtivo = false;

// Contador de falhas de reconexão (mínimo viável #2)
static uint8_t falhasReconexaoWiFi = 0;

// ================== FUNÇÕES AUXILIARES ==================

// Carrega sensores do arquivo JSON e retorna ponteiros
// MELHORIA: faz parse direto do File (stream), sem buffer grande na RAM
std::vector<Sensor *> carregarSensoresDeJSON(const char *path)
{
    std::vector<Sensor *> sensores;

    File file = LittleFS.open(path, "r");
    if (!file)
    {
        Serial.println("[ERRO] Falha ao abrir /sensores.json");
        return sensores;
    }

    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error)
    {
        Serial.print("[ERRO] Falha ao fazer parse do JSON: ");
        Serial.println(error.c_str());
        return sensores;
    }

    if (!doc.is<JsonArray>())
    {
        Serial.println("[ERRO] /sensores.json não é um array");
        return sensores;
    }

    for (JsonObject obj : doc.as<JsonArray>())
    {
        String nome = obj["nome"] | "";
        String tipoStr = obj["tipo"] | "";
        String pinoStr = obj["pino"] | "";
        String zona = obj["zona"] | "";
        bool ativo = obj["ativo"] | true;

        int pino = resolverPino(pinoStr);
        Sensor::Tipo tipo = Sensor::tipoFromString(tipoStr);

        Sensor *s = new Sensor(nome, tipo, pino, zona, ativo);
        sensores.push_back(s);
    }

    return sensores;
}

// Agrupa sensores por zona
std::map<String, Zona *> agruparSensoresPorZona(const std::vector<Sensor *> &sensores)
{
    std::map<String, Zona *> zonas;

    for (auto sensor : sensores)
    {
        String nomeZona = sensor->getZona();
        if (zonas.find(nomeZona) == zonas.end())
        {
            zonas[nomeZona] = new Zona(nomeZona);
            todasZonas.push_back(nomeZona);
        }

        zonas[nomeZona]->adicionarSensor(sensor);
    }

    return zonas;
}

// Extrai os nomes das zonas
std::vector<String> obterNomesZonas(const std::map<String, Zona *> &zonas)
{
    std::vector<String> nomes;
    for (auto &par : zonas)
        nomes.push_back(par.first);
    return nomes;
}

// NTP com timeout (não trava o boot)
// Retorna true se sincronizou, false se deu timeout/falha
bool configurarHorarioComTimeout(uint32_t timeoutMs)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("[NTP] WiFi não conectado, pulando NTP");
        return false;
    }

    configTime(-3 * 3600, 0, "br.pool.ntp.org", "pool.ntp.org", "time.google.com");

    const uint32_t t0 = millis();
    Serial.print("[NTP] Sincronizando");

    while ((uint32_t)(millis() - t0) < timeoutMs)
    {
        time_t agora = time(nullptr);

        if (agora > 1700000000 && agora < 4000000000)
        {
            Serial.printf("\n[NTP] OK: %s", ctime(&agora));
            return true;
        }

        delay(250);
        Serial.print(".");
        yield();
    }

    Serial.println("\n[NTP] Timeout. Continuando sem hora sincronizada.");
    return false;
}

// ======= (1) mDNS hardening: usa flag mdnsAtivo =======
static void reiniciarMDNS()
{
    if (mdnsAtivo)
    {
        MDNS.end();
        mdnsAtivo = false;
        delay(20);
        yield();
    }

    if (MDNS.begin(HOSTNAME))
    {
        MDNS.addService("http", "tcp", 80);
        mdnsAtivo = true;
        Serial.printf("[mDNS] OK: http://%s.local\n", HOSTNAME);
    }
    else
    {
        Serial.println("[mDNS] ERRO ao iniciar");
    }
}

static void onWifiConectado()
{
    wifiEstavaConectado = true;
    ultimoWifiOkMs = millis();

    // Reset do contador de falhas ao reconectar
    falhasReconexaoWiFi = 0;

    Serial.println("[WIFI] Conectado!");
    Serial.print("[WIFI] IP: ");
    Serial.println(WiFi.localIP());
    Serial.printf("[WIFI] RSSI: %d dBm\n", WiFi.RSSI());

    reiniciarMDNS();

    // força nova tentativa de NTP quando a rede volta
    ntpOk = false;
    proximaTentativaNtpMs = millis();
}

static void onWifiDesconectado()
{
    wifiEstavaConectado = false;
    Serial.println("[WIFI] Desconectado");
}
static void setHoraSentinela1970()
{
    // 1970-01-01 00:00:00 UTC (ou local; aqui é "bruto")
    // No ESP8266/Arduino, isso geralmente ajusta o relógio interno.
    timeval tv;
    tv.tv_sec = 0; // epoch 0
    tv.tv_usec = 0;
    settimeofday(&tv, nullptr);

    Serial.println("[TEMPO] Hora sentinela aplicada: 1970 (tempo não confiável)");
}

void configurarSistema()
{
    Serial.println("[SISTEMA] Reconfigurando sensores, zonas e horários...");

    alarme.limparZonas();
    todasZonas.clear();

    auto sensores = carregarSensoresDeJSON("/sensores.json");
    auto zonas = agruparSensoresPorZona(sensores);

    alarme.definirSirene(&sirene);

    for (auto &par : zonas)
    {
        alarme.adicionarZona(par.second);
    }

    alarme.setModo(Alarme::Modo::AUTOMATICO);
    alarme.armar(obterNomesZonas(zonas));

    loadHorariosFromFS();

    Serial.println("[SISTEMA] Reconfiguração concluída");
}

// ================== SETUP ==================
void setup()
{
    Serial.begin(115200);
    delay(100);
    Serial.println("\n\n=== SISTEMA DE ALARME INICIANDO ===");

    // 1) LittleFS
    if (!LittleFS.begin())
    {
        Serial.println("[ERRO] LittleFS não iniciado. Reiniciando...");
        delay(500);
        ESP.restart();
    }
    Serial.println("[OK] LittleFS pronto");

    // 2) WiFi config
    WiFi.setAutoReconnect(true);
    WiFi.persistent(false);

    // 3) WiFiManager (boot)
    WiFiManager wm;
    wm.setConnectTimeout(30);
    wm.setConfigPortalTimeout(180);

    bool res = wm.autoConnect(WIFI_AP_SSID, WIFI_AP_PASS);
    if (!res)
    {
        Serial.println("[ERRO] WiFi não conectado no boot");
        delay(2000);
        ESP.restart();
    }

    // marca WiFi OK
    onWifiConectado();
    ultimoWifiOkMs = millis();
    proximaTentativaWifiMs = millis() + WIFI_RECONNECT_INTERVAL_MS;

    // 4) Sobe OTA + WebServer ANTES do NTP
    setup_ota(server, HOSTNAME, OTA_USER, OTA_PASS);
    web_server_setup(&alarme);

    // 5) Configura o sistema
    configurarSistema();

    // 6) NTP inicial
    ntpOk = configurarHorarioComTimeout(15000);
    if (!ntpOk)
    {
        setHoraSentinela1970();
    }
    proximaTentativaNtpMs = millis() + NTP_RETRY_INTERVAL_MS;

    Serial.println("=== SETUP CONCLUÍDO ===");
}

// ================== LOOP ==================
void loop()
{
    const unsigned long now = millis();

    // 1) WiFi watchdog
    const bool wifiConectado = (WiFi.status() == WL_CONNECTED);

    if (wifiConectado)
    {
        if (!wifiEstavaConectado)
        {
            onWifiConectado();
        }
        ultimoWifiOkMs = now;
    }
    else
    {
        if (wifiEstavaConectado)
        {
            onWifiDesconectado();
        }

        // tenta reconectar periodicamente
        if ((int32_t)(now - (uint32_t)proximaTentativaWifiMs) >= 0)
        {
            proximaTentativaWifiMs = now + WIFI_RECONNECT_INTERVAL_MS;

            // ======= (2) WiFi hardening: reset suave após N falhas =======
            falhasReconexaoWiFi++;
            Serial.printf("[WIFI] Tentando reconnect... (falha %u)\n", falhasReconexaoWiFi);

            if (falhasReconexaoWiFi >= WIFI_RESET_SUAVE_APOS_FALHAS)
            {
                Serial.println("[WIFI] Reset suave: WiFi.disconnect(false) + reconnect");
                WiFi.disconnect(false); // não apaga credenciais

                // pequenas pausas com yield (evita travar alarme/web)
                for (int i = 0; i < 4; i++)
                {
                    delay(50);
                    yield();
                }

                falhasReconexaoWiFi = 0; // zera contador após reset suave
            }

            WiFi.reconnect();
        }

        // se ficar muito tempo sem WiFi, reinicia (para cair no WiFiManager no boot)
        if ((uint32_t)(now - (uint32_t)ultimoWifiOkMs) > WIFI_RESTART_AFTER_MS)
        {
            Serial.println("[WIFI] Muito tempo sem conexão. Reiniciando...");
            delay(200);
            ESP.restart();
        }
    }

    // 2) Serviços de rede (OTA HTTP depende disso)
    if (wifiConectado && mdnsAtivo)
    {
        MDNS.update();
    }
    server.handleClient();

    // 3) Tick do alarme (100ms)
    static unsigned long lastCheck = 0;
    if ((uint32_t)(now - lastCheck) >= INTERVALO_ALARME_MS)
    {
        lastCheck = now;
        alarme.atualizar();
        checkAutoSchedule(alarme);
        checkDailyRestart();
    }

    // 4) NTP periódico (não bloqueante)
    if (wifiConectado && !ntpOk && (int32_t)(now - (uint32_t)proximaTentativaNtpMs) >= 0)
    {
        proximaTentativaNtpMs = now + NTP_RETRY_INTERVAL_MS;
        ntpOk = configurarHorarioComTimeout(5000);
        if (!ntpOk)
        {
            setHoraSentinela1970();
        }
    }

    yield();
}
