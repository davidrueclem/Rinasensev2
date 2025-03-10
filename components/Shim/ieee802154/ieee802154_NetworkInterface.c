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
#include "BufferManagement.h"
#include "ieee802154_NetworkInterface.h"
#include "ieee802154_frame.h"
#include "configRINA.h"
#include "configSensor.h"
#include "common/mac.h"

#include "IPCP_manager.h"
#include "ieee802154_frame.h"

#include "common/rina_common.h"

/* ESP includes.*/
#include "esp_ieee802154.h"
#if ESP_IDF_VERSION_MAJOR > 4
#include "esp_mac.h"
#endif
#include "esp_event.h"
#include "esp_system.h"
#include "esp_event_base.h"
#include <esp_log.h>

/* Constants */
#define TAG_802154 "ieee802154"

enum if_state_t
{
    DOWN = 0,
    UP,
};

/* Variable State of Interface */
volatile static uint32_t xInterfaceState = DOWN;

void esp_ieee802154_receive_done(uint8_t *buffer, esp_ieee802154_frame_info_t *frame_info)
{
    if (!buffer || buffer[0] == 0)
    {
        LOGE(TAG_802154, "Received invalid packet");
        return;
    }

    LOGI(TAG_802154, "RX OK, received %d bytes", buffer[0]);

    xIeee802154NetworkInterfaceInput(&buffer[1], buffer[0], NULL);
}


bool_t xIeee802154NetworkInterfaceInitialise(MACAddress_t *pxPhyDev)
{
    LOGI(TAG_802154, "Initializing the network interface 802.15.4");

    /* Init ieee802154 interface */
    esp_ieee802154_enable();
    esp_ieee802154_set_rx_when_idle(true);
    esp_ieee802154_set_promiscuous(true);
    uint8_t ucMACAddress[MAC_ADDRESS_LENGTH_BYTES];
    esp_read_mac(ucMACAddress, ESP_MAC_IEEE802154);

    /* Reverse the MAC address */
    for (int i = 0; i < MAC_ADDRESS_LENGTH_BYTES; i++)
    {
        pxPhyDev->ucBytes[i] = ucMACAddress[MAC_ADDRESS_LENGTH_BYTES-1 - i];
    }
    esp_ieee802154_set_extended_address(pxPhyDev->ucBytes);
    esp_ieee802154_set_short_address(ieee802154_SHORT_ADDRESS);

    xInterfaceState = UP;
   
 

    return true;
}

bool_t xIeee802154NetworkInterfaceConnect(void){
    LOGI(TAG_802154, "Connecting to IEEE 802.15.4 network");

#ifdef ieee802154_COORDINATOR
    esp_ieee802154_set_coordinator(true);
#else
    esp_ieee802154_set_coordinator(false);
#endif
    esp_ieee802154_set_channel(ieee802154_CHANNEL);
    esp_ieee802154_set_panid(ieee802154_PANID_SOURCE);

    esp_ieee802154_receive();

    uint8_t extended_address[8];
    esp_ieee802154_get_extended_address(extended_address);
    LOGI(TAG_802154, "Connected: PAN ID: 0x%04x, Channel: %d, MAC: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
         esp_ieee802154_get_panid(), esp_ieee802154_get_channel(),
         extended_address[0], extended_address[1], extended_address[2], extended_address[3],
         extended_address[4], extended_address[5], extended_address[6], extended_address[7]);

    return true;
}

bool_t xIeee802154NetworkInterfaceDisconnect(void)
{
    esp_ieee802154_disable();
    LOGI(TAG_802154, "Disconnected from the IEEE 802.15.4 network");
    return true;
}


esp_err_t xIeee802154NetworkInterfaceInput(void *buffer, uint16_t len, void *eb)
{
    NetworkBufferDescriptor_t *pxNetworkBuffer = pxGetNetworkBufferWithDescriptor(len, 0);
    if (!pxNetworkBuffer)
    {
        LOGE(TAG_802154, "Failed to allocate network buffer");
        vReleaseNetworkBufferAndDescriptor(pxNetworkBuffer);
        return ESP_FAIL;
    }

    memcpy(pxNetworkBuffer->pucEthernetBuffer, buffer, len);
    pxNetworkBuffer->xEthernetDataLength = len;

    return xProcessIEEE802154Packet(pxNetworkBuffer);
}

bool_t xIeee802154NetworkInterfaceOutput(NetworkBufferDescriptor_t *const pxNetworkBuffer, bool_t xReleaseAfterSend)
{
    if (pxNetworkBuffer == NULL || pxNetworkBuffer->pucEthernetBuffer == NULL || pxNetworkBuffer->xDataLength == 0)
    {
        LOGE(TAG_802154, "Invalid parameters for network output");
        return false;
    }

    if (xInterfaceState == DOWN)
    {
        LOGI(TAG_802154, "IEEE 802.15.4 interface is down");
        return false;
    }

    LOGI(TAG_802154, "Forwarding packet to IEEE 802.15.4 frame handler");

    // Enviar los datos al Frame para ser transmitidos
    vIeee802154FrameSend(pxNetworkBuffer->pucEthernetBuffer, pxNetworkBuffer->xDataLength);

    LOGI(TAG_802154, "Packet sent successfully");

    if (xReleaseAfterSend == true)
    {
        vReleaseNetworkBufferAndDescriptor(pxNetworkBuffer);
    }

    return true;
}
