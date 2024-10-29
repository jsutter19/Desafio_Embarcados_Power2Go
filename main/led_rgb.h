#ifndef LED_RGB_H
#define LED_RGB_H

#include "led_strip_encoder.h"

#define RMT_LED_STRIP_RESOLUTION_HZ 10000000 // 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
#define RMT_LED_STRIP_GPIO_NUM      2
#define LED_STRIP_NUMBER            2

/*----------------------------------------------------------------*
 * Prototipos de função
 *----------------------------------------------------------------*/
void vTaskLedRGB ( void *pvParameter );
/*----------------------------------------------------------------*/

#endif