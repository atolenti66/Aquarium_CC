/*
 +---------------------------------------+
 |      Aquarium Command & Control       |
 |                  ACC                  |  
 |Procedure: TPA_REPOSITION              |      
 +---------------------------------------+
 Project Purpose: TPA Repositioning (RAN -> Aquarium)

 Author: Alberto Tolentino (and Gemini AI)
 
 == Version History ==
 04/11/2025 - 0.0.2 - Moved project to VSCode+PlatformIO to allow test without hardware with Unity.
                     Addedd <Arduino.h> on top of file to maximize compilation success. Using now
                     a new versioning number schema (Major.Minor.Patch)
 03/11/2025 - 0.0.1 - Primeira implementação do Fluxo Pós-Extração (Módulo 5.2)
                     Refatorado para usar variaveis e funcoes globais existentes.
== Functional specification ==



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
tpa_reposition    - (this file) Control return of water volume
blynk_interface.h - Blynk related functions and handlers   
sensors_interface.h - Mocks for OneWire and DallasTemperature for unit testing
rtc_interface.h   - RTC interface abstraction for unit testing
timelib_interface.h - Mocks for TimeLib for unit testing

*/
#include <Arduino.h>  // Para manter compatibilidade com PlatformIO 
#include "config.h"
#include "global.h"
#include "utils.h"

// --- 2. FUNÇÕES DE CONTROLE (Módulo 5.2) ---

/**
 * Inicia o fluxo de Reposição (Módulo 5.2). 
 * Esta função é chamada pelo TPA_MANAGER após a conclusão da extração.
 * * Dependências: 
 * - logSystemEvent() (main.ino)
 * - tpaPumpDurationMs (global.h, setado em actuators_manager.ino)
 */
void startTpaRepositionFlow() {
    // A duração da bomba de extração (tpaPumpDurationMs) é o tempo que a bomba
    // de reposição deve funcionar para devolver o volume exato.

    if (tpaRepositionCurrentState != TPA_REPOSITION_IDLE && tpaRepositionCurrentState != TPA_REPOSITION_FINISHED) {
        Serial.println(F("ERRO: O fluxo de reposicao ja esta em execucao."));
        logSystemEvent("error", "Tentativa de iniciar Reposicao em estado ativo.");
        return;
    }
    
    Serial.println(F("M5.2: Iniciando Fluxo de Execucao Pos-Extracao (RAN -> Aquario)..."));
    logSystemEvent("info", "Iniciando Reposicao TPA.");

    // --- CÁLCULO DA DURAÇÃO COM O VOLUME AJUSTADO PELO USUÁRIO ---
    repositionIntervalMs = calculatePumpDuration(volumeToRepositionLiters); 
    
    Serial.print(F("Volume de Reposicao (Ajustado): ")); 
    Serial.print(volumeToRepositionLiters, 2); 
    Serial.print(F(" L. Duracao: "));
    Serial.print(repositionIntervalMs / 1000); Serial.println(F("s."));

    // --- 1.1 Aguardar 5s ---
    repositionPreviousMillis = millis();
    repositionIntervalMs = SAFETY_PAUSE_MS; 
    tpaRepositionCurrentState = TPA_REPOSITION_WAIT_SAFETY_PAUSE;
    
    Serial.print(F("1.1 Aguardando pausa de seguranca: ")); 
    Serial.print(SAFETY_PAUSE_MS / 1000); Serial.println(F("s."));
}


/**
 * Função de loop principal para o Módulo 5.2. 
 * Deve ser chamada dentro da funcao loop() do main.ino.
 * * Dependencias: 
 * - digitalRead()/digitalWrite() para TPA_REPOSITION_PUMP_PIN
 * - RELAY_ON/RELAY_OFF (global.h ou config.h)
 */
void runTpaRepositionLoop() {
    unsigned long currentMillis = millis();

    switch (tpaRepositionCurrentState) {
        case TPA_REPOSITION_IDLE:
            // Nenhuma ação. Aguarda startTpaRepositionFlow().
            break;

        case TPA_REPOSITION_WAIT_SAFETY_PAUSE:
            // --- 1.1 Aguardar 5s ---
            if (currentMillis - repositionPreviousMillis >= repositionIntervalMs) {
                Serial.println(F("1.1 Pausa de seguranca (5s) concluida."));
                
                // Transição para a próxima etapa: Reposição Principal
                repositionPreviousMillis = currentMillis; // Reinicia o contador
                repositionIntervalMs = tpaPumpDurationMs; // Tempo de extração (em ms)
                
                // LIGA a Bomba de Reposição (RAN -> Aquário)
                digitalWrite(TPA_REPOSITION_PUMP_PIN, RELAY_ON); // **ASSUME PINO GLOBAL**
                Serial.print(F("1.2 Iniciando Reposicao Principal por "));
                Serial.print(tpaPumpDurationMs / 1000); Serial.println(F(" segundos..."));
                logSystemEvent("info", "Bomba de Reposicao ligada.");

                tpaRepositionCurrentState = TPA_REPOSITION_TRANSFER_RAN_TO_AQUARIO;
            }
            break;

        case TPA_REPOSITION_TRANSFER_RAN_TO_AQUARIO:
            // --- 1.2 Reposição Principal (RAN -> Aquário) ---
            if (currentMillis - repositionPreviousMillis >= repositionIntervalMs) {
                // DESLIGA a Bomba de Reposição
                digitalWrite(TPA_REPOSITION_PUMP_PIN, RELAY_OFF); // **ASSUME PINO GLOBAL**
                Serial.println(F("1.2 Reposicao Principal concluida."));
                logSystemEvent("info", "Reposicao TPA concluida.");
                
                // Transição para o estado final
                tpaRepositionCurrentState = TPA_REPOSITION_FINISHED;
                Serial.println(F("M5.2 CONCLUÍDO. Aguardando proximo módulo do TPA Manager."));
            }
            break;

        case TPA_REPOSITION_FINISHED:
            // A TPA_MANAGER deve verificar este estado e iniciar o proximo módulo (Enchimento do RAN).
            break;
    }
}

// --- FUNÇÃO PARA VERIFICAR O FIM DO FLUXO (CHAMADA PELO TPA_MANAGER) ---
bool isTpaRepositionFinished() {
    return tpaRepositionCurrentState == TPA_REPOSITION_FINISHED;
}

// --- FUNÇÃO PARA RESETAR O FLUXO (CHAMADA PELO TPA_MANAGER) ---
void resetTpaRepositionFlow() {
    if (tpaRepositionCurrentState != TPA_REPOSITION_IDLE) {
        tpaRepositionCurrentState = TPA_REPOSITION_IDLE;
        Serial.println(F("M5.2 Reposicao resetado para IDLE."));
    }
}