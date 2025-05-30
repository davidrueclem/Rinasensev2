#ifndef ENROLLMENT_API_H_INCLUDED
#define ENROLLMENT_API_H_INCLUDED

#include "common/rina_ids.h"

#include "IPCP_normal_defs.h"
#include "Enrollment.h"
#include "Ribd.h"

#ifdef __cplusplus
extern "C" {
#endif

void vEnrollmentInit(struct ipcpInstanceData_t *pxIpcpData, portId_t xPortId);

bool_t xEnrollmentEnroller(struct ribObject_t *pxEnrRibObj, serObjectValue_t *pxObjValue, string_t pcRemoteApName,
                           string_t pcLocalApName, int invokeId, portId_t xN1Port);

bool_t xEnrollmentHandleConnectR(struct ipcpInstanceData_t *pxData, string_t pcRemoteProcessName, portId_t xN1Port);
bool_t xEnrollmentHandleConnect(struct ipcpInstanceData_t *pxData, string_t pcRemoteProcessName, portId_t xN1Port);
bool_t xEnrollmentHandleStartR(string_t pcRemoteApName, serObjectValue_t *pxSerObjValue);
bool_t xEnrollmentHandleStopR(string_t pcRemoteApName);

bool_t xEnrollmentHandleStop(struct ribObject_t *pxEnrRibObj,
                             serObjectValue_t *pxObjValue, string_t pcRemoteApName,
                             string_t pcLocalProcessName, int invokeId, portId_t xN1Port);

bool_t xEnrollmentHandleOperationalStart(struct ribObject_t *pxOperRibObj, serObjectValue_t *pxSerObjectValue, string_t pcRemoteApName,
                                         string_t pxLocalApName, int invokeId, portId_t xN1Port);

address_t xEnrollmentGetNeighborAddress(string_t pcRemoteApName);
neighborInfo_t *pxEnrollmentFindNeighbor(string_t pcRemoteApName);

#ifdef __cplusplus
}
#endif

#endif // ENROLLMENT_API_H_INCLUDED
