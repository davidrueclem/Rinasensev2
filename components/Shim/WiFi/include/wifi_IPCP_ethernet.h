#ifndef _COMPONENTS_SHIM_WIFI_INCLUDE_WIFI_IPCP_ETHERNET_H
#define _COMPONENTS_SHIM_WIFI_INCLUDE_WIFI_IPCP_ETHERNET_H

#include "BufferManagement.h"
#include "Arp826_defs.h"
#include "wifi_IPCP_frames.h"

#ifdef __cplusplus
extern "C"
{
#endif
    EthernetHeader_t *vCastConstPointerTo_EthernetHeader_t(void *pvArgument);

    void prvProcessEthernetPacket(NetworkBufferDescriptor_t *const pxNetworkBuffer);
    eFrameProcessingResult_t eConsiderFrameForProcessing(const uint8_t *const pucEthernetBuffer);
    void prvHandleEthernetPacket(NetworkBufferDescriptor_t *pxBuffer);

#ifdef __cplusplus
}
#endif

#endif // _COMPONENTS_SHIM_WIFI_INCLUDE_WIFI_IPCP_ETHERNET_H