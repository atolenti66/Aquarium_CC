/*
 +---------------------------------------+
 |      Aquarium Command & Control       |
 |                  ACC                  |  
 |Procedure: ACTUATORS_MANAGER           |      
 +---------------------------------------+
 Project Purpose: Control an Aquarium parameters like Temperarture
                  pH, other water quality parameters from data received
                  of probes. It also will count with schedule to water
                  change useing actuators (like peristaltc pumps). All
                  data will be plotted on Web using an IoT cloud (like Blynk)
                  core will be an ESP32 board.


 Procedure Purpose: Deal with all pump e actuator hardware in the system

 Author: Alberto Tolentino (and Gemini AI)
 
== Functional specification ==



== Version History ==
04/11/2025 - 0.0.2 - Moved project to VSCode+PlatformIO to allow test without hardware with Unity.
                     Addedd <Arduino.h> on top of file to maximize compilation success. Using now
                     a new versioning number schema (Major.Minor.Patch)
01/11/2025 - 0.0.1 - First installment based on code created by Gemini AI (Google)

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
actuators_manager - (this file) Deal with all pump e actuator hardware in the system
tpa_manager       - Coordinate the partial water change (in portuguese TPA ou troca parcial de água)
tpa_reposition    - Control return of water volume

*/
#include <Arduino.h>  // Para manter compatibilidade com PlatformIO
#include "config.h"
#include "global.h"
#include "utils.h"

// --- SETUP DOS ATUADORES ---
void setupActuators() {
    // Configura o pino da bomba de extracao como OUTPUT
    pinMode(TPA_EXTRACTION_PUMP_PIN, OUTPUT);
    digitalWrite(TPA_EXTRACTION_PUMP_PIN, LOW); // Garante que a bomba comeca desligada
    
    Serial.print(F("Pino da bomba de Extracao configurado: "));
    Serial.println(TPA_EXTRACTION_PUMP_PIN);

    // --- SETUP MÓDULO 5.3: ENCHIMENTO DO RAN ---
    pinMode(RAN_SOLENOID_VALVE_PIN, OUTPUT);
    digitalWrite(RAN_SOLENOID_VALVE_PIN, RELAY_OFF); // Solenoide NC: OFF = Fechado
    
    // O sensor de nível do RAN é uma entrada simples
    pinMode(RAN_LEVEL_SENSOR_PIN, INPUT_PULLUP); // Assumindo sensor de boia com pull-up

    // Configuração da Bomba de Buffer (M5.4) - NOVO
    pinMode(TPA_BUFFER_PUMP_PIN, OUTPUT);
    digitalWrite(TPA_BUFFER_PUMP_PIN, RELAY_OFF); // Começa desligada
    
    // Configuração inicial
    ranLevelFull = readRanLevelSensor();
    ranLevelPercent = ranLevelFull ? 100 : 0; // Estado inicial do RAN

 
    Serial.println(F("Atuadores do RAN configurados. Solenoide FECHADA."));

}

// -------------------------------------------------------------
// --- Ações de Extração 
// -------------------------------------------------------------

// --- CONTROLE DA BOMBA DE EXTRACAO ---
void setExtractionPumpState(bool state) {
    if (tpaExtractionPumpState == state) return; // Nao faz nada se o estado for o mesmo

    tpaExtractionPumpState = state;
    // HIGH (ou LOW, dependendo da sua placa/rele) liga a bomba. Assumindo HIGH liga:
    digitalWrite(TPA_EXTRACTION_PUMP_PIN, state ? HIGH : LOW);

    // Sincronizar status com Blynk (usando LED Widget)
    if (Blynk.connected()) {
        Blynk.virtualWrite(VPIN_TPA_EXTRACTION_PUMP, state ? 255 : 0); 
    }

    // Log de evento
    String logMsg = state ? F("Bomba de Extracao ATIVADA.") : F("Bomba de Extracao DESLIGADA.");
    logSystemEvent(state ? "warning" : "info", logMsg.c_str());
    updateDisplay();
}

// --- CÁLCULO DA DURAÇÃO DA BOMBA ---
// Calcula o tempo em milissegundos necessario para extrair o volume desejado
unsigned long calculatePumpDuration(float volumeLiters) {
    // 1 L = 1000 mL
    float volumeML = volumeLiters * 1000.0f;
    
    // Calcula o tempo em segundos: Volume (mL) / Vazao (mL/s)
    float durationSeconds = volumeML / EXTRACTION_PUMP_FLOW_RATE_ML_PER_SEC;
    
    // Converte para milissegundos
    return (unsigned long)(durationSeconds * 1000.0f);
}


// --- ATUADOR: EXECUTA O CICLO DE EXTRACAO TPA ---
// Esta funcao sera chamada pelo agendador (TPA Manager)
void executeTpaExtraction() {
    if (tpaPumpDurationMs == 0) {
        Serial.println(F("ERRO: Duracao da bomba zero. Verifique configuracoes TPA."));
        logSystemEvent("error", "Tentativa de TPA com duracao zero.");
        return;
    }

    if (serviceModeActive) {
        Serial.println(F("TPA abortada: Modo de Servico ATIVO."));
        logSystemEvent("warning", "TPA abortada devido ao Modo de Servico.");
        return;
    }
    
    Serial.print(F("Iniciando Extracao TPA: "));
    Serial.print(volumeToExtractLiters, 2);
    Serial.print(F(" L por "));
    Serial.print(tpaPumpDurationMs / 1000);
    Serial.println(F(" segundos."));
    
    logSystemEvent("info", "TPA Extracao iniciada.");

    // 1. Ligar a bomba
    setExtractionPumpState(true); 
    tpaExtractionStartTime = millis(); // --- CAPTURA DO TEMPO INICIAL ---

    // 2. Esperar o tempo calculado (Aviso: Bloqueia o loop principal e Blynk.run()!)
    delay(tpaPumpDurationMs); 

    // 3. Desligar a bomba
    setExtractionPumpState(false);
    
    logSystemEvent("success", "TPA Extracao concluida.");
    // Inicia a reposicao (será implementada no Modulo 5.2)
}

// -------------------------------------------------------------
// --- Ações de Enchimento RAN 
// -------------------------------------------------------------

// --- FUNÇÕES AUXILIARES MÓDULO 5.3 (ENCHIMENTO DO RAN) ---

// Leitura do sensor de nível (assume LOW = Cheio, HIGH = Vazio/Não Cheio se for boia com pullup, ou vice-versa. Ajuste conforme seu sensor)
// Assumiremos: HIGH = RAN VAZIO/ABAIXO do Nível, LOW = RAN CHEIO
bool readRanLevelSensor() {
    // Retorna TRUE se o nível estiver OK (cheio ou no nível de segurança)
    // Se o sensor de nível for HIGH quando atingir o nível, mude a lógica para 'digitalRead(RAN_LEVEL_SENSOR_PIN) == HIGH'
    return (digitalRead(RAN_LEVEL_SENSOR_PIN) == LOW); 
}

// Controle da Válvula Solenoide (NC - Normalmente Fechada)
// ON (true) = ABERTO, OFF (false) = FECHADO
void setRANSolenoidState(bool state) {
    // RELAY_ON deve ser LOW para ativar um relé NC
    digitalWrite(RAN_SOLENOID_VALVE_PIN, state ? RELAY_ON : RELAY_OFF);
    Serial.print(F("Valvula Solenoide RAN: "));
    Serial.println(state ? F("ABERTA") : F("FECHADA"));
}


// --- INICIAR FLUXO DE ENCHIMENTO DO RAN (M5.3) ---
void startRanRefillFlow() {
    if (serviceModeActive) {
        Serial.println(F("Enchimento RAN abortado: Modo de Servico ATIVO."));
        logSystemEvent("warning", "Enchimento RAN abortado devido ao Modo de Servico.");
        ranRefillCurrentState = RAN_REFILL_FINISHED; // Pula para o fim
        return;
    }

    if (ranLevelFull) {
        Serial.println(F("RAN ja esta cheio. Pulando enchimento (M5.3)."));
        ranRefillCurrentState = RAN_REFILL_FINISHED;
        return;
    }
    
    Serial.println(F("Iniciando Enchimento do RAN (M5.3)..."));
    logSystemEvent("info", "Iniciando Enchimento do RAN (M5.3).");
    ranRefillStartTime = millis();
    ranRefillCurrentState = RAN_REFILL_START_DELAY; 
}


// --- MÁQUINA DE ESTADOS DO ENCHIMENTO DO RAN (M5.3) ---
void runRanRefillLoop() {
    if (ranRefillCurrentState == RAN_REFILL_IDLE || ranRefillCurrentState == RAN_REFILL_FINISHED) {
        // Nada a fazer
        return;
    }
    // 1. Monitorar o Sensor de Nível a cada loop
    ranLevelFull = readRanLevelSensor();
    ranLevelPercent = ranLevelFull ? 100 : 0;
    updateRanLevelDisplay(); // Atualiza Blynk e Display

    // 2. Controlar o fluxo
    switch (ranRefillCurrentState) {
        case RAN_REFILL_START_DELAY:
            // O RAN inicia o enchimento imediatamente
            setRANSolenoidState(true); // Abre a Solenoide
            ranRefillCurrentState = RAN_REFILL_FILLING;
            // Configurar um tempo máximo para o alerta de segurança
            // *** NECESSÁRIO IMPLEMENTAR UM TIMER DE SEGURANÇA AQUI ***
            break;

        case RAN_REFILL_FILLING:
            // Enchendo: Aguarda o sensor de nível
            if (ranLevelFull) {
                // Nível atingido!
                setRANSolenoidState(false); // Fecha a Solenoide
                Serial.println(F("Enchimento RAN concluido: Nivel atingido."));
                logSystemEvent("info", "Enchimento RAN concluido.");
                ranRefillCurrentState = RAN_REFILL_FINISHED;
            } else {                                                              // *** Checagem de Timeout ***
                if (millis() - ranRefillStartTime >= RAN_REFILL_TIMEOUT_MS) {
                    setRANSolenoidState(false); // **DESLIGA IMEDIATAMENTE POR SEGURANÇA**
                    ranRefillCurrentState = RAN_REFILL_FINISHED; // Move para o estado final                    
                    if (!ranRefillAlertSent) {
                        logSystemEvent("critical", "FALHA CRÍTICA: Timeout Enchimento RAN! Válvula fechada.");
                        ranRefillAlertSent = true;
                    }
                }
                // Implementação do timer de segurança será uma melhoria futura. [cite: 253]
            }

            break;
            
        case RAN_REFILL_FINISHED:
            // Estado de parada.
            break;
    }
}

// --- CHECAGEM DE ESTADO E RESET ---

bool isRanRefillFinished() {
    return ranRefillCurrentState == RAN_REFILL_FINISHED;
}

void resetRanRefillFlow() {
    ranRefillCurrentState = RAN_REFILL_IDLE;
    setRANSolenoidState(false); // Garantir que a válvula esteja FECHADA
    ranRefillAlertSent = false;
    Serial.println(F("Fluxo de Enchimento do RAN resetado."));
}

void checkRanRefillAlert() {
    // **NOTA:** A checagem de FALHA CRÍTICA (timeout) agora está no runRanRefillLoop()
    // Apenas garante a atualização do display/Blynk se o alerta tiver sido enviado.
    if (ranRefillAlertSent) {
        updateRanLevelDisplay();
    }
}

// --- ATUALIZAÇÃO BLYNK E DISPLAY PARA O RAN ---
void updateRanLevelDisplay() {
    // Note: Esta função é chamada repetidamente no runRanRefillLoop()
    // A logica aqui deve ser leve.

    // 1. Enviar para o Blynk
    if (Blynk.connected()) {
        // Envia o percentual de nível
        Blynk.virtualWrite(VPIN_RAN_LEVEL_PERCENT, ranLevelPercent); 

        // Envia o estado de alerta
        if (ranRefillAlertSent) {
            Blynk.virtualWrite(VPIN_RAN_REFILL_ALERT, 255);
        } else {
            Blynk.virtualWrite(VPIN_RAN_REFILL_ALERT, 0);
        }
    }
}

// -------------------------------------------------------------
// --- Ações de Buffer RAN 
// -------------------------------------------------------------

// --- CONTROLE DA BOMBA DE BUFFER (M5.4) ---
void setBufferPumpState(bool state) {
    if (serviceModeActive && state) {
        Serial.println(F("AVISO: Bomba de Buffer bloqueada pelo Modo de Serviço."));
        logSystemEvent("warning", "Bomba Buffer bloqueada (Servico ativo).");
        return;
    }
    digitalWrite(TPA_BUFFER_PUMP_PIN, state ? RELAY_ON : RELAY_OFF);
    Serial.print(F("Bomba de Buffer: "));
    Serial.println(state ? F("LIGADA") : F("DESLIGADA"));
}
