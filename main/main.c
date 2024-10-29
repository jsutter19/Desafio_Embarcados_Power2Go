/* 
 * Autor: Jhony Sutter
 * SDK-IDF: v5.3.1
 * Desafio Técnico Power2Go
 */

/*
 * C library
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

/*
 * FreeRTOS
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

/*
 * ESP32
 */
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_freertos_hooks.h"
#include "esp_spiffs.h"
#include "esp_event.h"

/*
 * Drivers
 */
#include "nvs_flash.h"
#include "driver/gpio.h"

/*
 * WiFi
 */
#include "esp_wifi.h"
#include "esp_private/wifi.h"

/*
 * LWIP
 */
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "lwip/err.h"
#include "lwip/sys.h"

/*
 * Logs
 */
#include "esp_log.h"

/*
 * Aplicação
 */
#include "defBasico.h"
#include "cJSON.h"
#include "sensor_temp_umid.h"
#include "led_rgb.h"
#include "mqtt_client.h"

#define BT_SEL	GPIO_NUM_13
#define GPIO_INPUT_PIN_SEL  ((1ULL<<BT_SEL))
/*------------------------------------------------------------------------------------------------------*/
extern uint32_t setLEDInterval; //0 = Desligado, 1 = Ligado, 2 = Pisca 1 seg, 3 = pisca 0.3 seg
extern uint32_t setLEDColor; //0 = Verde, 1 = Amarelo, 2 = Vermelho, 3 = Azul
extern float tempC;
extern float umid;
/*------------------------------------------------------------------------------------------------------*/
static const char *TAG = "MAIN";
static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;
static EventGroupHandle_t s_mqtt_event_group;
esp_mqtt_client_handle_t client;
/*------------------------------------------------------------------------------------------------------*/
TickType_t xBtCountTime;
uint32_t btSelect;
EventBits_t uxBits;
bool stsBt;
/*------------------------------------------------------------------------------------------------------*/
/**
 * Protótipo de Função;
 */
/*------------------------------------------------------------------------------------------------------*/
static void prvSetupHardware( void );
/*------------------------------------------------------------------------------------------------------*/
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
/*------------------------------------------------------------------------------------------------------*/
void wifi_init_sta(void);
/*------------------------------------------------------------------------------------------------------*/
static void log_error_if_nonzero(const char *message, int error_code);
/*------------------------------------------------------------------------------------------------------*/
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
/*------------------------------------------------------------------------------------------------------*/
static void mqtt_app_start(void);
/*------------------------------------------------------------------------------------------------------*/
void vTaskBT( void *pvParameter);
/*------------------------------------------------------------------------------------------------------*/
void monitoring_task(void *pvParameter);
/*------------------------------------------------------------------------------------------------------*/
void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    setLEDInterval = 0;
    setLEDColor = 0;
    xBtCountTime = 0;

    prvSetupHardware();

    wifi_init_sta();

    mqtt_app_start();

    xTaskCreatePinnedToCore( vTaskBT, "vTaskBT", 1024, NULL, (tskIDLE_PRIORITY + 1) , NULL, CORE_0 );
    xTaskCreatePinnedToCore( vDHTTask, "vDHTTask", 1024*2, NULL, (tskIDLE_PRIORITY + 2) , NULL, CORE_1 );
    xTaskCreatePinnedToCore( vTaskLedRGB, "vTaskLedRGB", 1024*3, NULL, (tskIDLE_PRIORITY + 2) , NULL, CORE_1 );
    uxBits = xEventGroupWaitBits(s_mqtt_event_group, MQTT_CONNECTED_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(240000));
    if((uxBits & MQTT_CONNECTED_BIT)!=0){
        xTaskCreatePinnedToCore( monitoring_task, "monitoring_task", 1024*4, NULL, (tskIDLE_PRIORITY + 3), NULL, CORE_0 );
    }
}
/*------------------------------------------------------------------------------------------------------*/
static void IRAM_ATTR gpio_isr_handler(void* arg){
    /**
     * Verifica qual foi o botão pressionado;
     */
    if( BT_SEL == (uint32_t) arg ){
        
		if( gpio_get_level( (uint32_t) arg ) == 0){
            btSelect = 1;
		}else{
            stsBt = false;
            btSelect = 0;
        }
	}
}
/*------------------------------------------------------------------------------------------------------*/
static void prvSetupHardware( void ){
    gpio_config_t io_conf = { //Cria a variável descritora para o drive GPIO.
        //Configura o descritor das Inputs	
        .intr_type = GPIO_INTR_ANYEDGE, //interrupção externa da(s) GPIO(s) habilitada e configurada para disparo na descida.
        .mode = GPIO_MODE_INPUT,  //Configura como entrada. 
        .pin_bit_mask = GPIO_INPUT_PIN_SEL, //Informa quais os pinos que serão configurados no drive. 
        .pull_down_en = GPIO_PULLDOWN_DISABLE, //ou  GPIO_PULLDOWN_ENABLE
        .pull_up_en = GPIO_PULLUP_ENABLE,  //ou GPIO_PULLUP_DISABLE
    };
    gpio_config(&io_conf); //Configura a(s) GPIO's conforme configuração do descritor.

    /**
     * Habilita a chave geral das interrupções das GPIO's; 
     */
    gpio_install_isr_service(0);

    /**
     * Registra a função de callback da ISR (Interrupção Externa);
     * O número da GPIO é passada como parâmetro para a função de callback, 
     * pois assim, é possível saber qual foi o botão pressionado;
     */
    gpio_isr_handler_add( BT_SEL, gpio_isr_handler, (void*) BT_SEL );
}
/*------------------------------------------------------------------------------------------------------*/
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            if(DEBUG) ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        if(DEBUG) ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        if(DEBUG) ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}
/*------------------------------------------------------------------------------------------------------*/
void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = ESP_WIFI_SSID,
            .password = ESP_WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    if(DEBUG) ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        if(DEBUG) ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", ESP_WIFI_SSID, ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        if(DEBUG) ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s", ESP_WIFI_SSID, ESP_WIFI_PASS);
    } else {
        if(DEBUG) ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}
/*------------------------------------------------------------------------------------------------------*/
static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
       if(DEBUG) ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}
/*------------------------------------------------------------------------------------------------------*/
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    client = event->client;
    int msg_id;
    char szTemp[256];
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        if(DEBUG) ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_subscribe(client, "/rel_estados/", 0);
        if(DEBUG) ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        msg_id = esp_mqtt_client_subscribe(client, "/set_estado_led/", 0);
        if(DEBUG) ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        xEventGroupSetBits(s_mqtt_event_group, MQTT_CONNECTED_BIT);
        break;
    case MQTT_EVENT_DISCONNECTED:
        if(DEBUG) ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        xEventGroupClearBits(s_mqtt_event_group, MQTT_CONNECTED_BIT);
        break;

    case MQTT_EVENT_SUBSCRIBED:
        if(DEBUG) ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        if(DEBUG) ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        if(DEBUG) ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        if(DEBUG) {
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
        }
        sprintf(szTemp,"%.*s",event->topic_len, event->topic);
        if(strncmp((char*)&szTemp, "/set_estado_led/", strlen("/set_estado_led/")) == 0){
            sprintf(szTemp,"%.*s",event->data_len, event->data);

            if(DEBUG) ESP_LOGI(TAG, "DATA=%.*s",event->data_len,szTemp);

            cJSON *getJson = cJSON_Parse(szTemp);
            cJSON *pszLedColor = cJSON_GetObjectItem(getJson,"cor_led");
            cJSON *pszLedSts = cJSON_GetObjectItem(getJson,"estado_led");

            setLEDColor = cJSON_GetNumberValue(pszLedColor);
            setLEDInterval = cJSON_GetNumberValue(pszLedSts);

            if(DEBUG) ESP_LOGI(TAG, "setLEDColor=%ld, setLEDInterval=%ld",setLEDColor,setLEDInterval);

            cJSON_Delete(getJson);

        }
        break;
    case MQTT_EVENT_ERROR:
        if(DEBUG) ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            if(DEBUG) ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        if(DEBUG) ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}
/*------------------------------------------------------------------------------------------------------*/
static void mqtt_app_start(void)
{
    s_mqtt_event_group = xEventGroupCreate();
    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.hostname = CONFIG_BROKER_HOSTNAME,
        .broker.address.port = CONFIG_BROKER_PORT,
        .broker.address.transport = MQTT_TRANSPORT_OVER_TCP,
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}
/*------------------------------------------------------------------------------------------------------*/
void vTaskBT( void *pvParameter )
{
    stsBt = false;
    xBtCountTime = 0;
    while(1){
        while (btSelect){
            xBtCountTime += pdMS_TO_TICKS(10);
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        
        if(xBtCountTime != 0 && xBtCountTime >= pdMS_TO_TICKS(1500)){
            xBtCountTime = 0;
            if(!stsBt) {
                stsBt = true;
                setLEDColor++;
            }
            if(setLEDColor > 3){
                setLEDColor = 0;
            }
        } 
        else if(xBtCountTime != 0 && xBtCountTime <= pdMS_TO_TICKS(1000)){
            xBtCountTime = 0;
            if(!stsBt) {
                stsBt = true;
                setLEDInterval++;
            }
            if(setLEDInterval > 3){
                setLEDInterval = 0;
            }

        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}
/*------------------------------------------------------------------------------------------------------*/
/*
 * Esta task é responsável em apresentar o valor em bytes do head.
 * O heap precisa ser monitorado em busca de possíveis falhas no programa
 * provocados por memory leak.
 */
void monitoring_task(void *pvParameter){
    char strLedColor[16];
    char strLedStatus[16];
    int msg_id;
    while(1){
        //ESP_LOGI( TAG,"free heap: %ld",esp_get_free_heap_size() );
        switch (setLEDInterval)
        {
        case 0:
            sprintf(strLedStatus, "Desligado");
            break;
        case 1:
            sprintf(strLedStatus, "Ligado");
            break;
        case 2:
            sprintf(strLedStatus, "Pisca 1s");
            break;
        case 3:
            sprintf(strLedStatus, "Pisca 0.3s");
            break;
        }
        switch (setLEDColor)
        {
        case 0:
            sprintf(strLedColor, "Verde");
            break;
        case 1:
            sprintf(strLedColor, "Amarelo");
            break;
        case 2:
            sprintf(strLedColor, "Vermelho");
            break;
        case 3:
            sprintf(strLedColor, "Azul");
            break;
        }
        
        ESP_LOGI( "Relatorio de estados","Temp: %0.2f °C, Umid: %0.2f , Estado LED: (Cor: %s, Estado: %s)",tempC,umid,strLedColor,strLedStatus );

        if((uxBits & MQTT_CONNECTED_BIT) != 0){
            cJSON *root = NULL;
            root=cJSON_CreateObject();
            cJSON_AddItemToObject(root, "temperatura", cJSON_CreateNumber(tempC*100));
            cJSON_AddItemToObject(root, "umidade", cJSON_CreateNumber(umid*100));
            cJSON_AddItemToObject(root, "cor_led", cJSON_CreateNumber(setLEDColor));
            cJSON_AddItemToObject(root, "estado_led", cJSON_CreateNumber(setLEDInterval));
            char *rendered = cJSON_Print(root);
            cJSON_Delete(root);
            msg_id = esp_mqtt_client_publish(client, "/rel_estados/", rendered, 0, 0, 0);
            if(DEBUG) ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        }
        vTaskDelay( 3000 / portTICK_PERIOD_MS );
    }
}
/*------------------------------------------------------------------------------------------------------*/