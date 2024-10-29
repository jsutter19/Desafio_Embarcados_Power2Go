# _Desafio Embarcados_

SDK-IDF: v5.3.1

Este projeto foi desenvolvido utilizando a extensão ESP-IDF no Visual Studio Code.

## Para utilizar o projeto

### Requisitos de Hardware

Este projeto foi executado utilizando:
- Placa ESP32 DEVKIT V1;
- Placa DHT22;
- Fita de Led WS2812;
- Micro Switch.

### Extensão ESP-IDF no VS Code

Para utilizar a extensão no VS Code, seguir o passo-a-passo no seguinte link: https://docs.espressif.com/projects/vscode-esp-idf-extension/en/latest/

### Compilar e Gravar

Após a instalação, em "File->Open Folder...", selecionar a pasta do projeto e clicar em "Selecionar Pasta".
Com a pasta aberta, antes de compilar, no arquivo "defBasico.h" alterar os defines ESP_WIFI_SSID e ESP_WIFI_PASS.
Selecionar a extensão "ESP-IDF" na barra lateral do VS Code, clicar em "Build" para compilar.
Após finalizar a compilação, clicar em "Select Serial Port" para selecionar a porta COM da placa ESP32. Clicar em "Flash" para iniciar a gravação.
Ao finalizar a gravação, clicar em "Monitor"para monitorar a porta serial do ESP32

## Exemplo da saída

```
I (8543) Relatorio de estados: Temp: 22.10 °C, Umid: 85.30 , Estado LED: (Cor: Verde, Estado: Desligado)
I (11543) Relatorio de estados: Temp: 22.10 °C, Umid: 85.30 , Estado LED: (Cor: Verde, Estado: Desligado)
I (14543) Relatorio de estados: Temp: 22.10 °C, Umid: 85.20 , Estado LED: (Cor: Verde, Estado: Ligado)
I (17543) Relatorio de estados: Temp: 22.10 °C, Umid: 85.20 , Estado LED: (Cor: Verde, Estado: Ligado)
I (20543) Relatorio de estados: Temp: 22.00 °C, Umid: 85.20 , Estado LED: (Cor: Verde, Estado: Ligado)
I (23543) Relatorio de estados: Temp: 22.10 °C, Umid: 85.30 , Estado LED: (Cor: Amarelo, Estado: Ligado)
I (26543) Relatorio de estados: Temp: 22.10 °C, Umid: 85.30 , Estado LED: (Cor: Amarelo, Estado: Ligado)
I (29543) Relatorio de estados: Temp: 22.10 °C, Umid: 85.30 , Estado LED: (Cor: Amarelo, Estado: Ligado)
I (32543) Relatorio de estados: Temp: 22.10 °C, Umid: 85.20 , Estado LED: (Cor: Amarelo, Estado: Ligado)
```
