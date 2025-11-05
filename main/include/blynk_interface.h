/*
 +---------------------------------------+
 |      Aquarium Command & Control       |
 |                  ACC                  |  
 |Procedure: BLYNK_INTERFACE.H           |      
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
blynk_interface.h - *this file) Blynk related functions and handlers   
sensors_interface.h - Mocks for OneWire and DallasTemperature for unit testing
rtc_interface.h   - RTC interface abstraction for unit testing
timelib_interface.h - Mocks for TimeLib for unit testing

*/

#ifndef BLYNK_INTERFACE_H
#define BLYNK_INTERFACE_H

// --- SE ESTAMOS NO AMBIENTE DE TESTE UNITÁRIO ---
#ifdef UNIT_TEST 

// 1. Simulação (Mocking) de Funções de Escrita e Status:
// As funções Blynk.virtualWrite, Blynk.connected, etc., são substituídas por
// macros que se expandem para nada ou para código de teste.

// Substituímos por (void)0 para que o compilador ignore a chamada,
// pois o teste não precisa realmente enviar dados.
#define Blynk_VirtualWrite(pin, value) (void)0 
#define Blynk_LogEvent(event)          (void)0 

// Simulação de status: Assumimos que o Blynk está sempre conectado para o teste de lógica.
#define Blynk_Connected()              true 

// 2. Mocking de Handlers (BLYNK_WRITE / BLYNK_CONNECTED):
// Esses handlers não são relevantes para o teste de lógica interna do seu módulo.
// Eles são substituídos por uma macro que não faz nada.
#define BLYNK_WRITE(pin)               (void)0 
#define BLYNK_CONNECTED()              (void)0

// --- SE ESTAMOS NO HARDWARE REAL (ESP32) ---
#else 

// 1. Inclusão da Biblioteca Real:
#include <BlynkSimpleEsp32.h>

// 2. Mapeamento Direto:
// Usamos as funções reais.
#define Blynk_VirtualWrite(pin, value) Blynk.virtualWrite(pin, value)
#define Blynk_LogEvent(event)          Blynk.logEvent(event)
#define Blynk_Connected()              Blynk.connected()

// 3. Handlers Reais:
// Mantemos os handlers como a biblioteca Blynk espera.
#define BLYNK_WRITE(pin)               BLYNK_WRITE(pin)
#define BLYNK_CONNECTED()              BLYNK_CONNECTED()

#endif // UNIT_TEST

#endif // BLYNK_INTERFACE_H