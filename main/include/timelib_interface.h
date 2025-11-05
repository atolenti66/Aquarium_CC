/*
 +---------------------------------------+
 |      Aquarium Command & Control       |
 |                  ACC                  |  
 |Procedure: TIMELIB_INTERFACE.H         |      
 +---------------------------------------+
 Project Purpose: Control an Aquarium parameters like Temperarture
                  pH, other water quality parameters from data received
                  of probes. It also will count with schedule to water
                  change useing actuators (like peristaltc pumps). All
                  data will be plotted on Web using an IoT cloud (like Blynk)
                  core will be an ESP32 board.


 Procedure Purpose: Mocks for TimeLib for unit testing

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
blynk_interface.h - *this file) Blynk related functions and handlers   
sensors_interface.h - Mocks for OneWire and DallasTemperature for unit testing
rtc_interface.h   - RTC interface abstraction for unit testing
timelib_interface.h - (this file) Mocks for TimeLib for unit testing
timelib_interface.h - Mocks for TimeLib for unit testing

*/

#ifndef TIMELIB_INTERFACE_H
#define TIMELIB_INTERFACE_H

// --- SE ESTIVER EM TESTE UNITÁRIO (MOCKING) ---
#ifdef UNIT_TEST 

// Variável Global de Injeção
// Permite que o teste defina qual deve ser o tempo 'agora' do TimeLib.
// (Geralmente, o teste irá simplesmente incrementar este valor)
extern unsigned long mockTimeLibNow; 

// Mock do tipo de função usado por setSyncProvider (o TimeLib real usa 'time_t (*)(void)')
typedef unsigned long (* TimeSyncProvider)();

// 1. Funções de Obtenção de Tempo (Retorno de valores fixos ou mockados)

// now() retorna o tempo Unix.
time_t now() {
    return (time_t)mockTimeLibNow;
}

// Funções de Componente de Tempo (Retornam valores fixos para simplificar o teste)
// No ambiente de teste, a lógica de agendamento se concentra no valor de now() e unixtime(), 
// então podemos retornar valores seguros para as componentes.
uint8_t year() { return (uint8_t)2025; }
uint8_t month() { return (uint8_t)11; }
uint8_t day() { return (uint8_t)5; }
uint8_t hour() { return (uint8_t)10; }
uint8_t minute() { return (uint8_t)30; }
uint8_t second() { return (uint8_t)0; }

// 2. Mock de setSyncProvider
// Apenas simula a função, pois o teste não precisa da sincronização real.
void setSyncProvider(TimeSyncProvider p) { 
    (void)p; // Apenas para evitar um aviso de "unused parameter"
}


// --- SE ESTIVER NO ESP32 REAL ---
#else 

// Inclui o cabeçalho real.
#include <TimeLib.h> 

#endif // UNIT_TEST

#endif // TIMELIB_INTERFACE_H
