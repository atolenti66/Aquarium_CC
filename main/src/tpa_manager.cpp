/*
 +---------------------------------------+
 |      Aquarium Command & Control       |
 |                  ACC                  |  
 |Procedure: TPA_MANAGER                 |      
 +---------------------------------------+
 Project Purpose: Control an Aquarium parameters like Temperarture
                  pH, other water quality parameters from data received
                  of probes. It also will count with schedule to water
                  change useing actuators (like peristaltc pumps). All
                  data will be plotted on Web using an IoT cloud (like Blynk)
                  core will be an ESP32 board.


 Procedure Purpose: Coordinate the partial water change (in portuguese TPA ou troca parcial de água)

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
actuators_manager - Deal with all pump e actuator hardware in the system
tpa_manager       - (this file) Coordinate the partial water change (in portuguese TPA ou troca parcial de água)
tpa_reposition    - Control return of water volume
*/
#include <Arduino.h>  // Para manter compatibilidade com PlatformIO
#include "config.h"
#include "global.h"
#include "utils.h"

// 1. --- LÓGICA DE CÁLCULO DE VOLUME ---
void calculateTpaVolume() {
    if (aquariumTotalVolume <= 0 || tpaExtractionPercent <= 0) {
        volumeToExtractLiters = 0.0f;
        tpaPumpDurationMs = 0;
        return;
    }

    // Volume (L) = Volume Total * Percentual / 100
    volumeToExtractLiters = aquariumTotalVolume * (tpaExtractionPercent / 100.0f);
    
    // Calcula a duracao necessaria 
    // OBS: A função calculatePumpDuration() deve ser implementada no actuators_manager.ino (Próximo módulo)
    // Por enquanto, usaremos um placeholder: 10ml/s (10.0 ml/s)
    tpaPumpDurationMs = (unsigned long)((volumeToExtractLiters * 1000.0f) / 10.0f) * 1000UL;

    Serial.print(F("TPA Calculado: Extrair "));
    Serial.print(volumeToExtractLiters, 2);
    Serial.print(F(" L em "));
    Serial.print(tpaPumpDurationMs / 1000);
    Serial.println(F(" segundos."));
    
    logSystemEvent("info", "Configuracao TPA recalculada.");
}


// 2. Lógica  de Coordenação do Loop TPA Master
void runTpaManagerLoop() {
    // 2.1. Lógica de Agendamento Local
    if (tpaMasterCurrentState == TPA_MASTER_IDLE) {
        // Se estiver IDLE, checa se a hora de um agendamento local (se ativo) chegou
        checkLocalSchedule();
    }
    
    // 2.2. Coordenação da Extração (M5.1)
// 1. MÓDULO 5.1: Extração (Se estiver em andamento)
    if (tpaMasterCurrentState == TPA_MASTER_EXTRACTION_RUNNING_M51) {
        // A lógica de execução do M5.1 está dentro da função executeTpaExtraction(), que é síncrona/bloqueante
        // Portanto, o estado M51 só existe enquanto a bomba está ligada.
        // O estado de M5.2 (Reposição) é quem assume após a extração.
    }

    // 2.3. Coordenação da Reposição (M5.2)
    else if (tpaMasterCurrentState == TPA_MASTER_REPOSITION_RUNNING_M52) {
        runTpaRepositionLoop(); // Executa o loop da reposição (bomba ligada por tempo)

        if (isTpaRepositionFinished()) {
            Serial.println(F("TPA: Reposicao concluida. Inicia Enchimento do RAN (M5.3)..."));
            // Transição para o Módulo 5.3
            tpaMasterCurrentState = TPA_MASTER_REFILL_RUNNING_M53; 
            startRanRefillFlow(); // Inicia o novo fluxo de enchimento do RAN
            resetTpaRepositionFlow(); // Limpa o estado do M5.2
        }
    }
    
    // 2.4: ENCHIMENTO DO RAN (RO -> RAN) ***
    else if (tpaMasterCurrentState == TPA_MASTER_REFILL_RUNNING_M53) {
        runRanRefillLoop(); // Executa a máquina de estados do enchimento por nível

        if (isRanRefillFinished()) {
            Serial.println(F("TPA: Enchimento do RAN concluido. Ciclo TPA finalizado."));
            // Transição para o estado final
            tpaMasterCurrentState = TPA_MASTER_COMPLETED; 
            resetRanRefillFlow(); // Limpa o estado do M5.3
            logSystemEvent("info", "Ciclo TPA completo.");
        }
    }

    // 2.5: DOSAGEM DE BUFFER (BUFFER -> RAN) ***
    else if (tpaMasterCurrentState == TPA_MASTER_BUFFER_DOSING_M54) {
        runTpaBufferDosingLoop();
        if (isTpaBufferDosingFinished()) {
            Serial.println(F("M5.4 CONCLUÍDO. Ciclo TPA finalizado."));
            resetTpaBufferDosingFlow(); // Limpa o estado do M5.4
            tpaMasterCurrentState = TPA_MASTER_COMPLETED;
            Blynk.virtualWrite(VPIN_TPA_MASTER_STATE, tpaMasterCurrentState);
        }
    }

    // 2.5. Se o processo terminou, volta ao IDLE apos um ciclo completo
    else if (tpaMasterCurrentState == TPA_MASTER_COMPLETED) {
        tpaMasterCurrentState = TPA_MASTER_IDLE;
    }

    // 2.6. MÓDULO DE AGENDAMENTO LOCAL (Funciona em IDLE)
    if (tpaMasterCurrentState == TPA_MASTER_IDLE) {
        // Checa se é hora de executar a TPA de acordo com o agendamento local.
        checkLocalSchedule();
    }

}


// 3. Modificação dos Handlers BLYNK_WRITE (Onde o processo é disparado):
// Assumindo que executeTpaExtraction() é uma função de INICIALIZAÇÃO da extração M5.1
BLYNK_WRITE(VPIN_EXTRACTION_BUTTON) {
    if (param.asInt() == 1) { 
        Serial.println(F("Comando Blynk: TPA Manual recebido."));
        if (tpaMasterCurrentState == TPA_MASTER_IDLE) {
            // Dispara o inicio da Extração M5.1
            tpaMasterCurrentState = TPA_MASTER_EXTRACTION_RUNNING_M51;
            executeTpaExtraction(); 
        } else {
            Serial.println(F("AVISO: TPA ja esta em execucao."));
        }
    }
}

// BLYNK_WRITE(VPIN_TPA_SCHEDULE)
BLYNK_WRITE(VPIN_TPA_SCHEDULE) {
    Serial.println(F("Agendamento TPA disparado pelo Blynk."));
    if (tpaMasterCurrentState == TPA_MASTER_IDLE) {
        tpaMasterCurrentState = TPA_MASTER_EXTRACTION_RUNNING_M51;
        executeTpaExtraction(); 
    } else {
        Serial.println(F("AVISO: TPA ja esta em execucao."));
    }
}

// 4.--- LÓGICA DE AGENDAMENTO LOCAL (FALLBACK) ---
void checkLocalSchedule() {
    // Nao execute se:
    // 1. O agendamento local nao estiver ativo
    // 2. A bomba ja estiver rodando
    // 3. O Blynk estiver conectado (A TPA deve ser agendada pelo Timer Widget do Blynk)
    if (!tpaLocalScheduleActive || tpaExtractionPumpState || Blynk.connected()) return;

    bool readyToExecute = false;
    unsigned long currentTimeMs = millis();
    
    // --- 4.1. Checa a Frequencia Minima (Intervalo de 23h para evitar execucoes em loop) ---
    // A logica do agendamento so checa uma vez por hora, mas a execucao real deve ser limitada.
    if (currentTimeMs - lastTpaExecution < (23 * 60 * 60 * 1000UL)) { 
        return; 
    }
    
    // Obtem dados atuais do RTC (ou NTP, via a função unificada)
    DateTime now = getDateTimeNow(); 
    
    // --- 4.2. Checa a Hora e Minuto ---
    if (now.hour() == tpaScheduleHour && now.minute() == tpaScheduleMinute) {
        // --- 4.3. Checa as Regras de Frequencia ---
        switch (tpaScheduleFrequency) {
            case 0: // Diaria
                readyToExecute = true;
                break;
            case 1: // Semanal (1=Dom, 7=Sab. dayOfTheWeek() retorna 0=Dom a 6=Sab. Ajuste!)
                // O Blynk/Config usa 1..7 (ou 1..6 + 0) para dias. 
                // Assumimos que tpaScheduleDay esta mapeado para 0 (Dom) a 6 (Sab)
                if (now.dayOfTheWeek() == tpaScheduleDay) { 
                    readyToExecute = true;
                }
                break;
            case 2: // Quinzenal 
                if (now.dayOfTheWeek() == tpaScheduleDay) {
                    // Logica de 14 dias: Se a ultima execucao foi ha mais de 13 dias
                    if (now.unixtime() - (lastTpaExecution / 1000UL) > (13 * 24 * 60 * 60UL)) {
                        readyToExecute = true;
                    }
                }
                break;
            case 3: // Mensal (Checa o dia do mes: 1-31)
                if (now.day() == tpaScheduleDay) {
                    readyToExecute = true;
                }
                break;
        }
    }

    if (readyToExecute) {
        logSystemEvent("warning", "Agendamento Local TPA disparado. (OFFLINE)");
        lastTpaExecution = currentTimeMs; // Registra o tempo de execucao em millis
        executeTpaExtraction();
    }
}

// ------------------------------------------------------------------
// MÓDULO 5.4: Gerenciador de Adição de Buffer ao RAN (Dosagem por Tempo)
// ------------------------------------------------------------------

void startTpaBufferDosing() {
    if (ranBufferVolumeML == 0) {
        Serial.println(F("Dosagem de Buffer ignorada: Volume configurado é zero."));
        tpaBufferCurrentState = TPA_BUFFER_FINISHED; // Completa o módulo instantaneamente
        return;
    }

    // 1. Calcula a duração do acionamento da bomba (em ms)
    // Converte mL para Litros (float) e usa a função de cálculo de duração existente
    float bufferVolumeLiters = (float)ranBufferVolumeML / 1000.0f;
    bufferDosingDurationMs = calculatePumpDuration(bufferVolumeLiters);

    Serial.print(F("Iniciando Dosagem de Buffer: "));
    Serial.print(ranBufferVolumeML);
    Serial.print(F(" mL por "));
    Serial.print(bufferDosingDurationMs / 1000);
    Serial.println(F(" segundos."));
    logSystemEvent("info", "Dosagem de Buffer (M5.4) iniciada.");

    // 2. Inicia a dosagem e seta o estado
    setBufferPumpState(true); // Função implementada em actuators_manager.ino
    bufferPreviousMillis = millis();
    tpaBufferCurrentState = TPA_BUFFER_DOSING;
}

void runTpaBufferDosingLoop() {
    if (tpaBufferCurrentState == TPA_BUFFER_DOSING) {
        unsigned long currentMillis = millis();

        // Checa se o tempo de dosagem terminou
        if (currentMillis - bufferPreviousMillis >= bufferDosingDurationMs) {
            // DESLIGA a Bomba de Buffer
            setBufferPumpState(false);
            
            Serial.println(F("Dosagem de Buffer concluída."));
            logSystemEvent("info", "Dosagem de Buffer (M5.4) concluída.");

            // Transição para o estado final
            tpaBufferCurrentState = TPA_BUFFER_FINISHED;
        }
    }
}

bool isTpaBufferDosingFinished() {
    return tpaBufferCurrentState == TPA_BUFFER_FINISHED;
}

void resetTpaBufferDosingFlow() {
    tpaBufferCurrentState = TPA_BUFFER_IDLE;
    setBufferPumpState(false); // Garantir que a bomba esteja desligada
    bufferPreviousMillis = 0;
    bufferDosingDurationMs = 0;
    Serial.println(F("Fluxo de Dosagem de Buffer resetado."));
}


// 5.--- HANDLERS BLYNK PARA PERSISTÊNCIA E SINCRONIZAÇÃO ---

// SINCRONIZAÇÃO BLYNK: Volume Total (L) ---
BLYNK_WRITE(VPIN_TOTAL_VOLUME) {
    float newVolume = param.asFloat();
    if (newVolume > 0 && newVolume <= 5000) { 
        aquariumTotalVolume = newVolume;
        calculateTpaVolume(); 
        configIsDirty = true;
    }
}

// SINCRONIZAÇÃO BLYNK: Percentual de Extracao (%) ---
BLYNK_WRITE(VPIN_EXTRACTION_PERCENT) {
    float newPercent = param.asFloat();
    if (newPercent > 0 && newPercent <= 50) { 
        tpaExtractionPercent = newPercent;
        calculateTpaVolume(); 
        configIsDirty = true; 
    }
}

// SINCRONIZAÇÃO BLYNK: Ativa Agendamento Local (Fallback) ---
BLYNK_WRITE(VPIN_LOCAL_SCHEDULE_ACTIVE) {
    tpaLocalScheduleActive = param.asInt() == 1;
    configIsDirty = true; 
}

// SINCRONIZAÇÃO BLYNK: Frequencia (0:Diaria, 1:Semanal, 2:Quinzenal, 3:Mensal)
BLYNK_WRITE(VPIN_SCHEDULE_FREQUENCY) {
    int freq = param.asInt();
    if (freq >= 0 && freq <= 3) {
        tpaScheduleFrequency = freq;
        configIsDirty = true; 
    }
}

// SINCRONIZAÇÃO BLYNK: Dia da Semana/Mes (1-31)
BLYNK_WRITE(VPIN_SCHEDULE_DAY) {
    int day = param.asInt();
    if (day >= 1 && day <= 31) {
        tpaScheduleDay = day;
        configIsDirty = true; 
    }
}

// SINCRONIZAÇÃO BLYNK: Hora (V16) - Recebe String do Input Web
BLYNK_WRITE(VPIN_SCHEDULE_HOUR) {
    String hourStr = param.asString();
    int hour = atoi(hourStr.c_str());
    
    if (hour >= 0 && hour <= 23) { // Validação de 0 a 23
        tpaScheduleHour = hour;
        Serial.printf("Hora Agendada (Blynk): %d\n", hour);
        configIsDirty = true;
    } else {
        Serial.print(F("ERRO: Hora agendada invalida ("));
        Serial.print(hourStr);
        Serial.println(F("). Deve ser 0-23."));
    }
}

// SINCRONIZAÇÃO BLYNK: Minuto (V30) - Recebe String do Input Web
BLYNK_WRITE(VPIN_SCHEDULE_MINUTE) {
    String minuteStr = param.asString();
    int minute = atoi(minuteStr.c_str());
    
    if (minute >= 0 && minute <= 59) { // Validação de 0 a 59
        tpaScheduleMinute = minute;
        Serial.printf("Minuto Agendado (Blynk): %d\n", minute);
        configIsDirty = true;
    } else {
        Serial.print(F("ERRO: Minuto agendado invalido ("));
        Serial.print(minuteStr);
        Serial.println(F("). Deve ser 0-59."));
    }
}


// SINCRONIZAÇÃO BLYNK: Slider de Volume de Reposição (VPIN_REPOSITION_VOLUME_L)
BLYNK_WRITE(VPIN_REPOSITION_VOLUME_L) {
    // Valor recebido do slider/widget.
    float newVolume = param.asFloat();
    
    // O valor do slider deve ser limitado (ex: entre 0.1L e 1.5x o volume extraído)
    // Usamos o volume extraído como referência de limite superior para evitar erros grotescos.
    float maxLimit = volumeToExtractLiters * 1.5;

    if (newVolume < 0.1f) {
        newVolume = 0.1f; // Limite mínimo razoável
    } else if (newVolume > maxLimit) {
        newVolume = maxLimit;
    }

    if (volumeToRepositionLiters != newVolume) {
        volumeToRepositionLiters = newVolume;
        configIsDirty = true; // Sinaliza que a configuração precisa ser salva
        
        Serial.print(F("Blynk: Volume de Reposicao ajustado para "));
        Serial.print(volumeToRepositionLiters, 2);
        Serial.println(F(" L."));
        logSystemEvent("info", "Volume de Reposicao ajustado via Blynk.");
    }
    
    // Feedback: Força a escrita do valor de volta para o widget.
    // Isso garante que, se o valor for limitado (como acima), o slider se ajuste
    // ao valor real aceito pelo sistema (uma boa prática de UI/UX).
    Blynk.virtualWrite(VPIN_REPOSITION_VOLUME_L, volumeToRepositionLiters);
}

// --- BLYNK: Handlers de Sincronização de Configuração (M5.4 - Buffer) ---

BLYNK_WRITE(VPIN_RAN_BUFFER_VOLUME) {
    int newVolume = param.asInt();

    // Validação de intervalo (0-999)
    if (newVolume >= BUFFER_VOLUME_MIN && newVolume <= BUFFER_VOLUME_MAX) {
        if (ranBufferVolumeML != newVolume) {
            ranBufferVolumeML = newVolume;
            configIsDirty = true; // Sinaliza que a configuração precisa ser salva
            Serial.print(F("Volume de Buffer ajustado via Blynk: "));
            Serial.print(ranBufferVolumeML);
            Serial.println(F(" mL."));
        }
    } else {
        Serial.print(F("ERRO BLYNK: Volume de Buffer ("));
        Serial.print(newVolume);
        Serial.println(F(") fora do intervalo."));
        // Opcional: Enviar o valor atual de volta para o Blynk para corrigir o slider/numeric input
        if (Blynk.connected()) {
            Blynk.virtualWrite(VPIN_RAN_BUFFER_VOLUME, ranBufferVolumeML);
        }
    }
}

// 6.--- SETUP E SINCRONIZAÇÃO INICIAL ---

void setupTpaManager() {
    // Nenhuma inicializacao de pinos de bomba aqui (sera feito em actuators_manager.ino)
    
    // Calcula o volume inicial com as configuracoes carregadas do LittleFS
    calculateTpaVolume();

    // --- GARANTE QUE O VOLUME DE REPOSIÇÃO COMEÇA IGUAL AO EXTRAÍDO ---
    if (volumeToRepositionLiters == 0.0f) {
        // Se a configuracao não carregou ou não existe, usa o valor calculado
        volumeToRepositionLiters = volumeToExtractLiters; 
    }

    // Sincroniza o Blynk com os valores persistidos
    if (Blynk.connected()) {
        Blynk.virtualWrite(VPIN_TOTAL_VOLUME, aquariumTotalVolume);
        Blynk.virtualWrite(VPIN_EXTRACTION_PERCENT, tpaExtractionPercent);
        Blynk.virtualWrite(VPIN_EXTRACTION_VOLUME_L, volumeToExtractLiters);
        Blynk.virtualWrite(VPIN_LOCAL_SCHEDULE_ACTIVE, tpaLocalScheduleActive);
        Blynk.virtualWrite(VPIN_SCHEDULE_FREQUENCY, tpaScheduleFrequency);
        Blynk.virtualWrite(VPIN_SCHEDULE_DAY, tpaScheduleDay);
        Blynk.virtualWrite(VPIN_SCHEDULE_HOUR, String(tpaScheduleHour));
        Blynk.virtualWrite(VPIN_SCHEDULE_MINUTE, String(tpaScheduleMinute));
        Blynk.virtualWrite(VPIN_REPOSITION_VOLUME_L, volumeToRepositionLiters);
        Blynk.virtualWrite(VPIN_RAN_BUFFER_VOLUME, ranBufferVolumeML);
    }


}