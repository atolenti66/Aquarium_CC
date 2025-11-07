/*
 +---------------------------------------+
 |      Aquarium Command & Control       |
 |                  ACC                  |  
 |Procedure: UTILS.H                     |      
 +---------------------------------------+
 Project Purpose: Control an Aquarium parameters like Temperarture
                  pH, other water quality parameters from data received
                  of probes. It also will count with schedule to water
                  change useing actuators (like peristaltc pumps). All
                  data will be plotted on Web using an IoT cloud (like Blynk)
                  core will be an ESP32 board.


 Procedure Purpose: Functions prototypes

 Author: Alberto Tolentino (and Gemini AI)
 
== Functional specification ==



== Version History ==
02/11/2025 - 0.02 - Added new functions prototypes fro actuators and display management
01/11/2025 - 0.02 - Added Display management functions prototypes
01/11/2025 - 0.01 - First installment based on code created by Gemini AI (Google)

== Project file structure ==
main.ino          - Main program with setup parameters and loop functions
config.h          - Pin definitions, passwords, IDs (Auth, SSID, Pass)
global.h          - Global definitions
utils.h           - (this file) Functions prototypes
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

#pragma once // Garante que este arquivo seja incluído apenas uma vez por unidade de compilação

// -----------------------------------------------------------------
// --- main - Protótipos de Funções 
// -----------------------------------------------------------------
void sendSensorData();            // Rotina do Ticker
void logSystemEvent(const char* category, const char* message); // Função de log para eventos do sistema
// -----------------------------------------------------------------


// -----------------------------------------------------------------
// --- rtc_time - Protótipos de Funções 
// -----------------------------------------------------------------
// --- Protótipos de Funções RTC/Tempo (Definidas em rtc_time.ino) ---
DateTime getDateTimeNow();           // Obtém a hora atual como objeto DateTime
String getCurrentTimeString();       // Obtém a hora atual como String formatada
time_t getRtcTime();                 // Obtém o tempo do RTC como time_t
void setupRTC();                     // Configura o RTC DS3231
void checkRtcStatus();               // Verifica o status do RTC (OSF)
void syncRtcFromNtp(time_t ntpTime); // Sincroniza o RTC com a hora NTP
// -----------------------------------------------------------------


// -----------------------------------------------------------------
// --- sensors - Protótipos de Funções 
// -----------------------------------------------------------------
float readTemperature();     // Leitura do sensor de temperatura DS18B20
void checkTempAlert(float tempC); // Verifica condições de alerta de temperatura
void resetCriticalAlerts();  // Reseta alertas críticos (pH, Temp)
void resetRtcOsfAlert();     // Reseta alerta OSF do RTC
void resetSensorData();      // Reseta dados dos sensores (pH, Temp)
// -----------------------------------------------------------------


// -----------------------------------------------------------------
// --- ph_sensor - Protótipos de Funções 
// -----------------------------------------------------------------
// --- Protótipos de Funções pH (Definidas em ph_sensor.ino) ---
float readPH();                     // Lê e converte o valor do pH
void executePhCalibration();  // Faz a calibração do sensor de pH
void checkPhAlert(float currentPh); // Verifica condições de alerta de pH
// -----------------------------------------------------------------


// -----------------------------------------------------------------
// --- config_manager - Protótipos de Funções 
// -----------------------------------------------------------------
void setupConfigManager();  // --- SETUP DO GERENCIADOR DE CONFIGURAÇÃO ---
void saveConfig();         // --- SALVA CONFIGURAÇÕES NO LITTLEFS ---
void loadConfig();          // --- CARREGA CONFIGURAÇÕES DO LITTLEFS ---
void checkConfigSave();    // --- VERIFICA SE AS CONFIGURAÇÕES PRECISAM SER SALVAS ---
// -----------------------------------------------------------------


// -----------------------------------------------------------------
// --- display_manager - Protótipos de Funções 
// -----------------------------------------------------------------
void setupDisplay();            // --- SETUP DO DISPLAY ---
void updateDisplay();           // --- FUNÇÃO CENTRAL DE ATUALIZAÇÃO ---
void renderPage0Dashboard();    // --- Para renderizar a Dashboard principal
void renderPage1TpaSchedule();  // --- Para programação de schedule como falback de TPA
void renderPage2TpaReposition(); // --- Para renderizar a página de reposição de TPA
void renderPage3TpaBuffer();     // --- Para renderizar a página de injeção de buffer
// -----------------------------------------------------------------


// -----------------------------------------------------------------
// --- hardware_manager - Protótipos de Funções 
// -----------------------------------------------------------------
void handleOledPageTap(Button2& btn);          // Alterna páginas do OLED ou entra em modo de edição
void handleOledPageLongPress(Button2& btn);    // Retorna à página 0 (Dashboard)
void handleUpTap(Button2& btn);                // Incrementa valores no modo de edição
void handleDownTap(Button2& btn);              // Decrementa valores no modo de edição
void handleAlertResetTap(Button2& btn);        // Reseta alertas críticos (curto)
void handleAlertResetLongPress(Button2& btn);  // Reseta valores min/max dos sensores (longo)
void handleRtcResetTap(Button2& btn);          // Reseta alerta OSF do RTC (curto)
void handlePhCalLongPress(Button2& btn);       // Inicia calibração de pH (longo)
void handleServiceModeLongPress(Button2& btn); // Alterna modo de serviço (longo)
void runHardwareManagerLoop();                 // Loop principal para gerenciar botões físicos
void setupHardwareButtons();                   // Define os botões
// -----------------------------------------------------------------


// -----------------------------------------------------------------
// --- actuators_manager - Protótipos de Funções 
// -----------------------------------------------------------------
void setupActuators();                   // --- SETUP DOS ATUADORES ---
unsigned long calculatePumpDuration(float volumeLiters); // Calcula duração da bomba para um volume específico
void setExtractionPumpState(bool state); // --- CONTROLE DA BOMBA DE EXTRACAO ---
void executeTpaExtraction();             // --- ATUADOR: EXECUTA O CICLO DE EXTRACAO TPA ---
bool readRanLevelSensor();               // Função auxiliar para ler o sensor
void setRANSolenoidState(bool state);    // Função auxiliar para controlar a válvula
void startRanRefillFlow();               // Inicia o processo de enchimento do RAN
void runRanRefillLoop();                 // Máquina de estados principal do enchimento
bool isRanRefillFinished();              // Verifica se o enchimento está concluído
void resetRanRefillFlow();               // Reseta o estado do enchimento
void resetRanRefillFlow();               // Reseta o estado do enchimento
void checkRanRefillAlert();              // Verifica se houve falha no enchimento
void updateRanLevelDisplay();              // Atualiza o display e Blynk com o nível do RAN
void setBufferPumpState(bool state);     // Controle da bomba de buffer (M5.4)
// -----------------------------------------------------------------


// -----------------------------------------------------------------
// --- tpa_manager - Protótipos de Funções 
// -----------------------------------------------------------------
void calculateTpaVolume();     // Lógica de cálculo de volume TPA
void runTpaManagerLoop();      // Coordenação do Loop TPA Master
void checkLocalSchedule();     // Verifica agendamento local de TPA
void startTpaBufferDosing();   // Inicia o processo de dosagem de buffer
void runTpaBufferDosingLoop(); // Executa a dosagem de buffer
bool isTpaBufferDosingFinished(); // Verifica se a dosagem de buffer está concluída
void resetTpaBufferDosingFlow();  // Reseta o estado da dosagem de buffer
void updateBufferBlynk();        // Atualiza o Blynk com o volume de buffer dosado
void setupTpaManager();        // Inicializa variáveis e estados do TPA Manager
void uupdateRepositionBlynk(); // Atualiza o Blynk com o volume de reposição
// -----------------------------------------------------------------


// -----------------------------------------------------------------
// --- tpa_reposition - Protótipos de Funções 
// -----------------------------------------------------------------
void startTpaRepositionFlow();   // Inicia o processo de reposição
void runTpaRepositionLoop();     // Executa a reposição até os limites estabelecidos.
bool isTpaRepositionFinished();  // Averigua se a reposição está encerrada
void resetTpaRepositionFlow();   // Reset do estado do fluxo de reposição
extern unsigned long calculatePumpDuration(float volumeLiters); // Calcula duração da bomba para um volume específico
// -----------------------------------------------------------------


// --- Protótipos de Funções para Módulo 5.4 (Dosagem de Buffer) ---
void startTpaBufferDosing();             // Inicia o processo de dosagem de buffer
void runTpaBufferDosingLoop();           // Máquina de estados principal da dosagem
bool isTpaBufferDosingFinished();        // Verifica se a dosagem está concluída
void resetTpaBufferDosingFlow();         // Reseta o estado da dosagem
void setBufferPumpState(bool state);     // Função auxiliar para controlar a bomba de buffer



// --- Protótipos das Funções (Para o compilador) ---







