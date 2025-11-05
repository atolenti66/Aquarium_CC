/*
 +---------------------------------------+
 |      Aquarium Command & Control       |
 |                  ACC                  |  
 |Procedure: RTC_INTERFACE.H             |      
 +---------------------------------------+
 Project Purpose: Control an Aquarium parameters like Temperarture
                  pH, other water quality parameters from data received
                  of probes. It also will count with schedule to water
                  change useing actuators (like peristaltc pumps). All
                  data will be plotted on Web using an IoT cloud (like Blynk)
                  core will be an ESP32 board.


 Procedure Purpose: Blynk related functions and handlers   

 Author: Alberto Tolentino (and Gemini AI)
 
== Functional specification ==



== Version History ==
04/11/2025 - 0.0.1 - First installment based on code created by Gemini AI (Google)

== Project file structure ==
main.ino          - Main program with setup parameters and loop functions
config.h          - Pin definitions, passwords, IDs (Auth, SSID, Pass)
global.h          - Global definitions
utils.h           - Functions prototypes
rtc_time.ino      - setupNTP(), getCurrentTime(), BLYNK_CONNECTED(), BLYNK_WRITE(RTC_RESET)
sensors.ino       - readTemperature(), readPH(), alert code
ph_sensor         - Deal with readings from pH probe
config_manager    - Used to save config/data on JSON on SPIFF(LittleFS) partition on ESP32
display_manager   - Deal with all output on OLED Display
hardware_manager  - Deal with all physical buttons in the project
actuators_manager - Deal with all pump e actuator hardware in the system
tpa_manager       - Coordinate the partial water change (in portuguese TPA ou troca parcial de água)
tpa_reposition    - Control return of water volume
blynk_interface.h - Blynk related functions and handlers   
sensors_interface.h - Mocks for OneWire and DallasTemperature for unit testing
rtc_interface.h   - (this file) RTC interface abstraction for unit testing
rtc_interface.h   - RTC interface abstraction for unit testing
timelib_interface.h - Mocks for TimeLib for unit testing

*/

#ifndef RTC_INTERFACE_H
#define RTC_INTERFACE_H

// --- SE ESTIVER EM TESTE UNITÁRIO (MOCKING) ---
#ifdef UNIT_TEST 

// VÁRIAVEIS GLOBAIS DE INJEÇÃO
// Estas variáveis serão definidas e controladas no seu arquivo de teste (test/*.cpp)
// para simular diferentes estados do RTC.

// Permite ao teste definir o tempo que o RTC deve retornar (em Unix Time)
extern unsigned long mockRtcTime; 
// Permite ao teste controlar se a inicialização do RTC foi bem-sucedida
extern bool mockRtcBeginSuccess; 
// Permite ao teste controlar a flag de perda de energia
extern bool mockRtcLostPower; 


// -----------------------------------------------------
// MOCK DA ESTRUTURA DateTime (Usada para encapsular o tempo)
// -----------------------------------------------------

class DateTime {
private:
    unsigned long _t; // Tempo interno do mock (Unix Epoch)

public:
    // Construtor principal para receber Unix time (usado por rtc.now())
    DateTime(unsigned long t = 0) : _t(t > 0 ? t : mockRtcTime) {} 
    
    // Construtores Mockados (para compatibilidade com as chamadas em rtc_time.cpp)
    DateTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour = 0, uint8_t minute = 0, uint8_t second = 0) : _t(mockRtcTime) {}
    DateTime(const char* date, const char* time) : _t(mockRtcTime) {}

    // Acessores (retornam o tempo mockado ou um valor fixo para evitar erros)
    time_t unixtime() const { return _t; }
    
    // Retornamos valores fixos para as partes do tempo, pois a lógica de teste
    // se concentra em unixtime() e no funcionamento do FSM/agendamento.
    uint8_t hour() const { return (uint8_t)10; } 
    uint8_t minute() const { return (uint8_t)30; }
    uint8_t second() const { return (uint8_t)0; }
    uint16_t year() const { return 2025; }
    uint8_t month() const { return 11; }
    uint8_t day() const { return 5; }
};

// -----------------------------------------------------
// MOCK DA CLASSE RTC_DS3231 (A instância de hardware)
// -----------------------------------------------------
class RTC_DS3231 {
public:
    // rtc.begin()
    bool begin() { return mockRtcBeginSuccess; }

    // rtc.now()
    DateTime now() { return DateTime(mockRtcTime); } 

    // rtc.lostPower()
    bool lostPower() { return mockRtcLostPower; }

    // rtc.clearAlarm(1/2)
    void clearAlarm(uint8_t alarm) { /* Mock: Não faz nada */ }

    // rtc.adjust(DateTime)
    void adjust(const DateTime& dt) { 
        // No mock, ajustar o RTC significa apenas mudar o tempo injetado.
        mockRtcTime = dt.unixtime();
    }
};

// --- SE ESTIVER NO ESP32 REAL ---
#else 

// Inclui os cabeçalhos reais.
#include <RTClib.h> 

#endif // UNIT_TEST

#endif // RTC_INTERFACE_H