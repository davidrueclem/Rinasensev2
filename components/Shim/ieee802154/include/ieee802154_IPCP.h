

#ifndef IEEE802154_IPCP_H__INCLUDED
#define IEEE802154_IPCP_H__INCLUDED

#include "common/rina_ids.h"
#include "common/rina_gpha.h"

// #include "Arp826.h"
// #include "Arp826_defs.h"
// #include "du.h"
#include "common/rina_common_port.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define ADDR_MODE_NONE (0)     // PAN ID and address fields are not present
#define ADDR_MODE_RESERVED (1) // Reseved
#define ADDR_MODE_SHORT (2)    // Short address (16-bit)
#define ADDR_MODE_LONG (3)     // Extended address (64-bit)

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

    /* The IPC Process using ieee 802.15.4 */
    name_t *pxAppName;
    name_t *pxDafName;

    /* RINARP related */
    struct rinarpHandle_t *pxAppHandle;
    struct rinarpHandle_t *pxDafHandle;

    /* Flow control between this IPCP and the associated netdev. */
    unsigned int ucTxBusy;
};

    struct ipcpInstance_t *pxShim802154Create(ipcProcessId_t xIpcpId);
    bool_t xFlowAllocateResponse(struct ipcpInstanceData_t *pxShimInstanceData,
                                 portId_t xPortId);

    bool_t xShim802154EnrollToDIF(struct ipcpInstanceData_t *pxShimInstanceData);
    bool_t xShimIEEE802154SDUWrite(struct ipcpInstanceData_t *pxData, portId_t xId, struct du_t *pxDu, bool_t uxBlocking);
    bool_t xShim802154FlowAllocateRequest(struct ipcpInstanceData_t *pxData, struct ipcpInstance_t *pxUserIpcp,
        name_t *pxSourceInfo,
        name_t *pxDestinationInfo,
        portId_t xPortId);

#ifdef __cplusplus
}
#endif

#endif /* _COMPONENTS_SHIM_802154_INCLUDE_802154_IPCP_H*/
