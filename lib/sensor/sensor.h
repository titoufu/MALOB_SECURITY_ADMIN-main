#ifndef SENSOR_H
#define SENSOR_H

#include <Arduino.h>

class Sensor
{
public:
    enum class Tipo
    {
        PIR,
        REED
    };
    enum class Estado
    {
        NAO_VIOLADO,
        VIOLADO
    };
    enum class Situacao
    {
        ATIVO,
        INATIVO
    };

    Sensor(const String &nome, Tipo tipo, int pino, const String &zona);
    Sensor(const String &nome, Tipo tipo, int pino, const String &zona, bool ativo); // novo construtor

    void atualizar();     // Atualiza estado com base na leitura do pino
    void resetarAlerta(); // Reseta todos os atributos de estado

    Estado getEstado() const;
    Situacao getSituacao() const;
    String getNome() const;
    String getZona() const;
    int getTentativas() const;
    bool estaIsolado() const;

    void ativar();
    void desativar();

    bool foiAlertaEmitido() const;
    void setAlertaEmitido(bool valor);

    Tipo getTipo() const;
    int getPino() const;

    static Tipo tipoFromString(const String& str); // nova função auxiliar
    bool estaAtivo() const { return situacaoAtual == Situacao::ATIVO; }
    String getStatusString() const;

private:
    String nome;
    Tipo tipo;
    int pino;
    String zona;

    Estado estadoAtual;
    Situacao situacaoAtual;

    unsigned long tempoUltimoAlerta;
    int tentativas;
    bool isolado;
    bool alertaEmitido;
};


#endif
