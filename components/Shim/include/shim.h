

#ifndef SHIM_H_
#define SHIM_H_

#include "common/rina_ids.h"
#include "common/mac.h"
#include "shim_IPCP_events.h"

#ifdef __cplusplus
extern "C"
{
#endif

    struct shim_ops
    {
        // int (*init)(struct ipcp_factory_data *data);
        // int (*fini)(struct ipcp_factory_data *data);

        struct ipcpInstance *(*create)(ipcProcessId_t xIpcpId);
        // int (*destroy)(struct ipcp_factory_data *data,struct ipcp_instance *inst);
    };

    bool_t xShimIpcpInit(void);

    /* This must be moved to another header file */
    bool_t xIsCallingFromShimIpcpTask(void);

    bool_t xSendEventStructToShimIPCPTask(const ShimTaskEvent_t *pxEvent,
                                          useconds_t uxTimeoutUS);
    bool_t xSendEventToShimIPCPTask(eShimEvent_t eEvent);

    bool_t prxHandleAllocateResponse(void);

#ifdef __cplusplus
}
#endif

#endif /* SHIM_H_*/
