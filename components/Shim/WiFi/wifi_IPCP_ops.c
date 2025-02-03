#include <stdio.h>
#include <string.h>

#include "common/list.h"
#include "common/rina_name.h"
#include "common/rina_ids.h"
#include "common/mac.h" //This is more common in the ShimIPCPs
#include "portability/port.h"

#include "configRINA.h"
#include "configSensor.h"

#include "NetworkInterface.h"
#include "IPCP_instance.h"
#include "IPCP_manager.h"
#include "IPCP_api.h"

#include "shim_IPCP_flows.h"
#include "wifi_IPCP.h"
#include "wifi_IPCP_ethernet.h"
#include "Arp826.h"
#include "du.h"

/* IPCP Shim Instance particular data structure */
struct ipcpInstanceData_t
{

    RsListItem_t xInstanceListItem;
    ipcProcessId_t xId;

    /* IPC Process name */
    name_t *pxName;
    name_t *pxDifName;
    string_t pcInterfaceName;

    MACAddress_t *pxPhyDev;
    struct flowSpec_t *pxFspec;

    /* The IPC Process using the shim-WiFi */
    name_t *pxAppName;
    name_t *pxDafName;

    /* Stores the state of flows indexed by port_id */
    RsList_t xFlowsList;

    /* RINARP related */
    struct rinarpHandle_t *pxAppHandle;
    struct rinarpHandle_t *pxDafHandle;

    /* Flow control between this IPCP and the associated netdev. */
    unsigned int ucTxBusy;
};

/** @brief Enrollment operation must be called by the IPCP manager after initializing
 * the shim IPCP task.
 * @param pxShimInstanceData is a pointer in the IPCP instance data. The IPCP instance is stored at the
 * IPCP manager list.
 */
bool_t xShimEnrollToDIF(struct ipcpInstanceData_t *pxShimInstanceData)
{
    LOGI(TAG_SHIM, "Enrolling to DIF");

    /* Initialization of WiFi interface */

    if (xNetworkInterfaceInitialise(pxShimInstanceData->pxPhyDev))
    {
        /* Initialize ARP Cache */
        vARPInitCache();

        /* Connect to remote point (WiFi AP) */
        if (xNetworkInterfaceConnect())
        {
            LOGI(TAG_SHIM, "Enrolled To DIF %s", SHIM_DIF_NAME);
            return true;
        }

        LOGE(TAG_SHIM, "Failed to enroll to DIF %s", SHIM_DIF_NAME);
        return false;
    }

    LOGE(TAG_SHIM, "Failed to enroll to DIF %s", SHIM_DIF_NAME);

    return false;
}

/** @brief Primitive invoked before all other functions. The N+1 DIF calls the application register:
 * - Transform the naming-info structure into a single string (application-name)
 * separated by "-": ProcessName-ProcessInstance-EntityName-EntityInstance
 * - (Update LocalAddressProtocol which is part of the ARP module).
 * It is assumed only there is going to be one IPCP process over the Shim-DIF.
 * pxAppName, and pxDafName come from the Normal IPCP (user_app), while the pxData refers to
 * the shimWiFi ipcp instance.
 * @return a pdTrue if Success or pdFalse Failure.
 * */
bool_t xShimApplicationRegister(struct ipcpInstanceData_t *pxData, name_t *pxAppName, name_t *pxDafName)
{
    LOGI(TAG_SHIM, "Registering Application");

    gpa_t *pxPa;
    gha_t *pxHa;

    if (!pxData)
    {
        LOGI(TAG_SHIM, "Data no valid ");
        return false;
    }
    if (!pxAppName)
    {
        LOGI(TAG_SHIM, "Name no valid ");
        return false;
    }
    if (pxData->pxAppName != NULL)
    {
        LOGI(TAG_SHIM, "AppName should not exist");
        return false;
    }

    pxData->pxAppName = pxRstrNameDup(pxAppName);

    if (!pxData->pxAppName)
    {
        LOGI(TAG_SHIM, "AppName not created ");
        return false;
    }

    pxPa = pxNameToGPA(pxAppName);

    if (!xIsGPAOK(pxPa))
    {
        LOGI(TAG_SHIM, "Protocol Address is not OK ");
        vRstrNameFini(pxData->pxAppName);
        return false;
    }

    if (!pxData->pxPhyDev)
    {
        xNetworkInterfaceInitialise(pxData->pxPhyDev);
    }

    pxHa = pxCreateGHA(MAC_ADDR_802_3, pxData->pxPhyDev);

    if (!xIsGHAOK(pxHa))
    {
        LOGI(TAG_SHIM, "Hardware Address is not OK ");
        vRstrNameFini(pxData->pxAppName);
        vGHADestroy(pxHa);
        return false;
    }

    pxData->pxAppHandle = pxARPAdd(pxPa, pxHa);

    if (!pxData->pxAppHandle)
    {
        // destroy all
        LOGI(TAG_SHIM, "APPHandle was not created ");
        vGPADestroy(pxPa);
        vGHADestroy(pxHa);
        vRstrNameFini(pxData->pxAppName);
        return false;
    }

    // vShimGPADestroy( pa );

    pxData->pxDafName = pxRstrNameDup(pxDafName);

    if (!pxData->pxDafName)
    {
        LOGE(TAG_SHIM, "Removing ARP Entry for DAF");
        xARPRemove(pxData->pxAppHandle->pxPa, pxData->pxAppHandle->pxHa);
        pxData->pxAppHandle = NULL;
        vRstrNameFree(pxData->pxAppName);
        vGHADestroy(pxHa);
        return false;
    }

    pxPa = pxNameToGPA(pxDafName);

    if (!xIsGPAOK(pxPa))
    {
        LOGE(TAG_SHIM, "Failed to create gpa");
        xARPRemove(pxData->pxAppHandle->pxPa, pxData->pxAppHandle->pxHa);
        pxData->pxAppHandle = NULL;
        vRstrNameFree(pxData->pxDafName);
        vRstrNameFree(pxData->pxAppName);
        vGHADestroy(pxHa);
        return false;
    }

    pxData->pxDafHandle = pxARPAdd(pxPa, pxHa);

    if (!pxData->pxDafHandle)
    {
        LOGE(TAG_SHIM, "Failed to register DAF in ARP");
        xARPRemove(pxData->pxAppHandle->pxPa, pxData->pxAppHandle->pxHa);
        pxData->pxAppHandle = NULL;
        vRstrNameFree(pxData->pxAppName);
        vRstrNameFree(pxData->pxDafName);
        vGPADestroy(pxPa);
        vGHADestroy(pxHa);
        return false;
    }

#if 0
    vARPPrintCache();
#endif

    return true;
}

/**
 * @brief FlowAllocateRequest. The N+1 DIF requests for allocating a flow. So, it sends the
 * name-info (source info and destination info).
 * - Check if there is a flow established (eALLOCATED), or a flow pending between the
 * source and destination application (ePENDING),
 * - If stated is eNULL then RINA_xARPAdd is called.
 *
 * @param xId PortId created in the IPCManager and allocated to this flow
 * @param pxUserIpcp IPCP who is going to use the flow
 * @param pxSourceInfo Source Information
 * @param pxDestinationInfo Destination Information
 * @param pxData Shim IPCP Data to update during the flow allocation request
 * @return BaseType_t
 */
bool_t xShimFlowAllocateRequest(struct ipcpInstanceData_t *pxData, struct ipcpInstance_t *pxUserIpcp,
                                name_t *pxSourceInfo,
                                name_t *pxDestinationInfo,
                                portId_t xPortId)
{

    LOGI(TAG_SHIM, "New flow allocation request");

    shimFlow_t *pxFlow;

    if (!pxData)
    {
        LOGE(TAG_SHIM, "Bogus data passed, bailing out");
        return false;
    }

    if (!pxSourceInfo)
    {
        LOGE(TAG_SHIM, "Bogus data passed, bailing out");
        return false;
    }
    if (!pxDestinationInfo)
    {
        LOGE(TAG_SHIM, "Bogus data passed, bailing out");
        return false;
    }

    if (!is_port_id_ok(xPortId))
    {
        LOGE(TAG_SHIM, "Bogus data passed, bailing out");
        return false;
    }

    LOGI(TAG_SHIM, "Finding Flows");
    pxFlow = prvShimFindFlowByPortId(pxData, xPortId);

    if (!pxFlow)
    {
        pxFlow = pvRsMemAlloc(sizeof(*pxFlow));
        if (!pxFlow)
            return false;

        pxFlow->xPortId = xPortId;
        pxFlow->ePortIdState = ePENDING;
        pxFlow->pxDestPa = pxNameToGPA(pxDestinationInfo);
        pxFlow->pxUserIpcp = pxUserIpcp;

        if (!xIsGPAOK(pxFlow->pxDestPa))
        {
            LOGE(TAG_SHIM, "Destination protocol address is not OK");
            prvShimUnbindDestroyFlow(pxData, pxFlow);

            return false;
        }

        // Register the flow in a list or in the Flow allocator
        LOGI(TAG_SHIM, "Created Flow: %p, portID: %d, portState: %d", pxFlow, pxFlow->xPortId, pxFlow->ePortIdState);
        vShimFlowAdd(pxFlow);

        pxFlow->pxSduQueue = prvShimCreateQueue();
        if (!pxFlow->pxSduQueue)
        {
            LOGE(TAG_SHIM, "Destination protocol address is not ok");
            prvShimUnbindDestroyFlow(pxData, pxFlow);
            return false;
        }

        //************ RINAARP RESOLVE GPA

        if (!xARPResolveGPA(pxFlow->pxDestPa, pxData->pxAppHandle->pxPa, pxData->pxAppHandle->pxHa))
        {
            prvShimUnbindDestroyFlow(pxData, pxFlow);
            return false;
        }
    }
    else if (pxFlow->ePortIdState == ePENDING)
    {
        LOGE(TAG_SHIM, "Port-id state is already pending");
    }
    else
    {
        LOGE(TAG_SHIM, "Allocate called in a wrong state");
        return false;
    }

    return true;
}

/**
 * @brief Response to Flow allocation request.
 *
 * @param pxShimInstanceData
 * @param pxUserIpcp
 * @param xPortId
 * @return bool_t
 */
bool_t xShimFlowAllocateResponse(struct ipcpInstanceData_t *pxShimInstanceData,
                                 portId_t xPortId)

{

    shimFlow_t *pxFlow;
    struct ipcpInstance_t *pxIpcp, *pxUserIpcp;

    LOGI(TAG_SHIM, "Generating a Flow Allocate Response for a pending request");

    pxIpcp = pxIpcManagerActiveShimInstance();

    if (!pxShimInstanceData)
    {
        LOGE(TAG_SHIM, "Bogus data passed, bailing out");
        return false;
    }

    if (!is_port_id_ok(xPortId))
    {
        LOGE(TAG_SHIM, "Invalid port ID passed, bailing out");
        return false;
    }

    if (!pxIpcp)
    {
        LOGE(TAG_SHIM, "Not Shim Ipcp Instance found it");
        return false;
    }

    /* Searching for the Flow registered into the shim Instance Flow list */
    // Should include the portId into the search.
    pxFlow = prvShimFindFlowByPortId(pxShimInstanceData, xPortId);
    if (!pxFlow)
    {
        LOGE(TAG_SHIM, "Flow does not exist, you shouldn't call this");
        return false;
    }

    /* Check if the flow is already allocated*/
    if (pxFlow->ePortIdState != ePENDING)
    {
        LOGE(TAG_SHIM, "Flow is already allocated");
        return false;
    }

    /* On positive response, flow should transition to allocated state */

    /*Retrieving the User IPCP Instance */
    pxUserIpcp = pxFlow->pxUserIpcp;

    /*Call to IPCP User to flow binding*/
    configASSERT(pxUserIpcp->pxOps);
    RsAssert(pxUserIpcp->pxOps->flowBindingIpcp);

    if (!pxUserIpcp->pxOps->flowBindingIpcp(pxUserIpcp->pxData,
                                            xPortId,
                                            pxIpcp)) // It is passing the Normal Ipcp Instance.
    {
        LOGE(TAG_SHIM, "Could not bind flow with user_ipcp");
        return pdFALSE;
    }

    pxFlow->ePortIdState = eALLOCATED;
    pxFlow->xPortId = xPortId;

    pxFlow->pxDestHa = pxARPLookupGHA(pxFlow->pxDestPa);

    if (pxFlow->ePortIdState == eALLOCATED)
    {
        /*Send a Messages or notify to the Flow Allocator*/
    }

    return true;
}

/**
 * @brief applicationUnregister (naming-info local)
 * Primitive invoked before all other functions:
 * - Transform the naming-info structure into a single string (application-name)
 * separated by "-": ProcessName-ProcessInstance-EntityName-EntityInstance
 * - (Update LocalAddressProtocol which is part of the ARP module).
 * It is assumed only there is going to be one IPCP process in the N+1 DIF (over the Shim-DIF)
 *
 * @param pxData Shim IPCP Data
 * @param pxName pxName to register. Normal Instance pxName or DIFName
 * @return bool_t
 */
bool_t xShimApplicationUnregister(struct ipcpInstanceData_t *pxData, const name_t *pxName)
{
    LOGI(TAG_SHIM, "Application Unregistering");

    if (!pxData)
    {
        LOGE(TAG_SHIM, "Bogus data passed, bailing out");
        return false;
    }

    if (!pxName)
    {
        LOGE(TAG_SHIM, "Invalid name passed, bailing out");
        return false;
    }

    if (!pxData->pxAppName)
    {
        LOGE(TAG_SHIM, "Shim-WiFi has no application registered");
        return false;
    }

    /* Remove from ARP cache */
    if (pxData->pxAppHandle)
    {
        if (xARPRemove(pxData->pxAppHandle->pxPa, pxData->pxAppHandle->pxHa))
        {
            LOGE(TAG_SHIM, "Failed to remove APP entry from the cache");
            return false;
        }
        pxData->pxAppHandle = NULL;
    }

    if (pxData->pxDafHandle)
    {

        if (xARPRemove(pxData->pxDafHandle->pxPa, pxData->pxDafHandle->pxHa))
        {
            LOGE(TAG_SHIM, "Failed to remove DAF entry from the cache");
            return false;
        }
        pxData->pxDafHandle = NULL;
    }

    vRstrNameFree(pxData->pxAppName);
    pxData->pxAppName = NULL;
    vRstrNameFree(pxData->pxDafName);
    pxData->pxDafName = NULL;

    LOGI(TAG_SHIM, "Application unregister");

    return true;
}

bool_t xShimSDUWrite(struct ipcpInstanceData_t *pxData, portId_t xId, struct du_t *pxDu, bool_t uxBlocking)
{
    shimFlow_t *pxFlow;
    NetworkBufferDescriptor_t *pxNetworkBuffer;
    EthernetHeader_t *pxEthernetHeader;
    gha_t *pxSrcHw;
    gha_t *pxDestHw;
    size_t uxHeadLen, uxLength;
    struct timespec ts;
    RINAStackEvent_t xTxEvent = {
        .eEventType = eNetworkTxEvent,
        .xData.PV = NULL};
    unsigned char *pucArpPtr;

    LOGI(TAG_SHIM, "SDU write received");

    if (!pxData)
    {
        LOGE(TAG_SHIM, "Bogus data passed, bailing out");
        return false;
    }

    uxHeadLen = sizeof(EthernetHeader_t);          // Header length Ethernet
    uxLength = pxDu->pxNetworkBuffer->xDataLength; // total length PDU

    if (uxLength > MTU)
    {
        LOGE(TAG_SHIM, "SDU too large (%zu), dropping", uxLength);
        xDuDestroy(pxDu);
        return false;
    }

    pxFlow = prvShimFindFlowByPortId(pxData, xId);
    if (!pxFlow)
    {
        LOGE(TAG_SHIM, "Flow does not exist, you shouldn't call this");
        xDuDestroy(pxDu);
        return false;
    }

    // spin_lock_bh(&data->lock);
    LOGI(TAG_SHIM, "SDUWrite: flow state check %d", pxFlow->ePortIdState);
    if (pxFlow->ePortIdState != eALLOCATED)
    {
        LOGE(TAG_SHIM, "Flow is not in the right state to call this");
        xDuDestroy(pxDu);
        return false;
    }

    LOGI(TAG_SHIM, "SDUWrite: creating source GHA");
    pxSrcHw = pxCreateGHA(MAC_ADDR_802_3, pxData->pxPhyDev);
    if (!pxSrcHw)
    {
        LOGE(TAG_SHIM, "Failed to get source HW addr");
        xDuDestroy(pxDu);
        return false;
    }

    /*
    vARPPrintMACAddress(pxFlow->pxDestHa);
    */
    // pxDestHw = pxShimCreateGHA(MAC_ADDR_802_3, pxFlow->pxDestHa->xAddress);
    pxDestHw = pxFlow->pxDestHa;
    if (!pxDestHw)
    {
        LOGE(TAG_SHIM, "Destination HW address is unknown");
        xDuDestroy(pxDu);
        return false;
    }

    LOGI(TAG_SHIM, "SDUWrite: Encapsulating packet into Ethernet Frame");
    /* Get a Network Buffer with size total ethernet + PDU size*/

    pxNetworkBuffer = pxGetNetworkBufferWithDescriptor(uxHeadLen + uxLength, 250 * 1000);

    if (pxNetworkBuffer == NULL)
    {
        LOGE(TAG_SHIM, "pxNetworkBuffer is null");
        xDuDestroy(pxDu);
        return false;
    }

    pxEthernetHeader = vCastConstPointerTo_EthernetHeader_t(pxNetworkBuffer->pucEthernetBuffer);

    pxEthernetHeader->usFrameType = RsHtoNS(ETH_P_RINA);

    memcpy(pxEthernetHeader->xSourceAddress.ucBytes, pxSrcHw->xAddress.ucBytes, sizeof(pxSrcHw->xAddress));
    memcpy(pxEthernetHeader->xDestinationAddress.ucBytes, pxDestHw->xAddress.ucBytes, sizeof(pxDestHw->xAddress));

    /*Copy from the buffer PDU to the buffer Ethernet*/
    pucArpPtr = (unsigned char *)(pxEthernetHeader + 1);

    memcpy(pucArpPtr, pxDu->pxNetworkBuffer->pucEthernetBuffer, uxLength);

    pxNetworkBuffer->xDataLength = uxHeadLen + uxLength;

    /* Generate an event to sent or send from here*/
    /* Destroy pxDU no need anymore the stackbuffer*/
    xDuDestroy(pxDu);
    // LOGE(TAG_SHIM, "Releasing Buffer used in RMT");

    // vReleaseNetworkBufferAndDescriptor( pxDu->pxNetworkBuffer);

    /* ReleaseBuffer, no need anymore that why pdTRUE here */

    xTxEvent.xData.PV = (void *)pxNetworkBuffer;

    if (xSendEventStructToIPCPTask(&xTxEvent, 250 * 1000) == false)
    {
        LOGE(TAG_WIFI, "Failed to enqueue packet to network stack %p, len %zu", pxNetworkBuffer, pxNetworkBuffer->xDataLength);
        vReleaseNetworkBufferAndDescriptor(pxNetworkBuffer);
        return false;
    }

    LOGI(TAG_SHIM, "Data sent to the Shim IPCP TAsk");

    return true;
}

/* Structure to define the IPCP instance Operations */
static struct ipcpInstanceOps_t xShimWifiOps = {
    .flowAllocateRequest = xShimFlowAllocateRequest,   // ok
    .flowAllocateResponse = xShimFlowAllocateResponse, // ok
    .flowDeallocate = NULL,                            // xShimFlowDeallocate,             // ok
    .flowPrebind = NULL,                               // ok
    .flowBindingIpcp = NULL,                           // ok
    .flowUnbindingIpcp = NULL,                         // ok
    .flowUnbindingUserIpcp = NULL,                     // ok
    .nm1FlowStateChange = NULL,                        // ok

    .applicationRegister = xShimApplicationRegister,     // ok
    .applicationUnregister = xShimApplicationUnregister, // ok

    .assignToDif = NULL,     // ok
    .updateDifConfig = NULL, // ok

    .connectionCreate = NULL,        // ok
    .connectionUpdate = NULL,        // ok
    .connectionDestroy = NULL,       // ok
    .connectionCreateArrived = NULL, // ok
    .connectionModify = NULL,        // ok

    .duEnqueue = NULL, // ok
    .duWrite = xShimSDUWrite,

    .mgmtDuWrite = NULL, // ok
    .mgmtDuPost = NULL,  // ok

    .pffAdd = NULL,    // ok
    .pffRemove = NULL, // ok
    //.pff_dump                  = NULL,
    //.pff_flush                 = NULL,
    //.pff_modify		   		   = NULL,

    //.query_rib		  		   = NULL,

    .ipcpName = NULL, // ok
    .difName = NULL,  // ok
    //.ipcp_id		  		   = NULL,

    //.set_policy_set_param      = NULL,
    //.select_policy_set         = NULL,
    //.update_crypto_state	   = NULL,
    //.address_change            = NULL,
    //.dif_name		   		   = NULL,
    .maxSduSize = NULL};

/***
 * @brief Create the Shim WiFi instance. This primitive must called by the Manager, who is
 * going to create. pxShimCreate is generic to add new Shims with the same process.
 * @param xIpcpId Unique Id provided by the Manager.
 * @return ipcpInstance structure for registering into the Manager IPCP List.
 * */

struct ipcpInstance_t *pxShimWiFiCreate(ipcProcessId_t xIpcpId)
{
    struct ipcpInstance_t *pxInst;
    struct ipcpInstanceData_t *pxInstData;
    struct flowSpec_t *pxFspec;
    string_t pcInterfaceName = SHIM_INTERFACE;
    name_t *pxName;
    MACAddress_t *pxPhyDev;

    pxPhyDev = pvRsMemAlloc(sizeof(*pxPhyDev));
    if (!pxPhyDev)
    {
        LOGE(TAG_WIFI, "Failed to allocate memory for WiFi shim instance");
        return NULL;
    }

    pxPhyDev->ucBytes[0] = 0x00;
    pxPhyDev->ucBytes[1] = 0x00;
    pxPhyDev->ucBytes[2] = 0x00;
    pxPhyDev->ucBytes[3] = 0x00;
    pxPhyDev->ucBytes[4] = 0x00;
    pxPhyDev->ucBytes[5] = 0x00;

    /* Create an instance */
    pxInst = pvRsMemAlloc(sizeof(*pxInst));
    if (!pxInst)
        return NULL;

    /* Create Data instance and Flow Specifications*/
    pxInstData = pvRsMemAlloc(sizeof(*pxInstData));
    if (!pxInstData)
        return NULL;

    pxInst->pxData = pxInstData;
    pxFspec = pvRsMemAlloc(sizeof(*pxFspec));
    pxInst->pxData->pxFspec = pxFspec;

    /*Create Dif Name and Daf Name*/
    pxName = pvRsMemAlloc(sizeof(*pxName));
    /*pxDafName = pvPortMalloc(sizeof(struct ipcpInstanceData_t));*/

    pxName->pcProcessName = SHIM_PROCESS_NAME;
    pxName->pcEntityName = SHIM_ENTITY_NAME;
    pxName->pcProcessInstance = SHIM_PROCESS_INSTANCE;
    pxName->pcEntityInstance = SHIM_ENTITY_INSTANCE;

    pxInst->pxData->pxAppName = NULL;
    pxInst->pxData->pxDafName = NULL;

    /*Filling the ShimWiFi instance properly*/
    pxInst->pxData->pxName = pxName;
    pxInst->pxData->xId = xIpcpId;
    pxInst->pxData->pxPhyDev = pxPhyDev;
    pxInst->pxData->pcInterfaceName = pcInterfaceName;

    pxInst->pxData->pxFspec->ulAverageBandwidth = 0;
    pxInst->pxData->pxFspec->ulAverageSduBandwidth = 0;
    pxInst->pxData->pxFspec->ulDelay = 0;
    pxInst->pxData->pxFspec->ulJitter = 0;
    pxInst->pxData->pxFspec->ulMaxAllowableGap = -1;
    pxInst->pxData->pxFspec->ulMaxSduSize = 1500;
    pxInst->pxData->pxFspec->xOrderedDelivery = 0;
    pxInst->pxData->pxFspec->xPartialDelivery = 1;
    pxInst->pxData->pxFspec->ulPeakBandwidthDuration = 0;
    pxInst->pxData->pxFspec->ulPeakSduBandwidthDuration = 0;
    pxInst->pxData->pxFspec->ulUndetectedBitErrorRate = 0;

    pxInst->pxOps = &xShimWifiOps;
    pxInst->xType = eShimWiFi;
    pxInst->xId = xIpcpId;

    /*Initialialise flows list*/
    vShimFlowListInit();

    LOGI(TAG_SHIM, "Instance Created: %p, IPCP id:%d, Type: %d", pxInst, pxInst->xId, pxInst->xType);

    return pxInst;
}

/*static struct shim_ops shimWiFi_ops = {
    //.init = tcp_udp_init,
    //.fini = tcp_udp_fini,
    .create = pxShimWiFiCreate
    //.destroy = tcp_udp_destroy,
};*/