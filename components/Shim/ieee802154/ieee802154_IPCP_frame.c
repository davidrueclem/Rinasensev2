/* Standard includes */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "portability/port.h"

/* FreeRTOS includes */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

/* RINA Components includes */
#include "BufferManagement.h"
#include "ieee802154_NetworkInterface.h"
#include "configRINA.h"
#include "configSensor.h"
#include "common/mac.h"

#include "ieee802154_frame.h"
#include "IPCP_manager.h"
#include "du.h"
#include "BufferManagement.h"
#include "common/rina_common.h"

/* ESP includes */
#include "esp_ieee802154.h"
#if ESP_IDF_VERSION_MAJOR > 4
#include "esp_mac.h"
#endif
#include "esp_event.h"
#include "esp_system.h"
#include "esp_event_base.h"
#include <esp_log.h>

#define TAG_802154 "ieee802154"
#define SHORT_BROADCAST 0xFFFF
#define PAN_BROADCAST 0xFFFF

static void debug_print_packet(uint8_t *packet, uint8_t packet_length);

/* Function to process received IEEE 802.15.4 frame */
void vHandleIEEE802154Frame(NetworkBufferDescriptor_t *pxNetworkBuffer)
{
    if (pxNetworkBuffer == NULL || pxNetworkBuffer->pucEthernetBuffer == NULL)
    {
        LOGE(TAG_802154, "Invalid network buffer");
        return;
    }

    uint8_t *pucBuffer = pxNetworkBuffer->pucEthernetBuffer;
    uint16_t usLength = pxNetworkBuffer->xDataLength;

    LOGI(TAG_802154, "Received packet of length %d", usLength);
    debug_print_packet(pucBuffer, usLength);

    mac_fcs_t *pxFrameHeader = (mac_fcs_t *)pucBuffer;
    if (pxFrameHeader->frameType == FRAME_TYPE_DATA)
    {
        LOGI(TAG_802154, "Processing DATA frame");
    }
    else if (pxFrameHeader->frameType == FRAME_TYPE_ACK)
    {
        LOGI(TAG_802154, "Received ACK frame");
    }
    else
    {
        LOGW(TAG_802154, "Unknown frame type received: %d", pxFrameHeader->frameType);
    }


}

esp_err_t xProcessIEEE802154Packet(NetworkBufferDescriptor_t *pxNetworkBuffer)
{
    uint8_t *packet = pxNetworkBuffer->pucEthernetBuffer;
    uint8_t position = 0;

    mac_fcs_t *fcs = (mac_fcs_t *)&packet[position];
    position += sizeof(uint16_t); // Frame Control Field size

    if (fcs->rfu1)
    {
        LOGE(TAG_802154, "Reserved field is set, ignoring packet");
        vReleaseNetworkBufferAndDescriptor(pxNetworkBuffer);
        return ESP_FAIL;
    }

    /* Verificar si el paquete es de datos */
    if (fcs->frameType != FRAME_TYPE_DATA)
    {
        LOGI(TAG_802154, "Ignoring non-data frame type: %d", fcs->frameType);
        vReleaseNetworkBufferAndDescriptor(pxNetworkBuffer);
        return ESP_FAIL;
    }

    /* Extraer direcciones y determinar si el paquete es para mí o es broadcast */
    uint16_t pan_id = 0;
    uint16_t short_dst_addr = 0;
    uint8_t dst_addr[8] = {0};
    bool is_for_me = false;
    bool is_broadcast = false;

    /* Obtener mi dirección */
    uint8_t my_long_addr[8];
    esp_ieee802154_get_extended_address(my_long_addr);
    uint16_t my_short_addr = esp_ieee802154_get_short_address();

    switch (fcs->destAddrType)
    {
    case ADDR_MODE_NONE:
        LOGI(TAG_802154, "No destination address, assuming it's for the PAN coordinator.");
        is_for_me = true;
        break;

    case ADDR_MODE_SHORT:
        pan_id = *((uint16_t *)&packet[position]);
        position += sizeof(uint16_t);
        short_dst_addr = *((uint16_t *)&packet[position]);
        position += sizeof(uint16_t);

        if (short_dst_addr == my_short_addr)
            is_for_me = true;
        else if (short_dst_addr == 0xFFFF)
            is_broadcast = true;
        break;

    case ADDR_MODE_LONG:
        pan_id = *((uint16_t *)&packet[position]);
        position += sizeof(uint16_t);
        for (uint8_t idx = 0; idx < sizeof(dst_addr); idx++)
        {
            dst_addr[idx] = packet[position + sizeof(dst_addr) - 1 - idx];
        }
        position += sizeof(dst_addr);

        if (memcmp(dst_addr, my_long_addr, sizeof(dst_addr)) == 0)
            is_for_me = true;
        break;

    default:
        LOGE(TAG_802154, "Invalid destination address type, ignoring packet");
        vReleaseNetworkBufferAndDescriptor(pxNetworkBuffer);
        return ESP_FAIL;
    }

    /* Si el paquete es broadcast, lo reenviamos */
    if (is_broadcast)
    {
        LOGI(TAG_802154, "Packet is broadcast, forwarding to network output");
        // xIeee802154NetworkInterfaceOutput(pxNetworkBuffer, true);
        vReleaseNetworkBufferAndDescriptor(pxNetworkBuffer);
        return ESP_OK;
    }

    /* Si el paquete NO es para mí, lo descartamos */
    if (!is_for_me)
    {
        LOGI(TAG_802154, "Packet is not for me, dropping.");
        vReleaseNetworkBufferAndDescriptor(pxNetworkBuffer);
        return ESP_FAIL;
    }

    LOGI(TAG_802154, "Packet is for me, processing...");

    /* Extraer la SDU eliminando la cabecera */
    uint8_t *sdu = &packet[position];
    uint16_t sdu_length = pxNetworkBuffer->xEthernetDataLength - position - sizeof(uint16_t); // Restar el checksum
    position += sdu_length;

    pxNetworkBuffer->pucRinaBuffer = sdu;
    pxNetworkBuffer->xRinaDataLength = sdu_length;

    /* Crear el du_t */
    struct du_t *pxMessagePDU = pvRsMemAlloc(sizeof(*pxMessagePDU));
    if (!pxMessagePDU)
    {
        LOGE(TAG_802154, "Failed to allocate du_t");
        vReleaseNetworkBufferAndDescriptor(pxNetworkBuffer);
        return ESP_FAIL;
    }

    pxMessagePDU->pxNetworkBuffer = pxNetworkBuffer; // Guardar el buffer dentro del du_t

    /* Obtener la instancia normal del IPCP para encolar el paquete */
    struct ipcpInstance_t *pxIpcpInstance = pxIpcManagerActiveNormalInstance();
    if (!pxIpcpInstance)
    {
        LOGE(TAG_802154, "No active normal IPCP instance found");
        vReleaseNetworkBufferAndDescriptor(pxNetworkBuffer);
        vRsMemFree(pxMessagePDU);
        return ESP_FAIL;
    }

    LOGI(TAG_802154, "Enqueuing packet to normal IPCP");

    if (!pxIpcpInstance->pxOps->duEnqueue(pxIpcpInstance->pxData, 1, pxMessagePDU))
    {
        LOGE(TAG_802154, "Failed to enqueue packet to normal IPCP");
        vReleaseNetworkBufferAndDescriptor(pxNetworkBuffer);
        vRsMemFree(pxMessagePDU);
        return ESP_FAIL;
    }

    LOGI(TAG_802154, "Packet successfully enqueued to IPCP");
    return ESP_OK;
}

/* Function to send an IEEE 802.15.4 frame */
void vIeee802154FrameSend(uint8_t *pucBuffer, uint16_t usLength)
{
    if (pucBuffer == NULL || usLength == 0)
    {
        LOGE(TAG_802154, "Invalid buffer for transmission");
        return;
    }

    LOGI(TAG_802154, "Transmitting IEEE 802.15.4 frame");
    LOGI(TAG_802154, "Frame length: %u", usLength); 

    if (esp_ieee802154_transmit(pucBuffer, false) == ESP_OK)
    {
        LOGI(TAG_802154, "Frame transmitted successfully");
    }
    else
    {
        LOGE(TAG_802154, "Failed to transmit frame");
    }
}


/* Helper function to print received IEEE 802.15.4 frame details */
static void debug_print_packet(uint8_t *packet, uint8_t packet_length)
{
    if (packet_length < sizeof(mac_fcs_t))
        return;

    uint8_t position = 0;
    mac_fcs_t *fcs = (mac_fcs_t *)&packet[position];
    position += sizeof(uint16_t);

    ESP_LOGI(TAG_802154, "Frame type:                   %x", fcs->frameType);
    ESP_LOGI(TAG_802154, "Security Enabled:             %s", fcs->secure ? "True" : "False");
    ESP_LOGI(TAG_802154, "Frame pending:                %s", fcs->framePending ? "True" : "False");
    ESP_LOGI(TAG_802154, "Acknowledge request:          %s", fcs->ackReqd ? "True" : "False");
    ESP_LOGI(TAG_802154, "PAN ID Compression:           %s", fcs->panIdCompressed ? "True" : "False");
    ESP_LOGI(TAG_802154, "Reserved:                     %s", fcs->rfu1 ? "True" : "False");
    ESP_LOGI(TAG_802154, "Destination addressing mode:  %x", fcs->destAddrType);
    ESP_LOGI(TAG_802154, "Frame version:                %x", fcs->frameVer);
    ESP_LOGI(TAG_802154, "Source addressing mode:       %x", fcs->srcAddrType);

    switch (fcs->destAddrType)
    {
    case ADDR_MODE_SHORT:
    {
        uint16_t pan_id = *((uint16_t *)&packet[position]);
        position += sizeof(uint16_t);
        uint16_t short_dst_addr = *((uint16_t *)&packet[position]);
        position += sizeof(uint16_t);
        ESP_LOGI(TAG_802154, "On PAN %04x to short address %04x", pan_id, short_dst_addr);
        break;
    }
    case ADDR_MODE_LONG:
    {
        uint16_t pan_id = *((uint16_t *)&packet[position]);
        position += sizeof(uint16_t);
        uint8_t dst_addr[8];
        memcpy(dst_addr, &packet[position], sizeof(dst_addr));
        position += sizeof(dst_addr);
        ESP_LOGI(TAG_802154, "On PAN %04x to long address %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
                 pan_id, dst_addr[0], dst_addr[1], dst_addr[2], dst_addr[3],
                 dst_addr[4], dst_addr[5], dst_addr[6], dst_addr[7]);
        break;
    }
    default:
        ESP_LOGE(TAG_802154, "Unknown destination address type");
        return;
    }

    if (fcs->srcAddrType == ADDR_MODE_LONG)
    {
        uint8_t src_addr[8];
        memcpy(src_addr, &packet[position], sizeof(src_addr));
        position += sizeof(src_addr);
        ESP_LOGI(TAG_802154, "Originating from long address %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
                 src_addr[0], src_addr[1], src_addr[2], src_addr[3],
                 src_addr[4], src_addr[5], src_addr[6], src_addr[7]);
    }
}

static void reverse_memcpy_local(uint8_t *restrict dst, const uint8_t *restrict src, size_t n);

uint8_t ieee802154_header(const uint16_t *pan_id, ieee802154_address_t *src, ieee802154_address_t *dst,
    uint8_t ack, uint8_t *header, uint8_t header_length) {
if (!pan_id || !src || !dst || !header || header_length < 9) {
return 0;
}

mac_fcs_t frame_header = {
.frameType = FRAME_TYPE_DATA,
.secure = false,
.framePending = false,
.ackReqd = ack,
.panIdCompressed = true,  
.rfu1 = false,
.sequenceNumberSuppression = false,
.informationElementsPresent = false,
.destAddrType = ADDR_MODE_SHORT,
.frameVer = FRAME_VERSION_STD_2003,
.srcAddrType = ADDR_MODE_SHORT
};

uint8_t position = 0;

// FCS
memcpy(&header[position], &frame_header, sizeof(frame_header));
position += 2;

// Sequence Number
header[position++] = 0x00;

// PAN ID (compartido)
memcpy(&header[position], pan_id, sizeof(uint16_t));
position += 2;

// Destino (short)
memcpy(&header[position], &dst->short_address, sizeof(uint16_t));
position += 2;

// Origen (short)
memcpy(&header[position], &src->short_address, sizeof(uint16_t));
position += 2;

return position; 
}


static void reverse_memcpy_local(uint8_t *restrict dst, const uint8_t *restrict src, size_t n)
{
    size_t i;

    for (i=0; i < n; ++i) {
        dst[n - 1 - i] = src[i];
    }
}