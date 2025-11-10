/*
 +---------------------------------------+
 |      Aquarium Command & Control       |
 |                  ACC                  |  
 |Procedure: MAIN                        |      
 +---------------------------------------+
 Project Purpose: Control an Aquarium parameters like Temperarture
                  pH, other water quality parameters from data received
                  of probes. It also will count with schedule to water
                  change useing actuators (like peristaltc pumps). All
                  data will be plotted on Web using an IoT cloud (like Blynk)
                  core will be an ESP32 board.


 Procedure Purpose: Main program, with setup() and loop() functions

 Author: Alberto Tolentino (and Gemini AI)
 
== Functional specification ==



== Version History ==
04/11/2025 - 0.0.6 - Moved project to VSCode+PlatformIO to allow test without hardware with Unity.
                     Addedd <Arduino.h> on top of file to maximize compilation success. Using now
                     a new versioning number schema (Major.Minor.Patch)
01/11/2025 - 0.0.5 - add Blynk log function to normalize events. Added Utils.h to function prototype.
                    House cleaning, moving prototypes, includes to .H correct file. Added on setup()
                    code to retrieve pH calibration data
31/10/2025 - 0.0.4 - split project in multiples files to allow easy maintenance
31/10/2025 - 0.0.3 - Added a fallback to NTP in case RTC fail and restrict # of events sent to Blynk
31/10/2025 - 0.0.2 - Replaced Blynktimer to Ticker (ESP native) to try to get an agnostic code about IoT cloud
30/10/2025 - 0.0.1 - First installment based on code created by Gemini AI (Google)

== Project file structure ==
main.ino          - (this file) Main program with setup parameters and loop functions
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

*/

// Inclui a configuração (Auth, Pinos) e variáveis Globais
#include <Arduino.h>  // Para manter compatibilidade com PlatformIO
#include "config.h"
#include "global.h"
#include "utils.h"

// --- Instanciação Global (Definições) ---
// É aqui que as variáveis são realmente criadas e alocadas na memória.
Ticker sensorDataTicker; 
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);


// Variáveis de estado
//Variáveis de estado para controle de arquivamento de configurações
bool configIsDirty = false; //Usada em config_manager, tpa_manager, hardware_manager, 

// Variáveis de estado para RTC
bool rtc_ok = false;
bool rtc_osf_flag = false; // Oscillator Stop Flag
bool rtcOsfAlertSent = false; //Alerta de RTC 

// Variáveis de estado - Módulo 1 (tempo e temperatura)
bool highTempAlertSent = false; // Usada para defnir o volume de alertas no Blynk
float temperatureC = 0.0f;      // Usada em display.manager.ino

// Variáveis de estado - Módulo 2 (pH)
float phValue = 7.0;             // Inicializado com um valor neutro
float phCalibrationOffset = 0.0; // Começa sem offset (deve ser salvo em EEPROM/LittleFS em um projeto final)
bool phCalibrationMode = false;  // O sistema não inicia em modo de calibração
bool phAlertSent = false;        // Começa sem alerta

// Variáveis de estado - Botões físicos
bool serviceModeActive = false; // Implementacao da variavel


// Variáveis de estado - OLED Display
int currentPage = 0;   // Rastreia a pagina atual exibida no OLED (usada pelo hardware_manager para trocar)
int page1EditMode = 0; // Rastreia qual item da Página 1 [TPA Schedule] esta sendo editado (0=Dia, 1=Hora, 2=Minuto, 3=Freq, 4=Salvar)
int page2EditMode = 0; // Rastreia o modo de edicao da Pagina 2 [TPA: RAN Reporition] (0=Visualizar, 1=Editar Volume)
int page3EditMode = 0; // Rastreia o modo de edicao da Pagina 3 [TPA: Buffer to RAN] (0=Visualizar, 1=Editar Volume)

// Variáveis de estado - RAN Buffer
int ranBufferVolumeML = 0; //Volue de Buffer a ser adicionado em ml (0-999)
BufferDosingState tpaBufferCurrentState = TPA_BUFFER_IDLE;
unsigned long bufferPreviousMillis = 0;
unsigned long bufferDosingDurationMs = 0;
float bufferVolumeLiters = 0;

// --- Definições Globais TPA (Módulo 5.1 tpa_manager) ---
float aquariumTotalVolume = 96.0f; // Volume Padrao de 96 Litros
float tpaExtractionPercent = 5.0f;  // Percentual Padrao de 5%
bool tpaExtractionPumpState = false; 
float volumeToExtractLiters = 0.0f;
unsigned long tpaPumpDurationMs = 0;
float volumeToRepositionLiters = 0.0f;
unsigned long tpaExtractionStartTime = 0; // --- CAPTURA DO TEMPO INICIAL ---

// --- Definições Globais TPA (Módulo 5.2 - tpa_reposition) ---
RepositionState tpaRepositionCurrentState = TPA_REPOSITION_IDLE; 
unsigned long repositionPreviousMillis = 0;
unsigned long repositionIntervalMs = 0; // Tempo de espera/execução atual (em ms)

// --- ESTADO DO AGENDAMENTO LOCAL TPA ---
bool tpaLocalScheduleActive = false; // Ativa o agendamento local (fallback)
int tpaScheduleDay = 1;              // Dia da semana (1=Dom, 7=Sab ou dia do mes para mensal)
int tpaScheduleHour = 0;             // Hora da execucao (0-23)
int tpaScheduleMinute = 0;           // Minuto da execucao (0-59)
int tpaScheduleFrequency = 0;        // 0=Diaria, 1=Semanal, 2=Quinzenal, 3=Mensal
unsigned long lastTpaExecution = 0;  // millis() ou RTC time da ultima execucao local
TpaMasterState tpaMasterCurrentState = TPA_MASTER_IDLE;

// --- Variáveis de Gerenciamento do RAN ---
RanRefillState ranRefillCurrentState = RAN_REFILL_IDLE;
unsigned long ranRefillStartTime =0;
bool ranLevelFull = false;
int ranLevelPercent = 0;
bool ranRefillAlertSent = false;

// --- ================================== ---
// ---               SETUP                ---
// --- ================================== ---
void setup() {
  // 1. Iniciliza a serial
  Serial.begin(115200);
  
  sensors.begin();
  
  // 2. Tentar inicializar o RTC
  if (!rtc.begin()) {
    rtc_ok = false;
  } else {
    rtc_ok = true;
    if (rtc.lostPower()) {
      rtc_osf_flag = true; 
      Serial.println(F("RTC PERDEU ENERGIA/HORA. Ajuste via NTP será feito na conexão Blynk."));
    } else {
      rtc_osf_flag = false;
    }
  }

  // 3. Inicialização da Conexão (Não Bloqueante)
  Blynk.config(auth, "blynk.cloud", 80); // Configure as credenciais.
  Blynk.connect(0); // Tenta iniciar a conexão (timeout de 0 segundos)

  // 4. Fallback/Alerta de Tempo
  if (!rtc_ok || rtc_osf_flag) {
    Serial.println(F("RTC problemático. Hora será sincronizada via Blynk/NTP."));
  }
  
  // 5. Inicializar o LittleFS e carregar as configuracoes salvas (ph, TPA, etc)
  setupConfigManager();

  // 6. Inicializar Pinos Físicos (Chamando o novo gerenciador de botões)
  setupHardwareButtons(); // Inicializa todos os pinos de botõs (32, 33, 34, 35)

  // 7. Inicializar Display OLED (NOVO)
  setupDisplay();

  // 8. Inicializa o Ticker
  sensorDataTicker.attach(5.0, sendSensorData); 
  Serial.println(F("--- Sistema Base Inicializado! ---"));
}

// --- ================================== ---
// ---                LOOP                ---
// --- ================================== ---
void loop() {
// 1. Cham as rotinas de medição
  Blynk.run(); 

// 2. Averigua os Botões Físicos
  runHardwareManagerLoop(); 

// 3. Gerenciamento de FSMs de Controle (NOVO e prioritário)
  // Essas funcoes devem rodar a cada loop para garantir tempo real e nao-bloqueio.
  runTpaManagerLoop();      // Coordena o fluxo geral (M5.1 -> M5.2 -> M5.3)
  runTpaRepositionLoop();   // Execução NÃO-BLOQUEANTE da FSM M5.2

// 4. Gerenciamento de FSMs de Controle (NOVO e prioritário)
  runRanRefillLoop();        // Executa o enchimento do RAN (M5.3)

// 5. Salvar Configuração (Lógica Lenta)
  checkConfigSave();

}

// ----------------------------------------------------------------------
// Funções Auxiliares ---------------------------------------------------
// ----------------------------------------------------------------------

// -------------------------------------------------------------------
// FUNÇÃO CENTRAL: Chamada pelo Ticker
// -------------------------------------------------------------------
void sendSensorData() {
  // 1. LÓGICA RTC/BATERIA
  if (rtc_ok && rtc_osf_flag && !rtcOsfAlertSent) {
      if (Blynk.connected()) {
        logSystemEvent("warning", "Bateria do RTC falhou! Usando tempo da Internet.");        
      }
      rtcOsfAlertSent = true;
  }

  // 2. LEITURA DE TEMPERATURA
  float tempC = readTemperature();
  if (tempC != -999.0) {
    Blynk.virtualWrite(VPIN_TEMP, tempC);
    checkTempAlert(tempC); // Verifica e envia alerta se necessário
  }

  // 3. LEITURA DE PH
    if (!phCalibrationMode) { // Não leia/envie dados se estiver no meio da calibração
        float currentPh = readPH();
        Blynk.virtualWrite(VPIN_PH_VAL, currentPh);
        checkPhAlert(float currentPh);
    }

  // 4. LEITURA DE TEMPO
  Blynk.virtualWrite(VPIN_TIME, getCurrentTimeString());

  // 5. ATUALIZAR DISPLAY OLED
  updateDisplay();

  // 6. Atualiza arquivo de persistência
  checkConfigSave();

}

// -------------------------------------------------------------------
// FUNÇÃO LOG EVENTOS: Para registro no Blink
// -------------------------------------------------------------------
/**
 * Envia um evento Blynk usando as categorias genéricas (log_info, log_critical, etc.)
 * para economizar o limite de 5 eventos únicos da conta gratuita.
 * @param category Deve ser "info", "warning", "critical", ou "content".
 * @param message A descrição detalhada do evento.
 */
void logSystemEvent(const char* category, const char* message) {
    if (Blynk.connected()) {
        String eventCode = "log_";
        eventCode += category;
        Blynk.logEvent(eventCode.c_str(), message);
    }
}

// -------------------------------------------------------------------
// HANDLER BLYNK: Switch de Modo de Serviço
// -------------------------------------------------------------------
BLYNK_WRITE(VPIN_SERVICE_MODE) {
    int switchState = param.asInt();
    
    // Atualiza o estado da variável global
    serviceModeActive = (switchState == 1); 
    
    String logMsg = serviceModeActive ? F("Modo Servico ATIVADO via Blynk.") : F("Modo Servico DESATIVADO via Blynk.");
    Serial.println(logMsg);
    logSystemEvent("warning", logMsg.c_str());
    
    // O display será atualizado no proximo sendSensorData
}