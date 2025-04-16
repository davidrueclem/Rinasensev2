/**
 * @file IPCP_manager.c
 * @author David Sarabia - i2CAT(you@domain.com)
 * @brief Handler IPCP events.
 * @version 0.1
 * @date 2022-02-23
 *
 * @copyright Copyright (c) 2022
 *
 */

#include <stdio.h>
#include <string.h>

#include "BufferManagement.h"
#include "IPCP_manager.h"
#include "IPCP_api.h"

#include "wifi_IPCP.h"
#include "ieee802154_IPCP.h"

static ipcManager_t *pxIpcManager;

/* Table to store instances created */
static InstanceTableRow_t xInstanceTable[INSTANCES_IPCP_ENTRIES];

/**
 * @brief Add an IPCP instance into the Ipcp Instance Table.
 *
 * @param pxIpcpInstaceToAdd to added into the table
 */
void prvIpcpMngrAddInstanceEntry(struct ipcpInstance_t *pxIpcpInstaceToAdd)
{
    num_t x = 0;

    for (x = 0; x < INSTANCES_IPCP_ENTRIES; x++)
    {
        if (xInstanceTable[x].xActive == false)
        {
            xInstanceTable[x].pxIpcpInstance = pxIpcpInstaceToAdd;
            xInstanceTable[x].pxIpcpType = pxIpcpInstaceToAdd->xType;
            xInstanceTable[x].xIpcpId = pxIpcpInstaceToAdd->xId;
            xInstanceTable[x].xActive = true;

            break;
        }
    }
}

struct ipcpInstance_t *pxIpcManagerFindInstanceById(ipcpInstanceId_t xIpcpId)
{
    num_t x = 0;

    for (x = 0; x < INSTANCES_IPCP_ENTRIES; x++)
    {
        if (xInstanceTable[x].xActive == true)
        {
            if (xInstanceTable[x].xIpcpId == xIpcpId)
            {
                LOGI(TAG_IPCPMANAGER, "Instance founded '%p'", xInstanceTable[x].pxIpcpInstance);
                return xInstanceTable[x].pxIpcpInstance;
                break;
            }
        }
    }
    return NULL;
}

/**
 * @brief Find an ipcp instances into the table by ipcp Type.
 *
 * @param xType Type of instance to be founded.
 * @return ipcpInstance_t* pointer to the ipcp instance.
 */

struct ipcpInstance_t *pxIpcManagerFindInstanceByType(ipcpInstanceType_t xType)
{
    num_t x = 0;

    for (x = 0; x < INSTANCES_IPCP_ENTRIES; x++)
    {
        if (xInstanceTable[x].xActive == true)
        {
            if (xInstanceTable[x].pxIpcpType == xType)
            {
                return xInstanceTable[x].pxIpcpInstance;
                break;
            }
        }
    }
    return NULL;
}

struct ipcpInstance_t *pxIpcManagerActiveNormalInstance(void)
{
    return pxIpcManagerFindInstanceByType(eNormal);
}

struct ipcpInstance_t *pxIpcManagerActiveShimInstance()
{
#ifdef SHIM_WIFI_MODULE
    return pxIpcManagerFindInstanceByType(eShimWiFi);
#endif
#ifdef SHIM_802154_MODULE
    return pxIpcManagerFindInstanceByType(eShim802154);
#endif
}

/**
 * @brief Initialize a IPC Manager object. Create a Port Id Manager
 * instance. Create an IPCP Id Manager. Finally Initialize the Factories
 * List
 *
 * @param pxIpcManager object created in the IPCP TASK.
 * @return BaseType_t
 */
bool_t xIpcMngrInit(void)
{
    pxIpcManager = pvRsMemAlloc(sizeof(*pxIpcManager));

    if ((pxIpcManager->pxPidm = pxNumMgrCreate(MAX_PORT_ID)) == NULL)
        return false;

    if ((pxIpcManager->pxIpcpIdm = pxNumMgrCreate(MAX_IPCP_ID)) == NULL)
        return false;

    if (xNetworkBuffersInitialise() != 1)
        return false;

    return true;
}

/** @brief create a shim instance by calling the Shim_API shimCreate() */
struct ipcpInstance_t *pxIpcMngrCreateShim()
{

    ipcProcessId_t xIpcpId;

    xIpcpId = ulNumMgrAllocate(pxIpcManager->pxIpcpIdm);

    // add the shimInstance into the instance list.

    /*#ifdef SHIM_WIFI_MODULE
        return pxShimWiFiCreate(xIpcpId);
    #endif*/
#ifdef SHIM_802154_MODULE
    return pxShim802154Create(xIpcpId);
#endif
}

/** @brief create a normal instance by calling the IPCP_api IpcpCreate() */
struct ipcpInstance_t *pxIpcMngrCreateNormal(void)
{

    ipcProcessId_t xIpcpId;

    xIpcpId = ulNumMgrAllocate(pxIpcManager->pxIpcpIdm);
    return pxIpcpCreate(xIpcpId);
}

/** @brief Allocate a PortId */
portId_t xIpcMngrAllocatePortId(void)
{
    return ulNumMgrAllocate(pxIpcManager->pxPidm);
}

bool_t xIpcMngrInitRinaStack(void)
{
    struct ipcpInstance_t *pxShimInstance;
    struct ipcpInstance_t *pxNormalInstance;

    pxNormalInstance = pxIpcMngrCreateNormal();

    prvIpcpMngrAddInstanceEntry(pxNormalInstance);

    if (!xIpcpInit())
        return false;

    pxShimInstance = pxIpcMngrCreateShim();

    // add into the instance into the table
    prvIpcpMngrAddInstanceEntry(pxShimInstance);

    // if (!xIpcMngrInitShim())
    //    return false;

#if SHIM_WIFI_MODULE
    if (!xShimEnrollToDIF(pxShimInstance->pxData))
        return false;
#endif

#if SHIM_802154_MODULE
    if (!xShim802154EnrollToDIF(pxShimInstance->pxData))
        return false;
#endif

    RINAStackEvent_t xEnrollEvent = {
        .eEventType = eShimEnrolledEvent,
        .xData.PV = NULL};

    xEnrollEvent.xData.PV = (void *)(pxShimInstance);
    xSendEventStructToIPCPTask(&xEnrollEvent, 50 * 1000);
#if 0

    /*Sending event to init validation*/

    ShimTaskEvent_t xUpEvent = {
        .eEventType = eNetworkUpEvent,
        .xData.PV = NULL};

    // xEnrollEvent.xData.PV = (void *)(pxShimInstance);
    xSendEventStructToShimIPCPTask(&xUpEvent, 50 * 1000);
#endif

    return true;
}
