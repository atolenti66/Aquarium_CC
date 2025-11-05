/*
 +---------------------------------------+
 |      Aquarium Command & Control       |
 |                  ACC                  |  
 |Procedure: CONFIG.H                    |      
 +---------------------------------------+
 Project Purpose: Control an Aquarium parameters like Temperarture
                  pH, other water quality parameters from data received
                  of probes. It also will count with schedule to water
                  change useing actuators (like peristaltc pumps). All
                  data will be plotted on Web using an IoT cloud (like Blynk)
                  core will be an ESP32 board.


 Procedure Purpose: Keep pin definitions, passwords, IDs

 Author: Alberto Tolentino (and Gemini AI)
 
== Functional specification ==



== Version History ==
01/11/2025 - 0.02 - Added Module 3 constants
31/10/2025 - 0.01 - First installment based on code created by Gemini AI (Google)

== Project file structure ==
main.ino          - Main program with setup parameters and loop functions
config.h          - (this file) Pin definitions, passwords, IDs (Auth, SSID, Pass)
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
rtc_interface.h   - RTC interface abstraction for unit testing
timelib_interface.h - Mocks for TimeLib for unit testing

*/

#pragma once

// --- Credenciais e Endereços BLYNK ---
char auth[] = "-B5ka8MiQzyJ7xYeCS1uxamXhfLHIkq_";  // Blynk Auth ID
char ssid[] = "HOMEWLAN";                          // WiFi SSID
char pass[] = "M3uC4oPr370";                       // WiFi password

// --- Pinos Virtuais BLYNK (Módulo 1: Teperatura, Tempo) ---
#define VPIN_TIME       V0    // Virtual Pin no Blynk para representar a hora
#define VPIN_TEMP       V5    // Virtual PIN no Blynk para representar temperatura
#define VPIN_RTC_RESET  V10   // Virtual Pin no Blynk para o botão de reset de alerta de bateria
#define VPIN_TEMP_ALERT V15   // Virtual Pin no Blynk para representar LED de alta temperatura

// --- Pinos Virtuais BLYNK (Módulo 2: pH) ---
#define VPIN_PH_VAL           V11    // Para exibir o valor do pH
#define VPIN_PH_CAL           V12    // Botão/Menu para acionar a calibração
#define VPIN_CAL_STATUS       V13    // Para exibir o status da calibração (e.g., "Calibrar pH 7")
#define VPIN_PH_ALERT         V14    // Para exibir o status do alerta de pH (LED ou Gauge Color)
#define VPIN_ALERT_RESET      V16    // Botão para resetar alertas criticos (PH, TEMP)
#define VPIN_SERVICE_MODE     V17    // Switch para ativar/desativar o modo de serviço

// --- Pinos Virtuais BLYNK (Módulo 5: TPA - actuators_manager.ino, tpa_manager.ino) ---
#define VPIN_TOTAL_VOLUME          V20 // Volume total do aquario (L)
#define VPIN_EXTRACTION_PERCENT    V21 // % de agua a ser extraida (Slider)
#define VPIN_TPA_SCHEDULE          V22 // Configuracao do agendamento TPA (Timer Widget)
#define VPIN_TPA_EXTRACTION_PUMP   V23 // Status/Controle da Bomba de Extracao (LED) em actuators_manager.ino
#define VPIN_LOCAL_SCHEDULE_ACTIVE V24 // Sincronização: Ativa/Desativa o Agendamento Local (Fallback). Altera a variável tpaLocalScheduleActive
#define VPIN_SCHEDULE_FREQUENCY    V25 // Sincronização: Define a frequência do Agendamento Local (0=Diária, 1=Semanal, 2=Quinzenal, 3=Mensal). Altera tpaScheduleFrequency.
#define VPIN_SCHEDULE_DAY          V26 // Sincronização: Define o dia da semana (1-7) ou dia do mês (1-31) para o Agendamento Local. Altera tpaScheduleDay.
#define VPIN_SCHEDULE_HOUR         V27 // Sincronização: Define a hora para o Agendamento Local. Altera tpaScheduleHour.
#define VPIN_EXTRACTION_BUTTON     V28 // Execução Manual: Botão de acionamento imediato da TPA de Extração (chamará executeTpaExtraction() diretamente).
#define VPIN_EXTRACTION_VOLUME_L   V29 // Saída: Mostra o volume calculado a ser extraído (e reposto) em Litros.
#define VPIN_SCHEDULE_MINUTE       V30 // Sincronização: Define o minuto para o Agendamento Local. Altera tpaScheduleMinute.
#define VPIN_REPOSITION_VOLUME_L   V31 // Informa o volume de reposição seja o calculado ou informado pelo usuário
#define VPIN_RAN_LEVEL_PERCENT     V32 // (SUGESTÃO) Percentual de Nível do RAN
#define VPIN_RAN_REFILL_ALERT      V33 // (SUGESTÃO) Alerta de Falha de Enchimento do RAN
#define VPIN_RAN_BUFFER_VOLUME     V34 // Volume de Buffer a ser dosado (0-999ml)
#define VPIN_TPA_MASTER_STATE      V35 // Status geral do TPA (para Gauge ou Display)

// --- Display OLED SSD1306 (Módulo 4: I2C) ---
#define SCREEN_WIDTH 128     // Largura do display OLED (em pixels)
#define SCREEN_HEIGHT 64     // Altura do display OLED (em pixels)
#define OLED_RESET -1        // Pino de Reset (necessário para o ESP32, use -1 para que a biblioteca gerencie)
#define SCREEN_ADDRESS 0x3C  // Endereço I2C comum para o SSD1306 (pode ser 0x3D)

// --- ATUADORES TPA (Módulo 5.2: Bombas de extração/reposição) ---
#define TPA_EXTRACTION_PUMP_PIN 25 // Pino GPIO para Bomba Peristaltica de Extracao
#define TPA_REPOSITION_PUMP_PIN 23 // Boma peristáltica paa reposição de TPA
#define TPA_BUFFER_PUMP_PIN 28 // Boma peristáltica paa reposição de TPA
#define EXTRACTION_PUMP_FLOW_RATE_ML_PER_SEC 10.0 // Vazao da Bomba (10 ml/s = 600 ml/min)

// --- ATUADORES RAN -Reservatório de água Nova) (Módulo 5.3: Válvula solenóide/sensor de nível) ---
#define RAN_SOLENOID_VALVE_PIN 24  // Pino GPIO da válvula solenóide
#define RAN_LEVEL_SENSOR_PIN   26  // Pino GPIO do sensor de nível

// Baseado na vazão (10.0 ml/s) e em um volume de segurança (Ex: 15L + 33% de margem).
// 2,000,000 ms (2000 segundos ou ~33.3 minutos)
#define RAN_REFILL_TIMEOUT_MS (2000UL * 1000UL)


// --- PAGINAÇÃO OLED (MÓDULO 5.4: ) ---
#define OLED_PAGE_BUTTON_PIN    27 // Botao Fisico para mudar a pagina/menu do OLED

// --- Pinos GPIO Físicos (botões físicos) ---
#define PH_CAL_BUTTON_PIN       32  // Calibração de pH (já existente)
#define ALERT_RESET_BUTTON_PIN  33  // Reset de Alerta CRITICO (Temp, pH)
#define SERVICE_MODE_BUTTON_PIN 34  // Ativa/Desativa Modo de Serviço/Manutenção
#define RTC_RESET_BUTTON_PIN    35  // Reset de Alerta de Bateria/OSF do RTC
#define UP_BUTTON_PIN           36  // Botão para incremento
#define DOWN_BUTTON_PIN         37  // Botão para decrmento

// --- Tempo de Acionamento ---
#define LONG_PRESS_MS 2000   // 2.0 segundos para acionamento de "Long Press"

// --- Pinos de Hardware ---
#define ONE_WIRE_BUS 4      // Pino para o Sensor DS18B20
#define PH_PIN       A0     // Pino analógico onde o módulo de pH está conectado

// --- CONFIGURAÇÕES OBRIGATÓRIAS DO BLYNK IOT ---
#define BLYNK_TEMPLATE_ID "TMPL2LaMxGLh9"
#define BLYNK_TEMPLATE_NAME "Aquarium Command and Control"

// --- Constantes usadas ---
// Constantes - Módulo 1 (temperatura, tempo)
const float UPPER_TEMP = 28.0;           // Limite superior de temperatura
const char* NTPSERVER = "pool.ntp.org";  // Servidores NTP públicos
const long GMTOFFSET_SEC = -3 * 3600;    // fuso horário de Brasília (UTC-3). -3 horas * 3600 seg/hora
const int   DAYLIGHTOFFSET_SEC = 0;      // Sem horário de verão

// Constantes - Módulo 2 (pH)
// --- Limites Operacionais ---
const float PH_MIN_LIMIT = 6.5;   // Limite mínimo de pH para alerta (Ácido)
const float PH_MAX_LIMIT = 7.8;   // Limite máximo de pH para alerta (Alcalino)
#define DEFAULT_PH_OFFSET 0.0     // Offset padrão ao iniciar


// Constantes - hardware_manager
const float VOLUME_STEP = 0.1; // Ajuste de 100mL

//Constantes - Módulo 3 (persistência de dados)
#define CONFIG_FILE_PATH "/config.json"


// Constantes - Módulo 5 (TPA Reposition)
const unsigned long SAFETY_PAUSE_MS = 5000; // Constantes de Tempo - 5 segundos para 1.1
#define RELAY_ON LOW
#define RELAY_OFF HIGH

// --- Constantes - Módulo 5.4 (Buffer) ---
#define TPA_BUFFER_PAGE_INDEX 3    // Índice da página de configuração de Buffer no OLED (Page 3)
#define BUFFER_VOLUME_MIN 0
#define BUFFER_VOLUME_MAX 999
