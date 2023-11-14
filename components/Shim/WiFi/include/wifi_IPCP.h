/*Application Level Configurations?
 * Shim_wifi name
 * Shim_wifi type (AP-STA)
 * Shim_wifi address
 */

// Include FreeRTOS

#ifndef WIFI_IPCP_H__INCLUDED
#define WIFI_IPCP_H__INCLUDED

#include "common/rina_ids.h"
#include "common/rina_gpha.h"

// #include "Arp826.h"
// #include "Arp826_defs.h"
// #include "du.h"
#include "common/rina_common_port.h"
#include "wifi_IPCP_events.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /* Flow states */
    typedef enum xFLOW_STATES

    {
        eNULL = 0,  // The Port_id cannot be used
        ePENDING,   // The protocol has either initiated the flow allocation or waiting for allocateResponse.
        eALLOCATED, // Flow allocated and the port_id can be used to read/write data from/to.
    } ePortidState_t;

    /* Holds the information related to one flow */

    typedef struct xSHIM_WIFI_FLOW
    {
        /* Harward Destination Address (MAC)*/
        gha_t *pxDestHa;

        /* Protocol Destination Address (Address RINA)*/
        gpa_t *pxDestPa;

        /* Port Id of???*/
        portId_t xPortId;

        /* State of the PortId */
        ePortidState_t ePortIdState;

        /* IPCP Instance who is going to use the Flow*/
        struct ipcpInstance *pxUserIpcp;

        /* Maybe this is not needed*/
        rfifo_t *pxSduQueue;

        /* Flow item to register in the List of Shim WiFi Flows */
        RsListItem_t xFlowItem;
    } shimFlow_t;

    struct ipcpInstance_t *pxShimWiFiCreate(ipcProcessId_t xIpcpId);
    bool_t xShimFlowAllocateResponse(struct ipcpInstanceData_t *pxShimInstanceData,
                                     portId_t xPortId);
    bool_t xShimIpcpInit(void);

    bool_t prxHandleAllocateResponse(void);

    /* This must be moved to another header file */
    bool_t xIsCallingFromShimWiFiIpcpTask(void);

    bool_t xShimEnrollToDIF(struct ipcpInstanceData_t *pxShimInstanceData);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_IPCP_H__*/
