/*
 +---------------------------------------+
 |      Aquarium Command & Control       |
 |                  ACC                  |  
 |Procedure: CONFIG_MANAGER              |      
 +---------------------------------------+
 Project Purpose: Control an Aquarium parameters like Temperarture
                  pH, other water quality parameters from data received
                  of probes. It also will count with schedule to water
                  change useing actuators (like peristaltc pumps). All
                  data will be plotted on Web using an IoT cloud (like Blynk)
                  core will be an ESP32 board.


 Procedure Purpose: Used to save config/data on JSON on SPIFF(LittleFS) partition on ESP32

 Author: Alberto Tolentino (and Gemini AI)
 
== Functional specification ==



== Version History ==
01/11/2025 - 0.01 - First installment based on code created by Gemini AI (Google)

== Project file structure ==
main.ino          - Main program with setup parameters and loop functions
config.h          - Pin definitions, passwords, IDs (Auth, SSID, Pass)
global.h          - Global definitions
utils.h           - Functions prototypes
rtc_time.ino      - setupNTP(), getCurrentTime(), BLYNK_CONNECTED(), BLYNK_WRITE(RTC_RESET)
sensors.ino       - readTemperature(), readPH(), alert code
ph_sensor         - Deal with readings from pH probe
config_manager    - (this file) Used to save config/data on JSON on SPIFF(LittleFS) partition on ESP32
display_manager   - Deal with all output on OLED Display
hardware_manager  - Deal with all physical buttons in the project
actuators_manager - Deal with all pump e actuator hardware in the system
tpa_manager       - Coordinate the partial water change (in portuguese TPA ou troca parcial de água)
tpa_reposition    - Control return of water volume

*/

// config_manager.ino

#include "config.h"
#include "global.h"
#include "utils.h"

// Protótipos de Funções que precisamos chamar (em tpa_manager.ino)
void calculateTpaVolume(); 

// --- SETUP: INICIALIZA LITTLEFS E CARREGA CONFIG ---
// Esta função DEVE ser chamada no main.ino setup()
void setupConfigManager() {
    if (!LittleFS.begin(true)) {
        Serial.println(F("ERRO: Falha ao montar o LittleFS. Configuracao nao persistira."));
        logSystemEvent("critical", "Falha na inicializacao do LittleFS.");
        return;
    }
    Serial.println(F("LittleFS montado com sucesso."));
    loadConfig();
}


void saveConfig() {
    // Documento JSON estático com tamanho suficiente para todas as configurações
    StaticJsonDocument<512> doc;

    // --- MÓDULO 2: Configuração do pH ---
    doc["phOffset"] = phCalibrationOffset;

    // --- MÓDULO 5: TPA Configuration (CÓDIGO DE PERSISTÊNCIA TPA: ESCRITA) ---
    doc["tpaVolumeL"] = aquariumTotalVolume;     // Salva o volume total
    doc["tpaPercent"] = tpaExtractionPercent;    // Salva o percentual de TPA
    doc["tpaReposL"] = volumeToRepositionLiters; // Salva o volume ajustado
    doc["bufferVolumeML"] = ranBufferVolumeML;   // Salva o volume de buffer a aplicar
    
    // --- NOVO: Agendamento Local TPA ---
    doc["tpaLocalSched"] = tpaLocalScheduleActive; // Schedule local ativo
    doc["tpaSchedDay"] = tpaScheduleDay;           // Dia definido (1=Domingo, 2=Segunda...)
    doc["tpaSchedHour"] = tpaScheduleHour;         // Hora definida (00:00) 
    doc["tpaSchedMin"] = tpaScheduleMinute;        // Minuto definido (00:00)
    doc["tpaSchedFreq"] = tpaScheduleFrequency;    // Frequência definida (0=Diária, 1=Semanal, 2=Quinzenal, 3=Mensal)


    File configFile = LittleFS.open(CONFIG_FILE_PATH, "w");
    if (!configFile) {
        Serial.println(F("ERRO: Falha ao abrir arquivo de configuracao para escrita."));
        logSystemEvent("error", "Falha ao salvar configuracao no LittleFS.");
        return;
    }

    if (serializeJson(doc, configFile) == 0) {
        Serial.println(F("ERRO: Falha ao escrever no arquivo de configuracao."));
        logSystemEvent("error", "Falha na serializacao JSON ao salvar.");
    }

    configFile.close();
    configIsDirty = false;
    Serial.println(F("Configuracoes salvas com sucesso no LittleFS."));
}

void loadConfig() {
    if (!LittleFS.exists(CONFIG_FILE_PATH)) {
        Serial.println(F("ARQUIVO: Arquivo de configuracao nao encontrado. Usando padroes."));
        // Se o arquivo não existe, definimos os defaults e calculamos TPA.
        calculateTpaVolume();
        return;
    }

    File configFile = LittleFS.open(CONFIG_FILE_PATH, "r");
    if (!configFile) {
        Serial.println(F("ERRO: Falha ao abrir arquivo de configuracao para leitura."));
        logSystemEvent("error", "Falha ao carregar configuracao do LittleFS.");
        return;
    }

    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, configFile);
    configFile.close();

    if (error) {
        Serial.print(F("ERRO: Falha na leitura JSON: "));
        Serial.println(error.f_str());
        logSystemEvent("error", "Falha ao desserializar JSON de configuracao.");
        return;
    }
    
    // --- MÓDULO 2: Configuração do pH ---
    phCalibrationOffset = doc["phOffset"] | DEFAULT_PH_OFFSET;
    
    // --- MÓDULO 5: TPA Configuration (CÓDIGO DE PERSISTÊNCIA TPA: LEITURA) ---
    aquariumTotalVolume = doc["tpaVolumeL"] | 96.0f; // Valor default de 96L (exemplo)
    tpaExtractionPercent = doc["tpaPercent"] | 5.0f; // Valor default de 5% (exemplo)
    volumeToRepositionLiters = doc["tpaReposL"] | volumeToExtractLiters;
    ranBufferVolumeML = doc["bufferVolumeML"] | 100; // Valor default de 100mL (exemplo)

    // --- NOVO: Agendamento Local TPA ---
    tpaLocalScheduleActive = doc["tpaLocalSched"] | false;
    tpaScheduleDay = doc["tpaSchedDay"] | 1;        // Default Domingo
    tpaScheduleHour = doc["tpaSchedHour"] | 0;      // Default 00:00
    tpaScheduleMinute = doc["tpaSchedMin"] | 0;     // Default 00:00
    tpaScheduleFrequency = doc["tpaSchedFreq"] | 0; // Default Diaria


    // Recalcula o volume e a duracao da bomba com os valores carregados
    calculateTpaVolume(); 
    
    Serial.println(F("Configuracoes carregadas com sucesso."));
}

void checkConfigSave() {
    if (configIsDirty && Blynk.connected()) {
        saveConfig();
    }
}