#ifndef SENSOR_TEMP_UMID_H
#define SENSOR_TEMP_UMID_H

#include "DHT.h"

#define DHT_PINOUT GPIO_NUM_4


/*----------------------------------------------------------------*
 * Prototipos de função
 *----------------------------------------------------------------*/
void vDHTTask(void *pvParameter);
/*----------------------------------------------------------------*/

#endif