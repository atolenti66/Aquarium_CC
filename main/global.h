/*
 +---------------------------------------+
 |      Aquarium Command & Control       |
 |                  ACC                  |  
 |Procedure: GLOBAl.H                    |      
 +---------------------------------------+
 Project Purpose: Control an Aquarium parameters like Temperarture
                  pH, other water quality parameters from data received
                  of probes. It also will count with schedule to water
                  change useing actuators (like peristaltc pumps). All
                  data will be plotted on Web using an IoT cloud (like Blynk)
                  core will be an ESP32 board.


 Procedure Purpose: Used to keep Global definitions

 Author: Alberto Tolentino (and Gemini AI)
 
== Functional specification ==



== Version History ==
02/11/2025 - 0.03 - Added TPA management flags and variables
01/11/2025 - 0.02 - Moved all libraries from other files to this one. Added all flag
                    varables from module 2 (pH)
31/10/2025 - 0.01 - First installment based on code created by Gemini AI (Google)

== Project file structure ==
main.ino          - Main program with setup parameters and loop functions
config.h          - Pin definitions, passwords, IDs (Auth, SSID, Pass)
global.h          - (this file) Global definitions
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

*/

#pragma once // Garante que o arquivo seja incluído apenas uma vez

// --- Bibliotecas usadas ---
#include <WiFi.h>                 // Principalmente utilizada em main.ino
#include <BlynkSimpleEsp32.h>     // Mantemos o Blynk, mas só para comunicação IoT. Utilizada em todos os módulos
#include <OneWire.h>              // Usada em sensors.ino
#include <DallasTemperature.h>    // Usada em sensors.ino
#include <DS3232RTC.h>              // RTC DS3232 Lib from Jack Christensen
#include <RTClib.h>               // Utilizada para comunicação com módulo RTC. Principalmente utilizada em rtc_time.ino
#include <Ticker.h>               // Utilizada para efetuar o schedule de funções de coletas de dados. Utilizada em main.ino
#include <time.h>                 // Adicione esta biblioteca para o NTP. Utilizada em rtc_time.ino
#include <TimeLib.h>              // Para declarar as funções hour(), minute() e second()
#include <Arduino.h>              // Used on ph_sensor.ino
#include <ArduinoJson.h>          // Para salvar e carregar dados estruturados (JSON)
#include <LittleFS.h>             // Para gerenciamento do sistema de arquivos
#include <Adafruit_GFX.h>         // Para uso de OLED Display (em main.ino)
#include <Adafruit_SSD1306.h>     // Para uso de OLED Display (em main.ino)
#include <Button2.h>              // Para lidar com botões

// --- DECLARAÇÕES DE OBJETOS/INSTÂNCIAS GLOBAIS ---
// Elas SÃO DEFINIDAS (alocadas) APENAS em main.ino
extern Ticker sensorDataTicker; 
extern OneWire oneWire;
extern DallasTemperature sensors;
extern RTC_DS3231 rtc;

// --- DECLARAÇÕES DE VARIÁVEIS DE ESTADO (FLAGS) ---
// Elas SÃO DEFINIDAS (alocadas) APENAS em main.ino
extern bool rtc_ok;            // Usada para definir o estado do RTC
extern bool rtc_osf_flag;      // Estado do Oscillator Stop Flag, usado para controlar bateria do RTC
extern bool rtcOsfAlertSent;   // Flag para garantir que o alerta de OSF seja enviado apenas uma vez
extern bool highTempAlertSent; // Usada para defnir o volume de alertas no Blynk
extern bool phAlertSent;       // Flag para controle de envio de alerta de pH
extern bool configIsDirty;     // Flag para definir que configurações devem ser salvas em SPIFFS/LittleFS

// --- VARIÁVEIS DE ESTADO E CALIBRAÇÃO (Módulo 2: pH) ---
extern float phValue;             // Último valor lido do pH
extern float phCalibrationOffset; // Offset para ajuste de calibração (ex: -1.5)
extern bool phCalibrationMode;    // Flag para indicar que o sistema está em modo de calibração
extern float temperatureC;        // Utilizada em sensors.ino

// --- Variáveis de Configuracao TPA (Módulo 5.1: TPA) ---
extern float aquariumTotalVolume;   // Volume total do aquario em Litros
extern float tpaExtractionPercent;  // Percentual de agua a ser extraida (%)

// --- Variáveis de Estado TPA (Módulo 5.1) ---
extern bool tpaExtractionPumpState;  // Estado atual da bomba de extração (ON/OFF)
extern float volumeToExtractLiters;   // Volume calculado para extração (L)
extern unsigned long tpaPumpDurationMs; // Duracao da bomba em ms para o volume
extern unsigned long tpaExtractionStartTime; // millis() que marca o inicio da extracao TPA

// --- VARIÁVEIS DE USO COM BOTÕES FÍSICOS ---
extern bool serviceModeActive; // Estado do modo de manutencao
extern Button2 oledPageButton;
extern Button2 upButton;
extern Button2 downButton;
extern Button2 phCalButton;
extern Button2 alertResetButton;
extern Button2 serviceModeButton;
extern Button2 rtcResetButton;

// --- MÓDULO 5: Agendamento Local (Fallback) ---
extern bool tpaLocalScheduleActive;     // Ativa o agendamento local (fallback)
extern int tpaScheduleDay;              // Dia da semana (1=Dom, 7=Sab ou dia do mes para mensal)
extern int tpaScheduleHour;             // Hora da execucao (0-23)
extern int tpaScheduleMinute;           // Minuto da execucao (0-59)
extern int tpaScheduleFrequency;        // 0=Diaria, 1=Semanal, 2=Quinzenal, 3=Mensal
extern unsigned long lastTpaExecution;  // millis() ou RTC time da ultima execucao local

// --- Variáveis de Gerenciamento de Display (Para hardware_manager e display_manager) ---
#define NUM_OLED_PAGES 4  // Total de paginas implementadas no OLED (0, 1, 2, 3...)
extern int currentPage;   // Rastreia a pagina atual exibida no OLED (usada pelo hardware_manager para trocar)
extern int page1EditMode; // Rastreia qual item da Página 1 (TPA Agendamento) esta sendo editado (0=Dia, 1=Hora, 2=Minuto, 3=Freq, 4=Salvar)
extern int page2EditMode; // Rastreia o modo de edicao da Pagina 2 (0=Visualizar, 1=Editar Volume)
extern int page3EditMode; // Rastreia o modo de edicao da Pagina 3 (Buffer)

//Variáveis para uso de reposição de TPA (tpa_reposition)
// --- TPA Master States (Controla o fluxo geral da TPA) ---
enum TpaMasterState {
    TPA_MASTER_IDLE,                     // TPA inativa, aguardando agendamento/comando
    TPA_MASTER_EXTRACTION_RUNNING_M51,   // Extração (M5.1) em andamento
    TPA_MASTER_REPOSITION_RUNNING_M52,   // Reposição (M5.2) em andamento
    TPA_MASTER_REFILL_RUNNING_M53,       // Enchimento (M5.3) em andamento (próximo módulo)
    TPA_MASTER_BUFFER_DOSING_M54,        // Dosagem de Buffer (M5.4) em andamento
    TPA_MASTER_COMPLETED                 // Ciclo TPA completo
};
extern TpaMasterState tpaMasterCurrentState;

// --- Módulo 5.2 (tpa_reposition.ino) ---
enum RepositionState {
    TPA_REPOSITION_IDLE,
    TPA_REPOSITION_WAIT_SAFETY_PAUSE,
    TPA_REPOSITION_TRANSFER_RAN_TO_AQUARIO,
    TPA_REPOSITION_FINISHED
};
extern RepositionState tpaRepositionCurrentState;

// --- Módulo 5.3 (tpa_refill) ---
enum RanRefillState {
    RAN_REFILL_IDLE,                 // Enchimento inativo
    RAN_REFILL_START_DELAY,          // Aguardando um breve momento antes de abrir a solenoide
    RAN_REFILL_FILLING,              // Solenoide aberta, enchendo o RAN
    RAN_REFILL_FINISHED              // Enchimento concluído, pronto para o próximo ciclo TPA
};
extern RanRefillState ranRefillCurrentState; // Estado atual do M5.3

// --- Variáveis de Monitoramento do RAN ---
extern bool ranLevelFull;         // Status do sensor de nível (true = cheio, false = baixo)
extern int ranLevelPercent;       // Nível do RAN em percentual (0-100)
extern bool ranRefillAlertSent;   // Flag para garantir que o alerta de falha de enchimento seja enviado apenas uma vez
extern unsigned long ranRefillStartTime; // Captura o início do procedimento de enchimento do RAN

// --- Variáveis de Monitoramento do reposição ---
extern unsigned long repositionPreviousMillis;
extern unsigned long repositionIntervalMs; // Tempo de espera/execução atual (em ms)
extern unsigned long tpaPumpDurationMs;    // Duração da bomba em ms para o volume
extern float volumeToExtractLiters;        // Volume de extração calculado (L)
extern float volumeToRepositionLiters;     // Volume de reposição, ajustável pelo usuário
extern int ranBufferVolumeML;              // Volume de buffer a ser dosado (mL, 0-999)

// --- Variáveis de Monitoramento do Dosagem de Buffer ---
extern unsigned long bufferPreviousMillis;    // Tempo de início da dosagem
extern unsigned long bufferDosingDurationMs;  // Duração total da dosagem em ms

// --- Módulo 5.4 (tpa_buffer_dosing) ---
enum BufferDosingState {
    TPA_BUFFER_IDLE,                     // Dosagem inativa
    TPA_BUFFER_DOSING,                   // Bomba ligada, dosando
    TPA_BUFFER_FINISHED                  // Dosagem concluída
};
extern BufferDosingState tpaBufferCurrentState; // Estado atual do M5.4
extern int ranBufferVolumeML;              // Volume de buffer a ser dosado (mL, 0-999)
extern unsigned long bufferPreviousMillis;    // Tempo de início da dosagem
extern unsigned long bufferDosingDurationMs;  // Duração total da dosagem em ms
extern float bufferVolumeLiters;          // Volume de buffer em Litros
// --- (Aqui entrarão as variáveis do Módulo 2: pH, etc.) ---
// extern float phValue;
// extern bool phCalibrationMode;