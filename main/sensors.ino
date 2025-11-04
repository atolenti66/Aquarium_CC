/*
 +---------------------------------------+
 |      Aquarium Command & Control       |
 |                  ACC                  |  
 |Procedure: SENSORS                     |      
 +---------------------------------------+
 Project Purpose: Control an Aquarium parameters like Temperarture
                  pH, other water quality parameters from data received
                  of probes. It also will count with schedule to water
                  change useing actuators (like peristaltc pumps). All
                  data will be plotted on Web using an IoT cloud (like Blynk)
                  core will be an ESP32 board.


 Procedure Purpose: Store functions like readTemperature(), readPH(), alert code
                    used to interact with aquarium probes

 Author: Alberto Tolentino (and Gemini AI)
 
== Functional specification ==



== Version History ==
11/01/2025 - 0.02 - Adjusted Blynk events to new log function. Included utils.h
31/10/2025 - 0.01 - First installment based on code created by Gemini AI (Google)

== Project file structure ==
main.ino          - Main program with setup parameters and loop functions
config.h          - Pin definitions, passwords, IDs (Auth, SSID, Pass)
global.h          - Global definitions
utils.h           - Functions prototypes
rtc_time.ino      - setupNTP(), getCurrentTime(), BLYNK_CONNECTED(), BLYNK_WRITE(RTC_RESET)
sensors.ino       - (this file) readTemperature(), readPH(), alert code
ph_sensor         - Deal with readings from pH probe
config_manager    - Used to save config/data on JSON on SPIFF(LittleFS) partition on ESP32
display_manager   - Deal with all output on OLED Display
hardware_manager  - Deal with all physical buttons in the project
actuators_manager - Deal with all pump e actuator hardware in the system
tpa_manager       - Coordinate the partial water change (in portuguese TPA ou troca parcial de água)
tpa_reposition    - Control return of water volume

*/

#include "config.h"
#include "global.h"
#include "utils.h"

// DECLARAÇÕES EXTERNAS
extern OneWire oneWire;
extern DallasTemperature sensors;
extern bool highTempAlertSent;

// --- Sub-rotina de Leitura de Temperatura ---
float readTemperature() {
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);
  
  // Tratar leitura de erro (se o sensor retornar -127)
  if (tempC == -127.00) {
    Serial.println(F("ERRO: Falha ao ler sensor DS18B20!"));
    return -999.0; // Retorna um valor de erro conhecido
  }
  return tempC;
}

// --- Lógica de Alerta de Temperatura ---
void checkTempAlert(float tempC) {
  if (tempC > UPPER_TEMP && !highTempAlertSent) {
    if (Blynk.connected()) {
      String msg = String("Temperatura alta: ") + tempC + F("°C");
      logSystemEvent("critical", msg.c_str());
      // Blynk.logEvent("high_temp_alert", String(F("ALERTA: Temperatura Alta: ")) + tempC + "C");
      Blynk.virtualWrite(VPIN_TEMP_ALERT, 255); // Acende LED V15
    }
    Serial.println(F("ALERTA: Temperatura elevada detectada."));
    highTempAlertSent = true;
  } 
  
  // Resetar o alerta se a temperatura baixar
  else if (tempC <= UPPER_TEMP && highTempAlertSent) {
    if (Blynk.connected()) {
        logSystemEvent("warning", "A temperatura voltou aos limites normais.");
        // Blynk.logEvent("high_temp_alert", F("A temperatura voltou aos limites normais."));
        Blynk.virtualWrite(VPIN_TEMP_ALERT, 0); // Apaga LED V15
    }
    highTempAlertSent = false;
    Serial.println(F("AVISO: Temperatura normalizada. Alerta resetado."));
  }
}

// Resetar o alerta se botão físico for pressionado
void resetCriticalAlerts() {
    if (highTempAlertSent || phAlertSent) {
        // Resetar Alertas de Temperatura e pH
        highTempAlertSent = false;
        phAlertSent = false;
        
        // Limpar LEDs virtuais
        if (Blynk.connected()) {
            Blynk.virtualWrite(VPIN_TEMP_ALERT, 0);
            Blynk.virtualWrite(VPIN_PH_ALERT, 0);
        }
        Serial.println(F("Todos os alertas criticos de sensores resetados."));
    } else {
        Serial.println(F("Nenhum alerta critico ativo para reset."));
    }
}

// --- RESET MANUAL DA FLAG OSF DO RTC ---
// Chamado por um botão físico para resetar o alerta de perda de energia
void resetRtcOsfAlert() {
    if (rtc_osf_flag) {
        rtc_osf_flag = false;
        
        // Embora rtc.lostPower() e rtc.clearOSF() sejam métodos da RTClib,
        // nao precisamos chama-los aqui, pois a flag rtc_osf_flag ja foi definida
        // internamente em checkRtcStatus() e será re-checada no proximo boot.
        // O importante é limpar a variavel global de alerta.

        Serial.println(F("Alerta de falha de energia do RTC (OSF) resetado manualmente."));
        logSystemEvent("info", "Alerta OSF RTC resetado manualmente.");
    } else {
        Serial.println(F("Nenhum alerta OSF RTC ativo para reset."));
    }
}

// --- RESET MANUAL DOS VALORES MIN/MAX DOS SENSORES (NOVO) ---
// Chamado por um botão físico (Long Press em ALERT_RESET_BUTTON_PIN)
void resetSensorData() {
    // Resetar para extremos, para que o proximo valor lido se torne o novo min/max
    temperatureC = 0.0f; 
    phValue = 0.0f;

    // Atualiza o Blynk para 0 (assumindo que VPIN_TEMP_MIN/MAX e VPIN_PH_MIN/MAX existem)
    if (Blynk.connected()) {
        Blynk.virtualWrite(VPIN_TEMP, 0); 
        Blynk.virtualWrite(VPIN_PH_VAL, 0);
    }

    Serial.println(F("Registros de Min/Max de sensores resetados."));
    logSystemEvent("info", "Registros de Min/Max de sensores limpos manualmente.");
}


// --- Lógica BLYNK: Botão de Reset de Alertas ---
BLYNK_WRITE(VPIN_ALERT_RESET) {
    int buttonState = param.asInt();
    
    if (buttonState == 1) { 
        Serial.println(F("Comando Blynk: Reset de Alertas Criticos recebido."));
        // Chama a mesma função que o botão físico chama
        resetCriticalAlerts(); 
    }
}