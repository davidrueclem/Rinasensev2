#ifndef _COMPONENTS_SHIM_WIFI_INCLUDE_WIFI_IPCP_EVENTS_H
#define _COMPONENTS_SHIM_WIFI_INCLUDE_WIFI_IPCP_EVENTS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum SHIMWIFI_EVENTS
    {
        eNoNetworkEvent = -1,
        eNetworkDownEvent, /* 0: The network interface has been lost and/or needs [re]connecting. */
        eNetworkRxEvent,   /* 1: The network interface has queued a received Ethernet frame. */
        eNetworkTxEvent,   /* 2: Let the Shim-task send a network packet. */
        eARPTimerEvent,    /* 4: The ARP timer expired. */
    } eShimWiFiEvent_t;

    /**
     * Structure for the information of the commands issued to the Shim task.
     */
    typedef struct xSHIM_TASK_COMMANDS
    {
        eShimWiFiEvent_t eEventType; /**< The event-type enum */
        union
        {
            void *PV;
            uint32_t UN;
            int32_t N;
            char C;
            uint8_t B;
        } xData; /**< The data in the event */
    } ShimWiFiTaskEvent_t;

    bool_t xSendEventToShimIPCPTask(eShimWiFiEvent_t eEvent);

    /* Returns true is this function is called from the IPCP-task */
    bool_t xIsCallingFromShimIPCPTask(void);

    bool_t xSendEventStructToShimIPCPTask(const ShimWiFiTaskEvent_t *pxEvent,
                                          useconds_t uxTimeoutUS);

#ifdef __cplusplus
}
#endif

#endif // _COMPONENTS_SHIM_WIFI_INCLUDE_WIFI_IPCP_EVENTS_H
