#include "zona.h"
#include "sensor.h"

Zona::Zona(const String &nome)
    : nome(nome), armada(true), estadoAtual(Estado::NAO_VIOLADA)
{
}

void Zona::adicionarSensor(Sensor *sensor)
{
    sensores.push_back(sensor);
}

void Zona::armar() {
    armada = true;
    for (auto sensor : sensores) {
        // Mantém o estado original (não força ativação)
        if (sensor->getSituacao() == Sensor::Situacao::ATIVO) {
            // Apenas atualiza o estado do sensor (sem alterar ATIVO/INATIVO)
            sensor->atualizar(); 
        }
    }
}

void Zona::desarmar() {
    armada = false;
    for (auto sensor : sensores) {
        // Não desativa completamente, apenas marca como não armado
        sensor->resetarAlerta(); // Ou outro método apropriado
    }
}
void Zona::atualizar()
{
    estadoAtual = Estado::NAO_VIOLADA;
    if (!armada)
        return;

    for (auto sensor : sensores)
    {
        sensor->atualizar();

        // Verificação mais explícita
        if (sensor->getSituacao() != Sensor::Situacao::ATIVO)
        {
            continue; // Pula sensores inativos
        }

        if (sensor->getEstado() == Sensor::Estado::VIOLADO &&
            sensor->estaAtivo() &&
            !sensor->estaIsolado())
        {
            estadoAtual = Estado::VIOLADA;
            break;
        }
    }
}
Zona::~Zona()
{
    for (auto s : sensores)
    {
        delete s;
    }
    sensores.clear();
}
Zona::Estado Zona::getEstado() const { return estadoAtual; }
bool Zona::estaViolada() const { return estadoAtual == Estado::VIOLADA; }
String Zona::getNome() const { return nome; }
const std::vector<Sensor *> &Zona::getSensores() const { return sensores; }
