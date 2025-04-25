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

#include "IPCP_api.h"
#include "IPCP_events.h"
#include "esp_event.h"
#include "esp_system.h"
#include "esp_event_base.h"
#include <esp_log.h>

/* Constants */
// #define TAG_802154 "ieee802154"

enum if_state_t
{
    DOWN = 0,
    UP,
};

#define ASSOCIATION_EVENT (1 << 0) // Define el bit para la asociación
#define IEEE802154_EVENT "IEEE802154_EVENT"
#define IEEE802154_ASSOCIATION_REQUEST 0x0001
#define IEEE802154_ASSOCIATION_RESPONSE 0x0002

#define IEEE802154_MAX_FRAME_SIZE 127

static uint8_t rx_temp_buffer[IEEE802154_MAX_FRAME_SIZE];
static uint16_t rx_temp_len = 0;
static TaskHandle_t xShimRxTaskHandle = NULL;
void vShimRxTask(void *pvParameters);


static EventGroupHandle_t s_wifi_event_group;

void reply_event_handler(void *arg)
{
    // Procesar la respuesta de asociación
    uint8_t *buffer = (uint8_t *)arg;

    LOGE(TAG_802154, "Respuesta asociación recibida");

    xEventGroupSetBits(s_wifi_event_group, ASSOCIATION_EVENT);

    /*
        if (is_association_response(buffer))
        {
            // La respuesta de asociación ha sido recibida
            xEventGroupSetBits(s_wifi_event_group, ASSOCIATION_EVENT);
        }
            */
}

/*static void request_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    LOGE(TAG_802154, "Petición asociación recibida");
    // Si el evento es una solicitud de asociación (Association Request)
    if (strcmp(event_base, IEEE802154_EVENT) == 0 && event_id == IEEE802154_ASSOCIATION_REQUEST)
    {
        // Procesa la solicitud de asociación
        association_request_t *assoc_request = (association_request_t *)event_data;

        // La respuesta de asociación ha sido recibida
        xEventGroupSetBits(s_wifi_event_group, ASSOCIATION_EVENT);
    }
}*/

/* Variable State of Interface */
volatile static uint32_t xInterfaceState = DOWN;

void esp_ieee802154_receive_done(uint8_t *buffer, esp_ieee802154_frame_info_t *frame_info)
{
    if (!buffer || buffer[0] == 0) {
        ESP_EARLY_LOGI(TAG_802154, "Received invalid packet");
        return;
    }

    if (buffer[0] > IEEE802154_MAX_FRAME_SIZE) {
        ESP_EARLY_LOGI(TAG_802154, "Packet too large");
        return;
    }

    // Copiar el paquete a un buffer temporal (excluyendo el primer byte de longitud)
    memcpy(rx_temp_buffer, &buffer[1], buffer[0]);
    rx_temp_len = buffer[0];

    // Notificar a la tarea
    vTaskNotifyGiveFromISR(xShimRxTaskHandle, NULL);
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
        pxPhyDev->ucBytes[i] = ucMACAddress[MAC_ADDRESS_LENGTH_BYTES - 1 - i];
    }
    esp_ieee802154_set_extended_address(pxPhyDev->ucBytes);
    esp_ieee802154_set_short_address(ieee802154_SHORT_ADDRESS);

    xInterfaceState = UP;
    xTaskCreate(vShimRxTask, "ShimRxTask", 4096, NULL, 10, &xShimRxTaskHandle);

    return true;
}

void xIeee802154NetworkInterfaceAssociation_Response(uint8_t *buffer)
{

    if (esp_ieee802154_transmit(buffer, false) == ESP_OK)
    {
        LOGI(TAG_802154, "Frame transmitted successfully");
    }
    else
    {
        LOGE(TAG_802154, "Failed to transmit frame");
    }
}

void xIeee802154NetworkInterfaceAssociation_Request(void)
{
    LOGI(TAG_802154, "Associating to IEEE 802.15.4 network");
    static uint8_t buffer[128];
    uint8_t hdrLen;
    uint8_t payload = 0x01; // request
    ieee802154_address_t srcAddr, dstAddr;

    uint16_t pan_id = ieee802154_PANID_SOURCE;
    esp_ieee802154_set_panid(pan_id);

    srcAddr.mode = ADDR_MODE_LONG;
    esp_ieee802154_get_extended_address(srcAddr.long_address);

    dstAddr.mode = ADDR_MODE_SHORT;
    dstAddr.short_address = ieee802154_SHORT_ADDRESS_DESTINATION; // Hardcoded Short Address

    hdrLen = ieee802154_header(&pan_id, &srcAddr, &dstAddr, false, &buffer[1], sizeof(buffer) - 1);

    buffer[0] = hdrLen + 1;
    heap_caps_check_integrity(MALLOC_CAP_DEFAULT, pdTRUE);
    memcpy(&buffer[1], &payload, 1);
    heap_caps_check_integrity(MALLOC_CAP_DEFAULT, pdTRUE);

    if (esp_ieee802154_transmit(buffer, false) == ESP_OK)
    {
        LOGI(TAG_802154, "Frame transmitted successfully");
    }
    else
    {
        LOGE(TAG_802154, "Failed to transmit frame");
    }
}

bool_t xIeee802154NetworkInterfaceConnect(void)
{
    LOGI(TAG_802154, "Connecting to IEEE 802.15.4 network");

    if (ieee802154_COORDINATOR == 0)
    {
        LOGI(TAG_802154, "--------------COORDINADOR------------");

        esp_ieee802154_set_coordinator(true);
    }
    else
    {
        esp_ieee802154_set_coordinator(false);
    }

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
/*

bool_t xIeee802154NetworkInterfaceConnect(void)
{
    LOGI(TAG_802154, "Connecting to IEEE 802.15.4 network");

    s_wifi_event_group = xEventGroupCreate();

    LOGI(TAG_SHIM, "Creating event Loop");

    ESP_ERROR_CHECK(esp_event_loop_create_default()); // EventTask init

    esp_event_handler_instance_t instance;

    if (ieee802154_COORDINATOR == 0)
    {
        LOGI(TAG_802154, "--------------COORDINADOR------------");

        esp_ieee802154_set_coordinator(true);

        esp_ieee802154_set_channel(ieee802154_CHANNEL);
        esp_ieee802154_set_panid(ieee802154_PANID_SOURCE);

        esp_ieee802154_receive();

        ESP_ERROR_CHECK(esp_event_handler_instance_register(
            IEEE802154_EVENT,               // Base del evento (en este caso IEEE 802.15.4)
            IEEE802154_ASSOCIATION_REQUEST, // Evento específico (solicitud de asociación)
            &request_event_handler,         // La función que manejará el evento
            NULL,                           // Datos adicionales para el handler (si es necesario)
            &instance                       // Instancia que identifica este manejador
            ));
    }
    else
    {

        esp_ieee802154_set_coordinator(false);

        esp_ieee802154_set_channel(ieee802154_CHANNEL);
        esp_ieee802154_set_panid(ieee802154_PANID_SOURCE);

        esp_ieee802154_receive();
        ESP_ERROR_CHECK(esp_event_handler_instance_register(
            IEEE802154_EVENT,                // Base del evento (en este caso IEEE 802.15.4)
            IEEE802154_ASSOCIATION_RESPONSE, // Evento específico (solicitud de asociación)
            &reply_event_handler,            // La función que manejará el evento
            NULL,                            // Datos adicionales para el handler (si es necesario)
            &instance                        // Instancia que identifica este manejador
            ));
    }

    // si no es coordinador entonces debe enviar un asociation request. Se debería hacer un scan primero para validar
    //  si está disponible la red con el pan ID definido.
    if (ieee802154_COORDINATOR != 0)
    {
        xIeee802154NetworkInterfaceAssociation_Request();

        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, ASSOCIATION_EVENT, pdFALSE, pdFALSE, portMAX_DELAY);

        if (bits & ASSOCIATION_EVENT)
        {
            // Recibió la respuesta de asociación correctamente
            printf("Asociación exitosa\n");
            // Continuar con el resto de las tareas
            uint8_t extended_address[8];
            esp_ieee802154_get_extended_address(extended_address);
            LOGI(TAG_802154, "Connected: PAN ID: 0x%04x, Channel: %d, MAC: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
                 esp_ieee802154_get_panid(), esp_ieee802154_get_channel(),
                 extended_address[0], extended_address[1], extended_address[2], extended_address[3],
                 extended_address[4], extended_address[5], extended_address[6], extended_address[7]);
            return true;
        }
        else
        {
            // Se agotó el tiempo de espera sin respuesta
            printf("Fallo en la asociación\n");
            return false;
        }
    }
    else
    {

        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, ASSOCIATION_EVENT, pdFALSE, pdFALSE, portMAX_DELAY);

        if (bits & ASSOCIATION_EVENT)
        {
            // Recibió la respuesta de asociación correctamente
            printf("Asociación exitosa\n");
            // Continuar con el resto de las tareas
            uint8_t extended_address[8];
            esp_ieee802154_get_extended_address(extended_address);
            LOGI(TAG_802154, "Connected: PAN ID: 0x%04x, Channel: %d, MAC: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
                 esp_ieee802154_get_panid(), esp_ieee802154_get_channel(),
                 extended_address[0], extended_address[1], extended_address[2], extended_address[3],
                 extended_address[4], extended_address[5], extended_address[6], extended_address[7]);
            return true;
        }
        else
        {
            // Se agotó el tiempo de espera sin respuesta
            printf("Fallo en la asociación\n");
            return false;
        }
    }
    return true;
}*/

bool_t xIeee802154NetworkInterfaceDisconnect(void)
{
    esp_ieee802154_disable();
    LOGI(TAG_802154, "Disconnected from the IEEE 802.15.4 network");
    return true;
}

void vIeee802154NetworkInterfaceInput(void *buffer, uint16_t len, void *eb)
{
    NetworkBufferDescriptor_t *pxNetworkBuffer = pxGetNetworkBufferWithDescriptor(len, 0);
    const TickType_t xDescriptorWaitTime = pdMS_TO_TICKS(0);

    RINAStackEvent_t xRxEvent = {
        .eEventType = eNetworkRxEvent,
        .xData.PV = NULL};

    if (!pxNetworkBuffer)
    {

        LOGE(TAG_802154, "Failed to allocate network buffer");
        vReleaseNetworkBufferAndDescriptor(pxNetworkBuffer);
    }

  
    memcpy(pxNetworkBuffer->pucEthernetBuffer, buffer, len);

    pxNetworkBuffer->xEthernetDataLength = len;

    xRxEvent.xData.PV = (void *)pxNetworkBuffer;

     
    if (xSendEventStructToIPCPTask(&xRxEvent, xDescriptorWaitTime) == pdFAIL)
    {   
    
        LOGE(TAG_WIFI, "Failed to enqueue packet to network stack %p, len %d", buffer, len);
   
    }

}
void vShimRxTask(void *pvParameters)
{
    for (;;)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);  // Esperar notificación

        // Procesar paquete recibido
        vIeee802154NetworkInterfaceInput(rx_temp_buffer, rx_temp_len, NULL);
    }
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
