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

// --- Protótipo da Função de Log (Definida em main.ino) ---
void sendSensorData();            // Rotina do Ticker
void logSystemEvent(const char* category, const char* message);

// --- Protótipos de Funções RTC/Tempo (Definidas em rtc_time.ino) ---
DateTime getDateTimeNow();
String getCurrentTimeString();
void setupRTC();
void checkRtcStatus();
void syncRtcFromNtp(time_t ntpTime);

// --- Protótipos de Funções Sensores (Definidas em sensors.ino) ---
float readTemperature();
void checkTempAlert(float tempC);

// --- Protótipos de Funções pH (Definidas em ph_sensor.ino) ---
float readPH();
void checkPhAlert(float currentPh);
void executePhCalibration();  // Faz a calibração do sensor de pH
void resetSensorData();  //Reset de Temperatura e pH para zero

// --- Protótipos de Funções de Configuração/Persistência (Definidas em config_manager.ino) ---
void setupConfigManager();
void loadConfig();
void saveConfig();
void checkConfigSave();

// --- Protótipos de Funções Display OLED (Definidas em display_manager.ino) ---
void setupDisplay();            // --- SETUP DO DISPLAY ---
void drawDashboard();           // --- DESENHO: DASHBOARD (Página 0) ---
void drawTpaExtractionPage();   // --- DESENHO: TPA EXTRAÇÃO (Página 1) ---
void drawTpaRepositionPage();   // --- DESENHO: TPA REPOSIÇÃO (Página 2) ---
void drawBufferInjectionPage(); // --- DESENHO: INJEÇÃO BUFFER (Página 3) ---
void drawCustomDashboardPage(); // --- DESENHO: CUSTOM DASH (Página 4) ---
void updateDisplay();           // --- FUNÇÃO CENTRAL DE ATUALIZAÇÃO ---
void renderPage1TpaSchedule();  // --- Para programação de schedule como falback de TPA
void updateRanLevelDisplay(); // Atualiza o percentual de nível no Blynk e Display

// --- Protótipos de Funções para ação com botões físicos (Definidas em hardware_manager.ino) ---
   
void resetCriticalAlerts();     // Botão para resetar alertas criticos (PH, TEMP)
void resetRtcOsfAlert();        // Botão para resetar alertas criticos (PH, TEMP)
void executePhCalibration();    // Botão/Menu para acionar a calibração
void executeTpaExtraction();    //Botão para execução de TPA
void calculateTpaVolume();      //Para cálculo de volume de TPA

// --- FUNÇÕES DE NAVEGAÇÃO E EDIÇÃO (CALLBACKS DO Button2) ---
void handleOledPageTap(Button2& btn);
void handleOledPageLongPress(Button2& btn);
void handleUpTap(Button2& btn);
void handleDownTap(Button2& btn);
void handleAlertResetTap(Button2& btn);
void handleAlertResetLongPress(Button2& btn);
void handleRtcResetTap(Button2& btn);
void handlePhCalLongPress(Button2& btn);
void handleServiceModeLongPress(Button2& btn);
void runHardwareManagerLoop();
void setupHardwareButtons(); // Define os botões


// --- Protótipos de Funções para ação com atuadores (Definidas em actuators_manager.ino) ---
void setupActuators();                   // --- SETUP DOS ATUADORES ---
void setExtractionPumpState(bool state); // --- CONTROLE DA BOMBA DE EXTRACAO ---
unsigned long calculatePumpDuration(float volumeLiters); // --- CÁLCULO DA DURAÇÃO DA BOMBA ---
void executeTpaExtraction();             // --- ATUADOR: EXECUTA O CICLO DE EXTRACAO TPA ---

// --- Protótipos de Funções para ação com atuadores (Definidas em tpa_manager.ino) ---
void calculateTpaVolume();                               // --- LÓGICA DE CÁLCULO DE VOLUME ---
unsigned long calculatePumpDuration(float volumeLiters); // Calcula a duração de acionamento da bomba
void executeTpaExtraction();                             // Executa a extração no TPA
void saveTpaConfig(StaticJsonDocument<1024>& doc);       // Salva as configurações no SPIFFS/LittleFS
void checkLocalSchedule();                               // Para verificar o schedule local

// --- Protótipos de Funções para ação com atuadores (Definidas em tpa_reposition.ino) ---
void startTpaRepositionFlow();   // Inicia o processo de reposição
void runTpaRepositionLoop();     // Executa a reposição até os limites estabelecidos.
bool isTpaRepositionFinished();  // Averigua se a reposição está encerrada
void resetTpaRepositionFlow();   // Reset do estado do fluxo de reposição
extern unsigned long calculatePumpDuration(float volumeLiters);

// --- Protótipos de Funções para Módulo 5.3 (Enchimento do RAN) ---
void setupRanRefill();                   // Inicializa pinos e estado do RAN (chamado em setupActuators)
void startRanRefillFlow();               // Inicia o processo de enchimento do RAN
void runRanRefillLoop();                 // Máquina de estados principal do enchimento
bool isRanRefillFinished();              // Verifica se o enchimento está concluído
void resetRanRefillFlow();               // Reseta o estado do enchimento
bool readRanLevelSensor();               // Função auxiliar para ler o sensor
void setRANSolenoidState(bool state);    // Função auxiliar para controlar a válvula
void checkRanRefillAlert();              // Verifica se houve falha no enchimento

// --- Protótipos das Funções (Para o compilador) ---





