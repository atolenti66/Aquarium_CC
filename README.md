# Aquarium Command & Control (ACC)

> Um sistema de automação e monitoramento para aquários de água salgada (ou doce) baseado em ESP32, PlatformIO e Blynk.

Este projeto tem como objetivo principal automatizar tarefas críticas de manutenção de um aquário, com foco especial na **Troca Parcial de Água (TPA)**. O sistema é totalmente modular, escrito em C++ e projetado para ser gerenciado via VS Code com extensão Arduino.

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

Este projeto é desenvolvido utilizando **VS Code** com a extensão **Arduino**.

As principais bibliotecas (gerenciadas via `arduino.json`) incluem:

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
* `config.h`: **Arquivo principal de configuração.** Define pinos, senhas, chaves de API, etc.
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
2.  Abra o projeto como um projeto PlatformIO no VS Code.
3.  **Abra o arquivo `config.h`.**
4.  Preencha suas credenciais de **WiFi** (SSID e Senha).
5.  Preencha seu **Auth Token do Blynk**.
6.  Ajuste a **mapeamento de pinos** do ESP32 de acordo com o seu hardware.
7.  Ajuste as constantes do aquário (ex: `AQUARIUM_TOTAL_VOLUME`).
8.  Compile e faça o upload para o seu ESP32.

---

## ✍️ Autor

* **Alberto Tolentino**
