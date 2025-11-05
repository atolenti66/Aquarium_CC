/*
 +---------------------------------------+
 |      Aquarium Command & Control       |
 |                  ACC                  |  
 |Procedure: SENSORS_INTERFACE.H           |      
 +---------------------------------------+
 Project Purpose: Control an Aquarium parameters like Temperarture
                  pH, other water quality parameters from data received
                  of probes. It also will count with schedule to water
                  change useing actuators (like peristaltc pumps). All
                  data will be plotted on Web using an IoT cloud (like Blynk)
                  core will be an ESP32 board.


 Procedure Purpose: Mocks for OneWire and DallasTemperature for unit testing   

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
sensors_interface.h - (this file) Mocks for OneWire and DallasTemperature for unit testing
rtc_interface.h   - RTC interface abstraction for unit testing
timelib_interface.h - Mocks for TimeLib for unit testing

*/

#ifndef SENSORS_INTERFACE_H
#define SENSORS_INTERFACE_H

// --- SE ESTIVER EM TESTE UNITÁRIO ---
#ifdef UNIT_TEST 

// 1. Variável Global para Injetar a Temperatura
// Esta variável será acessada pelo seu código de teste para forçar um valor de leitura.
extern float mockTemperatureValue; 

// 2. Mock da Classe OneWire
// Precisamos apenas do construtor, pois o código real só a instancia.
class OneWire {
public:
    OneWire(int pin) { /* Mock: Não faz nada */ }
};

// 3. Mock da Classe DallasTemperature
// Precisamos dos métodos chamados em main.cpp e sensors.cpp
class DallasTemperature {
public:
    // Construtor (chamado em main.cpp)
    DallasTemperature(OneWire* oneWire) { /* Mock: Não faz nada */ } 
    
    // sensors.begin() (chamado em main.cpp)
    void begin() { /* Mock: Não faz nada */ } 
    
    // sensors.requestTemperatures() (chamado em sensors.cpp)
    void requestTemperatures() { /* Mock: Não faz nada */ } 
    
    // sensors.getTempCByIndex(0) (chamado em sensors.cpp)
    // ESTE É O MÉTODO QUE RETORNA O VALOR MOCKADO
    float getTempCByIndex(int index) { 
        return mockTemperatureValue; 
    }
};

// --- SE ESTIVER NO ESP32 REAL ---
#else 

// Inclui os cabeçalhos reais. Eles devem ser incluídos aqui, e não nos seus arquivos de código.
#include <OneWire.h>
#include <DallasTemperature.h>

#endif // UNIT_TEST

#endif // SENSORS_INTERFACE_H