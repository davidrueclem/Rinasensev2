#ifndef _COMPONENTS_RINA_MANAGER_IPCP_MANAGER_H
#define _COMPONENTS_RINA_MANAGER_IPCP_MANAGER_H

#include "common/num_mgr.h"
#include "IPCP_instance.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define TAG_MNGR "[ipcp-manager]"

	typedef struct xINSTANCE_TABLE_ROW
	{
		/*The Ipcp Instance to register*/
		struct ipcpInstance_t *pxIpcpInstance;

		/*Type of the Ipcp Instance to register*/
		ipcpInstanceType_t pxIpcpType;

		/*The Ipcp Id to registered*/
		ipcProcessId_t xIpcpId;

		/*Is the Ipcp Instace active?*/
		bool_t xActive;

	} InstanceTableRow_t;

	typedef struct xIPC_MANAGER
	{
		/*List of the Ipcp factories registered*/
		// factories_t *pxFactories;

		RsList_t xShimInstancesList;

		// flowAllocator_t * pxFlowAllocator;
		// InstanceTableRow_t * pxInstanceTable[ INSTANCES_IPCP_ENTRIES ];

		/*port Id manager*/
		NumMgr_t *pxPidm;

		/*IPCProcess Id manager*/
		NumMgr_t *pxIpcpIdm;

	} ipcManager_t;

	bool_t xIpcMngrInit(void);
	struct ipcpInstance_t *pxIpcMngrCreateShim(void);
	// bool_t xIpcMngrInitShim();
	portId_t xIpcMngrAllocatePortId(void);

	bool_t xIpcMngrInitRinaStack(void);
	struct ipcpInstance_t *pxIpcManagerActiveNormalInstance(void);
	struct ipcpInstance_t *pxIpcManagerActiveShimInstance(void);

	void xTestEnroll(struct ipcpInstance_t *pxIpcpShimWiFi);
	struct ipcpInstance_t *pxIpcManagerFindInstanceByType(ipcpInstanceType_t xType);

#ifdef __cplusplus
}
#endif

#endif //_COMPONENTS_RINA_MANAGER_IPCP_MANAGER_H
