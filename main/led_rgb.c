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
#include "driver/gpio.h"
#include "driver/rmt_tx.h"

/*
 * Logs
 */
#include "esp_log.h"

/*
 * Aplicação
 */
#include "led_rgb.h"
#include "defBasico.h"

/*----------------------------------------------------------------*/
uint32_t setLEDInterval; //0 = Desligado, 1 = Ligado, 2 = Pisca 1 seg, 3 = pisca 0.3 seg
uint32_t setLEDColor; //0 = Verde, 1 = Amarelo, 2 = Vermelho, 3 = Azul
uint32_t lastSetLEDInterval;
uint32_t lastSetLEDColor;
uint32_t red;
uint32_t green;
uint32_t blue;
uint32_t ledBlinkTime;
bool stopTask;
bool bTaskInit;
/*----------------------------------------------------------------*/
static uint8_t led_strip_pixels[LED_STRIP_NUMBER * 3];
/*----------------------------------------------------------------*/
static const char *TAG = "led_rgb";
/*----------------------------------------------------------------*/
void ledState(uint32_t state);
/*----------------------------------------------------------------*/
void ledColor(uint32_t color);
/*----------------------------------------------------------------*/
void led_strip_color(uint32_t r, uint32_t g, uint32_t b);
/*----------------------------------------------------------------*/
void ledOff(void);
/*----------------------------------------------------------------*/
void ledOn(void);
/*----------------------------------------------------------------*/
void ledBlink(void *pvParameter);
/*----------------------------------------------------------------*/
void vTaskLedRGB ( void *pvParameter )
{   
    bTaskInit = false;
    lastSetLEDInterval = 3;
    lastSetLEDColor = 3;
    red = 0;
    green = 255;
    blue = 0;
    
    if(DEBUG) ESP_LOGI(TAG, "Create RMT TX channel");
    rmt_channel_handle_t led_chan = NULL;
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT, // select source clock
        .gpio_num = RMT_LED_STRIP_GPIO_NUM,
        .mem_block_symbols = 64, // increase the block size can make the LED less flickering
        .resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ,
        .trans_queue_depth = 4, // set the number of transactions that can be pending in the background
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &led_chan));

    if(DEBUG) ESP_LOGI(TAG, "Install led strip encoder");
    rmt_encoder_handle_t led_encoder = NULL;
    led_strip_encoder_config_t encoder_config = {
        .resolution = RMT_LED_STRIP_RESOLUTION_HZ,
    };
    ESP_ERROR_CHECK(rmt_new_led_strip_encoder(&encoder_config, &led_encoder));

    if(DEBUG) ESP_LOGI(TAG, "Enable RMT TX channel");
    ESP_ERROR_CHECK(rmt_enable(led_chan));

    if(DEBUG) ESP_LOGI(TAG, "Start LED");
    rmt_transmit_config_t tx_config = {
        .loop_count = 0, // no transfer loop
    };

    while(1){
        if(setLEDColor != lastSetLEDColor){
            lastSetLEDColor = setLEDColor;
            ledColor(setLEDColor);
            if(setLEDInterval==0) ledOff();
        }
        if(setLEDInterval != lastSetLEDInterval){
            lastSetLEDInterval = setLEDInterval;
            ledState(setLEDInterval);
        }
        // Flush RGB values to LEDs
        ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config));
        ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}
/*----------------------------------------------------------------*/
void ledState(uint32_t state)
{   
    uint8_t ret1;
    switch (state)
    {
    case 0:
        stopTask = true;
        bTaskInit = false;
        vTaskDelay(ledBlinkTime);
        ledOff();
        if(DEBUG) ESP_LOGI(TAG, "led off");
        break;
    case 1:
        stopTask = true;
        bTaskInit = false;
        vTaskDelay(ledBlinkTime);
        ledOn();
        if(DEBUG) ESP_LOGI(TAG, "led on");
        break;
    case 2:
        if(DEBUG) ESP_LOGI(TAG, "led Blink 1s");
        ledBlinkTime = 1000 / portTICK_PERIOD_MS;
        if(!bTaskInit) {
            ret1 = xTaskCreatePinnedToCore( ledBlink, "ledBlink", 1024*2, NULL, tskIDLE_PRIORITY, NULL, tskNO_AFFINITY );
            if(ret1 == pdTRUE) {
                bTaskInit = true;
            }
        }
        break;
    case 3:
        if(DEBUG) ESP_LOGI(TAG, "led Blink 0.3s");
        ledBlinkTime = 300 / portTICK_PERIOD_MS;
        if(!bTaskInit) {
            ret1 = xTaskCreatePinnedToCore( ledBlink, "ledBlink", 1024*2, NULL, tskIDLE_PRIORITY, NULL, tskNO_AFFINITY );
            if(ret1 == pdTRUE) {
                bTaskInit = true;
            }
        }
        break;
    }
}
/*----------------------------------------------------------------*/
void ledColor(uint32_t color)
{
    switch (color)
    {
    case 0:
        if(DEBUG) ESP_LOGI(TAG, "led Color Green");
        red = 0;
        green = 255;
        blue = 0;
        led_strip_color(red, green, blue);
        break;
    case 1:
        if(DEBUG) ESP_LOGI(TAG, "led Color Yellow");
        red = 255;
        green = 255;
        blue = 0;
        led_strip_color(red, green, blue);
        break;
    case 2:
        if(DEBUG) ESP_LOGI(TAG, "led Color Red");
        red = 255;
        green = 0;
        blue = 0;
        led_strip_color(red, green, blue);
        break;
    case 3:
        if(DEBUG) ESP_LOGI(TAG, "led Color Blue");
        red = 0;
        green = 0;
        blue = 255;
        led_strip_color(red, green, blue);
        break;
    }
}
/*----------------------------------------------------------------*/
void led_strip_color(uint32_t r, uint32_t g, uint32_t b){
    for (int j = 0; j < LED_STRIP_NUMBER; j++) {
        led_strip_pixels[j * 3 + 0] = g;
        led_strip_pixels[j * 3 + 1] = r;
        led_strip_pixels[j * 3 + 2] = b;
    }
}
/*----------------------------------------------------------------*/
void ledOff(void){
    memset(led_strip_pixels, 0, sizeof(led_strip_pixels));
}
/*----------------------------------------------------------------*/
void ledOn(void){
    led_strip_color(red, green, blue);
}
/*----------------------------------------------------------------*/
void ledBlink(void *pvParameter)
{
	int ledSts = 0;
    stopTask = false;

    if(DEBUG) ESP_LOGI(TAG, "Start ledBlink");

	while(1)
	{
        if(!ledSts) ledOff();
        else ledOn();
        ledSts = !ledSts;
        if(stopTask) break;
		vTaskDelay(ledBlinkTime);
	}
    if(DEBUG) ESP_LOGI(TAG, "Stop ledBlink");
    vTaskDelete(NULL);
}