#include <stdio.h>
#include <string.h>

#include "common/list.h"
#include "common/rina_name.h"
#include "common/rina_ids.h"
#include "common/rina_common_port.h"
#include "common/mac.h" //This is more common in the ShimIPCPs
#include "portability/port.h"

#include "configRINA.h"
#include "configSensor.h"

#include "NetworkInterface.h"
#include "IPCP_instance.h"
#include "IPCP_manager.h"
#include "shim_IPCP_flows.h"
#include "Arp826.h"
#include "du.h"

// hashtab_t *pxShimFlowHashTable = pxHtNewHashTable("ShimFlows", 1, 5, 1, 5);

/* Stores the state of flows indexed by port_id */
RsList_t xShimFlowsList;

void vShimFlowListInit(void)
{
    vRsListInit((&xShimFlowsList));
}

void vShimFlowAdd(shimFlow_t *pxFlow)
{
    vRsListInitItem(&pxFlow->xFlowItem, pxFlow);
    vRsListInsert(&xShimFlowsList, &pxFlow->xFlowItem);
}

shimFlow_t *prvShimFindFlowByPortId(struct ipcpInstanceData_t *pxData, portId_t xPortId)
{

    shimFlow_t *pxFlow;
    RsListItem_t *pxListItem;

    RsAssert(pxData);

    pxFlow = pvRsMemAlloc(sizeof(*pxFlow));
    if (!pxFlow)
    {
        LOGE(TAG_SHIM, "Failed to allocate memory for flow");
        return NULL;
    }

#if 0
    /* FIXME: Is this validation at all necessary? */
	if (!RsListIsInitilised(&pxData->xFlowsList))
	{
		LOGE(TAG_SHIM, "Flow list is not initilized");
		return NULL;
	}
#endif
    if (!unRsListLength(&xShimFlowsList))
    {
        LOGI(TAG_SHIM, "Flow list is empty");
        return NULL;
    }

    /* Find a way to iterate in the list and compare the addesss*/
    pxListItem = pxRsListGetFirst(&xShimFlowsList);

    while (pxListItem != NULL)
    {
        pxFlow = (shimFlow_t *)pxRsListGetItemOwner(pxListItem);

        if (pxFlow)
        {
            // LOGI(TAG_SHIM, "Flow founded: %p, portID: %d, portState:%d", pxFlow, pxFlow->xPortId, pxFlow->ePortIdState);
            if (pxFlow->xPortId == xPortId)
            {
                return pxFlow;
            }
        }

        pxListItem = pxRsListGetNext(pxListItem);
    }

    LOGI(TAG_SHIM, "Flow not found");
    return NULL;
}

shimFlow_t *prvShimFindFlow(struct ipcpInstanceData_t *pxData)
{

    shimFlow_t *pxFlow;
    RsListItem_t *pxListItem;

    pxFlow = pvRsMemAlloc(sizeof(*pxFlow));

    /* Find a way to iterate in the list and compare the addesss*/
    pxListItem = pxRsListGetFirst(&xShimFlowsList);

    while (pxListItem != NULL)
    {
        pxFlow = (shimFlow_t *)pxRsListGetItemOwner(pxListItem);

        if (pxFlow)
        {
            // LOGI(TAG_SHIM, "Flow founded: %p, portID: %d, portState:%d", pxFlow, pxFlow->xPortId, pxFlow->ePortIdState);

            return pxFlow;
            // return true;
        }

        pxListItem = pxRsListGetNext(pxListItem);
    }

    LOGI(TAG_SHIM, "Flow not found");
    return NULL;
}

bool_t prvShimFlowDestroy(struct ipcpInstanceData_t *xData, shimFlow_t *xFlow)
{

    /* FIXME: Complete what to do with xData*/
    if (xFlow->pxDestPa)
        vGPADestroy(xFlow->pxDestPa);
    if (xFlow->pxDestHa)
        vGHADestroy(xFlow->pxDestHa);
    if (xFlow->pxSduQueue)
        vRsQueueDelete(xFlow->pxSduQueue->xQueue);
    vRsMemFree(xFlow);

    return true;
}

bool_t prvShimUnbindDestroyFlow(struct ipcpInstanceData_t *pxData,
                                shimFlow_t *pxFlow)
{

    /*if (pxFlow->pxUserIpcp)
    {
        ASSERT(pxFlow->pxUserIpcp->pxOps);
        pxFlow->pxUserIpcp->pxOps->flow_unbinding_ipcp(pxFlow->pxUserIpcp->pxData,
                                                       pxFlow->xPortId);
    }*/
    // Check this
    LOGI(TAG_SHIM, "Shim-WiFi unbinded port: %u", pxFlow->xPortId);
    if (prvShimFlowDestroy(pxData, pxFlow))
    {
        LOGE(TAG_SHIM, "Failed to destroy Shim-WiFi flow");
        return false;
    }

    return true;
}

rfifo_t *prvShimCreateQueue(void)
{
    rfifo_t *xFifo = pvRsMemAlloc(sizeof(*xFifo));

    xFifo->xQueue = pxRsQueueCreate("ShimIPCPQueue", SIZE_SDU_QUEUE, sizeof(uint32_t));

    if (!xFifo->xQueue)
    {
        vRsMemFree(xFifo);
        return NULL;
    }

    return xFifo;
}

int QueueDestroy(rfifo_t *f,
                 void (*dtor)(void *e))
{
    if (!f)
    {
        LOGE(TAG_SHIM, "Bogus input parameters, can't destroy NULL");
        return -1;
    }
    if (!dtor)
    {
        LOGE(TAG_SHIM, "Bogus input parameters, no destructor provided");
        return -1;
    }

    vRsQueueDelete(f->xQueue);

    LOGI(TAG_SHIM, "FIFO %pK destroyed successfully", f);

    vRsMemFree(f);

    return 0;
}