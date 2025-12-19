#include "alarme.h"
#include "event_logger.h"
#include "system_config.h"
#include "web_server.h"
#include <time.h>

extern std::vector<String> todasZonas;

// ====== DEFINIÇÕES (precisam existir em 1 .cpp só) ======
int  HORA_RESTART      = 23;
bool RESTART_CONFIG    = false;
int  ultimoDiaReinicio = -1;

// Construtor
Alarme::Alarme()
: estadoAtual(Estado::DESARMADO),
  modoAtual(Modo::MANUAL),
  sirene(nullptr)
{}

void Alarme::definirSirene(Sirene *s) { sirene = s; }
void Alarme::adicionarZona(Zona *zona) { zonas.push_back(zona); }

void Alarme::armar(const std::vector<String> &zonasSelecionadas)
{
    estadoAtual = Estado::ARMADO;
    zonasAtivas = zonasSelecionadas;

    for (auto zona : zonas)
    {
        if (zonaEstaAtiva(zona->getNome())) zona->armar();
        else                               zona->desarmar();
    }

    if (sirene) sirene->desativar();
}

void Alarme::desarmar()
{
    estadoAtual = Estado::DESARMADO;
    for (auto zona : zonas) zona->desarmar();
    if (sirene) sirene->desativar();
}

void Alarme::atualizar()
{
    if (estadoAtual != Estado::ARMADO) return;

    for (auto zona : zonas)
    {
        if (!zonaEstaAtiva(zona->getNome())) continue;

        zona->atualizar();

        if (zona->estaViolada())
        {
            for (auto sensor : zona->getSensores())
            {
                if (sensor->getEstado() == Sensor::Estado::VIOLADO &&
                    sensor->getSituacao() == Sensor::Situacao::ATIVO &&
                    !sensor->foiAlertaEmitido())
                {
                    String msg = "[ALERTA] Zona " + zona->getNome() + " violada (" + sensor->getNome() + ").";
                    registrarEvento(msg);
                    sensor->setAlertaEmitido(true);
                    if (sirene) sirene->ativar(sensor);
                }
            }
        }
    }

    if (sirene) sirene->atualizar();
}

bool Alarme::zonaEstaAtiva(const String &nomeZona) const
{
    for (const auto &z : zonasAtivas) if (z == nomeZona) return true;
    return false;
}

Alarme::Estado Alarme::getEstado() const { return estadoAtual; }
Alarme::Modo   Alarme::getModo()   const { return modoAtual; }

void Alarme::setModo(Modo m)
{
    modoAtual = m;

    // no AUTOMÁTICO, por padrão, considera todas as zonas ativas
    if (modoAtual == Modo::AUTOMATICO)
    {
        zonasAtivas.clear();
        for (auto zona : zonas) zonasAtivas.push_back(zona->getNome());
    }
}

const std::vector<String>& Alarme::getZonasAtivas() const { return zonasAtivas; }
const std::vector<Zona*>&  Alarme::getZonas() const { return zonas; }

void Alarme::imprimirDados() const
{
    Serial.println("\n--- Dados do Alarme ---");
    Serial.print("Estado: "); Serial.println(estadoAtual == Estado::DESARMADO ? "DESARMADO" : "ARMADO");
    Serial.print("Modo: ");   Serial.println(modoAtual == Modo::MANUAL ? "MANUAL" : "AUTOMATICO");

    Serial.print("Zonas Ativas: ");
    if (zonasAtivas.empty()) Serial.println("Nenhuma");
    else { for (auto &z : zonasAtivas) Serial.print(z + ", "); Serial.println(); }

    Serial.println("------------------------\n");
}

void Alarme::limparZonas()
{
    for (auto zona : zonas) delete zona;
    zonas.clear();
}

// =================== AUTO SCHEDULE ===================
// Regra: se hora NÃO for confiável (sem NTP ou ano==1970), o modo automático vira “sempre armado”.
void checkAutoSchedule(Alarme &alarme)
{
    if (alarme.getModo() != Alarme::Modo::AUTOMATICO) return;

    struct tm timeinfo;
    const bool temHora = getLocalTime(&timeinfo);

    const int ano = temHora ? (timeinfo.tm_year + 1900) : 1970;

    // Se sem hora OU ano==1970 => sempre armado
    if (!temHora || ano == 1970)
    {
        if (alarme.getEstado() != Alarme::Estado::ARMADO)
        {
            Serial.println("[AUTO] Hora não confiável (1970/sem NTP) => mantendo SEMPRE ARMADO");
            alarme.armar(todasZonas);
            registrarEvento("[AUTO] Hora não confiável => alarme mantido sempre armado");
        }
        return;
    }

    // Hora confiável: aplica agenda normal
    const int hour    = timeinfo.tm_hour;
    const int weekday = timeinfo.tm_wday; // 0=dom, 1=seg...6=sab

    const int armHour    = (weekday >= 1 && weekday <= 5) ? ARM_HOUR_WEEKDAY    : ARM_HOUR_WEEKEND;
    const int disarmHour = (weekday >= 1 && weekday <= 5) ? DISARM_HOUR_WEEKDAY : DISARM_HOUR_WEEKEND;

    const bool dentroDaJanela = (hour >= armHour || hour < disarmHour);

    if (dentroDaJanela && alarme.getEstado() != Alarme::Estado::ARMADO)
    {
        alarme.armar(todasZonas);
        registrarEvento("[INFO] Alarme armado automaticamente (por horário)");
        Serial.println("[INFO] Alarme armado automaticamente (por horário)");
    }
    else if (!dentroDaJanela && alarme.getEstado() != Alarme::Estado::DESARMADO)
    {
        alarme.desarmar();
        registrarEvento("[INFO] Alarme desarmado automaticamente (por horário)");
        Serial.println("[INFO] Alarme desarmado automaticamente (por horário)");
    }
}

// =================== DAILY RESTART ===================
void checkDailyRestart()
{
    if (!RESTART_CONFIG) return;

    time_t now = time(nullptr);

    // se hora não confiável (ex: 1970), não reinicia por agenda
    if (now < 1700000000) return;

    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    const int horaAtual   = timeinfo.tm_hour;
    const int minutoAtual = timeinfo.tm_min;

    const int diaId = (int)(now / 86400);
    const bool dentroDaJanela = (horaAtual == HORA_RESTART) && (minutoAtual <= 2);

    if (dentroDaJanela && diaId != ultimoDiaReinicio)
    {
        Serial.println("[INFO] Reiniciando o sistema conforme horário configurado...");
        registrarEvento("[REINICIO] Reiniciando o sistema conforme horário configurado...");

        ultimoDiaReinicio = diaId;
        salvarUltimoDiaReinicio(diaId);

        delay(2000);
        ESP.restart();
    }
}
