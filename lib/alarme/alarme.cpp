#include "alarme.h"
#include "event_logger.h"
#include "system_config.h"
#include "web_server.h"

extern std::vector<String> todasZonas;

int HORA_RESTART = 23;
bool RESTART_CONFIG = false;
int ultimoDiaReinicio = -1; // último dia que ocorreu o reinício

// Construtor
Alarme::Alarme() : estadoAtual(Estado::DESARMADO), modoAtual(Modo::MANUAL), sirene(nullptr) {}

void Alarme::definirSirene(Sirene *s)
{
    sirene = s;
}

void Alarme::adicionarZona(Zona *zona)
{
    zonas.push_back(zona);
}

// Arma zonas específicas
void Alarme::armar(const std::vector<String> &zonasSelecionadas)
{
    estadoAtual = Estado::ARMADO;
    zonasAtivas = zonasSelecionadas;

    for (auto zona : zonas)
    {
        if (zonaEstaAtiva(zona->getNome()))
        {
            zona->armar();
        }
        else
        {
            zona->desarmar();
        }
    }

    if (sirene)
        sirene->desativar();
}

// Desarma todas as zonas
void Alarme::desarmar()
{
    estadoAtual = Estado::DESARMADO;

    for (auto zona : zonas)
    {
        zona->desarmar();
    }

    if (sirene)
        sirene->desativar();
}

// Atualiza o estado do alarme verificando zonas e sensores
void Alarme::atualizar()
{
    if (estadoAtual != Estado::ARMADO)
        return;
    for (auto zona : zonas)
    {
        if (!zonaEstaAtiva(zona->getNome()))
            continue;

        zona->atualizar();

        if (zona->estaViolada())
        {
            for (auto sensor : zona->getSensores())
            {
                if (sensor->getEstado() == Sensor::Estado::VIOLADO &&
                    sensor->getSituacao() == Sensor::Situacao::ATIVO &&
                    !sensor->foiAlertaEmitido())
                {
                    String msg = "[ALERTA] Zona " + zona->getNome() + " violada " + " (" + sensor->getNome() + ").";
                    registrarEvento(msg);
                    sensor->setAlertaEmitido(true);
                    if (sirene)
                        sirene->ativar(sensor);
                }
            }
        }
    }

    if (sirene)
        sirene->atualizar();
}

// Verifica se uma zona está na lista de zonas ativas
bool Alarme::zonaEstaAtiva(const String &nomeZona) const
{
    for (const auto &z : zonasAtivas)
    {
        if (z == nomeZona)
            return true;
    }
    return false;
}

// Getters e setters
Alarme::Estado Alarme::getEstado() const { return estadoAtual; }
Alarme::Modo Alarme::getModo() const { return modoAtual; }
void Alarme::setModo(Modo m)
{
    modoAtual = m;
    if (modoAtual == Modo::AUTOMATICO)
    {
        zonasAtivas.clear();
        for (auto zona : zonas)
        {
            zonasAtivas.push_back(zona->getNome());
        }
    }
}

const std::vector<String> &Alarme::getZonasAtivas() const { return zonasAtivas; }

// Imprime no terminal o estado atual do sistema
void Alarme::imprimirDados() const
{
    Serial.println("\n--- Dados do Alarme ---");

    Serial.print("Estado: ");
    Serial.println(estadoAtual == Estado::DESARMADO ? "DESARMADO" : "ARMADO");

    Serial.print("Modo: ");
    Serial.println(modoAtual == Modo::MANUAL ? "MANUAL" : "AUTOMATICO");

    Serial.print("Zonas Ativas: ");
    if (zonasAtivas.empty())
    {
        Serial.println("Nenhuma");
    }
    else
    {
        for (const String &zona : zonasAtivas)
        {
            Serial.print(zona + ", ");
        }
        Serial.println();
    }

    Serial.print("Zonas Monitoradas: ");
    if (zonas.empty())
    {
        Serial.println("Nenhuma");
    }
    else
    {
        for (auto zona : zonas)
        {
            Serial.print(zona->getNome() + ", ");
        }
        Serial.println();
    }

    Serial.print("Sirene Configurada: ");
    Serial.println(sirene != nullptr ? "Sim" : "Não");

    Serial.println("------------------------\n");
}

// Controle de agendamento automático
void checkAutoSchedule(Alarme &alarme)
{
    if (alarme.getModo() != Alarme::Modo::AUTOMATICO)
        return;

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        if (alarme.getEstado() != Alarme::Estado::ARMADO)
        {
            Serial.println("[AVISO] Sem hora válida — ativando alarme por contingência");
            alarme.armar(todasZonas);
            registrarEvento("[INFO] Alarme armado automaticamente por falta de internet/hora");
        }
        return;
    }

    int hour = timeinfo.tm_hour;
    int weekday = timeinfo.tm_wday;

    int armHour = (weekday >= 1 && weekday <= 5) ? ARM_HOUR_WEEKDAY : ARM_HOUR_WEEKEND;
    int disarmHour = (weekday >= 1 && weekday <= 5) ? DISARM_HOUR_WEEKDAY : DISARM_HOUR_WEEKEND;

    bool dentroDaJanela = (hour >= armHour || hour < disarmHour);

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

// Reinício diário programado (corrigido)
void checkDailyRestart()
{
    if (!RESTART_CONFIG) return;

    time_t now = time(nullptr);

    // 1) Só reinicia se a hora for válida (evita 1970 / sem NTP)
    if (now < 1700000000) {
        // Serial.println("[REINICIO] Hora inválida (sem NTP), não reiniciando.");
        return;
    }

    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    const int horaAtual   = timeinfo.tm_hour;
    const int minutoAtual = timeinfo.tm_min;

    // 2) Identificador de dia robusto (não repete no mês)
    //    "epoch day": número de dias desde 1970-01-01
    const int diaId = (int)(now / 86400);

    // 3) (Opcional, mas bom) janela de 2 minutos pra evitar reinício em qualquer minuto da hora
    const bool dentroDaJanela = (horaAtual == HORA_RESTART) && (minutoAtual <= 2);

    // 4) Reinicia uma vez por dia
    if (dentroDaJanela && diaId != ultimoDiaReinicio)
    {
        Serial.println("[INFO] Reiniciando o sistema conforme horário configurado...");
        registrarEvento("[REINICIO] Reiniciando o sistema conforme horário configurado...");

        // Atualiza RAM + FS antes de reiniciar (evita loop de reboot)
        ultimoDiaReinicio = diaId;
        salvarUltimoDiaReinicio(diaId);

        delay(2000);
        ESP.restart();
    }
}
const std::vector<Zona *> &Alarme::getZonas() const
{
    return zonas;
}
void Alarme::limparZonas()
{
    for (auto zona : zonas)
    {
        delete zona; // libera memória
    }
    zonas.clear();
}
