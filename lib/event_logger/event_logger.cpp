#include "event_logger.h"
#include <FS.h>
#include <LittleFS.h>
#include <time.h>

void registrarEvento(const String &mensagem)
{
    File f = LittleFS.open("/historico.json", "a+");
    if (f)
    {
        time_t agora = time(nullptr);
        char buffer[30];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", localtime(&agora));
        String linha = "{\"hora\":\"" + String(buffer) + "\",\"evento\":\"" + mensagem + "\"},\n";
        f.print(linha);
        f.close();
    }
}