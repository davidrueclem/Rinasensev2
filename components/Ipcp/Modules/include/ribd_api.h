#ifndef _RIBD_API_H_INCLUDED
#define _RIBD_API_H_INCLUDED

#include "IPCP_normal_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

bool_t xRibdConnectToIpcp(struct ipcpInstanceData_t *pxIpcpData, name_t *pxSource, name_t *pxDestInfo, portId_t xN1flowPortId, authPolicy_t *pxAuth);
bool_t xRibdDisconnectToIpcp(portId_t xN1flowPortId);
bool_t xRibdConnectRToIpcp(struct ipcpInstanceData_t *pxIpcpData, name_t *pxSource, name_t *pxDestInfo, portId_t xN1flowPortId, authPolicy_t *pxAuth);
bool_t xRibdDisconnectToIpcp(portId_t xN1flowPortId);
bool_t xRibdProcessLayerManagementPDU(struct ipcpInstanceData_t *pxData, portId_t xN1flowPortId, struct du_t *pxDu);
bool_t xRibdSendRequest(string_t pcObjClass, string_t pcObjName, long objInst,
                        opCode_t eOpCode, portId_t xN1flowPortId, serObjectValue_t *pxObjVal);

bool_t xRibdSendResponse(string_t pcObjClass, string_t pcObjName, long objInst,
                         int result, string_t pcResultReason,
                         opCode_t eOpCode, int invokeId, portId_t xN1Port,
                         serObjectValue_t *pxObjVal);

bool_t xTest(void);

#ifdef __cplusplus
}
#endif

#endif // _RIBD_API_H_INCLUDED
