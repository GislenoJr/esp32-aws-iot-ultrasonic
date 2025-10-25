# Projeto ESP32 com AWS IoT Core - Sensor Ultrassônico

## Descrição
Projeto desenvolvido para a disciplina de Sistemas Embarcados que integra um ESP32 com sensor ultrassônico HC-SR04 à AWS IoT Core para monitoramento remoto de distância.

## Funcionalidades
- Leitura de distância com sensor ultrassônico
- Transmissão de dados para AWS IoT Core
- Controle remoto de LED via MQTT
- Conexão segura com certificados TLS

## Estrutura do Projeto
MQTT_AWS/
|-- main/
|   |-- certs/          # Certificados AWS (nao versionado)
|   |-- main.c          # Codigo principal
|   `-- CMakeLists.txt
|-- CMakeLists.txt
|-- README.md
`-- .gitignore

## Tópicos MQTT
- `esp32-ultrassonico/sensor`: Dados do sensor (ESP32 → AWS)
- `esp32-ultrassonico/control`: Comandos LED (AWS → ESP32)

## Componentes
- ESP32
- Sensor Ultrassônico HC-SR04
- LED
- Resistor 220Ω

## Conexões
- Sensor TRIGGER: GPIO 12
- Sensor ECHO: GPIO 13  
- LED: GPIO 2

## Pré-requisitos
- ESP-IDF
- Conta AWS
- Certificados AWS IoT

## Configuração
1. Configurar certificados em `main/certs/`
2. Atualizar SSID e senha Wi-Fi no código
3. Compilar com `idf.py build`
4. Flashear com `idf.py flash`

## Desenvolvedores
- Gisleno Júnior - 511938
- Calebe Sucupira - 540209
- Josue Sucupira - 540630

## Disciplina
Sistemas Embarcados
