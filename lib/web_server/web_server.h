#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <ESP8266WebServer.h>
class Alarme;
String getEstadoAtualJson();
std::vector<String> splitZonas(const String &zonasStr);

extern ESP8266WebServer server;

void web_server_setup(Alarme* alarme);
bool credenciais_validas(String usuario, String senha);
void registrarEvento(const String &mensagem);
void loadHorariosFromFS();
void salvarUltimoDiaReinicio(int dia);


#endif
