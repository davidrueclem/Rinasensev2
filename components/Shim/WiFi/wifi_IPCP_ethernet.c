#include <stdio.h>
#include <string.h>

#include "common/list.h"
#include "common/rina_name.h"
#include "common/rina_ids.h"
#include "portability/port.h"

// #include "ShimWiFi.h"
#include "Arp826.h"
#include "wifi_IPCP.h"
#include "IPCP_api.h"
#include "IPCP_events.h"
#include "NetworkInterface.h"
#include "configRINA.h"
#include "configSensor.h"
#include "BufferManagement.h"
// #include "du.h"
// #include "IpcManager.h"

EthernetHeader_t *vCastConstPointerTo_EthernetHeader_t(const void *pvArgument)
{
    return (const void *)(pvArgument);
}

/**
 * @brief Process the Ethernet frame
 *
 * @param pxNetworkBuffer
 */
void prvProcessEthernetPacket(NetworkBufferDescriptor_t *const pxNetworkBuffer)
{
    const EthernetHeader_t *pxEthernetHeader;
    eFrameProcessingResult_t eReturned = eFrameConsumed;
    uint16_t usFrameType;

    RsAssert(pxNetworkBuffer != NULL);

    /* Interpret the Ethernet frame. */
    if (pxNetworkBuffer->xEthernetDataLength >= sizeof(EthernetHeader_t))
    {
        /* Map the buffer onto the Ethernet Header struct for easy access to the fields. */
        pxEthernetHeader = (EthernetHeader_t *)pxNetworkBuffer->pucEthernetBuffer;
        usFrameType = RsNtoHS(pxEthernetHeader->usFrameType);

        /* Interpret the received Ethernet packet. */
        switch (usFrameType)
        {
        case ETH_P_RINA_ARP:

            /* The Ethernet frame contains an ARP packet. */
            LOGI(TAG_WIFI, "ARP Packet Received");

            if (pxNetworkBuffer->xEthernetDataLength >= sizeof(ARPPacket_t))
            {
                /*Process the Packet ARP in case of REPLY -> eProcessBuffer, REQUEST -> eReturnEthernet to
                 * send to the destination a REPLY (It requires more processing tasks) */
                eReturned = eARPProcessPacket(vCastPointerTo_ARPPacket_t(pxNetworkBuffer->pucEthernetBuffer));
            }
            else
            {
                /*If ARP packet is not correct estructured then release buffer*/
                eReturned = eReleaseBuffer;
            }

            break;

        case ETH_P_RINA:

            LOGD(TAG_WIFI, "RINA Packet Received");

            uint8_t *ptr;
            size_t uxRinaLength;
            struct du_t *pxMessagePDU;

            pxMessagePDU = pvRsMemAlloc(sizeof(*pxMessagePDU));

            if (!pxMessagePDU)
            {
                LOGE(TAG_IPCPMANAGER, "pxMessagePDU was not allocated");
                return;
            }

            RINAStackEvent_t ePacketStackReceive = {
                .eEventType = eStackRxEvent,
                .xData.PV = NULL};

            // NetworkBufferDescriptor_t *pxBuffer;

            // removing Ethernet Header
            uxRinaLength = pxNetworkBuffer->xEthernetDataLength - (size_t)14;

            // ESP_LOGE(TAG_ARP, "Taking Buffer to copy the RINA PDU: ETH_P_RINA");
            // pxBuffer = pxGetNetworkBufferWithDescriptor(xlength, (TickType_t)0U);

            // Copy into the newBuffer but just the RINA PDU, and not the Ethernet Header
            ptr = (uint8_t *)pxNetworkBuffer->pucEthernetBuffer + 14;

            pxNetworkBuffer->xRinaDataLength = uxRinaLength;
            pxNetworkBuffer->pucRinaBuffer = ptr;

            pxMessagePDU->pxNetworkBuffer = pxNetworkBuffer;

            // Release the buffer with the Ethernet header, it is not needed any more
            // ESP_LOGE(TAG_ARP, "Releasing Buffer to copy the RINA PDU: ETH_P_RINA");
            // vReleaseNetworkBufferAndDescriptor(pxNetworkBuffer);

            // must be void function
            /*This must be changed. Once the RINA packet is ready it must be enqueed */
            // vIpcManagerRINAPackettHandler(pxIpcpData, pxNetworkBuffer); // must change
            ePacketStackReceive.xData.PV = (void *)(pxMessagePDU);
            xSendEventStructToIPCPTask(&ePacketStackReceive, 0);

            break;

        default:
            LOGE(TAG_WIFI, "No Case Ethernet Type, Drop Frame");
            eReturned = eReleaseBuffer;

            break;
        }
    }
    //}

    /* Perform any actions that resulted from processing the Ethernet frame. */
    switch (eReturned)
    {
    case eReturnEthernetFrame:

        /* The Ethernet frame will have been updated (maybe it was
         * an ARP request) and should be sent back to
         * its source. */
        // vReturnEthernetFrame( pxNetworkBuffer, pdTRUE );

        /* parameter pdTRUE: the buffer must be released once
         * the frame has been transmitted */
        break;

    case eFrameConsumed:

        /* The frame is in use somewhere, don't release the buffer
         * yet. */
        LOGI(TAG_SHIM, "Frame Consumed");
        break;

    case eReleaseBuffer:
        // ESP_LOGI(TAG_SHIM, "Releasing Buffer: ProcessEthernet");
        if (pxNetworkBuffer != NULL)
        {
            vReleaseNetworkBufferAndDescriptor(pxNetworkBuffer);
        }

        break;
    case eProcessBuffer:
        /*ARP process buffer, call to ShimAllocateResponse*/

        /* Finding an instance of eShimiFi and call the flow allocate Response using this instance*/

        if (!prxHandleAllocateResponse())
        {
            LOGE(TAG_WIFI, "Error during the Allocation Request at Shim");
            vReleaseNetworkBufferAndDescriptor(pxNetworkBuffer);
        }
        else
        {
            LOGI(TAG_WIFI, "Buffer Processed");
            vReleaseNetworkBufferAndDescriptor(pxNetworkBuffer);
            xSendEventToIPCPTask(eShimFATimerEvent);
        }

        break;
    default:

        /* The frame is not being used anywhere, and the
         * NetworkBufferDescriptor_t structure containing the frame should
         * just be released back to the list of free buffers. */
        // ESP_LOGI(TAG_SHIM, "Default: Releasing Buffer");
        vReleaseNetworkBufferAndDescriptor(pxNetworkBuffer);
        break;
    }
}

eFrameProcessingResult_t eConsiderFrameForProcessing(const uint8_t *const pucEthernetBuffer)
{
    eFrameProcessingResult_t eReturn = eReleaseBuffer;
    const EthernetHeader_t *pxEthernetHeader;
    uint16_t usFrameType;

    /* Map the buffer onto Ethernet Header struct for easy access to fields. */
    pxEthernetHeader = (EthernetHeader_t *)pucEthernetBuffer;

    usFrameType = RsNtoHS(pxEthernetHeader->usFrameType);

    // Just ETH_P_ARP and ETH_P_RINA Should be processed by the stack
    if (usFrameType == ETH_P_RINA_ARP || usFrameType == ETH_P_RINA)
    {
        eReturn = eProcessBuffer;
        LOGD(TAG_WIFI, "Ethernet packet of type %xu: ACCEPTED", usFrameType);
    }
    else
        LOGD(TAG_WIFI, "Ethernet packet of type %xu: REJECTED", usFrameType);

    return eReturn;
}

/**
 * @brief Analysis what to do with the ethernet frame that has arrived into the WiFi Driver
 *
 * @param pxBuffer Buffer Descriptor that contains the frame info.
 */
void prvHandleEthernetPacket(NetworkBufferDescriptor_t *pxBuffer)
{

#if (USE_LINKED_RX_MESSAGES == 0)
    {
        /* When ipconfigUSE_LINKED_RX_MESSAGES is not set to 0 then only one
         * buffer will be sent at a time.  This is the default way for +TCP to pass
         * messages from the MAC to the TCP/IP stack. */
        LOGI(TAG_WIFI, "Packet to shim IPCP task %p, len %zu", pxBuffer, pxBuffer->xDataLength);
        prvProcessEthernetPacket(pxBuffer);
    }
#else  /* configUSE_LINKED_RX_MESSAGES */
    {
        LOGI(TAG_TAG_WIFI, "Packet to network stack 2 %p, len %d", pxBuffer, pxBuffer->xDataLength);
        NetworkBufferDescriptor_t *pxNextBuffer;

        /* An optimisation that is useful when there is high network traffic.
         * Instead of passing received packets into the IP task one at a time the
         * network interface can chain received packets together and pass them into
         * the IP task in one go.  The packets are chained using the pxNextBuffer
         * member.  The loop below walks through the chain processing each packet
         * in the chain in turn. */
        do
        {
            /* Store a pointer to the buffer after pxBuffer for use later on. */
            pxNextBuffer = pxBuffer->pxNextBuffer;

            /* Make it NULL to avoid using it later on. */
            pxBuffer->pxNextBuffer = NULL;

            prvProcessEthernetPacket(pxBuffer);
            pxBuffer = pxNextBuffer;

            /* While there is another packet in the chain. */
        } while (pxBuffer != NULL);
    }
#endif /* USE_LINKED_RX_MESSAGES */
}