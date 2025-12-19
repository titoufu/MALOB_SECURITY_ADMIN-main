#ifndef SIRENE_H
#define SIRENE_H

#include <Arduino.h>

class Sensor;  // Forward declaration

class Sirene {
public:
    Sirene(int pino, unsigned long tempoHigh, unsigned long tempoLow, int ciclosMaximos);

    void ativar(Sensor* sensorAlvo);
    void atualizar();
    void desativar();
    bool estaAtiva() const;
    bool ciclosEncerrados() const;

private:
    int pino;
    unsigned long tempoHigh;
    unsigned long tempoLow;
    int ciclosMaximos;

    bool ativa;
    unsigned long ultimaTroca;
    int ciclosAtuais;
    bool emHigh;
    Sensor* sensorAlvo;
};

#endif
