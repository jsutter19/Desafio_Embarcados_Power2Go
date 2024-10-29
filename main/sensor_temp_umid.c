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
#include "defBasico.h"
#include "sensor_temp_umid.h"

float tempC;
float umid;
/*------------------------------------------------------------------------------------------------------*/
static const char *TAG = "Sensor Temp Umid";
/*------------------------------------------------------------------------------------------------------*/

void vDHTTask(void *pvParameter)
{
    setDHTgpio(DHT_PINOUT);
    if(DEBUG) ESP_LOGI(TAG, "Starting DHT Task");

    while (1)
    {
        int ret = readDHT();

        errorHandler(ret);

        umid = getHumidity();
        tempC = getTemperature();

        // -- wait at least 2 sec before reading again ------------
        // The interval of whole process must be beyond 2 seconds !!
        vTaskDelay(4000 / portTICK_PERIOD_MS);
    }
}