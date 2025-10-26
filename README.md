# Projeto ESP32 com AWS IoT Core - Sensor Ultrassônico

## Descrição
Projeto desenvolvido para a disciplina de Sistemas Embarcados que integra um ESP32 com sensor ultrassônico HC-SR04 à AWS IoT Core para monitoramento remoto de distância.

## Funcionalidades
- Leitura de distância com sensor ultrassônico
- Transmissão de dados para AWS IoT Core
- Controle remoto de LED via MQTT
- Conexão segura com certificados TLS

cat > README.md << 'EOF'
# Projeto ESP32 com AWS IoT Core - Sensor Ultrassônico

## Descrição
Projeto ESP32 com sensor ultrassônico HC-SR04 conectado à AWS IoT Core.

## Funcionalidades
- Leitura de distância com sensor ultrassônico
- Transmissão de dados para AWS IoT Core
- Controle remoto de LED via MQTT

## Estrutura do Projeto
MQTT_AWS/
├── main/
│   ├── certs/
│   ├── main.c
│   └── CMakeLists.txt
├── CMakeLists.txt
├── README.md
└── .gitignore

## Tópicos MQTT
- esp32-ultrassonico/sensor: Dados do sensor
- esp32-ultrassonico/control: Comandos LED

## Componentes
- ESP32
- Sensor Ultrassônico HC-SR04
- LED
- Resistor 220Ω

## Conexões
- Sensor TRIGGER: GPIO 12
- Sensor ECHO: GPIO 13
- LED: GPIO 2

## Configuração AWS IoT

### 1. Criar Thing
- Acesse AWS IoT Console
- Manage -> Things -> Create things
- Nome: esp32-ultrassonico

### 2. Gerar Certificados
- Auto-generate a new certificate
- Baixe: certificate.pem.crt, private.pem.key, Amazon Root CA 1

### 3. Criar Política
```json
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Effect": "Allow",
      "Action": ["iot:Publish","iot:Receive","iot:Subscribe","iot:Connect"],
      "Resource": ["*"]
    }
  ]
}

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
