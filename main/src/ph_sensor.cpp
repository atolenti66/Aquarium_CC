/*
 +---------------------------------------+
 |      Aquarium Command & Control       |
 |                  ACC                  |  
 |Procedure: PH_SENSOR                   |      
 +---------------------------------------+
 Project Purpose: Control an Aquarium parameters like Temperarture
                  pH, other water quality parameters from data received
                  of probes. It also will count with schedule to water
                  change useing actuators (like peristaltc pumps). All
                  data will be plotted on Web using an IoT cloud (like Blynk)
                  core will be an ESP32 board.


 Procedure Purpose: Deal with readings from pH probe

 Author: Alberto Tolentino (and Gemini AI)
 
== Functional specification ==



== Version History ==
04/11/2025 - 0.0.3 - Moved project to VSCode+PlatformIO to allow test without hardware with Unity.
                     Addedd <Arduino.h> on top of file to maximize compilation success. Using now
                     a new versioning number schema (Major.Minor.Patch)
01/11/2025 - 0.0.2 - Adjusted Blynk events to new log function. Included utils.h
                    Created executePhCalibration() as instead og Blynk handler
                    to allow re-usage of calibration process with different interfaces.
                    Added on executePhCalibration() call to saveConfig() to allow calibration
                    data storage.
31/10/2025 - 0.0.1 - First installment based on code created by Gemini AI (Google)

== Project file structure ==
main.ino          - Main program with setup parameters and loop functions
config.h          - Pin definitions, passwords, IDs (Auth, SSID, Pass)
global.h          - Global definitions
utils.h           - Functions prototypes
rtc_time.ino      - setupNTP(), getCurrentTime(), BLYNK_CONNECTED(), BLYNK_WRITE(RTC_RESET)
sensors.ino       - readTemperature(), readPH(), alert code
ph_sensor         - (this file) Deal with readings from pH probe
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

// ph_sensor.ino
#include <Arduino.h>  // Para manter compatibilidade com PlatformIO
#include "config.h"
#include "global.h"
#include "utils.h"
#include "blynk_interface.h"


// --- 1. FUNÇÃO DE LEITURA E CONVERSÃO DE PH ---
// Esta função faz a leitura bruta e aplica a conversão para pH.
// Nota: O ponto neutro (pH 7.0) é geralmente 1500mV. O A0 do ESP32 tem Vref 3.3V.
// O ponto neutro do módulo de pH é ajustado via potenciômetro (3.3V/2 = 1.65V, se o módulo for 3.3V)
// Assumiremos uma conversão básica, que será corrigida pelo offset.

float readPH() {
    // 1. Leitura do valor analógico
    int analogValue = analogRead(PH_PIN);

    // 2. Conversão para Tensão (Assumindo 3.3V Vref e ADC de 12 bits = 4095)
    // Se o seu ESP estiver usando 10 bits (1023), ajuste para 1023.
    // ESP32: ADC de 12 bits (0-4095).
    float voltage = (float)analogValue / 4095.0 * 3.3; // Tensão lida no pino

    // 3. Conversão para pH (Fórmula baseada em um modelo DFrobot ou similar)
    // O valor de 1.65V (meia tensão) é onde o pH é 7.0. A sensibilidade é de aprox. 5.7 * pH/V
    
    // A fórmula mais comum é:
    // pH = 7 + ((2.5 - Tensão) / 0.18) (Para Vref 5V e pH 7.0 em 2.5V)
    
    // Para 3.3V, vamos simplificar para:
    // pH = 7.0 - ((voltage - 1.65) / 0.2) 
    // O '0.2' é um coeficiente de sensibilidade que será corrigido pelo offset

    // Para fins de simulação e calibração por offset, usaremos uma fórmula base
    float raw_pH = 7.0 - ((voltage - 1.65) / 0.2); 
    
    // 4. Aplicar o Offset de Calibração
    float final_pH = raw_pH + phCalibrationOffset;
    
    // 5. Atualizar a variável global e retornar
    phValue = final_pH;
    
    return phValue;
}


// --- 2. FUNÇÃO CENTRAL DE CALIBRAÇÃO (REUTILIZÁVEL) ---
// Esta função contém a lógica de calibração. Ela pode ser chamada
// pelo Blynk, por um botão físico, ou por qualquer outra fonte.
void executePhCalibration() {
    Serial.println(F("CALIBRAÇÃO DE PH (Offset Simples) INICIADA."));
        
    // 1. Inicia o modo de calibração
    phCalibrationMode = true; 
    
    // 2. Faz uma leitura de estabilização (mantendo a lógica)
    float sumVoltage = 0;
    int numReadings = 10;
    
    for (int i = 0; i < numReadings; i++) {
        // Assume-se que o PH_PIN está sendo lido com ADC de 12 bits (4095) e Vref 3.3V
        sumVoltage += (float)analogRead(PH_PIN) / 4095.0 * 3.3; 
        delay(50); 
    }
    float measuredVoltage = sumVoltage / numReadings;

    // 3. Cálculos de Offset
    const float neutralVoltage = 1.65; // Tensão alvo para pH 7.0
    const float sensitivity = 0.2;     // Coeficiente de sensibilidade (para fórmula base)
    
    // pH Bruto = 7.0 - ((Tensão Lida - Tensão Neutra) / Sensibilidade)
    float raw_pH_at_7 = 7.0 - ((measuredVoltage - neutralVoltage) / sensitivity); 
    
    // O offset é a diferença para levar o valor bruto a 7.0
    phCalibrationOffset = 7.0 - raw_pH_at_7;
    
    // 4. Finaliza e Notifica
    phCalibrationMode = false;
    
    // Logs no Serial
    Serial.print(F("Leitura de Tensão (Media): ")); Serial.print(measuredVoltage, 4); Serial.println(F("V"));
    Serial.print(F("pH Bruto Calculado: ")); Serial.print(raw_pH_at_7, 2);
    Serial.print(F(" | Novo Offset Aplicado: ")); Serial.println(phCalibrationOffset, 3);
    
    // 5. Notificação Blynk
    if (Blynk.connected()) {
        Blynk.virtualWrite(VPIN_CAL_STATUS, String(F("Offset: ")) + phCalibrationOffset);
        String msg = String(F("Novo offset de pH aplicado: ")) + phCalibrationOffset;
        logSystemEvent("info", msg.c_str());
    }
    
    // 6. Persistência de dados (salvando no SPIFFS/LitleFS)
    saveConfig();
    
}

// --- 3. MANIPULADOR BLYNK (WRAPPER) ---
// O BLYNK_WRITE apenas reage ao pino virtual e chama a função central
BLYNK_WRITE(VPIN_PH_CAL) {
    int buttonState = param.asInt();
    
    if (buttonState == 1) { // Ação ao pressionar (DOWN) o botão no Blynk
        executePhCalibration();
    }
}

// --- 3. LÓGICA DE ALERTA DE PH ---
void checkPhAlert(float currentPh) {
    bool alertCondition = false;
    String alertMessage = "";

    if (currentPh < PH_MIN_LIMIT) {
        alertCondition = true;
        alertMessage = String(F("ALERTA: PH MUITO ÁCIDO! Valor: ")) + currentPh;
    } else if (currentPh > PH_MAX_LIMIT) {
        alertCondition = true;
        alertMessage = String(F("ALERTA: PH MUITO ALCALINO! Valor: ")) + currentPh;
    }

    if (alertCondition) {
        // A condição de alerta está ativa

        if (!phAlertSent) {
            // Envia o alerta apenas se ainda não tiver sido enviado
            Serial.println(alertMessage);
            
            if (Blynk.connected()) {
                logSystemEvent("critical", alertMessage.c_str());
                // Blynk.logEvent("ph_alert", alertMessage);
                Blynk.virtualWrite(VPIN_PH_ALERT, 255); // Ex: Acende o LED (valor 255)
            }
            phAlertSent = true;
        }

    } else {
        // Condição normal: o pH está dentro dos limites

        if (phAlertSent) {
            // Envia notificação de "tudo resolvido" se o alerta estava ativo
            Serial.println(F("PH ESTÁVEL. Condição de alerta resolvida."));
            
            if (Blynk.connected()) {
                logSystemEvent("warning", "O pH voltou aos limites operacionais.");
                // Blynk.logEvent("ph_stable", "O pH voltou aos limites operacionais.");
                Blynk.virtualWrite(VPIN_PH_ALERT, 0); // Ex: Apaga o LED (valor 0)
            }
            // Resetar a flag para permitir um novo alerta no futuro
            phAlertSent = false;
        }
        
        // Se a condição estava normal, apenas garante que o LED está desligado (0)
        // Isso evita que o Blynk.virtualWrite seja chamado a cada loop se o LED já estiver em 0
        if (Blynk.connected()) {
            Blynk.virtualWrite(VPIN_PH_ALERT, 0); 
        }
    }
}