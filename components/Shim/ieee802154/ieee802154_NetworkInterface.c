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

#include "shim.h"

#include "ieee802154_frame.h"

#include "common/rina_common.h"

/*
#include "wifi_IPCP.h" //temporal
#include "wifi_IPCP_ethernet.h"
#include "wifi_IPCP_events.h"
#include "wifi_IPCP_frames.h"*/

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

/*Variable State of Interface */
volatile static uint32_t xInterfaceState = DOWN;

static QueueHandle_t rx_queue = NULL;

static void handler_task(void *pvParameters);

static void handler_task(void *pvParameters)
{
	uint8_t packet[257];
	while (xQueueReceive(rx_queue, &packet, portMAX_DELAY) != false)
	{

		vPrintBytes((void *)&packet[1], packet[0]);
		(void)vFrameForProcessing(&packet[1], packet[0]);
	}

	ESP_LOGE("debug_handler_task", "Terminated");
	vTaskDelete(NULL);
}

bool_t xIeee802154NetworkInterfaceInitialise(MACAddress_t *pxPhyDev)
{
	LOGI(TAG_802154, "Initializing the network interface 802.15.4");
	uint8_t ucMACAddress[MAC_ADDRESS_LENGTH_BYTES];

	/*Init ieee802154 interface*/
	esp_ieee802154_enable();
	esp_ieee802154_set_rx_when_idle(true);
	esp_ieee802154_set_promiscuous(true);

	/*Energy Detection Scan, during 10 us*/
	/*esp_ieee802154_energy_detect(10);
	esp_ieee802154_energy_detect_done(ucPower);*/

	esp_read_mac(ucMACAddress, ESP_MAC_IEEE802154);
	uint8_t eui64_rev[8] = {0};
	for (int i = 0; i < 8; i++)
	{
		eui64_rev[7 - i] = ucMACAddress[i];
	}
	esp_ieee802154_set_extended_address(eui64_rev);

	esp_ieee802154_set_short_address(ieee802154_SHORT_ADDRESS);

	vARPUpdateMACAddress(eui64_rev, pxPhyDev);
	xInterfaceState = UP;

	rx_queue = xQueueCreate(8, 257);
	xTaskCreate(handler_task, "handler_task", 8192, NULL, 20, NULL);

	return true;
}

bool_t xIeee802154NetworkInterfaceConnect(void)
{
	LOGI(TAG_802154, "%s", __func__);

	/*Set type of device*/

#ifdef ieee802154_COORDINATOR
	esp_ieee802154_set_coordinator(true);
#else
	esp_ieee802154_set_coordinator(false);
#endif
	/*Set Channel*/
	esp_ieee802154_set_channel(ieee802154_CHANNEL);
	esp_ieee802154_set_panid(ieee802154_PANID_SOURCE);

	/*activating the received mode*/
	esp_ieee802154_receive();

	uint8_t extended_address[8];
	esp_ieee802154_get_extended_address(extended_address);
	LOGI(TAG_802154, "802.15.4 Ready, panId=0x%04x, channel=%d, long=%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, short=%04x",
		 esp_ieee802154_get_panid(), esp_ieee802154_get_channel(),
		 extended_address[0], extended_address[1], extended_address[2], extended_address[3],
		 extended_address[4], extended_address[5], extended_address[6], extended_address[7],
		 esp_ieee802154_get_short_address());

	return true;
}

void esp_ieee802154_receive_failed(uint16_t error)
{
	ESP_EARLY_LOGI(TAG_802154, "rx failed, error %d", error);
}

void esp_ieee802154_receive_sfd_done(void)
{
	ESP_EARLY_LOGI(TAG_802154, "rx sfd done, Radio state: %d", esp_ieee802154_get_state());
}
void esp_ieee802154_receive_done(uint8_t *buffer, esp_ieee802154_frame_info_t *frame_info)
{

	ESP_EARLY_LOGI(TAG_802154, "rx OK, received %d bytes", buffer[0]);
	BaseType_t task;
	xQueueSendToBackFromISR(rx_queue, buffer, &task);

	/*NetworkBufferDescriptor_t *pxNetworkBuffer;

	const TickType_t xDescriptorWaitTime = pdMS_TO_TICKS(0);
	struct timespec ts;

	ShimTaskEvent_t xRxEvent = {
		.eEventType = eNetworkRxEvent,
		.xData.PV = NULL};

	pxNetworkBuffer = pxFrameForProcessing(buffer[1], buffer[0]);

	if (pxNetworkBuffer != NULL)
	{

		if (xSendEventStructToShimIPCPTask(&xRxEvent, xDescriptorWaitTime) == pdFAIL)
		{
			LOGE(TAG_WIFI, "Failed to enqueue packet to network stack %p, len %d", buffer, buffer[0]);
			vReleaseNetworkBufferAndDescriptor(pxNetworkBuffer);
		}
	}

	else
	{
		LOGE(TAG_WIFI, "Failed to get buffer descriptor");
		vReleaseNetworkBufferAndDescriptor(pxNetworkBuffer);
	}*/
}

bool_t xIeee802154NetworkInterfaceOutput(NetworkBufferDescriptor_t *const pxNetworkBuffer,
										 bool_t xReleaseAfterSend)
{
	if ((pxNetworkBuffer == NULL) || (pxNetworkBuffer->pucEthernetBuffer == NULL) || (pxNetworkBuffer->xDataLength == 0))
	{
		LOGE(TAG_WIFI, "Invalid parameters");
		return false;
	}

	esp_err_t ret;

	if (xInterfaceState == DOWN)
	{
		LOGI(TAG_WIFI, "Interface down");
		ret = ESP_FAIL;
	}
	else
	{

		ret = esp_ieee802154_transmit(pxNetworkBuffer->pucEthernetBuffer, false);

		if (ret != ESP_OK)
		{
			LOGE(TAG_WIFI, "Failed to tx buffer %p, len %d, err %d", pxNetworkBuffer->pucEthernetBuffer, pxNetworkBuffer->xDataLength, ret);
		}
		else
		{
			LOGI(TAG_WIFI, "Packet Sended"); //
		}
	}

	if (xReleaseAfterSend == pdTRUE)
	{
		// LOGE(TAG_WIFI, "Releasing Buffer interface WiFi after send");

		vReleaseNetworkBufferAndDescriptor(pxNetworkBuffer);
	}

	return ret == ESP_OK ? true : false;
}