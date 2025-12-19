#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

// ======================== CREDENCIAIS Wi-Fi ========================
#define WIFI_SSID     "ZEUS_2G"
#define WIFI_PASS     "tito2000"  // Substitua por sua senha 


// Constantes de segurança
#define SENHA_ADMIN_PADRAO "presidente"  // ALTERE PARA UMA SENHA FORTE!
#define MAX_TENTATIVAS_LOGIN 3
#define TEMPO_BLOQUEIO_LOGIN 300000  // 5 minutos em ms


// ======================== PINOS DO HARDWARE ========================
// PIRs
/* #define PIR1_PIN      D0  // RECICLO
#define PIR2_PIN      D1  // TRIAGEM
#define PIR4_PIN      D2  // BAZAR

// REEDs
#define REED2_PIN     D6  // TRIAGEM
#define REED3_PIN     D7  // HORTA
#define REED4_PIN     D4  // BAZAR */

// Outros dispositivos
#define BTN_ARM_PIN   D3  // Botão físico de armar/desarmar
#define BUZZER_PIN    D5  // Sirene/Buzzer sonoro

/* // ======================== NOMES DAS ZONAS ==========================
#define ZONA_1_NAME   "RECICLO"
#define ZONA_2_NAME   "TRIAGEM"
#define ZONA_3_NAME   "HORTA"
#define ZONA_4_NAME   "BAZAR"
 */
// ======================== HORÁRIOS AUTOMÁTICOS =====================
extern int ARM_HOUR_WEEKDAY ;
extern int  DISARM_HOUR_WEEKDAY;
extern int  ARM_HOUR_WEEKEND ;
extern int  DISARM_HOUR_WEEKEND ; 
// ======================== ARQUIVOS NO SISTEMA ======================
#define HISTORICO_PATH      "/historico.json"
#define HTML_INDEX_PATH     "/index.html"
#define HTML_ADMIN_PATH     "/admin.html"
#define USUARIOS_PATH       "/usuarios.json"
#define LOGO_PATH           "/LOGO_OTIMIZADO.png"

// ======================== PARÂMETROS DO HISTÓRICO =================
#define HISTORICO_MAX_REGISTROS  100

#endif
