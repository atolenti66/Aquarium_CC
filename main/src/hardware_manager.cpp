/*
 +---------------------------------------+
 |      Aquarium Command & Control       |
 |                  ACC                  |  
 |Procedure: HARDWARE_MANAGER            |      
 +---------------------------------------+
 Project Purpose: Control an Aquarium parameters like Temperarture
                  pH, other water quality parameters from data received
                  of probes. It also will count with schedule to water
                  change useing actuators (like peristaltc pumps). All
                  data will be plotted on Web using an IoT cloud (like Blynk)
                  core will be an ESP32 board.


 Procedure Purpose: Deal with all phisical buttons in the system

 Author: Alberto Tolentino (and Gemini AI)
 
== Functional specification ==



== Version History ==
04/11/2025 - 0.0.4 - Moved project to VSCode+PlatformIO to allow test without hardware with Unity.
                     Addedd <Arduino.h> on top of file to maximize compilation success. Using now
                     a new versioning number schema (Major.Minor.Patch)
02/11/2025 - 0.0.3 - Re-factoring code to use Buttons2 library
02/11/2025 - 0.0.2 - Re-factoring code to know use a page button, and combination of
                    short, long press for several inputs and actions
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
hardware_manager  - (this file) Deal with all physical buttons in the project
actuators_manager - Deal with all pump e actuator hardware in the system
tpa_manager       - Coordinate the partial water change (in portuguese TPA ou troca parcial de água)
tpa_reposition    - Control return of water volume
blynk_interface.h - Blynk related functions and handlers   
sensors_interface.h - Mocks for OneWire and DallasTemperature for unit testing
rtc_interface.h   - RTC interface abstraction for unit testing
timelib_interface.h - Mocks for TimeLib for unit testing

*/

#include <Arduino.h>  // Para manter compatibilidade com PlatformIO
#include "config.h"
#include "global.h"
#include "utils.h"
#include "blynk_interface.h"

// --- DECLARAÇÃO DE OBJETOS Button2 (Estas deveriam ser movidas para global.h) ---
// Estes objetos gerenciam o estado de cada botão físico
Button2 oledPageButton;
Button2 upButton;
Button2 downButton;
Button2 phCalButton;
Button2 alertResetButton;
Button2 serviceModeButton;
Button2 rtcResetButton;

// --- FUNÇÕES DE NAVEGAÇÃO E EDIÇÃO (CALLBACKS DO Button2) ---

void handleOledPageTap(Button2& btn) {
    int logMode = -1; // Usada para o log no final
    
    // --- LÓGICA DE EDIÇÃO: Página 1 (Agendamento TPA) ---
    if (currentPage == 1) {
        if (page1EditMode == -1) {
            // Entra no primeiro campo de edição (Dia)
            page1EditMode = 0; 
            logMode = 0;
        } else {
            // Próximo campo de edição (0 a 4)
            page1EditMode = (page1EditMode + 1) % 5; 
            logMode = page1EditMode;
        }
    } 
    // --- LÓGICA DE EDIÇÃO: Página 2 (Volume Reposição) ---
    else if (currentPage == 2) {
        // Alterna o modo de edição: 0=Visualizar, 1=Editar Volume
        page2EditMode = (page2EditMode == 0) ? 1 : 0;
        logMode = page2EditMode;
    }
    // 🧽 NOVO: LÓGICA DE EDIÇÃO: Página 3 (Volume de Buffer) 🧽
    else if (currentPage == 3) {
        // Alterna o modo de edição: 0=Visualizar, 1=Editar Volume
        page3EditMode = (page3EditMode == 0) ? 1 : 0;
        logMode = page3EditMode;
    }

    // A página 0 (Dashboard) não tem modo de edição.
    
    if (logMode != -1) {
        Serial.print(F("Botao PAGE CURTO: Pagina "));
        Serial.print(currentPage);
        Serial.print(F(" - Modo de Edicao: "));
        Serial.println(logMode);
    }
}

void handleOledPageLongPress(Button2& btn) {
    // 1. Sai de qualquer modo de edição ao trocar de página
    page1EditMode = -1;
    page2EditMode = 0;
    page3EditMode = 0; // **GARANTE SAÍDA DO MODO DE EDIÇÃO DO BUFFER**

    // 2. Troca de página (Loop: 0, 1, 2, 3, ...)
    currentPage = (currentPage + 1) % NUM_OLED_PAGES;

    Serial.print(F("Botao PAGE LONGO: Mudou para Pagina "));
    Serial.println(currentPage);
}

void handleUpTap(Button2& btn) {
    if (currentPage == 1 && page1EditMode != -1 && page1EditMode != 4) {
        // Lógica de incremento da Pagina 1
        switch (page1EditMode) {
            case 0: // Dia (tpaScheduleDay)
                tpaScheduleDay = (tpaScheduleDay % 7) + 1; // 1 a 7
                break;
            case 1: // Hora (tpaScheduleHour)
                tpaScheduleHour = (tpaScheduleHour + 1) % 24; // 0 a 23
                break;
            case 2: // Minuto (tpaScheduleMinute)
                tpaScheduleMinute = (tpaScheduleMinute + 1) % 60; // 0 a 59
                break;
            case 3: // Frequência (tpaScheduleFrequency)
                tpaScheduleFrequency = (tpaScheduleFrequency + 1) % 4; // 0 a 3
                break;
        }
        Serial.print(F("Botao UP: Valor ajustado. Campo: ")); Serial.println(page1EditMode);
    }
}

void handleDownTap(Button2& btn) {
    if (currentPage == 1 && page1EditMode != -1 && page1EditMode != 4) {
        // Lógica de decremento da Pagina 1
        switch (page1EditMode) {
            case 0: // Dia (tpaScheduleDay)
                tpaScheduleDay = (tpaScheduleDay == 1) ? 7 : (tpaScheduleDay - 1); // 1 a 7
                break;
            case 1: // Hora (tpaScheduleHour)
                tpaScheduleHour = (tpaScheduleHour == 0) ? 23 : (tpaScheduleHour - 1); // 0 a 23
                break;
            case 2: // Minuto (tpaScheduleMinute)
                tpaScheduleMinute = (tpaScheduleMinute == 0) ? 59 : (tpaScheduleMinute - 1); // 0 a 59
                break;
            case 3: // Frequência (tpaScheduleFrequency)
                tpaScheduleFrequency = (tpaScheduleFrequency == 0) ? 3 : (tpaScheduleFrequency - 1); // 0 a 3
                break;
        }
        Serial.print(F("Botao DOWN: Valor ajustado. Campo: ")); Serial.println(page1EditMode);
    }
}

// --- FUNÇÕES DE AÇÃO (CALLBACKS DO Button2) ---

void handleAlertResetTap(Button2& btn) {
    // Ação: Reset de Alertas Críticos (Short Press)
    resetCriticalAlerts();
    Serial.println(F("Botao ALERT_RESET acionado (CURTO): Reset de Alertas."));
    logSystemEvent("info", "Reset de Alertas Críticos.");
}

void handleAlertResetLongPress(Button2& btn) {
    // Ação: Reset de Valores Min/Max de Sensores (Long Press)
    // Se estiver em uma página de configuração (ex: 2), poderia forçar o salvamento.
    // Usaremos a ação de reset de min/max dos sensores.
    resetSensorData();
    Serial.println(F("Botao ALERT_RESET (LONGO): Reset de valores min/max dos sensores."));
    logSystemEvent("info", "Reset de min/max dos sensores.");
}

void handleRtcResetTap(Button2& btn) {
    // Ação: Reset de Alerta de Perda de Energia RTC (Short Press)
    resetRtcOsfAlert();
    Serial.println(F("Botao RTC_RESET acionado (CURTO): Reset de Alerta de Bateria RTC."));
    logSystemEvent("info", "Alerta OSF RTC resetado manualmente.");
}

void handlePhCalLongPress(Button2& btn) {
    // Ação: Iniciar Calibração de pH (Long Press)
    executePhCalibration();
    Serial.println(F("Botao PH_CAL acionado (LONGO): Calibracao de PH iniciada."));
    logSystemEvent("info", "Calibração de PH iniciada.");
}

void handleServiceModeLongPress(Button2& btn) {
    // Ação: Alternar Modo de Serviço (Long Press)
    serviceModeActive = !serviceModeActive;
    String logMsg = serviceModeActive ? F("Modo Servico ATIVADO.") : F("Modo Servico DESATIVADO.");
    Serial.println(logMsg);
    logSystemEvent("warning", logMsg.c_str());

    // Sincroniza o estado com o Blynk
    Blynk.virtualWrite(VPIN_SERVICE_MODE, serviceModeActive ? 1 : 0);
}


// --- SETUP: INICIALIZAÇÃO DOS BOTÕES FÍSICOS (COM Button2) ---
void setupHardwareButtons() {
    Serial.println(F("Configurando pinos de botoes fisicos com Button2..."));

    // --- 1. Botões de NAVEGAÇÃO OLED ---
    oledPageButton.begin(OLED_PAGE_BUTTON_PIN);
    oledPageButton.setClickHandler(handleOledPageTap); 
    oledPageButton.setLongClickHandler(handleOledPageLongPress);

    upButton.begin(UP_BUTTON_PIN);
    upButton.setClickHandler(handleUpTap); 

    downButton.begin(DOWN_BUTTON_PIN);
    downButton.setClickHandler(handleDownTap);

    // --- 2. Botões de AÇÃO ---
    phCalButton.begin(PH_CAL_BUTTON_PIN);
    phCalButton.setLongClickHandler(handlePhCalLongPress); 

    alertResetButton.begin(ALERT_RESET_BUTTON_PIN);
    alertResetButton.setClickHandler(handleAlertResetTap); // CORREÇÃO
    alertResetButton.setLongClickHandler(handleAlertResetLongPress); // CORREÇÃO

    serviceModeButton.begin(SERVICE_MODE_BUTTON_PIN);
    serviceModeButton.setLongClickHandler(handleServiceModeLongPress); 

    rtcResetButton.begin(RTC_RESET_BUTTON_PIN);
    rtcResetButton.setClickHandler(handleRtcResetTap); // CORREÇÃO

    Serial.println(F("Botoes fisicos configurados. Logica manual removida."));
}


// --- LOOP: FUNÇÃO PRINCIPAL DE GERENCIAMENTO DE HARDWARE ---
// Esta função é chamada repetidamente no loop() principal (main.ino)
void runHardwareManagerLoop() {
    // Otimizado: Apenas chame o método loop() de cada botão.
    // A lógica de debounce, tempo e disparo das ações foi delegada ao Button2.
    oledPageButton.loop();
    upButton.loop();
    downButton.loop();
    phCalButton.loop();
    alertResetButton.loop();
    serviceModeButton.loop();
    rtcResetButton.loop();

    // NOTA: Toda a lógica de detecção de botão manual (processButton, handleShortPress,
    // handleLongPress) que estava aqui foi REMOVIDA.
}
