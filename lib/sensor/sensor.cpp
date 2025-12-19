#include "sensor.h"
Sensor::Sensor(const String &nome, Tipo tipo, int pino, const String &zona, bool ativo)
    : nome(nome), tipo(tipo), pino(pino), zona(zona),
      estadoAtual(Estado::NAO_VIOLADO),
      situacaoAtual(ativo ? Situacao::ATIVO : Situacao::INATIVO),
      tempoUltimoAlerta(0), tentativas(0), isolado(false), alertaEmitido(false)
{
    pinMode(pino, INPUT);
}

void Sensor::atualizar()
{
    if (situacaoAtual == Situacao::INATIVO || isolado)
    {
        // Serial.printf("[DEBUG] Ignorado: %s (situacao: %s, isolado: %d)\n",
        //               nome.c_str(),
        //               situacaoAtual == Situacao::ATIVO ? "ATIVO" : "INATIVO",
        //               isolado);
        return; // ← agora sim, só retorna se for para ignorar
    }

    bool violado = false;

    if (tipo == Tipo::PIR)
    {
        violado = digitalRead(pino) == LOW;
    }
    else if (tipo == Tipo::REED)
    {
        violado = digitalRead(pino) == LOW;
    }

    // Serial.printf("[DEBUG] Leitura digital do sensor %s: %d => violado: %s\n",
    //               nome.c_str(), digitalRead(pino), violado ? "SIM" : "NAO");

    if (violado)
    {
        if (estadoAtual == Estado::NAO_VIOLADO)
        {
            estadoAtual = Estado::VIOLADO;
            tempoUltimoAlerta = millis();
            tentativas = 1;
            alertaEmitido = false;
        }
        else if (millis() - tempoUltimoAlerta >= 60000 && tentativas < 4)
        {
            tentativas++;
            tempoUltimoAlerta = millis();
        }
        else if (tentativas >= 4)
        {
            isolado = true;
        }
    }
    else
    {
        if (estadoAtual == Estado::VIOLADO)
        {
            estadoAtual = Estado::NAO_VIOLADO;
            alertaEmitido = false;
        }

        if (!isolado)
        {
            tentativas = 0;
        }

        // Serial.printf("[DEBUG] Estado atualizado para %s, tentativas: %d\n",
        //               estadoAtual == Estado::VIOLADO ? "VIOLADO" : "NAO_VIOLADO",
        //               tentativas);
    }
}

void Sensor::resetarAlerta()
{
    estadoAtual = Estado::NAO_VIOLADO;
    tentativas = 0;
    isolado = false;
}

Sensor::Estado Sensor::getEstado() const { return estadoAtual; }
Sensor::Situacao Sensor::getSituacao() const { return situacaoAtual; }
String Sensor::getNome() const { return nome; }
String Sensor::getZona() const { return zona; }
int Sensor::getTentativas() const { return tentativas; }
bool Sensor::estaIsolado() const { return isolado; }

void Sensor::ativar() { situacaoAtual = Situacao::ATIVO; }
void Sensor::desativar() { situacaoAtual = Situacao::INATIVO; }

bool Sensor::foiAlertaEmitido() const { return alertaEmitido; }
void Sensor::setAlertaEmitido(bool valor) { alertaEmitido = valor; }

Sensor::Tipo Sensor::getTipo() const { return tipo; }
int Sensor::getPino() const { return pino; }

Sensor::Tipo Sensor::tipoFromString(const String &str)
{
    if (str == "REED")
        return Tipo::REED;
    return Tipo::PIR;
}

String Sensor::getStatusString() const
{
    char buffer[150];
    snprintf(buffer, sizeof(buffer),
             "Sensor: %-10s | Tipo: %-4s | Pino: %-2d | Zona: %-10s | "
             "Estado: %-12s | Situacao: %-8s | Alerta: %s",
             nome.c_str(),
             (tipo == Tipo::PIR ? "PIR" : "REED"),
             pino,
             zona.c_str(),
             (estadoAtual == Estado::VIOLADO ? "VIOLADO" : "NAO_VIOLADO"),
             (situacaoAtual == Situacao::ATIVO ? "ATIVO" : "INATIVO"),
             (alertaEmitido ? "SIM" : "NAO"));
    return String(buffer);
}