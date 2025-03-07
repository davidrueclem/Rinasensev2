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

    portId_t xAppPortId;
    struct rinaFlowSpec_t *xFlowSpec = pvPortMalloc(sizeof(*xFlowSpec));
    uint8_t Flags = 1;
    int32_t xBytes;
    int i = 0;
    char json[200];
    void *buffer;
    size_t xLenBuffer = 1024;
    char *data;

    buffer = pvPortMalloc(xLenBuffer);

    memset(buffer, 0, xLenBuffer);

    vTaskDelay(1000);

    ESP_LOGI(TAG_APP, "----------- Requesting a Flow ----- ");

    xAppPortId = RINA_flow_alloc("slice1.DIF", "STH1", "sensor1", xFlowSpec, Flags);

    ESP_LOGI(TAG_APP, "Flow Port id: %d ", xAppPortId);
    if (xAppPortId != -1)
    {

        while (i < 100)
        {

            // ESP_LOGI(TAG_APP, "Temperature: 30 C");

            sprintf(json, "Temperature: 30 C\n");

            ESP_LOGI(TAG_APP, "json:%s", json);
            if (RINA_flow_write(xAppPortId, (void *)json, strlen(json)))
            {
                ESP_LOGI(TAG_APP, "Sent Data successfully");
            }

            vTaskDelay(8000 / portTICK_RATE_MS);

            i = i + 1;
        }
    }
}