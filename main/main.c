#include <stdio.h>
#include "rina_api.h"
#include "nvs_flash.h"

void app_main(void)
{

    nvs_flash_init();

    RINA_init();

    vTaskDelay(1000);
}