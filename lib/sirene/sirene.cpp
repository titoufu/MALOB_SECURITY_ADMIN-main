#include "sirene.h"
#include "sensor.h"
#include "event_logger.h"

Sirene::Sirene(int pino, unsigned long tempoHigh, unsigned long tempoLow, int ciclosMaximos)
    : pino(pino),
      tempoHigh(tempoHigh),
      tempoLow(tempoLow),
      ciclosMaximos(ciclosMaximos),
      ativa(false),
      ultimaTroca(0),
      ciclosAtuais(0),
      emHigh(false),
      sensorAlvo(nullptr)
{
    pinMode(pino, OUTPUT);
    digitalWrite(pino, LOW);
}

void Sirene::ativar(Sensor *sensor)
{
    if (ativa)
        return;
    ativa = true;
    ciclosAtuais = 0;
    emHigh = true;
    ultimaTroca = millis();
    sensorAlvo = sensor;
    digitalWrite(pino, HIGH);
}

void Sirene::atualizar()
{
    if (!ativa || !sensorAlvo)
        return;

    unsigned long agora = millis();

    if (emHigh && agora - ultimaTroca >= tempoHigh)
    {
        digitalWrite(pino, LOW);
        emHigh = false;
        ultimaTroca = agora;
    }
    else if (!emHigh && agora - ultimaTroca >= tempoLow)
    {
        ciclosAtuais++;
        if (ciclosAtuais >= ciclosMaximos)
        {
            if (sensorAlvo->getEstado() == Sensor::Estado::VIOLADO)
            {
                // 👉 REGISTRA o evento e DESATIVA o sensor
                registrarEvento("[ALERTA] Sirene tocou 4 vezes seguidas. Sensor " + sensorAlvo->getNome() + " da zona " + sensorAlvo->getZona() + " foi desabilitado.");
                sensorAlvo->desativar();
            }
            desativar();
            return;
        }

        if (sensorAlvo->getEstado() == Sensor::Estado::VIOLADO)
        {
            digitalWrite(pino, HIGH);
            emHigh = true;
            ultimaTroca = agora;
        }
        else
        {
            desativar(); // zona não violada, encerra
        }
    }
}

void Sirene::desativar()
{
    digitalWrite(pino, LOW);
    ativa = false;
    sensorAlvo = nullptr;
}

bool Sirene::estaAtiva() const
{
    return ativa;
}
