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