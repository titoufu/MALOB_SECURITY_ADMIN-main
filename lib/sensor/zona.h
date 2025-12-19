#ifndef ZONA_H
#define ZONA_H

#include <Arduino.h>
#include <vector>
#include "sensor.h"

class Zona
{
public:
    enum class Estado
    {
        NAO_VIOLADA,
        VIOLADA
    };

    Zona(const String &nome);
    ~Zona(); 
    void adicionarSensor(Sensor *sensor);
    void armar();
    void desarmar();
    void atualizar();

    Estado getEstado() const;
    String getNome() const;

    bool estaViolada() const;
    const std::vector<Sensor *> &getSensores() const;
    bool sensorPodeViolar(const Sensor *sensor) const
    {
        return sensor->getSituacao() == Sensor::Situacao::ATIVO &&
               !sensor->estaIsolado();
    }

private:
    String nome;
    std::vector<Sensor *> sensores;
    bool armada;
    Estado estadoAtual;
};

#endif
