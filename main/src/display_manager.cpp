/*
 +---------------------------------------+
 |      Aquarium Command & Control       |
 |                  ACC                  |  
 |Procedure: DISPLAY_MANAGER             |      
 +---------------------------------------+
 Project Purpose: Control an Aquarium parameters like Temperarture
                  pH, other water quality parameters from data received
                  of probes. It also will count with schedule to water
                  change useing actuators (like peristaltc pumps). All
                  data will be plotted on Web using an IoT cloud (like Blynk)
                  core will be an ESP32 board.


 Procedure Purpose: Deal with all output on OLED Display

 Author: Alberto Tolentino (and Gemini AI)
 
== Functional specification ==



== Version History ==
04/11/2025 - 0.0.3 - Moved project to VSCode+PlatformIO to allow test without hardware with Unity.
                     Addedd <Arduino.h> on top of file to maximize compilation success. Using now
                     a new versioning number schema (Major.Minor.Patch)
02/11/2025 - 0.0.2 - Re-factored to implement OLED pagination
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
display_manager   - (this file) Deal with all output on OLED Display
hardware_manager  - Deal with all physical buttons in the project
actuators_manager - Deal with all pump e actuator hardware in the system
tpa_manager       - Coordinate the partial water change (in portuguese TPA ou troca parcial de água)
tpa_reposition    - Control return of water volume

*/

// display_manager.ino
#include <Arduino.h>  // Para manter compatibilidade com PlatformIO
#include "config.h"
#include "global.h"
#include "utils.h"

// Define o objeto display (extern em global.h, mas inicializado aqui)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Array para mapear a Frequencia para String
const char* freqNames[] = {"Diaria", "Semanal", "Quinzenal", "Mensal"};
// Array de dias da semana (1=Dom, 7=Sab)
const char* dayNames[] = {"N/A", "Dom", "Seg", "Ter", "Qua", "Qui", "Sex", "Sab"};


// --- SETUP DO DISPLAY ---
void setupDisplay() {
    Wire.begin(); 

    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("Falha ao inicializar SSD1306. Verifique conexao e endereco I2C."));
        logSystemEvent("critical", "Falha na inicializacao do Display OLED.");
        return;
    }
    
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE); 
    
    // Tela de Boas-Vindas
    display.setTextSize(2); 
    display.setCursor(0, 0);
    display.println(F("Aquarium CC"));
    display.setTextSize(1);
    display.setCursor(0, 20);
    display.println(F("OLED OK."));
    display.display();
    delay(2000);
}


// --- ATUALIZAÇÃO PRINCIPAL DO DISPLAY ---
void updateDisplay() {
    display.clearDisplay(); 
    
    // 1. Renderiza a pagina atual
    switch (currentPage) {
        case 0:
            renderPage0Dashboard();
            break;
        case 1:
            renderPage1TpaSchedule(); 
            break;
        case 2:
            renderPage2TpaReposition();
            display.setTextSize(2); display.setCursor(0, 20);
            display.println(F("P2: TPA Reposicao"));
            break;
        case 3:
            renderPage3TpaBuffer();
            display.setTextSize(2); display.setCursor(0, 20);
            display.println(F("P3: Inj. Buffer"));
            break;
        default:
            renderPage0Dashboard();
            break;
    }

    // 2. Exibir o que foi desenhado no buffer
    display.display();
}


// --- PÁGINA 0: DASHBOARD ---
void renderPage0Dashboard() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    // --- LINHA 1: TÍTULO / HORA ---
    display.setCursor(0, 0);
    display.print(F("AQUARIO ACC | "));
    display.print(getCurrentTimeString());

    // --- LINHA 2: TEMPERATURA ---
    display.setCursor(0, 10);
    display.print(F("Temp: "));
    display.setTextSize(2);
    display.print(temperatureC, 1);
    display.print((char)247); // Símbolo de grau
    display.print(F("C"));
    display.setTextSize(1);
    
    // --- LINHA 3: PH ---
    display.setCursor(0, 30);
    display.print(F("pH: "));
    display.setTextSize(2);
    display.print(phValue, 2);
    display.setTextSize(1);

    // --- LINHA 3.1 STATUS E PH OFFSET (Rodapé) ---
    display.setTextSize(1);
    display.setCursor(0, 35);
    display.print(F("OFF: "));
    display.print(phCalibrationOffset, 3);


    // --- LINHA 4: STATUS TPA (NOVA IMPLEMENTAÇÃO) ---
    display.setCursor(0, 50);

    if (tpaMasterCurrentState != TPA_MASTER_IDLE && tpaMasterCurrentState != TPA_MASTER_COMPLETED) {
        // TPA em progresso (M5.1 ou M5.2)

        // 1. Determina o Status
        const __FlashStringHelper* statusText;
        
        if (tpaMasterCurrentState == TPA_MASTER_EXTRACTION_RUNNING_M51) {
            statusText = F("TPA: EXTRAINDO ");
        } else if (tpaMasterCurrentState == TPA_MASTER_REPOSITION_RUNNING_M52) {
            statusText = F("TPA: REPOSICIONANDO ");
        } else {
            statusText = F("TPA: AQUARDANDO "); // Para futuros estados
        }
        display.print(statusText);

        // 2. Calcula Volume em Tempo Real (Apenas se for Extração M5.1)
        if (tpaMasterCurrentState == TPA_MASTER_EXTRACTION_RUNNING_M51) {
            
            // Vazão em Litros/ms: (ML/s / 1000) / 1000 = L/ms
            const float FLOW_RATE_L_PER_MS = EXTRACTION_PUMP_FLOW_RATE_ML_PER_SEC / 1000000.0f; 
            
            // Tempo decorrido (em ms)
            unsigned long elapsedTimeMs = millis() - tpaExtractionStartTime;
            
            // Volume Extraído (Liters)
            float extractedVolumeL = (float)elapsedTimeMs * FLOW_RATE_L_PER_MS;

            // Garante que o volume extraído não exceda o total programado
            if (extractedVolumeL > volumeToExtractLiters) {
                extractedVolumeL = volumeToExtractLiters;
            }

            // Exibe Volume Atual / Volume Total
            display.print(extractedVolumeL, 2); // Volume extraído (xx.xx)
            display.print(F("/"));
            display.print(volumeToExtractLiters, 1); // Volume total (tt.t)
            display.print(F(" L"));

        } else if (tpaMasterCurrentState == TPA_MASTER_REPOSITION_RUNNING_M52) {
            // Para Reposição M5.2, exibir a porcentagem ou a duração restante seria mais complexo 
            // no display (que tem pouco espaço). Simplificamos para mostrar o total.
            display.print(volumeToExtractLiters, 1); 
            display.print(F(" L"));
        }
    } else {
        // TPA Parada (IDLE ou COMPLETED)
        display.print(F("TPA: PARADO"));
    }

    // Se estiver no Modo de Serviço, sobrescreve o status
    if (serviceModeActive) {
        display.setCursor(90, 50);
        display.print(F("SERVICE!")); 
    }
    // --- LINHA 5: Nível do RAN ---
    display.setCursor(0, 56); // Ultima linha
    display.print(F("RAN: "));
    display.print(ranLevelPercent);
    display.print(F("% ("));
    display.print(ranLevelFull ? F("OK") : F("BAIXO"));
    display.print(F(")"));
    
    display.display(); // Envia o buffer para o display
}


//-----------------------------------------------
// --- PÁGINA 1: TPA SCHEDULE (EXTRAÇÃO) ---
void renderPage1TpaSchedule() {
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print(F("P1: TPA Agendamento Local"));

    // Linha 1: Status Ativo (Usando o botao D para ligar/desligar)
    display.setCursor(0, 10);
    display.print(F("Status: "));
    display.print(tpaLocalScheduleActive ? F("ATIVO") : F("INATIVO"));

    // Linha 2: Volume (Informativo, não editável nesta tela)
    display.setCursor(0, 20);
    display.print(F("Vol: "));
    display.print(volumeToExtractLiters, 2);
    display.print(F(" L ("));
    display.print(tpaExtractionPercent, 1);
    display.print(F("%)"));

    // Linha 3: Dia
    display.setCursor(0, 32);
    display.print(F("Dia: "));
    display.print(dayNames[tpaScheduleDay]); 

    // Linha 4: Hora
    display.setCursor(0, 42);
    display.print(F("Hora: "));
    if (tpaScheduleHour < 10) display.print('0');
    display.print(tpaScheduleHour);
    display.print(F(":"));
    if (tpaScheduleMinute < 10) display.print('0');
    display.print(tpaScheduleMinute);

    // Linha 5: Frequencia
    display.setCursor(0, 52);
    display.print(F("Freq: "));
    display.print(freqNames[tpaScheduleFrequency]);
    
    // --- LÓGICA DE MODO DE EDICAO/SELEÇÃO (REALCE) ---
    int y_pos = 0;
    int x_start = 0;
    int width = 0;
    
    if (page1EditMode == 0) { y_pos = 32; x_start = 45; width = 30; }     // Dia
    else if (page1EditMode == 1) { y_pos = 42; x_start = 45; width = 20; } // Hora
    else if (page1EditMode == 2) { y_pos = 42; x_start = 70; width = 20; } // Minuto
    else if (page1EditMode == 3) { y_pos = 52; x_start = 45; width = 60; } // Frequencia
    
    // 1. Desenha o retângulo de realce (Inverte as cores)
    if (y_pos > 0) {
        display.fillRect(x_start, y_pos - 1, width, 9, SSD1306_WHITE); 
        
        // 2. Define a cor do texto para PRETO sobre o fundo BRANCO
        display.setTextColor(SSD1306_BLACK, SSD1306_WHITE); 
        
        // 3. Re-escreve o valor do campo sobre o realce
        display.setCursor(x_start + 1, y_pos);
        if (page1EditMode == 0) {
            display.print(dayNames[tpaScheduleDay]);
        } else if (page1EditMode == 1) {
            if (tpaScheduleHour < 10) display.print('0');
            display.print(tpaScheduleHour);
        } else if (page1EditMode == 2) {
            if (tpaScheduleMinute < 10) display.print('0');
            display.print(tpaScheduleMinute);
        } else if (page1EditMode == 3) {
            display.print(freqNames[tpaScheduleFrequency]);
        }
        
        // 4. Volta a cor do texto para BRANCO (para o restante do display)
        display.setTextColor(SSD1306_WHITE); 
    }
    
    // Indicador de Modo de Edição
    if (page1EditMode < 4) {
        display.setCursor(110, 52);
        display.print(F("EDIT"));
    } else if (page1EditMode == 4) {
        display.setCursor(110, 52);
        display.print(F("SAVE"));
    }

     display.display();
}


//-----------------------------------------------
// --- PÁGINA 2: TPA (REPOSIÇÃO) 
//-----------------------------------------------
void renderPage2TpaReposition() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    // Título da Página
    display.setCursor(0, 0);
    display.print(F("CONFIG: REPOSICAO TPA"));
    display.drawFastHLine(0, 9, 128, SSD1306_WHITE); // Linha separadora

    // --- LINHA 1: Volume Extraído (Apenas para referência) ---
    display.setCursor(0, 15);
    display.print(F("Extraido (Ref): "));
    display.print(volumeToExtractLiters, 2);
    display.print(F(" L"));

    // --- LINHA 2: Volume de Reposição (Ajustável) ---
    display.setCursor(0, 28);
    display.print(F("Volume Reposicao:"));

    // Exibição do valor ajustável
    display.setTextSize(2);
    display.setCursor(10, 40);
    display.print(volumeToRepositionLiters, 2);
    display.print(F(" L"));
    display.setTextSize(1);
    
    // Status/Instrução
    display.setCursor(0, 58);
    if (page2EditMode == 1) {
        display.print(F("UP/DOWN: Ajusta. SELECT: Salva/Sai."));
    } else {
        display.print(F("SELECT CURTO para editar volume."));
    }
    
    // --- REALCE (HIGHLIGHT) DO CAMPO DE EDIÇÃO ---
    if (page2EditMode == 1) {
        // Realça o valor ajustável
        display.fillRect(8, 39, 65, 17, SSD1306_WHITE); 
        display.setTextColor(SSD1306_BLACK, SSD1306_WHITE); 
        
        // Re-escreve o valor realçado
        display.setTextSize(2);
        display.setCursor(10, 40);
        display.print(volumeToRepositionLiters, 2);
        display.setTextSize(1); 
    }
    
    display.display();
}


//-----------------------------------------------
// --- PÁGINA 3: BUFFER 
//-----------------------------------------------
void renderPage3TpaBuffer() {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    
    // Título da Página
    display.setCursor(0, 0);
    display.print(F("CONFIG: BUFFER RAN (M5.4)"));
    display.drawFastHLine(0, 9, 128, SSD1306_WHITE); // Linha separadora

    // --- LINHA 1: Volume de Buffer (Ajustável) ---
    display.setCursor(0, 15);
    display.print(F("Volume Buffer:"));

    // Exibição do valor ajustável
    display.setTextSize(3); // Fonte maior para destaque
    display.setCursor(10, 28);
    // Renderiza o valor
    display.print(ranBufferVolumeML); 
    display.print(F(" mL"));

    // Realce no modo de edição
    if (page3EditMode == 1) { // Apenas um campo para editar
        int y_pos = 28; // Posição Y (Ajuste para o setTextSize(3))
        int x_start = 8;
        int width = 70; // Largura do campo do número

        // 1. Desenha o retângulo de realce (Inverte as cores)
        display.fillRect(x_start, y_pos, width, 26, SSD1306_WHITE); 
        
        // 2. Define a cor do texto para PRETO sobre o fundo BRANCO
        display.setTextColor(SSD1306_BLACK, SSD1306_WHITE); 
        
        // 3. Re-escreve o valor do campo sobre o realce
        display.setCursor(x_start + 2, y_pos);
        display.print(ranBufferVolumeML);
        display.print(F(" mL"));
        
        // Retorna o texto para branco
        display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
    }

    display.setTextSize(1);
    
    // Status/Instrução
    display.setCursor(0, 58);
    if (page3EditMode == 1) {
        display.print(F("UP/DOWN: Ajusta. SELECT: Salva/Sai."));
    } else {
        display.print(F("SELECT CURTO para editar volume."));
    }

    display.display();
}