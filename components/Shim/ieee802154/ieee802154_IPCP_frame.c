
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include <time.h>
#include <unistd.h>

#include "common/list.h"
#include "common/rina_name.h"
#include "common/rina_ids.h"
#include "common/rina_common_port.h"
#include "portability/port.h"
#include "common/rina_common.h"

// #include "ShimWiFi.h"
#include "Arp826.h"
#include "wifi_IPCP.h"
#include "IPCP_api.h"
#include "IPCP_events.h"
#include "NetworkInterface.h"
#include "configRINA.h"
#include "configSensor.h"
#include "BufferManagement.h"
#include "ieee802154_frame.h"
#include "ieee802154_IPCP.h"
#include "shim.h"

#include "esp_ieee802154.h"

// #include "du.h"

ieee802154_address_t *vCastPointerTo_Ieee802154Header_t(void *pvArgument)
{
    return (void *)(pvArgument);
}

uint8_t ieee802154_header(const uint16_t *src_pan, ieee802154_address_t *src, const uint16_t *dst_pan,
                          ieee802154_address_t *dst, uint8_t ack, uint8_t *header, uint8_t header_length, uint8_t pcFrameType)
{
    uint8_t frame_header_len = 2;
    mac_fcs_t frame_header = {
        .frameType = pcFrameType,
        .secure = false,
        .framePending = false,
        .ackReqd = ack,
        .panIdCompressed = false,
        .rfu1 = false,
        .sequenceNumberSuppression = false,
        .informationElementsPresent = false,
        .destAddrType = dst->mode,
        .frameVer = FRAME_VERSION_STD_2003,
        .srcAddrType = src->mode};

    LOGE(TAG_802154, "src:mode:%d", src->mode);
    LOGE(TAG_802154, "src:mode:%d", src->short_address);

    bool src_present = src != NULL && src->mode != ADDR_MODE_NONE;
    bool dst_present = dst != NULL && dst->mode != ADDR_MODE_NONE;
    bool src_pan_present = src_pan != NULL;
    bool dst_pan_present = dst_pan != NULL;

    if (src_pan_present && dst_pan_present && src_present && dst_present)
    {
        if (*src_pan == *dst_pan)
        {
            LOGI(TAG_802154, "PAN ID Compressed activated");
            frame_header.panIdCompressed = true;
        }
    }

    if (!frame_header.sequenceNumberSuppression)
    {

        LOGI(TAG_802154, "Sequence Number Suppresion no activated");
        frame_header_len += 1;
    }

    if (dst_pan_present)
    {
        frame_header_len += 2;
    }

    if (frame_header.destAddrType == ADDR_MODE_SHORT)
    {
        frame_header_len += 2;
    }
    else if (frame_header.destAddrType == ADDR_MODE_LONG)
    {
        frame_header_len += 8;
    }

    if (src_pan_present && !frame_header.panIdCompressed)
    {
        frame_header_len += 2;
    }

    if (frame_header.srcAddrType == ADDR_MODE_SHORT)
    {
        frame_header_len += 2;
    }
    else if (frame_header.srcAddrType == ADDR_MODE_LONG)
    {
        frame_header_len += 8;
    }

    if (header_length < frame_header_len)
    {
        return 0;
    }

    uint8_t position = 0;
    memcpy(&header[position], &frame_header, sizeof frame_header);
    position += 2;
    LOGI(TAG_802154, "Length FCS:%d", position);

    if (!frame_header.sequenceNumberSuppression)
    {
        header[position++] = 0;
        LOGI(TAG_802154, "Length FCS+SequenceNumber:%d", position);
    }

    if (dst_pan != NULL)
    {
        memcpy(&header[position], dst_pan, sizeof(uint16_t));
        position += 2;
        LOGI(TAG_802154, "Length FCS+SequenceNumber+DstPAN:%d", position);
    }

    if (frame_header.destAddrType == ADDR_MODE_SHORT)
    {
        memcpy(&header[position], &dst->short_address, sizeof dst->short_address);
        position += 2;
        LOGI(TAG_802154, "Length FCS+SequenceNumber+DstPAN+DstAddrShort:%d", position);
    }
    else if (frame_header.destAddrType == ADDR_MODE_LONG)
    {
        reverse_memcpy(&header[position], (uint8_t *)&dst->long_address, sizeof dst->long_address);
        position += 8;
        LOGI(TAG_802154, "Length FCS+SequenceNumber+DstPAN+DstAddrLong:%d", position);
    }

    if (src_pan != NULL)
    {
        memcpy(&header[position], src_pan, sizeof(uint16_t));
        position += 2;
        LOGI(TAG_802154, "Length FCS+SequenceNumber+DstPAN+DstAddr+SrcPAN:%d", position);
    }

    if (frame_header.srcAddrType == ADDR_MODE_SHORT)
    {
        memcpy(&header[position], &src->short_address, sizeof src->short_address);
        position += 2;
        LOGI(TAG_802154, "Length FCS+SequenceNumber+DstPAN+DstAddr+SrcPAN+SrcAddrShort:%d", position);
    }
    else if (frame_header.srcAddrType == ADDR_MODE_LONG)
    {
        reverse_memcpy(&header[position], (uint8_t *)&src->long_address, sizeof src->long_address);
        position += 8;
        LOGI(TAG_802154, "Length FCS+SequenceNumber+DstPAN+DstAddr+SrcPAN+SrcAddrLong:%d", position);
    }
    LOGI(TAG_802154, "Total Header Lenght: %d", position);

    return position;
}

#if 0
void vIeee802154_Broadcast(uint16_t pan_id)
{
    LOGI(TAG_802154, "Send broadcast from pan %04x", pan_id);
    uint8_t buffer[256];

    esp_ieee802154_set_panid(pan_id);

    uint8_t eui64[8];
    esp_ieee802154_get_extended_address(eui64);

    ieee802154_address_t src = {
        .mode = ADDR_MODE_LONG,
        .long_address = {eui64[0], eui64[1], eui64[2], eui64[3], eui64[4], eui64[5], eui64[6], eui64[7]}};

    ieee802154_address_t *dst;

    dst = pvRsMemAlloc(sizeof(*dst));

    dst->mode = ADDR_MODE_SHORT;
    dst->short_address = SHORT_BROADCAST;

    LOGE(TAG_802154, "src:mode:%d", src->mode);
    LOGE(TAG_802154, "src:mode:%d", src->short_address);

    uint16_t dst_pan = PAN_BROADCAST;

    /*uint8_t ieee802154_header(const uint16_t *src_pan, ieee802154_address_t *src, const uint16_t *dst_pan,
                              ieee802154_address_t *dst, uint8_t ack, uint8_t *header, uint8_t header_length);*/
    uint8_t hdr_len = ieee802154_header(&pan_id, &src, &dst_pan, dst, false, &buffer[1], sizeof(buffer) - 1, FRAME_TYPE_MULTIPURPOSE);

    vPrintBytes((void *)&buffer, hdr_len);
    // Add
    memcpy(&buffer[1 + hdr_len], eui64, 8);

    // packet length
    buffer[0] = hdr_len + 8;

    vPrintBytes((void *)&buffer, buffer[0]);

    esp_ieee802154_transmit(buffer, false);
}
#endif

void vIeee802154FrameBroadcast(NetworkBufferDescriptor_t *pxNetworkBuffer, gha_t *pxSha, gha_t *pxTha, uint8_t pcFrameType)
{

    uint8_t buffer[256];
    uint8_t ucHeaderLen;
    ieee802154_address_t *pxSrc;
    ieee802154_address_t *pxDst;

    uint16_t dst_pan = PAN_BROADCAST;
    uint16_t pan_id = ieee802154_PANID_DESTINATION;

    uint8_t eui64[8];

    pxDst = pvRsMemAlloc(sizeof(*pxDst));
    pxSrc = pvRsMemAlloc(sizeof(*pxSrc));

    pxSrc->mode = ADDR_MODE_LONG;
    pxSrc->long_address[0] = pxSha->xAddress.ucBytes[0];
    pxSrc->long_address[1] = pxSha->xAddress.ucBytes[1];
    pxSrc->long_address[2] = pxSha->xAddress.ucBytes[2];
    pxSrc->long_address[3] = pxSha->xAddress.ucBytes[3];
    pxSrc->long_address[4] = pxSha->xAddress.ucBytes[4];
    pxSrc->long_address[5] = pxSha->xAddress.ucBytes[5];
    pxSrc->long_address[6] = pxSha->xAddress.ucBytes[6];
    pxSrc->long_address[7] = pxSha->xAddress.ucBytes[7];

    //, pxSha->ucAddressMode[1], pxSha->ucAddressMode[2], pxSha->ucAddressMode[3], pxSha->ucAddressMode[4], pxSha->ucAddressMode[5], pxSha->ucAddressMode[6], pxSha->ucAddressMode[7]};

    pxDst->mode = ADDR_MODE_SHORT;
    pxDst->short_address = SHORT_BROADCAST;

    /*uint8_t ieee802154_header(const uint16_t *src_pan, ieee802154_address_t *src, const uint16_t *dst_pan,
                              ieee802154_address_t *dst, uint8_t ack, uint8_t *header, uint8_t header_length);*/
    // If 0 ARP, else DATA
    if (pcFrameType == 0)
    {
        ucHeaderLen = ieee802154_header(&pan_id, pxSrc, &dst_pan, pxDst, false, &buffer[1], sizeof(buffer) - 1, FRAME_TYPE_MULTIPURPOSE);
    }
    else
    {
        ucHeaderLen = ieee802154_header(&pan_id, pxSrc, &dst_pan, pxDst, false, &buffer[1], sizeof(buffer) - 1, FRAME_TYPE_DATA);
    }

    // Copy SDU into header
    memcpy(&buffer[1 + ucHeaderLen], pxNetworkBuffer->pucEthernetBuffer, pxNetworkBuffer->xDataLength);

    // packet length
    buffer[0] = ucHeaderLen + pxNetworkBuffer->xDataLength;

    LOGI(TAG_802154, "Calling Interface to send data");

    LOGI(TAG_802154, "Sending %d Bytes", ucHeaderLen + pxNetworkBuffer->xDataLength);
    esp_ieee802154_transmit(buffer, false);
    vReleaseNetworkBufferAndDescriptor(pxNetworkBuffer);
}

void vFrameForProcessing(uint8_t *pucBuffer, uint8_t ucLen)
{
    eFrameResult_t eReturn = eIeee802154ReleaseBuffer;
    const mac_fcs_t *pxIeee802154Header;
    NetworkBufferDescriptor_t *pxNetworkBuffer;
    uint16_t usFrameType;
    uint8_t position = 0;

    vPrintBytes((void *)pucBuffer, ucLen);

    struct timespec ts;

    /* Map the buffer onto Ethernet Header struct for easy access to fields. */
    pxIeee802154Header = (mac_fcs_t *)pucBuffer;
    position += sizeof(uint16_t);

    usFrameType = pxIeee802154Header->frameType;

    LOGI(TAG_802154, "Frame type:                   %x", pxIeee802154Header->frameType);
    LOGI(TAG_802154, "Security Enabled:             %s", pxIeee802154Header->secure ? "True" : "False");
    LOGI(TAG_802154, "Frame pending:                %s", pxIeee802154Header->framePending ? "True" : "False");
    LOGI(TAG_802154, "Acknowledge request:          %s", pxIeee802154Header->ackReqd ? "True" : "False");
    LOGI(TAG_802154, "PAN ID Compression:           %s", pxIeee802154Header->panIdCompressed ? "True" : "False");
    LOGI(TAG_802154, "Reserved:                     %s", pxIeee802154Header->rfu1 ? "True" : "False");
    LOGI(TAG_802154, "Sequence Number Suppression:  %s", pxIeee802154Header->sequenceNumberSuppression ? "True" : "False");
    LOGI(TAG_802154, "Information Elements Present: %s", pxIeee802154Header->informationElementsPresent ? "True" : "False");
    LOGI(TAG_802154, "Destination addressing mode:  %x", pxIeee802154Header->destAddrType);
    LOGI(TAG_802154, "Frame version:                %x", pxIeee802154Header->frameVer);
    LOGI(TAG_802154, "Source addressing mode:       %x", pxIeee802154Header->srcAddrType);

    LOGI(TAG_SHIM_802154, "Ieee802154 packet of type %xu", usFrameType);
    // set an if structure to validate if the packet is ok before allocating a buffer.
    if (!rstime_waitmsec(&ts, 250))
    {
        LOGE(TAG_802154, "time error");
    }

    pxNetworkBuffer = pxGetNetworkBufferWithDescriptor(ucLen, &ts);

    // Just ETH_P_ARP and ETH_P_RINA Should be processed by the stack

    if (pxNetworkBuffer != NULL)
    {
        switch (usFrameType)
        {

        case FRAME_TYPE_MULTIPURPOSE:
            LOGI(TAG_SHIM_802154, "ARP Packet Received");
            uint8_t sequence_number = pucBuffer[position];
            position += sizeof(uint8_t);
            LOGI(TAG_802154, "Data (%u)", sequence_number);

            uint16_t pan_id = 0;
            uint8_t dst_addr[8] = {0};
            uint8_t src_addr[8] = {0};
            uint16_t short_dst_addr = 0;
            uint16_t short_src_addr = 0;
            bool broadcast = false;

            switch (pxIeee802154Header->destAddrType)
            {
            case ADDR_MODE_NONE:
            {
                LOGI(TAG_802154, "Without PAN ID or address field");
                break;
            }
            case ADDR_MODE_SHORT:
            {
                pan_id = *((uint16_t *)&pucBuffer[position]);
                LOGI(TAG_802154, "Broadcast on PAN %04x", pan_id);
                position += sizeof(uint16_t);
                short_dst_addr = *((uint16_t *)&pucBuffer[position]);
                position += sizeof(uint16_t);
                if (pan_id == 0xFFFF && short_dst_addr == 0xFFFF)
                {
                    broadcast = true;
                    pan_id = *((uint16_t *)&pucBuffer[position]); // srcPan
                    position += sizeof(uint16_t);
                    LOGI(TAG_802154, "Broadcast on PAN %04x", pan_id);
                }
                else
                {
                    LOGI(TAG_802154, "On PAN %04x to short address %04x", pan_id, short_dst_addr);
                }
                break;
            }
            case ADDR_MODE_LONG:
            {
                pan_id = *((uint16_t *)&pucBuffer[position]);
                position += sizeof(uint16_t);
                for (uint8_t idx = 0; idx < sizeof(dst_addr); idx++)
                {
                    dst_addr[idx] = pucBuffer[position + sizeof(dst_addr) - 1 - idx];
                }
                position += sizeof(dst_addr);
                LOGI(TAG_802154, "On PAN %04x to long address %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x", pan_id, dst_addr[0],
                     dst_addr[1], dst_addr[2], dst_addr[3], dst_addr[4], dst_addr[5], dst_addr[6], dst_addr[7]);
                break;
            }
            default:
            {
                LOGE(TAG_802154, "With reserved destination address type, ignoring packet");
                return;
            }
            }

            switch (pxIeee802154Header->srcAddrType)
            {
            case ADDR_MODE_NONE:
            {
                LOGI(TAG_802154, "Originating from the PAN coordinator");
                break;
            }
            case ADDR_MODE_SHORT:
            {
                short_src_addr = *((uint16_t *)&pucBuffer[position]);
                position += sizeof(uint16_t);
                LOGI(TAG_802154, "Originating from short address %04x", short_src_addr);
                break;
            }
            case ADDR_MODE_LONG:
            {
                for (uint8_t idx = 0; idx < sizeof(src_addr); idx++)
                {
                    src_addr[idx] = pucBuffer[position + sizeof(src_addr) - 1 - idx];
                }
                position += sizeof(src_addr);
                LOGI(TAG_802154, "Originating from long address %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x", src_addr[0], src_addr[1],
                     src_addr[2], src_addr[3], src_addr[4], src_addr[5], src_addr[6], src_addr[7]);
                break;
            }
            default:
            {
                LOGE(TAG_802154, "With reserved source address type, ignoring packet");
                return;
            }
            }

            uint8_t *header = &pucBuffer[0];
            uint8_t header_length = position;
            uint8_t *data = &pucBuffer[position];
            uint8_t data_length = ucLen - position - sizeof(uint16_t);
            position += data_length;

            LOGI(TAG_802154, "Data length: %u", data_length);

            uint16_t checksum = *((uint16_t *)&pucBuffer[position]);

            LOGI(TAG_802154, "Checksum: %04x", checksum);

            LOGI(TAG_802154, "PAN %04x S %04x %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X to %04x %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X %s", pan_id,
                 short_src_addr, src_addr[0], src_addr[1], src_addr[2], src_addr[3], src_addr[4], src_addr[5], src_addr[6], src_addr[7],
                 short_dst_addr, dst_addr[0], dst_addr[1], dst_addr[2], dst_addr[3], dst_addr[4], dst_addr[5], dst_addr[6], dst_addr[7],
                 broadcast ? "BROADCAST" : "");

            if (broadcast)
                for (uint8_t idx = 0; idx < 8; idx++)
                    dst_addr[idx] = 0xFF;

            /* Set the packet size, in case a larger buffer was returned. */
            pxNetworkBuffer->xEthernetDataLength = ucLen;

            /* Copy the packet data. */
            memcpy(pxNetworkBuffer->pucEthernetBuffer, (void *)pucBuffer, ucLen);

            if (pxNetworkBuffer->xEthernetDataLength >= sizeof(ARPPacket_t))
            {
                /*Process the Packet ARP in case of REPLY -> eProcessBuffer, REQUEST -> eReturnEthernet to
                 * send to the destination a REPLY (It requires more processing tasks) */
                eARPProcessPacket(vCastPointerTo_ARPPacket_t(pxNetworkBuffer->pucEthernetBuffer));
            }
            else
            {
                /*If ARP packet is not correct estructured then release buffer*/
                // eReturned = eReleaseBuffer;
            }

            break;
        case FRAME_TYPE_DATA:
            LOGI(TAG_SHIM_802154, "Data Packet");

            /* Set the packet size, in case a larger buffer was returned. */
            pxNetworkBuffer->xEthernetDataLength = ucLen;

            /* Copy the packet data. */
            memcpy(pxNetworkBuffer->pucEthernetBuffer, (void *)pucBuffer, ucLen);

            LOGI(TAG_SHIM_802154, "Ieee802154 packet COPIED!!!");

            const TickType_t xDescriptorWaitTime = pdMS_TO_TICKS(0);
            struct timespec ts;

            ShimTaskEvent_t xRxEvent = {
                .eEventType = eNetworkRxEvent,
                .xData.PV = NULL};

            if (xSendEventStructToShimIPCPTask(&xRxEvent, xDescriptorWaitTime) == false)
            {
                LOGE(TAG_WIFI, "Failed to enqueue packet to network stack %p, len %d", pxNetworkBuffer, ucLen);
                vReleaseNetworkBufferAndDescriptor(pxNetworkBuffer);
            }

            else
            {
                // LOGE(TAG_WIFI, "Failed to get buffer descriptor");
                vReleaseNetworkBufferAndDescriptor(pxNetworkBuffer);
            }

            break;

        default:
            break;
        }
    }
}
