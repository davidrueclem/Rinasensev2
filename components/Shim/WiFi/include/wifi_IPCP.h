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

#ifdef __cplusplus
extern "C"
{
#endif

    bool_t xShimFlowAllocateResponse(struct ipcpInstanceData_t *pxShimInstanceData,
                                     portId_t xPortId);

    bool_t xShimEnrollToDIF(struct ipcpInstanceData_t *pxShimInstanceData);
    struct ipcpInstance_t *pxShimWiFiCreate(ipcProcessId_t xIpcpId);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_IPCP_H__*/
