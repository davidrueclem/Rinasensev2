#ifndef _COMPONENTS_SHIM_INCLUDE_SHIM_IPCP_FLOWS_H
#define _COMPONENTS_SHIM_INCLUDE_SHIM_IPCP_FLOWS_H

#include <stdint.h>

#include "common/rina_gpha.h"
#include "common/rina_common_port.h"

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

    typedef struct xSHIM_FLOW
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

        /* Flow item to register in the List of Shim Flows */
        RsListItem_t xFlowItem;
    } shimFlow_t;

    void vShimFlowListInit(void);
    void vShimFlowAdd(shimFlow_t *pxFlow);
    shimFlow_t *prvShimFindFlowByPortId(struct ipcpInstanceData_t *pxData, portId_t xPortId);
    shimFlow_t *prvShimFindFlow(struct ipcpInstanceData_t *pxData);
    bool_t prvShimFlowDestroy(struct ipcpInstanceData_t *xData, shimFlow_t *xFlow);
    bool_t prvShimUnbindDestroyFlow(struct ipcpInstanceData_t *pxData, shimFlow_t *pxFlow);
    rfifo_t *prvShimCreateQueue(void);
    int QueueDestroy(rfifo_t *f, void (*dtor)(void *e));

#ifdef __cplusplus
}
#endif

#endif // _COMPONENTS_SHIM_INCLUDE_SHIM_IPCP_FLOWS_H