/*
 +---------------------------------------+
 |      Aquarium Command & Control       |
 |                  ACC                  |  
 |Procedure: RTC_TIME                    |      
 +---------------------------------------+
 Project Purpose: Control an Aquarium parameters like Temperarture
                  pH, other water quality parameters from data received
                  of probes. It also will count with schedule to water
                  change useing actuators (like peristaltc pumps). All
                  data will be plotted on Web using an IoT cloud (like Blynk)
                  core will be an ESP32 board.


 Procedure Purpose: Store functions like setupNTP(), getCurrentTime(), 
                    BLYNK_CONNECTED(), BLYNK_WRITE(RTC_RESET) and are
                    used to deal with RTC module (time)

 Author: Alberto Tolentino (and Gemini AI)
 
== Functional specification ==



== Version History ==
31/10/2025 - 0.02 - Adjusted Blynk events to new log function. Included utils.h
                    Added checkRtcOsf() to deal with power lost
31/10/2025 - 0.01 - First installment based on code created by Gemini AI (Google)

== Project file structure ==
main.ino          - Main program with setup parameters and loop functions
config.h          - Pin definitions, passwords, IDs (Auth, SSID, Pass)
global.h          - Global definitions
utils.h           - Functions prototypes
rtc_time.ino      - (this file) setupNTP(), getCurrentTime(), BLYNK_CONNECTED(), BLYNK_WRITE(RTC_RESET)
sensors.ino       - readTemperature(), readPH(), alert code
ph_sensor         - Deal with readings from pH probe
config_manager    - Used to save config/data on JSON on SPIFF(LittleFS) partition on ESP32
display_manager   - Deal with all output on OLED Display
hardware_manager  - Deal with all physical buttons in the project
actuators_manager - Deal with all pump e actuator hardware in the system
tpa_manager       - Coordinate the partial water change (in portuguese TPA ou troca parcial de água)
tpa_reposition    - Control return of water volume

*/

#include "config.h"
#include "global.h"
#include "utils.h"

// Define o objeto RTC (global, extern em global.h)
RTC_DS3231 rtc;

// --- FUNÇÕES GERAIS DE HORA ---

// Função principal para obter a hora atual para LOGICA (retorna um objeto DateTime)
DateTime getDateTimeNow() {
    // 1. Prioriza o RTC fisico se estiver OK
    if (rtc_ok && !rtc_osf_flag) {
        return rtc.now();
    } 
    
    // 2. Fallback para hora NTP/Internet (se o Blynk estiver conectado)
    if (Blynk.connected()) {
        // A função now() do TimeLib (incluída pelo Blynk) retorna a hora sincronizada
        return DateTime(year(), month(), day(), hour(), minute(), second());
    }

    // 3. Fallback final: Retorna uma hora padrao (necessario para evitar erros de tipo)
    // Se o RTC e o Blynk estiverem OFF, retorna o ultimo registro de boot
    return DateTime(F(__DATE__), F(__TIME__));
}


// Função auxiliar para obter a hora atual para DISPLAY (retorna uma String)
String getCurrentTimeString() {
    DateTime now = getDateTimeNow();
    
    String timeStr = "";
    if (now.hour() < 10) timeStr += '0';
    timeStr += now.hour();
    timeStr += ':';
    if (now.minute() < 10) timeStr += '0';
    timeStr += now.minute();
    timeStr += ':';
    if (now.second() < 10) timeStr += '0';
    timeStr += now.second();
    
    return timeStr;
}


// --- SETUP E SINCRONIZAÇÃO RTC ---

time_t getRtcTime() {
    return rtc.now().unixtime();
}

void setupRTC() {
    if (!rtc.begin()) {
        Serial.println(F("ERRO: Nao foi possivel encontrar o RTC DS3231."));
        logSystemEvent("critical", "Falha ao iniciar RTC DS3231.");
        rtc_ok = false;
    } else {
        rtc_ok = true;
        checkRtcStatus();
    }

    // Seta a flag do TimeLib para usar o RTC como fonte principal
    if (rtc_ok) {
        // Sincroniza o TimeLib com a hora do RTC no boot
        setSyncProvider(getRtcTime); 
        // Se a hora for invalida (menos que 2020), nao usa o RTC
        if (year() < 2020) {
             Serial.println(F("RTC com hora invalida. Aguardando NTP."));
        } else {
             Serial.println("RTC OK. Hora: " + getCurrentTimeString());
        }
    }
}

void checkRtcStatus() {
    if (!rtc_ok) return;

    rtc_osf_flag = rtc.lostPower();

    if (rtc_osf_flag) {
        Serial.println(F("AVISO: RTC perdeu energia! Sera sincronizado pelo NTP."));
        logSystemEvent("warning", "RTC perdeu energia. Sincronizando via NTP.");
    }
    rtc.clearAlarm(1);
    rtc.clearAlarm(2);
}

void syncRtcFromNtp(time_t ntpTime) {
    if (!rtc_ok) return;
    
    // Converte time_t para DateTime
    DateTime ntpDt(ntpTime); 
    
    // Seta a hora do RTC
    rtc.adjust(ntpDt); 
    rtc_osf_flag = false; // Reseta a flag apos ajuste
    
    Serial.println(F("RTC ajustado pelo NTP."));
    logSystemEvent("info", "RTC sincronizado via NTP.");
}


// --- HANDLERS BLYNK ---

// Chamado automaticamente quando o Blynk se conecta
BLYNK_CONNECTED() {
    // Solicita o tempo atual do servidor Blynk para sincronizar o RTC
    if (Blynk.connected()) {
        Blynk.syncAll();
        // A função BLYNK_APP_CONNECTED será chamada logo apos, onde o NTP sera usado
    }
}

// BLYNK_WRITE para reset manual do RTC
// Usar um botao no VPIN_RTC_RESET (V5, por exemplo)
BLYNK_WRITE(VPIN_RTC_RESET) {
    if (param.asInt() == 1) { // Acao de PUSH do botao
        Serial.println(F("Comando de Reset/Sincronizacao RTC recebido do Blynk."));
        // Forca a sincronizacao usando a hora do servidor
        syncRtcFromNtp(now()); 
    }
}