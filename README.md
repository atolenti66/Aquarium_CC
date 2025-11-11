# Aquarium Command & Control (ACC)

> Um sistema de automação e monitoramento para aquários de água salgada (ou doce) baseado em ESP32, **VS Code (com extensão Arduino)** e Blynk.

Este projeto tem como objetivo principal automatizar tarefas críticas de manutenção de um aquário, com foco especial na **Troca Parcial de Água (TPA)**. O sistema é totalmente modular, escrito em C++ e projetado para ser gerenciado via VS Code com a extensão Arduino.

---

## 🐠 Principais Funcionalidades

* **Monitoramento de Sensores:**
    * Leitura de **Temperatura** da água (DS18B20).
    * Leitura de **pH** com calibração (Sonda de pH analógica).
    * Monitoramento de falha de energia do RTC (flag OSF).
* **Automação da TPA (Troca Parcial de Água):**
    * Máquina de estados complexa para gerenciar a extração, reposição e dosagem de buffer.
    * Agendamento local (baseado em dia/hora) ou acionamento manual via Blynk.
    * Cálculo de volumes baseado na percentagem de troca e volume total do aquário.
* **Controle de Atuadores:**
    * Gerenciamento de bombas peristálticas (Extração TPA, Reposição TPA, Buffer).
    * Controle de válvulas solenóide (Enchimento do reservatório RAN).
    * Lógica de segurança para evitar operação a seco (sensores de nível).
* **Interface e Conectividade:**
    * Integração total com a plataforma **Blynk IoT** para monitoramento e controle remoto.
    * Display **OLED (SSD1306)** local com paginação para visualização de status.
    * Interface de **botões físicos** (Button2) para navegação no display e acionamento de funções (Modo Serviço, Reset de Alerta, etc.).
* **Gestão de Sistema:**
    * Persistência de configurações (calibração de pH, volumes de TPA) no **LittleFS** (memória flash).
    * Gerenciamento de tempo preciso com **RTC (DS3231)** e sincronização via **NTP**.
    * **Modo de Serviço** que desabilita todos os atuadores para manutenção segura.

---

## 🔩 Hardware (Componentes Principais)

* **Controlador:** ESP32 (qualquer variante, ex: NodeMCU-32S)
* **Sensores:**
    * Sonda de pH (com módulo amplificador)
    * Sensor de Temperatura (DS18B20, à prova d'água)
    * Módulo RTC (DS3231)
* **Display:** OLED 0.96" I2C (SSD1306)
* **Atuadores:**
    * Bombas peristálticas (12V ou 24V)
    * Válvulas solenóide (para água)
    * Módulos de Relé (para acionar as bombas e válvulas)
* **Interface:** Botões (Push-buttons)

---

## 📚 Software e Bibliotecas

Este projeto é desenvolvido utilizando **VS Code** com a extensão **[Arduino for VS Code](https://marketplace.visualstudio.com/items?itemName=vsciot-vscode.vscode-arduino)** (Arduino Community Edition).

As principais bibliotecas são gerenciadas através do **Gerenciador de Bibliotecas** da própria extensão Arduino (acessado via `Ctrl+Shift+P` > "Arduino: Library Manager") e incluem:

* `Blynk` (para conectividade IoT)
* `RTClib` (para o DS3231)
* `OneWire` e `DallasTemperature` (para o DS18B20)
* `Adafruit_GFX` e `Adafruit_SSD1306` (para o display OLED)
* `ArduinoJson` (para salvar e ler configurações)
* `Button2` (para gerenciamento avançado de botões)
* `Ticker` (para tarefas agendadas não bloqueantes)

---

## 🗂️ Estrutura do Projeto

O código é 100% modular para facilitar a manutenção e a depuração. Cada "responsabilidade" do sistema está em seu próprio par de arquivos `.cpp` / `.h`.

* `main.cpp`: Ponto de entrada, `setup()` e `loop()` principal.
* `config.h`: **Arquivo principal de configuração.** Define pinos, constantes do aquário, etc.
* `secrets.h`: (**Recomendado/Ignorado pelo Git**) Armazena senhas de WiFi e tokens de API.
* `global.h`: Declaração de variáveis globais (`extern`) e `enum`s.
* `utils.h`: Protótipos de funções de utilidade (ex: `logSystemEvent`).
* `config_manager.cpp`: Gerencia a leitura e escrita de JSON no LittleFS.
* `display_manager.cpp`: Controla toda a lógica de renderização no OLED.
* `hardware_manager.cpp`: Gerencia a leitura de todos os botões físicos.
* `rtc_time.cpp`: Gerencia o RTC, o tempo e a sincronização NTP.
* `sensors.cpp`: Faz a leitura do sensor de temperatura (DS18B20).
* `ph_sensor.cpp`: Faz a leitura e calibração do sensor de pH.
* `actuators_manager.cpp`: Lógica de baixo nível para ligar/desligar relés (bombas, válvulas).
* `tpa_manager.cpp`: Orquestrador principal da TPA (máquina de estados).
* `tpa_reposition.cpp`: Sub-módulo da TPA focado na reposição.

---

## ⚙️ Configuração

1.  Clone este repositório.
2.  Abra a pasta do projeto no VS Code (`Arquivo > Abrir Pasta...`).
3.  Certifique-se de ter a extensão **"Arduino for VS Code"** instalada e o **Arduino IDE** (preferencialmente 2.x) instalado (a extensão o utiliza nos bastidores).
4.  No canto inferior direito da barra de status do VS Code, configure a extensão:
    * **Arquivo Principal:** Selecione `main.cpp`.
    * **Placa:** Selecione sua placa (ex: `ESP32 Dev Module`).
    * **Porta:** Selecione a porta COM onde o ESP32 está conectado.
5.  Use o Gerenciador de Bibliotecas (`Ctrl+Shift+P` > "Arduino: Library Manager") para instalar as bibliotecas listadas acima.
6.  **Crie o arquivo `secrets.h`** (baseado no `secrets.h.example`, se houver) na mesma pasta do `config.h`.
7.  Preencha o `secrets.h` com suas credenciais de **WiFi** (SSID e Senha) e seu **Auth Token do Blynk**.
8.  Ajuste o arquivo **`config.h`** com o mapeamento de pinos do seu hardware e as constantes do aquário (ex: `AQUARIUM_TOTAL_VOLUME`).
9.  Use os ícones no canto superior direito do VS Code para **Verificar** (compilar) e **Fazer Upload**.
