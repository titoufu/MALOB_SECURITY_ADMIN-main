#ifndef ALARME_H
#define ALARME_H

#include <Arduino.h>
#include <vector>
#include "zona.h"
#include "sirene.h"

class Alarme
{
public:
    enum class Estado { DESARMADO, ARMADO };
    enum class Modo { MANUAL, AUTOMATICO };

    Alarme();

    void armar(const std::vector<String> &zonas);
    void desarmar();
    void atualizar();

    void definirSirene(Sirene *s);
    void adicionarZona(Zona *zona);

    Estado getEstado() const;
    Modo getModo() const;
    void setModo(Modo m);

    bool zonaEstaAtiva(const String &nomeZona) const;
    const std::vector<String> &getZonasAtivas() const;
    const std::vector<Zona*>& getZonas() const;
    void imprimirDados() const;
    void limparZonas();

private:
    Estado estadoAtual;
    Modo modoAtual;
    std::vector<Zona *> zonas;
    std::vector<String> zonasAtivas;
    Sirene *sirene;
};

void checkAutoSchedule(Alarme &alarme);
void checkDailyRestart();

extern int HORA_RESTART;
extern bool RESTART_CONFIG;

#endif
