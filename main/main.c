#include <stdio.h>
#include <math.h>
#include <string.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "mqtt_client.h"

// ============================================================================
// CONFIGURAÇÕES DE TÓPICOS AWS IOT CORE
// ============================================================================

// TÓPICOS PRINCIPAIS 
#define TOPICO_SENSOR_DADOS    "esp32-ultrassonico/sensor"   
#define TOPICO_CONTROLE_LED    "esp32-ultrassonico/control"  

// ============================================================================
// CONFIGURAÇÕES DE REDE E DISPOSITIVO
// ============================================================================

// Wi-Fi
#define WIFI_SSID "teste"
#define WIFI_PASS "12345678"

// Ultrassônico
#define TRIGGER_GPIO 12
#define ECHO_GPIO    13
#define TIMEOUT_US   30000
#define DIST_MAX_CM  50.0
#define NUM_MEDIDAS_VALIDAS 5

// LED
#define LED_GPIO     2

// AWS IoT Core
#define AWS_IOT_ENDPOINT "a2w49l6f3ulfvn-ats.iot.us-east-2.amazonaws.com"
#define AWS_IOT_CLIENT_ID "esp32-ultrassonico"

// ============================================================================
// CERTIFICADOS AWS (arquivos externos)
// ============================================================================

extern const char aws_root_ca_pem_start[] asm("_binary_aws_root_ca_pem_start");
extern const char certificate_pem_crt_start[] asm("_binary_certificate_pem_crt_start");
extern const char private_pem_key_start[] asm("_binary_private_pem_key_start");

// ============================================================================
// VARIÁVEIS GLOBAIS
// ============================================================================

static esp_mqtt_client_handle_t mqtt_client = NULL;
static bool mqtt_connected = false;

// Eventos Wi-Fi
#define WIFI_CONNECTED_BIT BIT0
static EventGroupHandle_t wifi_event_group;

// TAGs
static const char *TAG_ULTRA = "ULTRASONIC";
static const char *TAG_MQTT = "MQTT_AWS";
static const char *TAG_WIFI = "WIFI";

// ============================================================================
// FUNÇÃO DE DELAY SIMPLES
// ============================================================================

static void delay_us(uint32_t us) {
    uint32_t start = esp_timer_get_time();
    while (esp_timer_get_time() - start < us) {
        // Busy wait
    }
}

// ============================================================================
// SENSOR ULTRASSÔNICO
// ============================================================================

void ultrasonic_init() {
    gpio_reset_pin(TRIGGER_GPIO);
    gpio_reset_pin(ECHO_GPIO);
    gpio_set_direction(TRIGGER_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(ECHO_GPIO, GPIO_MODE_INPUT);
    gpio_set_level(TRIGGER_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
}

float ultrasonic_read_cm() {
    int64_t start_time = 0, end_time = 0;

    gpio_set_level(TRIGGER_GPIO, 0);
    delay_us(2);
    gpio_set_level(TRIGGER_GPIO, 1);
    delay_us(15);
    gpio_set_level(TRIGGER_GPIO, 0);

    int64_t timeout = esp_timer_get_time() + TIMEOUT_US;
    while (gpio_get_level(ECHO_GPIO) == 0) {
        if (esp_timer_get_time() > timeout) return NAN;
    }
    start_time = esp_timer_get_time();

    while (gpio_get_level(ECHO_GPIO) == 1) {
        end_time = esp_timer_get_time();
        if (end_time - start_time > TIMEOUT_US) return NAN;
    }

    float distance_cm = (end_time - start_time) / 58.0f;
    if (distance_cm < 0.5 || distance_cm > DIST_MAX_CM) return NAN;
    return distance_cm;
}

float ultrasonic_get_avg_cm(int tentativas_max) {
    float soma = 0.0;
    int validas = 0, tentativas = 0;

    while (validas < NUM_MEDIDAS_VALIDAS && tentativas < tentativas_max) {
        float d = ultrasonic_read_cm();
        if (!isnan(d)) {
            soma += d;
            validas++;
        }
        tentativas++;
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    return (validas == 0) ? NAN : soma / validas;
}

void ultrasonic_task(void *pvParam) {
    ultrasonic_init();

    while (1) {
        float distancia = ultrasonic_get_avg_cm(10);
        if (!isnan(distancia)) {
            ESP_LOGI(TAG_ULTRA, "Distância: %.2f cm", distancia);
            if (mqtt_client && mqtt_connected) {
                char msg[100];
                snprintf(msg, sizeof(msg), 
                         "{\"device\":\"%s\",\"distance_cm\":%.2f,\"timestamp\":%lld}",
                         AWS_IOT_CLIENT_ID, distancia, esp_timer_get_time());
                
                //  PUBLICANDO NO TÓPICO DE DADOS DO SENSOR 
                int msg_id = esp_mqtt_client_publish(mqtt_client, TOPICO_SENSOR_DADOS, msg, 0, 1, 0);
                if (msg_id == -1) {
                    ESP_LOGE(TAG_ULTRA, "Falha ao publicar mensagem AWS IoT");
                } else {
                    ESP_LOGI(TAG_ULTRA, "Mensagem AWS publicada (ID: %d) no tópico: %s", msg_id, TOPICO_SENSOR_DADOS);
                }
            } else {
                ESP_LOGW(TAG_ULTRA, "AWS IoT não conectado, ignorando publicação");
            }
        } else {
            ESP_LOGW(TAG_ULTRA, "Medição inválida.");
        }
        vTaskDelay(pdMS_TO_TICKS(2000)); // 2 segundos entre medições
    }
}

// ============================================================================
// MQTT AWS IOT CORE
// ============================================================================

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;

    switch (event->event_id) {
        case MQTT_EVENT_BEFORE_CONNECT:
            ESP_LOGI(TAG_MQTT, "Conectando ao AWS IoT Core...");
            mqtt_connected = false;
            break;

        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG_MQTT, "✅ Conectado ao AWS IoT Core!");
            mqtt_connected = true;
            
            // INSCREVENDO NO TÓPICO DE CONTROLE DO LED 
            int msg_id = esp_mqtt_client_subscribe(mqtt_client, TOPICO_CONTROLE_LED, 1);
            ESP_LOGI(TAG_MQTT, "Inscrito no tópico: %s (ID: %d)", TOPICO_CONTROLE_LED, msg_id);
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG_MQTT, "Desconectado do AWS IoT Core");
            mqtt_connected = false;
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG_MQTT, "📨 Mensagem recebida - Tópico: %.*s | Mensagem: %.*s",
                     event->topic_len, event->topic,
                     event->data_len, event->data);

            // 📥📥📥 VERIFICANDO SE É O TÓPICO DE CONTROLE DO LED 📥📥📥
            if (strncmp(event->topic, TOPICO_CONTROLE_LED, event->topic_len) == 0) {
                if (strncmp(event->data, "on", event->data_len) == 0) {
                    gpio_set_level(LED_GPIO, 1);
                    ESP_LOGI(TAG_MQTT, "💡 LED ligado via AWS IoT (tópico: %s)", TOPICO_CONTROLE_LED);
                } else if (strncmp(event->data, "off", event->data_len) == 0) {
                    gpio_set_level(LED_GPIO, 0);
                    ESP_LOGI(TAG_MQTT, "💡 LED desligado via AWS IoT (tópico: %s)", TOPICO_CONTROLE_LED);
                }
            }
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG_MQTT, "❌ Erro AWS IoT Core: %s", 
                     esp_err_to_name(event->error_handle->esp_tls_last_esp_err));
            mqtt_connected = false;
            break;

        default:
            break;
    }
}

void mqtt_app_start() {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address.uri = "mqtts://" AWS_IOT_ENDPOINT ":8883",
            .verification.certificate = aws_root_ca_pem_start,
        },
        .credentials = {
            .authentication = {
                .certificate = certificate_pem_crt_start,
                .key = private_pem_key_start,
            },
            .client_id = AWS_IOT_CLIENT_ID,
        },
        .session = {
            .keepalive = 60,
            .disable_clean_session = false,
        },
        .network = {
            .reconnect_timeout_ms = 10000,
            .disable_auto_reconnect = false,
        }
    };

    ESP_LOGI(TAG_MQTT, "Inicializando cliente MQTT...");
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
}

// ============================================================================
// WI-FI
// ============================================================================

void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        ESP_LOGI(TAG_WIFI, "Conectando ao Wi-Fi...");
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG_WIFI, "Wi-Fi desconectado, tentando reconectar...");
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) data;
        ESP_LOGI(TAG_WIFI, "✅ Conectado ao Wi-Fi! IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init() {
    wifi_event_group = xEventGroupCreate();
    
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG_WIFI, "Aguardando conexão Wi-Fi...");
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);
}

// ============================================================================
// APP MAIN
// ============================================================================

void app_main() {
    ESP_LOGI("APP", "🚀 Iniciando aplicação ESP32 + AWS IoT Core");
    ESP_LOGI("APP", "📤 Tópico SENSOR: %s", TOPICO_SENSOR_DADOS);
    ESP_LOGI("APP", "📥 Tópico CONTROLE: %s", TOPICO_CONTROLE_LED);
    ESP_LOGI("APP", "🆔 Client ID: %s", AWS_IOT_CLIENT_ID);
    
    // Inicializa NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Configura LED
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_GPIO, 0);

    // Inicializa Wi-Fi
    wifi_init();
    
    // Aguarda um pouco antes de conectar MQTT
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Inicializa MQTT
    mqtt_app_start();
    
    // Aguarda mais um pouco antes de iniciar sensor
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // Inicia task do sensor
    xTaskCreate(ultrasonic_task, "ultrasonic_task", 4096, NULL, 5, NULL);
    
    ESP_LOGI("APP", "✅ Aplicação inicializada com sucesso!");
    ESP_LOGI("APP", "👉 Para ver dados: Inscreva-se no tópico: %s", TOPICO_SENSOR_DADOS);
    ESP_LOGI("APP", "👉 Para controlar LED: Publique no tópico: %s", TOPICO_CONTROLE_LED);
}