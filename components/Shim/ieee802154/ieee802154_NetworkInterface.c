/* Standard includes. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "portability/port.h"

/* FreeRTOS includes. */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

/* RINA Components includes. */
#include "Arp826.h"
// #include "ShimIPCP.h"
#include "BufferManagement.h"
#include "NetworkInterface.h"
#include "configRINA.h"
#include "configSensor.h"
#include "common/mac.h"
/* ESP includes.*/
#include "esp_ieee802154.h"
#if ESP_IDF_VERSION_MAJOR > 4
#include "esp_mac.h"
#endif
#include "esp_event.h"
#include "esp_system.h"
#include "esp_event_base.h"
#include <esp_log.h>

// #include "nvs_flash.h"

// #define WIFI_CONNECTED_BIT BIT0
// #define WIFI_FAIL_BIT BIT1

// #define MAX_STA_CONN (5)
// #define ESP_MAXIMUM_RETRY MAX_STA_CONN

/**NetworkInterfaceInput
 * NetworkInterfaceInitialise
 * NetworkInterfaceOutput
 * NetworkInterfaceDisconnect
 * NetworkInterfaceConnection
 * NetworkNotifyIFDown
 * NetworkNotifyIFUp*/

enum if_state_t
{
	DOWN = 0,
	UP,
};