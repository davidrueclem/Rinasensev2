/**
 * @file main.c
 * @author David Sarabia (david.sarabia@i2cat.net)
 * @brief Dummy application that writes a json file into the RINA flow. This json file is constant and aims
 * to emulate the json file made from the DHT sensor. This dummy application aims to test the RINA_flow_write
 * and the RINA_flow_read APIs.
 *
 * @version 0.1
 * @date 2022-07-25
 *
 * @copyright Copyright (c) 2022
 *
 */
#include "freertos/FreeRTOS.h"
#include <string.h>

#include "configRINA.h"

#include "RINA_API.h"

#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

#define TAG_APP "[Sensor-APP]"

void app_main(void)
{
    nvs_flash_init();
    /*
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);*/

    RINA_init();

    // All done, the rest is up to handlers
    while (true)
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}